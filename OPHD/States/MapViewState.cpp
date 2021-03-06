#include "MapViewState.h"
#include "MainMenuState.h"
#include "MainReportsUiState.h"

#include "../Constants.h"
#include "../DirectionOffset.h"
#include "../Cache.h"
#include "../GraphWalker.h"
#include "../StructureCatalogue.h"
#include "../StructureManager.h"

#include "../Map/Tile.h"
#include "../Map/TileMap.h"

#include "../Things/Robots/Robots.h"
#include "../Things/Structures/Structures.h"

#include <NAS2D/Utility.h>
#include <NAS2D/EventHandler.h>
#include <NAS2D/Renderer/Renderer.h>

#include <algorithm>
#include <sstream>
#include <vector>

// Disable some warnings that can be safely ignored.
#pragma warning(disable: 4244) // possible loss of data (floats to int and vice versa)


using namespace NAS2D;


const std::string MAP_TERRAIN_EXTENSION = "_a.png";
const std::string MAP_DISPLAY_EXTENSION = "_b.png";

extern Point<int> MOUSE_COORDS;


Rectangle<int> RESOURCE_PANEL_PIN{0, 1, 8, 19};
Rectangle<int> POPULATION_PANEL_PIN{675, 1, 8, 19};

std::string CURRENT_LEVEL_STRING;

std::map <int, std::string> LEVEL_STRING_TABLE = 
{
	{ constants::DepthSurface, constants::LevelSurface },
	{ constants::DepthUnderground1, constants::Levelunderground1 },
	{ constants::DepthUnderground2, constants::Levelunderground2 },
	{ constants::DepthUnderground3, constants::Levelunderground3 },
	{ constants::DepthUnderground4, constants::Levelunderground4 }
};


const Font* MAIN_FONT = nullptr;


/** \fixme Find a sane place for these */
struct RobotMeta
{
	std::string name;
	const int sheetIndex;
};

const std::map<Robot::Type, RobotMeta> RobotMetaTable
{
	{ Robot::Type::Digger, RobotMeta{constants::Robodigger, constants::RobodiggerSheetId}},
	{ Robot::Type::Dozer, RobotMeta{constants::Robodozer, constants::RobodozerSheetId}},
	{ Robot::Type::Miner, RobotMeta{constants::Robominer, constants::RobominerSheetId}}
};


static NAS2D::Rectangle<int> buildAreaRectFromTile(const Tile& centerTile, int radius)
{
	const NAS2D::Point areaStartPoint
	{
		std::clamp(centerTile.position().x - radius, 0, 299),
		std::clamp(centerTile.position().y - radius, 0, 149)
	};

	const NAS2D::Point areaEndPoint
	{
		std::clamp(centerTile.position().x + radius, 0, 299),
		std::clamp(centerTile.position().y + radius, 0, 149)
	};

	return NAS2D::Rectangle<int>::Create(areaStartPoint, areaEndPoint);
}


static void pushAgingRobotMessage(const Robot* robot, const Point<int> position, NotificationArea& notificationArea)
{
	const auto robotLocationText = "(" + std::to_string(position.x) + ", " + std::to_string(position.y) + ")";

	if (robot->fuelCellAge() == 190) /// \fixme magic number
	{
		notificationArea.push("Aging Robot",
			"Robot '" + robot->name() + "' at location " + robotLocationText + " is approaching its maximum age.",
			position,
			NotificationArea::NotificationType::Warning);
	}
	else if (robot->fuelCellAge() == 195) /// \fixme magic number
	{
		notificationArea.push("Aging Robot",
			"Robot '" + robot->name() + "' at location " + robotLocationText + " will fail in a few turns. Replace immediately.",
			position,
			NotificationArea::NotificationType::Critical);
	}
}


MapViewState::MapViewState(MainReportsUiState& mainReportsState, const std::string& savegame) :
	mMainReportsState(mainReportsState),
	mCrimeExecution(mNotificationArea),
	mLoadingExisting(true),
	mExistingToLoad(savegame)
{
	ccLocation() = CcNotPlaced;
	Utility<EventHandler>::get().windowResized().connect(this, &MapViewState::onWindowResized);
}


MapViewState::MapViewState(MainReportsUiState& mainReportsState, const Planet::Attributes& planetAttributes, Difficulty selectedDifficulty) :
	mMainReportsState(mainReportsState),
	mTileMap(new TileMap(planetAttributes.mapImagePath, planetAttributes.tilesetPath, planetAttributes.maxDepth, planetAttributes.maxMines, planetAttributes.hostility)),
	mCrimeExecution(mNotificationArea),
	mPlanetAttributes(planetAttributes),
	mMapDisplay{std::make_unique<Image>(planetAttributes.mapImagePath + MAP_DISPLAY_EXTENSION)},
	mHeightMap{std::make_unique<Image>(planetAttributes.mapImagePath + MAP_TERRAIN_EXTENSION)}
{
	difficulty(selectedDifficulty);
	ccLocation() = CcNotPlaced;
	Utility<EventHandler>::get().windowResized().connect(this, &MapViewState::onWindowResized);
}


MapViewState::~MapViewState()
{
	delete mPathSolver;

	scrubRobotList();
	delete mTileMap;

	Utility<Renderer>::get().setCursor(PointerType::POINTER_NORMAL);

	auto& eventHandler = Utility<EventHandler>::get();
	eventHandler.activate().disconnect(this, &MapViewState::onActivate);
	eventHandler.keyDown().disconnect(this, &MapViewState::onKeyDown);
	eventHandler.mouseButtonDown().disconnect(this, &MapViewState::onMouseDown);
	eventHandler.mouseButtonUp().disconnect(this, &MapViewState::onMouseUp);
	eventHandler.mouseDoubleClick().disconnect(this, &MapViewState::onMouseDoubleClick);
	eventHandler.mouseMotion().disconnect(this, &MapViewState::onMouseMove);
	eventHandler.mouseWheel().disconnect(this, &MapViewState::onMouseWheel);
	eventHandler.windowResized().disconnect(this, &MapViewState::onWindowResized);

	eventHandler.textInputMode(false);

	NAS2D::Utility<std::map<class MineFacility*, Route>>::get().clear();
}


void MapViewState::setPopulationLevel(PopulationLevel popLevel)
{
	mLandersColonist = static_cast<int>(popLevel);
	mLandersCargo = 2; ///\todo This should be set based on difficulty level.
}


/**
 * Initialize values, the UI and set up event handling.
 */
void MapViewState::initialize()
{
	// UI
	initUi();
	auto& renderer = Utility<Renderer>::get();

	renderer.setCursor(PointerType::POINTER_NORMAL);

	setupUiPositions(renderer.size());

	CURRENT_LEVEL_STRING = constants::LevelSurface;

	mPopulationPool.population(&mPopulation);

	if (mLoadingExisting) 
	{ 
		load(mExistingToLoad); 
	}
	else 
	{ 
		// StructureCatalogue is initialized in load routine if saved game present to load existing structures
		StructureCatalogue::init(mPlanetAttributes.meanSolarDistance); 
	}

	resetPoliceOverlays();

	Utility<Renderer>::get().fadeIn(constants::FadeSpeed);

	auto& eventHandler = Utility<EventHandler>::get();

	eventHandler.activate().connect(this, &MapViewState::onActivate);
	eventHandler.keyDown().connect(this, &MapViewState::onKeyDown);
	eventHandler.mouseButtonDown().connect(this, &MapViewState::onMouseDown);
	eventHandler.mouseButtonUp().connect(this, &MapViewState::onMouseUp);
	eventHandler.mouseDoubleClick().connect(this, &MapViewState::onMouseDoubleClick);
	eventHandler.mouseMotion().connect(this, &MapViewState::onMouseMove);
	eventHandler.mouseWheel().connect(this, &MapViewState::onMouseWheel);

	eventHandler.textInputMode(true);

	MAIN_FONT = &fontCache.load(constants::FONT_PRIMARY, constants::FontPrimaryNormal);

	delete mPathSolver;
	mPathSolver = new micropather::MicroPather(mTileMap);
}


void MapViewState::_activate()
{
	unhideUi();
}


void MapViewState::_deactivate()
{
	mGameOverDialog.enabled(false);
	mGameOptionsDialog.enabled(false);

	hideUi();
}


void MapViewState::focusOnStructure(Structure* structure)
{
	if (!structure) { return; }
	mTileMap->centerMapOnTile(&Utility<StructureManager>::get().tileFromStructure(structure));
}


void MapViewState::difficulty(Difficulty difficulty)
{
	mDifficulty = difficulty;
	mCrimeRateUpdate.difficulty(difficulty);
	mCrimeExecution.difficulty(difficulty);
}


/**
 * Updates the entire state of the game.
 */
State* MapViewState::update()
{
	auto& renderer = Utility<Renderer>::get();
	const auto renderArea = NAS2D::Rectangle<int>::Create({0, 0}, renderer.size());

	// Game's over, don't bother drawing anything else
	if (mGameOverDialog.visible())
	{
		renderer.drawBoxFilled(renderArea, NAS2D::Color::Black);
		mGameOverDialog.update();

		return this;
	}

	renderer.drawImageStretched(mBackground, renderArea);

	// explicit current level
	const Font* font = &fontCache.load(constants::FONT_PRIMARY_BOLD, constants::FontPrimaryMedium);
	const auto currentLevelPosition = mMiniMapBoundingBox.crossXPoint() - font->size(CURRENT_LEVEL_STRING) - NAS2D::Vector{0, 12};
	renderer.drawText(*font, CURRENT_LEVEL_STRING, currentLevelPosition, NAS2D::Color::White);

	if (!modalUiElementDisplayed())
	{
		mTileMap->injectMouse(MOUSE_COORDS);
	}

	mTileMap->draw();

	// FIXME: Ugly / hacky
	if (modalUiElementDisplayed())
	{
		renderer.drawBoxFilled(renderArea, NAS2D::Color{0, 0, 0, 165});
	}

	drawUI();

	return this;
}


/**
 * Get the total amount of storage given a structure class and capacity of each
 * structure.
 */
int MapViewState::totalStorage(Structure::StructureClass structureClass, int capacity)
{
	int storageCapacity = 0;

	// Command Center has a limited amount of storage for when colonists first land.
	if (ccLocation() != CcNotPlaced)
	{
		storageCapacity += constants::BaseStorageCapacity;
	}

	const auto& structures = Utility<StructureManager>::get().structureList(structureClass);
	for (auto structure : structures)
	{
		if (structure->operational() || structure->isIdle())
		{
			storageCapacity += capacity;
		}
	}

	return storageCapacity;
}


int MapViewState::refinedResourcesInStorage()
{
	int total = 0;
	for (size_t i = 0; i < mResourcesCount.resources.size(); ++i)
	{
		total += mResourcesCount.resources[i];
	}
	return total;
}


void MapViewState::countPlayerResources()
{
	auto& storageTanks = NAS2D::Utility<StructureManager>::get().getStructures<StorageTanks>();
	auto& command = NAS2D::Utility<StructureManager>::get().getStructures<CommandCenter>();

	std::vector<Structure*> storage;
	storage.insert(storage.end(), command.begin(), command.end());
	storage.insert(storage.end(), storageTanks.begin(), storageTanks.end());

	StorableResources resources;
	for (auto structure : storage)
	{
		resources += structure->storage();
	}
	mResourcesCount = resources;
}


/**
 * Window activation handler.
 */
void MapViewState::onActivate(bool /*newActiveValue*/)
{
	mLeftButtonDown = false;
}


void MapViewState::onWindowResized(NAS2D::Vector<int> newSize)
{
	setupUiPositions(newSize);
	mTileMap->initMapDrawParams(newSize);
}


/**
 * Key down event handler.
 */
void MapViewState::onKeyDown(EventHandler::KeyCode key, EventHandler::KeyModifier mod, bool /*repeat*/)
{
	if (!active()) { return; }

	// FIXME: Ugly / hacky
	if (modalUiElementDisplayed())
	{
		return;
	}

	if (key == EventHandler::KeyCode::KEY_F1)
	{
		mReportsUiSignal();
		return;
	}

	bool viewUpdated = false; // don't like flaggy code like this
	Point<int> pt = mTileMap->mapViewLocation();

	switch(key)
	{
		case EventHandler::KeyCode::KEY_w:
		case EventHandler::KeyCode::KEY_UP:
			viewUpdated = true;
			pt += DirectionNorth;
			break;

		case EventHandler::KeyCode::KEY_s:
		case EventHandler::KeyCode::KEY_DOWN:
			viewUpdated = true;
			pt += DirectionSouth;
			break;

		case EventHandler::KeyCode::KEY_a:
		case EventHandler::KeyCode::KEY_LEFT:
			viewUpdated = true;
			pt += DirectionWest;
			break;

		case EventHandler::KeyCode::KEY_d:
		case EventHandler::KeyCode::KEY_RIGHT:
			viewUpdated = true;
			pt += DirectionEast;
			break;

		case EventHandler::KeyCode::KEY_0:
			viewUpdated = true;
			changeViewDepth(0);
			break;

		case EventHandler::KeyCode::KEY_1:
			viewUpdated = true;
			changeViewDepth(1);
			break;

		case EventHandler::KeyCode::KEY_2:
			viewUpdated = true;
			changeViewDepth(2);
			break;

		case EventHandler::KeyCode::KEY_3:
			viewUpdated = true;
			changeViewDepth(3);
			break;

		case EventHandler::KeyCode::KEY_4:
			viewUpdated = true;
			changeViewDepth(4);
			break;

		case EventHandler::KeyCode::KEY_PAGEUP:
			viewUpdated = true;
			changeViewDepth(mTileMap->currentDepth() - 1);
			break;

		case EventHandler::KeyCode::KEY_PAGEDOWN:
			viewUpdated = true;
			changeViewDepth(mTileMap->currentDepth() + 1);
			break;


		case EventHandler::KeyCode::KEY_HOME:
			viewUpdated = true;
			changeViewDepth(0);
			break;

		case EventHandler::KeyCode::KEY_END:
			viewUpdated = true;
			changeViewDepth(mTileMap->maxDepth());
			break;

		case EventHandler::KeyCode::KEY_F10:
			if (Utility<EventHandler>::get().control(mod) && Utility<EventHandler>::get().shift(mod))
			{
				StorableResources resourcesToAdd{ 1000, 1000, 1000, 1000 };
				addRefinedResources(resourcesToAdd);
				countPlayerResources();
				updateStructuresAvailability();
			}
			break;

		case EventHandler::KeyCode::KEY_F2:
			mFileIoDialog.scanDirectory(constants::SaveGamePath);
			mFileIoDialog.setMode(FileIo::FileOperation::Save);
			mFileIoDialog.show();
			break;

		case EventHandler::KeyCode::KEY_F3:
			mFileIoDialog.scanDirectory(constants::SaveGamePath);
			mFileIoDialog.setMode(FileIo::FileOperation::Load);
			mFileIoDialog.show();
			break;

		case EventHandler::KeyCode::KEY_ESCAPE:
			clearMode();
			resetUi();
			break;

		case EventHandler::KeyCode::KEY_ENTER:
			if (mBtnTurns.enabled()) { nextTurn(); }
			break;

		default:
			break;
	}

	if (viewUpdated)
	{
		mTileMap->mapViewLocation(pt);
	}
}


/**
 * Mouse Down event handler.
 */
void MapViewState::onMouseDown(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (!active()) { return; }

	if (modalUiElementDisplayed()) { return; }

	if (mWindowStack.pointInWindow(MOUSE_COORDS))
	{
		mWindowStack.updateStack(MOUSE_COORDS);
		return;
	}

	if (button == EventHandler::MouseButton::Right || button == EventHandler::MouseButton::Middle)
	{
		if (mInsertMode != InsertMode::None)
		{
			resetUi();
			return;
		}

		if (!mTileMap->tileHighlightVisible()) { return; }
		if (!mTileMap->isValidPosition(mTileMap->tileMouseHover())) { return; }

		auto& tile = mTileMap->getTile(mTileMap->tileMouseHover());
		if (tile.empty() && mTileMap->boundingBox().contains(MOUSE_COORDS))
		{
			clearSelections();
			mTileInspector.tile(&tile);
			mTileInspector.show();
			mWindowStack.bringToFront(&mTileInspector);
		}
		else if (tile.thingIsRobot())
		{
			mRobotInspector.focusOnRobot(tile.robot());
			mRobotInspector.show();
			mWindowStack.bringToFront(&mRobotInspector);
		}
		else if (tile.thingIsStructure())
		{
			Structure* structure = tile.structure();

			const bool inspectModifier = NAS2D::Utility<EventHandler>::get().query_shift() ||
				button == EventHandler::MouseButton::Middle;

			const bool notDisabled = structure->operational() || structure->isIdle();

			if (structure->isFactory() && notDisabled && !inspectModifier)
			{
				mFactoryProduction.factory(static_cast<Factory*>(structure));
				mFactoryProduction.show();
				mWindowStack.bringToFront(&mFactoryProduction);
			}
			else if (structure->isWarehouse() && notDisabled && !inspectModifier)
			{
				mWarehouseInspector.warehouse(static_cast<Warehouse*>(structure));
				mWarehouseInspector.show();
				mWindowStack.bringToFront(&mWarehouseInspector);
			}
			else if (structure->isMineFacility() && notDisabled && !inspectModifier)
			{
				mMineOperationsWindow.mineFacility(static_cast<MineFacility*>(structure));
				mMineOperationsWindow.show();
				mWindowStack.bringToFront(&mMineOperationsWindow);
			}
			else
			{
				mStructureInspector.structure(structure);
				mStructureInspector.show();
				mWindowStack.bringToFront(&mStructureInspector);
			}
		}
	}

	if (button == EventHandler::MouseButton::Left)
	{
		mLeftButtonDown = true;

		Point<int> pt = mTileMap->mapViewLocation();

		if (mTooltipSystemButton.rect().contains(MOUSE_COORDS))
		{
			mGameOptionsDialog.show();
			resetUi();
			return;
		}

		if (RESOURCE_PANEL_PIN.contains(MOUSE_COORDS)) { mPinResourcePanel = !mPinResourcePanel; }
		if (POPULATION_PANEL_PIN.contains(MOUSE_COORDS)) { mPinPopulationPanel = !mPinPopulationPanel; }

		if (mMoveNorthIconRect.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionNorth);
		}
		else if (mMoveSouthIconRect.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionSouth);
		}
		else if (mMoveEastIconRect.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionEast);
		}
		else if (mMoveWestIconRect.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionWest);
		}
		else if (mMoveUpIconRect.contains(MOUSE_COORDS))
		{
			changeViewDepth(mTileMap->currentDepth() - 1);
		}
		else if (mMoveDownIconRect.contains(MOUSE_COORDS))
		{
			changeViewDepth(mTileMap->currentDepth()+1);
		}

		// MiniMap Check
		if (mMiniMapBoundingBox.contains(MOUSE_COORDS) && !mWindowStack.pointInWindow(MOUSE_COORDS))
		{
			setMinimapView();
		}
		// Click was within the bounds of the TileMap.
		else if (mTileMap->boundingBox().contains(MOUSE_COORDS))
		{
			auto& eventHandler = Utility<EventHandler>::get();
			if (mInsertMode == InsertMode::Structure)
			{
				placeStructure();
			}
			else if (mInsertMode == InsertMode::Robot)
			{
				placeRobot();
			}
			else if ( (mInsertMode == InsertMode::Tube) && eventHandler.query_shift())
			{
				placeTubeStart();
			}
			else if (mInsertMode == InsertMode::Tube)
			{
				placeTubes();
			}
		}
	}
}


void MapViewState::onMouseDoubleClick(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (!active()) { return; }

	if (button == EventHandler::MouseButton::Left)
	{
		if (mWindowStack.pointInWindow(MOUSE_COORDS)) { return; }
		if (!mTileMap->tileHighlightVisible()) { return; }
		if (!mTileMap->isValidPosition(mTileMap->tileMouseHover())) { return; }

		auto& tile = mTileMap->getTile(mTileMap->tileMouseHover());
		if (tile.thingIsStructure())
		{
			Structure* structure = tile.structure();

			if (structure->isFactory()) { mMainReportsState.selectFactoryPanel(structure); }
			else if (structure->isWarehouse()) { mMainReportsState.selectWarehousePanel(structure); }
			else if (structure->isMineFacility() || structure->structureClass() == Structure::StructureClass::Smelter) { mMainReportsState.selectMinePanel(structure); }
			else { return; } // avoids showing the full-screen UI on unhandled structures.

			mReportsUiSignal();
		}
	}
}


/**
* Mouse Up event handler.
*/
void MapViewState::onMouseUp(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (button == EventHandler::MouseButton::Left)
	{
		mLeftButtonDown = false;
		auto& eventHandler = Utility<EventHandler>::get();
		if ((mInsertMode == InsertMode::Tube) && eventHandler.query_shift())
		{
			placeTubeEnd();
		}
	}
}


/**
* Mouse motion event handler.
*/
void MapViewState::onMouseMove(int /*x*/, int /*y*/, int /*rX*/, int /*rY*/)
{
	if (!active()) { return; }


	if (mLeftButtonDown)
	{
		if (mMiniMapBoundingBox.contains(MOUSE_COORDS))
		{
			setMinimapView();
		}
	}

	mTileMapMouseHover = mTileMap->tileMouseHover();
}


/**
 * Mouse wheel event handler.
 */
void MapViewState::onMouseWheel(int /*x*/, int y)
{
	if (mInsertMode != InsertMode::Tube) { return; }

	y > 0 ? mConnections.decrementSelection() : mConnections.incrementSelection();
}


/**
 * Changes the current view depth.
 */
void MapViewState::changeViewDepth(int depth)
{
	if (mBtnTogglePoliceOverlay.toggled())
	{
		changePoliceOverlayDepth(mTileMap->currentDepth(), depth);
	}

	mTileMap->currentDepth(depth);

	if (mInsertMode != InsertMode::Robot) { clearMode(); }
	populateStructureMenu();
	updateCurrentLevelString(mTileMap->currentDepth());
}


void MapViewState::setMinimapView()
{
	const auto viewSizeInTiles = NAS2D::Vector{mTileMap->edgeLength(), mTileMap->edgeLength()};
	const auto position = NAS2D::Point{0, 0} + (MOUSE_COORDS - mMiniMapBoundingBox.startPoint()) - viewSizeInTiles / 2;

	mTileMap->mapViewLocation(position);
}


/**
 * Clears the build mode.
 */
void MapViewState::clearMode()
{
	mInsertMode = InsertMode::None;
	Utility<Renderer>::get().setCursor(PointerType::POINTER_NORMAL);

	mCurrentStructure = StructureID::SID_NONE;
	mCurrentRobot = Robot::Type::None;

	clearSelections();
}


void MapViewState::insertTube(ConnectorDir dir, int depth, Tile* tile)
{

	if (dir == ConnectorDir::CONNECTOR_VERTICAL)
	{
		throw std::runtime_error("MapViewState::insertTube() called with invalid ConnectorDir paramter.");
	}

	Utility<StructureManager>::get().addStructure(new Tube(dir, depth != 0), tile);
}


void MapViewState::placeTubes()
{
	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	// Check the basics.
	if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	auto cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	if (validTubeConnection(mTileMap, mTileMapMouseHover, cd))
	{
		insertTube(cd, mTileMap->currentDepth(), &mTileMap->getTile(mTileMapMouseHover));

		// FIXME: Naive approach -- will be slow with larger colonies.
		Utility<StructureManager>::get().disconnectAll();
		checkConnectedness();
	}
	else
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertTubeInvalidLocation);
	}
}

void MapViewState::placeTubeStart()
{
	mPlacingTube = false;

	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	// Check the basics.
	if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	ConnectorDir cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	if (!validTubeConnection(mTileMap, mTileMapMouseHover, cd))
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertTubeInvalidLocation);
		return;
	}
	mTubeStart = tile->position();
	mPlacingTube = true;
}


void MapViewState::placeTubeEnd()
{
	if (!mPlacingTube) return;
	mPlacingTube = false;
	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	ConnectorDir cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	const auto startEndDirection = tile->position() - mTubeStart;
	NAS2D::Vector<int> tubeEndOffset;

	switch (cd)
	{
	case ConnectorDir::CONNECTOR_INTERSECTION:
		// Determine direction of largest change, and snap to that axis
		if (abs(startEndDirection.x) >= abs(startEndDirection.y)){
			tubeEndOffset = {startEndDirection.x, 0};
		}else{
			tubeEndOffset = {0, startEndDirection.y};
		}
		break;
	case ConnectorDir::CONNECTOR_RIGHT:
		tubeEndOffset = {startEndDirection.x, 0};
		break;
	case ConnectorDir::CONNECTOR_LEFT:
		tubeEndOffset = {0, startEndDirection.y};
		break;
	default:
		return;
	}
	// Tube is axis aligned, so either x or y is 0
	const int tubeLength = abs(tubeEndOffset.x + tubeEndOffset.y);
	const auto tubeDirection = tubeEndOffset / tubeLength;
	const auto tubeEnd = mTubeStart + tubeEndOffset;

	auto position = mTubeStart;
	bool endReach = false;

	do {
		tile = mTileMap->getVisibleTile(mTubeStart, mTileMap->currentDepth());
		if (!tile) {
			endReach = true;
		}else if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()){
			endReach = true;
		}else if (!validTubeConnection(mTileMap, position, cd)){
			endReach = true;
		}else{
			insertTube(cd, mTileMap->currentDepth(), &mTileMap->getTile(position));

			// FIXME: Naive approach -- will be slow with larger colonies.
			Utility<StructureManager>::get().disconnectAll();
			checkConnectedness();
		}

		if (position == tubeEnd) endReach = true;
		position += tubeDirection;
	} while (!endReach);
}


void MapViewState::placeRobodozer(Tile& tile)
{
	Robot* robot = mRobotPool.getDozer();

	if (tile.thing() && !tile.thingIsStructure())
	{
		return;
	}
	else if (tile.index() == TerrainType::Dozed && !tile.thingIsStructure())
	{
		doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertTileBulldozed);
		return;
	}
	else if (tile.mine())
	{
		if (tile.mine()->depth() != mTileMap->maxDepth() || !tile.mine()->exhausted())
		{
			doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertMineNotExhausted);
			return;
		}

		mMineOperationsWindow.hide();
		mTileMap->removeMineLocation(mTileMap->tileMouseHover());
		tile.pushMine(nullptr);
		for (int i = 0; i <= mTileMap->maxDepth(); ++i)
		{
			auto& mineShaftTile = mTileMap->getTile(mTileMap->tileMouseHover(), i);
			Utility<StructureManager>::get().removeStructure(mineShaftTile.structure());
		}
	}
	else if (tile.thingIsStructure())
	{
		if (mStructureInspector.structure() == tile.structure()) { mStructureInspector.hide(); }

		Structure* structure = tile.structure();

		if (structure->isMineFacility()) { return; }
		if (structure->structureClass() == Structure::StructureClass::Command)
		{
			doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertCannotBulldozeCc);
			return;
		}

		if (structure->structureClass() == Structure::StructureClass::Lander && structure->age() == 0)
		{
			doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertCannotBulldozeLandingSite);
			return;
		}

		if (structure->isRobotCommand())
		{
			deleteRobotsInRCC(robot, static_cast<RobotCommand*>(structure), mRobotPool, mRobotList, &tile);
		}

		if (structure->isFactory() && static_cast<Factory*>(structure) == mFactoryProduction.factory())
		{
			mFactoryProduction.hide();
		}

		if (structure->isWarehouse())
		{
			if (simulateMoveProducts(static_cast<Warehouse*>(structure))) { moveProducts(static_cast<Warehouse*>(structure)); }
			else { return; }
		}

		if (structure->structureClass() == Structure::StructureClass::Communication)
		{
			checkCommRangeOverlay();
		}

		auto recycledResources = StructureCatalogue::recyclingValue(structure->structureId());
		addRefinedResources(recycledResources);

		/**
		 * \todo	This could/should be some sort of alert message to the user instead of dumped to the console
		 */
		if (!recycledResources.isEmpty()) { std::cout << "Resources wasted demolishing " << structure->name() << std::endl; }

		countPlayerResources();
		updateStructuresAvailability();

		tile.connected(false);
		Utility<StructureManager>::get().removeStructure(structure);
		tile.deleteThing();
		Utility<StructureManager>::get().disconnectAll();
		static_cast<Robodozer*>(robot)->tileIndex(static_cast<std::size_t>(TerrainType::Dozed));
		checkConnectedness();
	}

	int taskTime = tile.index() == TerrainType::Dozed ? 1 : static_cast<int>(tile.index());
	robot->startTask(taskTime);
	mRobotPool.insertRobotIntoTable(mRobotList, robot, &tile);
	static_cast<Robodozer*>(robot)->tileIndex(static_cast<std::size_t>(tile.index()));
	tile.index(TerrainType::Dozed);

	if (!mRobotPool.robotAvailable(Robot::Type::Dozer))
	{
		mRobots.removeItem(constants::Robodozer);
		clearMode();
	}
}


void MapViewState::placeRobodigger(Tile& tile)
{
	// Keep digger within a safe margin of the map boundaries.
	if (!NAS2D::Rectangle<int>::Create({ 4, 4 }, NAS2D::Point{ -4, -4 } + mTileMap->size()).contains(mTileMapMouseHover))
	{
		doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertDiggerEdgeBuffer);
		return;
	}

	// Check for obstructions underneath the the digger location.
	if (tile.depth() != mTileMap->maxDepth() && !mTileMap->getTile(tile.position(), tile.depth() + 1).empty())
	{
		doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertDiggerBlockedBelow);
		return;
	}

	if (tile.hasMine())
	{
		if (!doYesNoMessage(constants::AlertDiggerMineTile, constants::AlertDiggerMine)) { return; }

		const auto position = tile.position();
		std::cout << "Digger destroyed a Mine at (" << position.x << ", " << position.y << ")." << std::endl;
		mTileMap->removeMineLocation(position);
	}

	// Die if tile is occupied or not excavated.
	if (!tile.empty())
	{
		if (tile.depth() > constants::DepthSurface)
		{
			if (tile.thingIsStructure() && tile.structure()->connectorDirection() != ConnectorDir::CONNECTOR_VERTICAL) //air shaft
			{
				doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertStructureInWay);
				return;
			}
			else if (tile.thingIsStructure() && tile.structure()->connectorDirection() == ConnectorDir::CONNECTOR_VERTICAL && tile.depth() == mTileMap->maxDepth())
			{
				doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertMaxDigDepth);
				return;
			}
		}
		else
		{
			doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertStructureInWay);
			return;
		}
	}

	if (!tile.thing() && mTileMap->currentDepth() > 0) { mDiggerDirection.cardinalOnlyEnabled(); }
	else { mDiggerDirection.downOnlyEnabled(); }

	mDiggerDirection.setParameters(&tile);

	// If we're placing on the top level we can only ever go down.
	if (mTileMap->currentDepth() == constants::DepthSurface)
	{
		mDiggerDirection.selectDown();
	}
	else
	{
		mDiggerDirection.show();
		mWindowStack.bringToFront(&mDiggerDirection);

		// Popup to the right of the mouse
		auto position = MOUSE_COORDS + NAS2D::Vector{ 20, -32 };
		// Check if popup position is off the right edge of the display area
		if (position.x + mDiggerDirection.size().x > Utility<Renderer>::get().size().x)
		{
			// Popup to the left of the mouse
			position = MOUSE_COORDS + NAS2D::Vector{ -20 - mDiggerDirection.size().x, -32 };
		}
		mDiggerDirection.position(position);
	}
}


void MapViewState::placeRobominer(Tile& tile)
{
	if (tile.thing()) { doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertMinerTileObstructed); return; }
	if (mTileMap->currentDepth() != constants::DepthSurface) { doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertMinerSurfaceOnly); return; }
	if (!tile.mine()) { doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertMinerNotOnMine); return; }

	Robot* robot = mRobotPool.getMiner();
	robot->startTask(constants::MinerTaskTime);
	mRobotPool.insertRobotIntoTable(mRobotList, robot, &tile);
	tile.index(TerrainType::Dozed);

	if (!mRobotPool.robotAvailable(Robot::Type::Miner))
	{
		mRobots.removeItem(constants::Robominer);
		clearMode();
	}

}


void MapViewState::placeRobot()
{
	Tile* tile = mTileMap->getVisibleTile();
	if (!tile) { return; }
	if (!tile->excavated()) { return; }
	if (!mRobotPool.robotCtrlAvailable()) { return; }

	if (!inCommRange(tile->position()))
	{
		doAlertMessage(constants::AlertInvalidRobotPlacement, constants::AlertOutOfCommRange);
		return;
	}

	switch (mCurrentRobot)
	{
	case Robot::Type::Dozer:
		placeRobodozer(*tile);
		break;
	case Robot::Type::Digger:
		placeRobodigger(*tile);
		break;
	case Robot::Type::Miner:
		placeRobominer(*tile);
		break;
	default:
		break;
	}
}


/**
 * Checks the robot selection interface and if the robot is not available in it, adds
 * it back in.
 */
void MapViewState::checkRobotSelectionInterface(Robot::Type rType)
{
	const auto& robotInfo = RobotMetaTable.at(rType);
	if (!mRobots.itemExists(robotInfo.name))
	{
		mRobots.addItemSorted(robotInfo.name, robotInfo.sheetIndex, static_cast<int>(rType));
	}
}


/**
 * Places a structure into the map.
 */
void MapViewState::placeStructure()
{
	if (mCurrentStructure == StructureID::SID_NONE) { throw std::runtime_error("MapViewState::placeStructure() called but mCurrentStructure == STRUCTURE_NONE"); }

	Tile* tile = mTileMap->getVisibleTile();
	if (!tile) { return; }

	if (!structureIsLander(mCurrentStructure) && !selfSustained(mCurrentStructure) &&
		!isPointInRange(tile->position(), ccLocation(), constants::RobotCommRange))
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureOutOfRange);
		return;
	}

	if (tile->mine())
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureMineInWay);
		return;
	}

	if (tile->thing())
	{
		if (tile->thingIsStructure())
		{
			doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureTileObstructed);
		}
		else
		{
			doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureTileThing);
		}
		return;
	}

	if ((!tile->bulldozed() && !structureIsLander(mCurrentStructure)))
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureTerrain);
		return;
	}

	if (!tile->excavated())
	{
		doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureExcavated);
		return;
	}

	// The player may only place one seed lander per game.
	if (mCurrentStructure == StructureID::SID_SEED_LANDER)
	{
		insertSeedLander(mTileMapMouseHover);
	}
	else if (mCurrentStructure == StructureID::SID_COLONIST_LANDER)
	{
		if (!validLanderSite(*tile)) { return; }

		ColonistLander* s = new ColonistLander(tile);
		s->deploySignal().connect(this, &MapViewState::onDeployColonistLander);
		Utility<StructureManager>::get().addStructure(s, tile);

		--mLandersColonist;
		if (mLandersColonist == 0)
		{
			clearMode();
			resetUi();
			populateStructureMenu();
		}
	}
	else if (mCurrentStructure == StructureID::SID_CARGO_LANDER)
	{
		if (!validLanderSite(*tile)) { return; }

		CargoLander* cargoLander = new CargoLander(tile);
		cargoLander->deploySignal().connect(this, &MapViewState::onDeployCargoLander);
		Utility<StructureManager>::get().addStructure(cargoLander, tile);

		--mLandersCargo;
		if (mLandersCargo == 0)
		{
			clearMode();
			resetUi();
			populateStructureMenu();
		}
	}
	else
	{
		if (!validStructurePlacement(mTileMap, mTileMapMouseHover) && !selfSustained(mCurrentStructure))
		{
			doAlertMessage(constants::AlertInvalidStructureAction, constants::AlertStructureNoTube);
			return;
		}

		// Check build cost
		if (!StructureCatalogue::canBuild(mResourcesCount, mCurrentStructure))
		{
			resourceShortageMessage(mResourcesCount, mCurrentStructure);
			return;
		}

		Structure* structure = StructureCatalogue::get(mCurrentStructure);
		if (!structure) { throw std::runtime_error("MapViewState::placeStructure(): NULL Structure returned from StructureCatalog."); }

		Utility<StructureManager>::get().addStructure(structure, tile);

		// FIXME: Ugly
		if (structure->isFactory())
		{
			static_cast<Factory*>(structure)->productionComplete().connect(this, &MapViewState::onFactoryProductionComplete);
			static_cast<Factory*>(structure)->resourcePool(&mResourcesCount);
		}

		if (structure->structureId() == StructureID::SID_MAINTENANCE_FACILITY)
		{
			static_cast<MaintenanceFacility*>(structure)->resources(mResourcesCount);
		}

		auto cost = StructureCatalogue::costToBuild(mCurrentStructure);
		removeRefinedResources(cost);
		countPlayerResources();
		updateStructuresAvailability();
	}
}


/**
 * Checks that the clicked tile is a suitable spot for the SEED Lander and
 * then inserts it into the the TileMap.
 */
void MapViewState::insertSeedLander(NAS2D::Point<int> point)
{
	// Has to be built away from the edges of the map
	if (NAS2D::Rectangle<int>::Create({4, 4}, NAS2D::Point{-4, -4} + mTileMap->size()).contains(point))
	{
		// check for obstructions
		if (!landingSiteSuitable(mTileMap, point))
		{
			return;
		}

		SeedLander* s = new SeedLander(point);
		s->deploySignal().connect(this, &MapViewState::onDeploySeedLander);
		Utility<StructureManager>::get().addStructure(s, &mTileMap->getTile(point)); // Can only ever be placed on depth level 0

		clearMode();
		resetUi();

		mStructures.clear();
		mBtnTurns.enabled(true);
	}
	else
	{
		doAlertMessage(constants::AlertLanderLocation, constants::AlertSeedEdgeBuffer);
	}
}


/**
 * Updates all robots.
 */
void MapViewState::updateRobots()
{
	auto robot_it = mRobotList.begin();
	while(robot_it != mRobotList.end())
	{
		auto robot = robot_it->first;
		auto tile = robot_it->second;

		robot->update();

		const auto position = tile->position();
		pushAgingRobotMessage(robot, position, mNotificationArea);

		if (robot->dead())
		{
			std::cout << "dead robot" << std::endl;

			const auto robotLocationText ="(" +  std::to_string(position.x) + ", " + std::to_string(position.y) + ")";

			if (robot->selfDestruct())
			{
				mNotificationArea.push("Robot Self-Destructed",
					robot->name() + " at location " + robotLocationText + " self destructed.",
					position,
					NotificationArea::NotificationType::Critical);
			}
			else if (robot->type() != Robot::Type::Miner)
			{
				const auto text = "Your " + robot->name() + " at location " + robotLocationText + " has broken down. It will not be able to complete its task and will be removed from your inventory.";
				mNotificationArea.push("Robot Broke Down", text, position, NotificationArea::NotificationType::Critical);
				resetTileIndexFromDozer(robot, tile);
			}

			if (tile->thing() == robot)
			{
				tile->removeThing();
			}

			for (auto rcc : Utility<StructureManager>::get().getStructures<RobotCommand>())
			{
				rcc->removeRobot(robot);
			}

			if (mRobotInspector.focusedRobot() == robot) { mRobotInspector.hide(); }

			mRobotPool.erase(robot);
			delete robot;
			robot_it = mRobotList.erase(robot_it);
		}
		else if (robot->idle())
		{
			if (tile->thing() == robot)
			{
				tile->removeThing();
			}
			robot_it = mRobotList.erase(robot_it);

			if (robot->taskCanceled())
			{
				resetTileIndexFromDozer(robot, tile);
				checkRobotSelectionInterface(robot->type());
				robot->reset();
			}
		}
		else
		{
			++robot_it;
		}
	}

	updateRobotControl(mRobotPool);
}


/**
 * Checks and sets the current structure mode.
 */
void MapViewState::setStructureID(StructureID type, InsertMode mode)
{

	if (type == StructureID::SID_NONE)
	{
		clearMode();
		return;
	}

	mCurrentStructure = type;

	mInsertMode = mode;
	Utility<Renderer>::get().setCursor(PointerType::POINTER_PLACE_TILE);

}


/**
 * Checks the connectedness of all tiles surrounding
 * the Command Center.
 */
void MapViewState::checkConnectedness()
{
	if (ccLocation() == CcNotPlaced)
	{
		return;
	}

	// Assumes that the 'thing' at mCCLocation is in fact a Structure.
	auto& tile = mTileMap->getTile(ccLocation(), 0);
	Structure* cc = tile.structure();

	if (!cc)
	{
		throw std::runtime_error("CC coordinates do not actually point to a Command Center.");
	}

	if (cc->state() == StructureState::UnderConstruction)
	{
		return;
	}

	tile.connected(true);

	// Start graph walking at the CC location.
	mConnectednessOverlay.clear();
	GraphWalker graphWalker(ccLocation(), 0, *mTileMap, mConnectednessOverlay);
}


void MapViewState::checkCommRangeOverlay()
{
	mCommRangeOverlay.clear();

	auto& structureManager = NAS2D::Utility<StructureManager>::get();

	const auto& commTowers = structureManager.getStructures<CommTower>();
	const auto& command = structureManager.getStructures<CommandCenter>();

	for (auto cc : command)
	{
		if (!cc->operational()) { continue; }
		auto& centerTile = structureManager.tileFromStructure(cc);
		fillRangedAreaList(mCommRangeOverlay, centerTile, cc->getRange());
	}

	for (auto tower : commTowers)
	{
		if (!tower->operational()) { continue; }
		auto& centerTile = structureManager.tileFromStructure(tower);
		fillRangedAreaList(mCommRangeOverlay, centerTile, tower->getRange());
	}
}


void MapViewState::checkSurfacePoliceOverlay()
{
	resetPoliceOverlays();

	auto& structureManager = NAS2D::Utility<StructureManager>::get();

	const auto& policeStations = structureManager.getStructures<SurfacePolice>();

	for (auto policeStation : policeStations)
	{
		if (!policeStation->operational()) { continue; }
		auto& centerTile = structureManager.tileFromStructure(policeStation);
		fillRangedAreaList(mPoliceOverlays[0], centerTile, policeStation->getRange());
	}

	const auto& undergroundPoliceStations = structureManager.getStructures<UndergroundPolice>();

	for (auto undergroundPoliceStation : undergroundPoliceStations)
	{
		if (!undergroundPoliceStation->operational()) { continue; }
		auto depth = structureManager.tileFromStructure(undergroundPoliceStation).depth();
		auto& centerTile = structureManager.tileFromStructure(undergroundPoliceStation);
		fillRangedAreaList(mPoliceOverlays[depth], centerTile, undergroundPoliceStation->getRange(), depth);
	}
}


void MapViewState::resetPoliceOverlays()
{
	mPoliceOverlays.clear();
	for (int i = 0; i <= mTileMap->maxDepth(); ++i)
	{
		mPoliceOverlays.push_back(TileList());
	}
}


void MapViewState::fillRangedAreaList(TileList& tileList, Tile& centerTile, int range)
{
	fillRangedAreaList(tileList, centerTile, range, 0);
}

void MapViewState::fillRangedAreaList(TileList& tileList, Tile& centerTile, int range, int depth)
{
	auto area = buildAreaRectFromTile(centerTile, range + 1);

	for (int y = 0; y < area.height; ++y)
	{
		for (int x = 0; x < area.width; ++x)
		{
			auto& tile = (*mTileMap).getTile({ x + area.x, y + area.y }, depth);
			if (isPointInRange(centerTile.position(), tile.position(), range))
			{
				if (std::find(tileList.begin(), tileList.end(), &tile) == tileList.end())
				{
					tileList.push_back(&tile);
				}
			}
		}
	}
}


/**
 * Removes deployed robots from the TileMap to
 * prevent dangling pointers. Yay for raw memory!
 */
void MapViewState::scrubRobotList()
{
	for (auto it : mRobotList)
	{
		it.second->removeThing();
	}
}


/**
 * Update the value of the current level string
 */
void MapViewState::updateCurrentLevelString(int currentDepth)
{
	CURRENT_LEVEL_STRING = LEVEL_STRING_TABLE[currentDepth];
}
