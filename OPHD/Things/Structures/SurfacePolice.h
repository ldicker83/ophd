#pragma once

#include "Structure.h"

#include "../../Constants.h"

class SurfacePolice : public Structure
{
public:
	SurfacePolice() : Structure(constants::SURFACE_POLICE, "structures/police_surface.sprite", StructureClass::SurfacePolice)
	{
		maxAge(500);
		turnsToBuild(4);

		requiresCHAP(true);
	}

protected:
	void defineResourceInput() override
	{
		resourcesIn().energy(5);
	}
};
