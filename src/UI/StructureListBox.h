#pragma once

#include "NAS2D/Signal.h"

#include "Core/ListBoxBase.h"

class Structure;

/**
 * Implements a ListBox control.
 */
class StructureListBox : public ListBoxBase
{
public:
	using SelectionChangedCallback = NAS2D::Signals::Signal<Structure*>;

public:
	class StructureListBoxItem : public ListBoxItem
	{
	public:
		StructureListBoxItem(Structure* s);
		~StructureListBoxItem() override;

	public:
		Structure* structure = nullptr;	/**< Pointer to a Structure. */
		std::string structureState;		/**< String description of the state of a Structure. */
		size_t colorIndex = 0;			/**< Index to use from the listbox color table. */
	};

public:
	StructureListBox();
	~StructureListBox() override;

	void addItem(Structure*);
	void removeItem(Structure*);
	void currentSelection(Structure*);

	Structure* selectedStructure();

	StructureListBoxItem* last();

	void update() override final;

private:
	void _init();

private:
	using StructureItemList = std::vector<StructureListBoxItem>;
};
