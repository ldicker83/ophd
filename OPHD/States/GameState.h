#pragma once

#include <NAS2D/State.h>

#include <memory>


class MainReportsUiState;
class MapViewState;
class Structure;
class Wrapper;


class GameState : public NAS2D::State
{
public:
	GameState();
	~GameState() override;

	void mapviewstate(MapViewState*);
	MainReportsUiState& getMainReportsState();

	void initialize() override;
	State* update() override;

private:
	void onMouseMove(int x, int y, int relX, int relY);

	void onFadeComplete();
	void onMusicComplete();

	void onQuit();
	void onShowReports();
	void onHideReports();
	void onMapChange();

	void onTakeMeThere(Structure*);

private:
	NAS2D::State* mReturnState = this;
	std::unique_ptr<MapViewState> mMapView;
	Wrapper* mActiveState = nullptr;
	std::unique_ptr<MainReportsUiState> mMainReportsState;
};
