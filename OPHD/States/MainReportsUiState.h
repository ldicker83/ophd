#pragma once

#include "Wrapper.h"

#include <NAS2D/Signal/Signal.h>
#include <NAS2D/EventHandler.h>

#include <vector>


class Structure;

class MainReportsUiState : public Wrapper
{
public:
	using ReportsUiSignal = NAS2D::Signal<>;
	using TakeMeThere = NAS2D::Signal<Structure*>;
	using TakeMeThereList = std::vector<TakeMeThere*>;

public:
	MainReportsUiState();
	~MainReportsUiState() override;

	void selectFactoryPanel(Structure*);
	void selectWarehousePanel(Structure*);
	void selectMinePanel(Structure*);

	void clearLists();

	ReportsUiSignal::Source& hideReports() { return mReportsUiSignal; }
	TakeMeThereList takeMeThere();

protected:
	void initialize() override;
	State* update() override;

private:
	void _deactivate() override;
	void _activate() override;

private:
	void onKeyDown(NAS2D::EventHandler::KeyCode key, NAS2D::EventHandler::KeyModifier mod, bool repeat);
	void onMouseDown(NAS2D::EventHandler::MouseButton button, int x, int y);
	void onWindowResized(NAS2D::Vector<int> newSize);

	void deselectAllPanels();

	void exit();

private:
	ReportsUiSignal mReportsUiSignal;
};
