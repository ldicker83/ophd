#pragma once

#include <iostream>

using namespace std;

namespace constants
{
	// MISCELLANEOUS
	const string	EMPTY_STR = "";
	const string	SAVE_GAME_VERSION = "0.01";

	// ROBOTS
	const string	ROBODIGGER = "Robodigger";
	const string	ROBODOZER = "Robodozer";
	const string	ROBOMINER = "Robominer";


	// STRUCTURES
	const string	AGRIDOME = "Agricultural Dome";
	const string	AIR_SHAFT = "Air Shaft";
	const string	CHAP = "CHAP Facility";
	const string	COMMAND_CENTER = "Command Center";
	const string	MINE_FACILITY = "Mine Facility";
	const string	RESIDENCE = "Residential Facility";
	const string	SEED_FACTORY = "SEED Factory";
	const string	SEED_LANDER = "SEED Lander";
	const string	SEED_POWER = "SEED Power";
	const string	SEED_SMELTER = "SEED Smelter";
	const string	STORAGE_TANKS = "Storage Tanks";
	const string	TUBE = "Tube";

	// TUBE STATES
	const string	AG_TUBE_INTERSECTION = "ag_intersection";
	const string	AG_TUBE_RIGHT = "ag_right";
	const string	AG_TUBE_LEFT = "ag_left";

	const string	UG_TUBE_INTERSECTION = "ug_intersection";
	const string	UG_TUBE_RIGHT = "ug_right";
	const string	UG_TUBE_LEFT = "ug_left";

	// STRUCTURE ANIMATION STATES
	const string	STRUCTURE_STATE_CONSTRUCTION = "construction";
	const string	STRUCTURE_STATE_OPERATIONAL = "operational";
	const string	STRUCTURE_STATE_OPERATIONAL_UG = "operational-ug";
	const string	STRUCTURE_STATE_DESTROYED = "destroyed";

	// PLANET SPRITE SHEET PATHS
	const string	PLANET_TYPE_MERCURY_PATH = "planets/planet_d.png";
	const string	PLANET_TYPE_MARS_PATH = "planets/planet_c.png";
	const string	PLANET_TYPE_GANYMEDE_PATH = "planets/planet_e.png";

}
