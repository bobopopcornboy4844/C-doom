For the game to load it needs a map file "map.json",
when making custom maps there are things you need to know
you do not need the "tileX" property in the wall
THE ERROR CODES:
-- player spawn --
100 : No "playerSpawn"
101 : "playerSpawn" is not an array
102 : "playerSpawn" does not have exactly 2 ints
-- sectors --
201 : No "sectors"
202 : "sectors" is not an array
203 : "sectors" is empty
-- walls --
301 : one of the sectors does not have "walls"
302 : one of the sectors "walls" is not an array
311 : one of walls does nto contain "positions"
312 : one of the walls "positions" is not an array
313 : one of the walls "positions" length is not exactly 4
321 : one of the walls does not have the "texture" property
322 : one of the walls "texture" propery is not a string
