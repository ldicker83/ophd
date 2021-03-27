#pragma once

#include "Things/Structures/Structure.h"


namespace NAS2D {
	namespace Xml {
		class XmlElement;
	}
}

class Tile;
class PopulationPool;
struct StorableResources;

class StructureComponent;
typedef int ComponentTypeID; // TODO: replace by enum class?

/**
 * Key type for identifying a specific structure instance.
 * The key for any given structure is guaranteed to remain unchanged for the lifetime of the structure.
 * The key for any given structure is guaranteed to be unique during the lifetime of the structure.
 *
 * Every structure has a Structure instance. The Structure pointer is used as key to allow O(1) access
 * to the Structure instance. This is an internal detail and should not be relied upon by code handling the key.
 */
class SKey
{
private:
	Structure* mStructure;
public:
	SKey(Structure* structure) : mStructure(structure) {}

	/** Comparison operators to allow using this type in ordered containers such as maps and sets. */
	bool operator<(const SKey& rhs) const { return mStructure < rhs.mStructure; }

	/** Do not call this function directly. It is intended only for GetComponent/TryGetComponent. */
	Structure* getInternal() { return mStructure; }
};


/**
 * Handles structure updating and resource management for structures.
 *
 * Keeps track of which structures are operational, idle and disabled.
 */
class StructureManager
{
public:
	void addStructure(Structure* structure, Tile* tile);
	void removeStructure(Structure* structure);

	const StructureList& structureList(Structure::StructureClass structureClass);
	Tile& tileFromStructure(Structure* structure);

	void disconnectAll();
	void dropAllStructures();

	int count() const;

	int getCountInState(Structure::StructureClass structureClass, StructureState state);

	int disabled();
	int destroyed();

	bool CHAPAvailable();

	void updateEnergyProduction();
	void updateEnergyConsumed();
	int totalEnergyProduction() const { return mTotalEnergyOutput; }
	int totalEnergyUsed() const { return mTotalEnergyUsed; }
	int totalEnergyAvailable() const { return mTotalEnergyOutput - mTotalEnergyUsed; }

	void assignColonistsToResidences(PopulationPool&);

	void update(const StorableResources&, PopulationPool&);

	void serialize(NAS2D::Xml::XmlElement* element);

	/**
	 * Associate a StructureComponent with a structure.
	 * This transfers ownership of the component instance to the
	 * structure manager. The component will be destroyed when the
	 * structure with which it is associated is removed.
	 */
	template<typename ComponentTy>
	void attachComponent(SKey s, ComponentTy* component)
	{
		auto& table = mComponents[ComponentTy::componentTypeID];
		bool success = table.insert(std::make_pair(s, static_cast<StructureComponent*>(component))).second;
#if defined(_DEBUG)
		if (!success)
		{
			throw std::runtime_error("Structure::Attach() was called on a Structure that already had the component!");
		}
#else
		UNUSED(success);
#endif
	}

	/**
	 * Return a reference to the given StructureComponent type belonging to
	 * a structure. The structure is assumed to have the given component,
	 * and it is an error to try to get a component from a structure that
	 * does not have it.
	 */
	template<typename ComponentTy>
	ComponentTy& get(SKey s)
	{
		ComponentTy* component = tryGet<ComponentTy>(s);
#if defined(_DEBUG)
		if (!component)
		{
			throw std::runtime_error("StructureManager::get() was called on a Structure without the requested component!");
		}
#endif
		return *component;
	}

	/**
	 * Return a pointer to the given StructureComponent type belonging
	 * to a structure, if it has the corresponding component type.
	 * Otherwise return nullptr.
	 */
	template<typename ComponentTy>
	ComponentTy* tryGet(SKey s)
	{
		auto& table = mComponents[ComponentTy::componentTypeID];
		auto it = table.find(s);
		if (it != table.end())
			return static_cast<ComponentTy*>(it->second);
		return nullptr;
	}

private:
	using StructureTileTable = std::map<Structure*, Tile*>;
	using StructureClassTable = std::map<Structure::StructureClass, StructureList>;

	void updateStructures(const StorableResources&, PopulationPool&, StructureList&);

	bool structureConnected(Structure* structure);

	StructureTileTable mStructureTileTable; /**< List mapping Structures to a particular tile. */
	StructureClassTable mStructureLists; /**< Map containing all of the structure list types available. */

	/**
	 * Master table of all StructureComponent instances.
	 * It is divided into one sub-table per StructureComponent type.
	 * Each sub-table maps structure keys to a StructureComponent-derived instance.
	 * Only keys to structures that actually have a given StructureComponent type
	 * are present in the respective sub-tables.
	 */
	std::map<ComponentTypeID, std::map<SKey, StructureComponent*>> mComponents;

	int mTotalEnergyOutput = 0; /**< Total energy output of all energy producers in the structure list. */
	int mTotalEnergyUsed = 0;
};

/**
 * Return a reference to the Structure type belonging to a structure.
 * This allows writing code that's agnostic to whether Structure is a component or not.
 */
template<>
inline Structure& StructureManager::get<Structure>(SKey s)
{
	return *s.getInternal();
}

/**
 * Return a pointer to the Structure type belonging to a structure.
 * This allows writing code that's agnostic to whether Structure is a component or not.
 */
template<>
inline Structure* StructureManager::tryGet<Structure>(SKey s)
{
	return s.getInternal();
}

