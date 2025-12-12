// doom.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "doom.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include "json.hpp"
using json = nlohmann::json;





#define MAX_LOADSTRING 100
//structures
struct Texture {
    int w;
    int h;
    uint32_t* pixels;
    Texture() : w(0), h(0), pixels(nullptr) {}
};
Texture texture_notfound;
Texture texture_brick;

struct Wall {
    float x1, y1, x2, y2;
    float tileX;
	Texture* texture;
    Wall(float x1N, float y1N, float x2N, float y2N, Texture* img) {
        x1 = x1N;
        y1 = y1N;
        x2 = x2N;
        y2 = y2N;
        texture = img;
    }
    Wall() {}
};;

struct Sector {
    std::vector<Wall> walls;
    Sector() {
        walls.push_back(Wall(100, 100, 200, 100, nullptr));
        walls.push_back(Wall(200, 100, 200, 200, nullptr));
        walls.push_back(Wall(200, 200, 100, 200, nullptr));
        walls.push_back(Wall(100, 200, 100, 100, nullptr));
    }
};

struct lineIntersection {
    bool intersect;
    float t, u;
    float x, y;
    Texture* texture;
    Wall* wall;
    lineIntersection() : intersect(false), t(0), u(0), x(0), y(0), wall(new Wall()), texture(nullptr) {}
};

struct Player {
    float x, y;
    float angle;
};;
// Globals
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
int width = 320;
int height = 200;
int currentSector = 0;
bool ctrlRPrev = false;
int frame = 0;
std::unordered_map<std::string, Texture> textures;
std::vector<Sector> map;
uint32_t* framebuffer = nullptr;
BITMAPINFO bmi;
Player player = { 150.0f, 150.0f, 0.0f };
const float PI_F = 3.14159265359f;
const float EPS = 1e-6f;


// temporary per-ray hit lists
std::vector<lineIntersection> intersectList;
// the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DOOM, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DOOM));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

void DrawPixel(int x, int y, uint32_t color)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    framebuffer[y * width + x] = color;
}

Texture LoadBMP(const char* filename)
{
    Texture img;
    FILE* f = nullptr;
    if (fopen_s(&f, filename, "rb") != 0 || !f) return img;

    BITMAPFILEHEADER fileHeader;
    if (fread(&fileHeader, sizeof(fileHeader), 1, f) != 1) { fclose(f); return img; }

    BITMAPINFOHEADER infoHeader;
    if (fread(&infoHeader, sizeof(infoHeader), 1, f) != 1) { fclose(f); return img; }

    img.w = infoHeader.biWidth;
    img.h = infoHeader.biHeight;

    if (infoHeader.biCompression != BI_RGB) { fclose(f); img.w = img.h = 0; return img; }

    int bytesPerPixel = infoHeader.biBitCount / 8;
    int dataSize = img.w * img.h * 4;
    uint32_t* pixels = (uint32_t*)malloc(dataSize);
    if (!pixels) { fclose(f); img.w = img.h = 0; return img; }

    int rowSize = ((infoHeader.biBitCount * img.w + 31) / 32) * 4;
    uint8_t* row = (uint8_t*)malloc(rowSize);
    if (!row) { free(pixels); fclose(f); img.w = img.h = 0; return img; }

    for (int y = img.h - 1; y >= 0; --y) {
        fread(row, rowSize, 1, f);
        for (int x = 0; x < img.w; x++) {
            uint8_t b = row[x * bytesPerPixel + 0];
            uint8_t g = row[x * bytesPerPixel + 1];
            uint8_t r = row[x * bytesPerPixel + 2];
            uint32_t pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
            pixels[y * img.w + x] = pixel;
        }
    }

    free(row);
    fclose(f);
    img.pixels = pixels;
    return img;
}

std::string readFileToString(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return ""; // Return an empty string or handle the error appropriately
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the entire file buffer into the stringstream
    file.close();
    return buffer.str(); // Get the string from the stringstream
}

void DrawLine(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ClearFrame(uint32_t color = 0xFF000000) // default opaque black
{
    for (int i = 0; i < width * height; ++i) framebuffer[i] = color;
}


static lineIntersection linelineintersection(Wall ray, Wall wall)
{
    lineIntersection li;
    
    float x1 = ray.x1;
    float y1 = ray.y1;
    float x2 = ray.x2;
    float y2 = ray.y2;
    float x3 = wall.x1;
    float y3 = wall.y1;
    float x4 = wall.x2;
    float y4 = wall.y2;

    float bottom = (x1-x2)*(y3-y4) - (y1-y2)*(x3-x4);
    if (fabs(bottom) < EPS*2) {
        li.intersect = false;
        return li;
    }

    float t = ((x1-x3)*(y3-y4) - (y1-y3)*(x3-x4)) / bottom;
    float u = ((x1-x2)*(y1-y3) - (y1-y2)*(x1-x3)) / bottom;
    u = -u;

    li.t = t;
    li.u = u;

    // Segment vs segment check
    li.intersect = (t >= 0.0f && t <= 1.0f) && (u >= 0.0f && u <= 1.0f);

    li.x = ray.x1 + t * (x2-x1);
    li.y = ray.y1 + t * (y2-y1);
    li.texture = wall.texture;
    li.wall = &wall;
    return li;
}


void calcRoom(const Sector& sector, float rayAngle)
{
    intersectList.clear();
    float maxDist = 2000.0f;
    Wall ray = { player.x, player.y,
                 player.x + cosf(rayAngle) * maxDist,
                 player.y + sinf(rayAngle) * maxDist,
                 nullptr };
    for (int i = 0; i < sector.walls.size(); ++i) {
        lineIntersection li = linelineintersection(ray, sector.walls[i]);
        if (li.intersect) intersectList.push_back(li);
    }
}


int reloadMap() {
    std::ifstream file("map.json");
    json j;
    file >> j;
    if (!j.contains("playerSpawn")) {
        std::cout << "attribute \"playerSpawn\" with a type of array was not found\n";
        return 1;
    }
    if (!j["playerSpawn"].is_array()) {
        std::cout << "attribute \"playerSpawn\" with a type of array was not found\n";
        return 1;
    }
    std::cout << j;

    map.clear();
    player.x = j["playerSpawn"][0];
    player.y = j["playerSpawn"][1];

    for (int i = 0;i < j.size();i++) {
        json v = j["sectors"][i];
        int wallCount = v["walls"].size();
        std::vector<Wall> walls = {};
        for (const auto& wallJ : v["walls"]) {
            std::string textureName = wallJ["texture"];
            if (textures.find(textureName) == textures.end())
                textures[textureName] = LoadBMP(textureName.c_str());

            std::vector<int> positions = wallJ["positions"].get<std::vector<int>>();
            Wall wall{
                static_cast<float>(positions[0]),
                static_cast<float>(positions[1]),
                static_cast<float>(positions[2]),
                static_cast<float>(positions[3]),
                &textures[textureName]
            };
            wall.tileX = wallJ["tileX"];
            walls.push_back(wall);
        }
        Sector sector = Sector();
        sector.walls = walls;
        map.push_back(sector);
    }

    return 0;
    //std::cout << name << "\n";
}
//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOOM));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    // load textures into the Image instances (not pointer indexing)
    texture_brick = LoadBMP("textures/bricks.bmp");
    if (!texture_brick.pixels)
        MessageBoxA(0, "Failed to load BMP! \"textures/bricks.bmp\", cotinueing witout", "Error", 0);

    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    RECT rc = { 0, 0, width*2, height*2 };
    AdjustWindowRect(&rc, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        0, 0,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    reloadMap();

    framebuffer = new uint32_t[width * height];
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    SetTimer(hWnd, 1, 16, NULL);

    
    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        StretchDIBits(hdc, 0, 0, w, h, 0, 0, width, height, framebuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        delete[] framebuffer;
        PostQuitMessage(0);
        break;
    case WM_TIMER:
    {
        frame++;

        HWND focusedWindow = GetFocus();
        if (focusedWindow) {

            if (GetAsyncKeyState(VK_LEFT) & 0x8000) player.angle -= 0.04f;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) player.angle += 0.04f;

            float speed = 2.5f;
            // typical forward/back: add cos/sin for forward
            if (GetAsyncKeyState('W') & 0x8000) {
                player.x += cosf(player.angle) * speed;
                player.y += sinf(player.angle) * speed;
            }
            if (GetAsyncKeyState('S') & 0x8000) {
                player.x -= cosf(player.angle) * speed;
                player.y -= sinf(player.angle) * speed;
            }
            if (GetAsyncKeyState('A') & 0x8000) {
                player.x -= cosf(player.angle + (PI_F / 2.0f)) * speed;
                player.y -= sinf(player.angle + (PI_F / 2.0f)) * speed;
            }
            if (GetAsyncKeyState('D') & 0x8000) {
                player.x += cosf(player.angle + (PI_F / 2.0f)) * speed;
                player.y += sinf(player.angle + (PI_F / 2.0f)) * speed;
            }
            bool ctrlRNow =
                (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState('R') & 0x8000);

            if (ctrlRNow && !ctrlRPrev) {
                reloadMap(); // fires 
            }
            ctrlRPrev = ctrlRNow;
        }
        ClearFrame(0xFF202020);
        if (map.size() < 1) {
            break;
        }

        const float FOV = PI_F / 2.0f;
        const float halfFOV = FOV * 0.5f;

        for (int x = 0; x < width; ++x) {
            float sx = (float)x / (float)width;
            float angleOffset = (sx - 0.5f) * FOV;
            float rayAngle = player.angle + angleOffset;

            calcRoom(map[currentSector], rayAngle);

            float closestT = INFINITY;
            float closestU = 0.0f;
            Wall* closestWall = new Wall();
            Texture* tex = &texture_notfound;
            for (size_t i = 0; i < intersectList.size(); ++i) {
                if (intersectList[i].t < closestT) {
                    closestT = intersectList[i].t;
                    closestU = intersectList[i].u;
                    closestWall = intersectList[i].wall;
                    if (intersectList[i].texture) tex = intersectList[i].texture;
                }
            }

            if (closestT < INFINITY) {
                float maxDist = 2000.0f;
                float hitDistWorld = closestT * maxDist;
                float perpDist = hitDistWorld * cosf(angleOffset);
                if (perpDist < 0.0001f) perpDist = 0.0001f;

                float projPlaneDist = (width * 0.5f) / tanf(FOV * 0.5f);
                const float wallHeight = 64.0f;
                int lineHeight = (int)((wallHeight / perpDist) * projPlaneDist);

                int drawStart = -lineHeight / 2 + height / 2;
                int drawEnd = lineHeight / 2 + height / 2;
              //  if (drawStart < 0) drawStart = 0;
              //  if (drawEnd >= height) drawEnd = height - 1;
                int columnHeight = drawEnd - drawStart;
                if (columnHeight <= 0) columnHeight = 1;

                float per = closestU * closestWall->tileX;
                per = per * tex->w;
                per = fmodf(per,tex->w);
                int texX = (int)(per);
                //int texX = (int)(closestU * (tex->w + 1));
                if (texX < 0) texX = 0;
                if (texX >= tex->w) texX = tex->w - 1;

                if (tex && tex->w > 0 && tex->h > 0) {
                    for (int y = drawStart; y <= drawEnd; ++y) {
                        float v = (float)(y - drawStart) / (float)columnHeight;
                        int texY = (int)(v * (tex->h - 1));
                        if (texY < 0) texY = 0;
                        if (texY >= tex->h) texY = tex->h - 1;

                        uint32_t sample = tex->pixels[texY * tex->w + texX];
						//sample = 0xFF000000; // ensure opaque
                        DrawPixel(x, y, sample);
                    }
                }
                else {
                    // draw a solid color if no texture
                    for (int y = drawStart; y <= drawEnd; ++y) {
						float v = (float)(y - drawStart) / (float)columnHeight;
                        int texY = (int)(v * (32 - 1));
                        int texX = (int)(closestU * (32 - 1));
                        if (texY < 0) texY = 0;
                        if (texY >= 32) texY = 32 - 1;
                        
                        if ((texY < 16) ^ (texX < 16))
                            DrawPixel(x, y, 0xFF000000); // black for missing texture
						else
                            DrawPixel(x, y, 0xFFFF00FF); // magenta for missing texture
					}
                }
            }

        }

        InvalidateRect(hWnd, NULL, FALSE);
    }
    break;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}