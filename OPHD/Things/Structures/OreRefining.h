#pragma once

#include "Structure.h"
#include <string>
#include <array>

/**
 * \class	PowerStructure
 * \brief	Virtual class for structures whose primary purpose is power production
 *
 * \note	PowerStructure is an abstract class
 */
class OreRefining : public Structure
{
public:
	OreRefining(const std::string& name, const std::string& spritePath, StructureClass structureClass, StructureID id) :
		Structure(name, spritePath, structureClass, id) {}

	StringTable createInspectorViewTable() override
	{
		StringTable stringTable(3, 5);

		stringTable.setColumnFont(0, stringTable.GetDefaultFont());
		stringTable.setRowFont(0, stringTable.GetDefaultTitleFont());
		stringTable.setHorizontalPadding(20);
		stringTable.setColumnJustification(1, StringTable::Justification::Center);
		stringTable.setColumnJustification(2, StringTable::Justification::Center);

		stringTable.setColumnText(
			0,
			{
				"",
				"Common Metal",
				"Rare Metal",
				"Common Minerals",
				"Rare Minerals"
			});

		stringTable.setRowText(
			0,
			{
				"Material",
				"Storage",
				"Ore Conversion Rate"
			});

		auto& resources = storage().resources;

		stringTable[{1, 1}].text = writeStorageAmount(resources[0]);
		stringTable[{2, 1}].text = std::to_string(OreConversionDivisor[0]) + " : 1";

		stringTable[{1, 2}].text = writeStorageAmount(resources[2]);
		stringTable[{2, 2}].text = std::to_string(OreConversionDivisor[2]) + " : 1";

		stringTable[{1, 3}].text = writeStorageAmount(resources[1]);
		stringTable[{2, 3}].text = std::to_string(OreConversionDivisor[1]) + " : 1";

		stringTable[{1, 4}].text = writeStorageAmount(resources[3]);
		stringTable[{2, 4}].text = std::to_string(OreConversionDivisor[3]) + " : 1";

		return stringTable;
	}

	/**
     * Capacity of an individual type of refined resource
     */
	int IndividualMaterialCapacity() const { return storageCapacity() / 4; }

protected:
	std::array<int, 4> OreConversionDivisor{ 2, 2, 3, 3 };

	void think() override
	{
		if (isIdle())
		{
			if (storage() < StorableResources{ IndividualMaterialCapacity() })
			{
				enable();
			}
		}

		if (operational())
		{
			updateProduction();
		}
	}

	virtual void updateProduction()
	{
		int processingMinimum = constants::MINIMUM_RESOURCES_REQUIRE_FOR_SMELTING;

		StorableResources converted{ 0 };
		auto& ore = production();

		for (size_t i = 0; i < ore.resources.size(); ++i)
		{
			if (ore.resources[i] >= processingMinimum)
			{
				converted.resources[i] = processingMinimum / OreConversionDivisor[i];
				ore.resources[i] = ore.resources[i] - processingMinimum;
			}
		}

		auto total = storage() + converted;
		auto capped = total.cap(IndividualMaterialCapacity());
		auto overflow = total - capped;

		storage() = storage() + capped;

		if (overflow > StorableResources{ 0 })
		{
			ore = ore + overflow;
			idle(IdleReason::InternalStorageFull);
		}
	}

private:
	std::string writeStorageAmount(int storageAmount) const
	{
		return std::to_string(storageAmount) + " / " + std::to_string(IndividualMaterialCapacity());
	}
};
