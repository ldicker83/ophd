#pragma once

#include "Structure.h"

#include "../../Mine.h"

/**
 * Implements the Mine Facility.
 */
class MineFacility: public Structure
{
public:
	MineFacility(Mine* mine);
	virtual ~MineFacility();
	
	void mine(Mine* _m) { mMine = _m; }

	Mine* mine() { return mMine; }

protected:
	virtual void think();

private:
	MineFacility() = delete;
	MineFacility(const MineFacility&) = delete;
	MineFacility& operator=(const MineFacility&) = delete;

private:
	virtual void defineResourceInput() { mMine->active(true); }
	virtual void defineResourceOutput()	{}

	virtual void activated() final;

private:
	Mine*			mMine;
};
