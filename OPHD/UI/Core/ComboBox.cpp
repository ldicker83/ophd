// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ComboBox.h"

#include <NAS2D/Utility.h>
#include <NAS2D/MathUtils.h>

#include <algorithm>


using namespace NAS2D;

/**
 * C'tor
 */
ComboBox::ComboBox()
{
	init();
}


/**
 * D'tor
 */
ComboBox::~ComboBox()
{
	resized().disconnect(this, &ComboBox::resizedHandler);
	moved().disconnect(this, &ComboBox::repositioned);
	lstItems.selectionChanged().disconnect(this, &ComboBox::lstItemsSelectionChanged);
	Utility<EventHandler>::get().mouseButtonDown().disconnect(this, &ComboBox::onMouseDown);
	Utility<EventHandler>::get().mouseWheel().disconnect(this, &ComboBox::onMouseWheel);
}


/**
 * Internal initializer.
 * 
 * Performs basic init'ing of internal components.
 */
void ComboBox::init()
{
	Utility<EventHandler>::get().mouseButtonDown().connect(this, &ComboBox::onMouseDown);
	Utility<EventHandler>::get().mouseWheel().connect(this, &ComboBox::onMouseWheel);

	btnDown.image("ui/icons/down.png");
	btnDown.size({20, 20});

	txtField.editable(false);
	lstItems.visible(false);
	lstItems.height(300);

	resized().connect(this, &ComboBox::resizedHandler);
	moved().connect(this, &ComboBox::repositioned);
	lstItems.selectionChanged().connect(this, &ComboBox::lstItemsSelectionChanged);
}


/**
 * Resized event handler.
 */
void ComboBox::resizedHandler(Control* /*control*/)
{
	// Enforce minimum size
	if (mRect.width < 50 || mRect.height < 20)
	{
		size({std::max(mRect.width, 50), std::max(mRect.height, 20)});
	}

	txtField.size(size() - NAS2D::Vector{20, 0});
	btnDown.position(txtField.rect().crossXPoint());
	btnDown.height(mRect.height);
	lstItems.width(mRect.width);
	lstItems.position(rect().crossYPoint());

	mBaseArea = Rectangle<int>::Create(position(), NAS2D::Vector{mRect.width, btnDown.size().y});
}


/**
 * Position changed event handler.
 */
void ComboBox::repositioned(int, int)
{
	txtField.position(position());
	btnDown.position(txtField.rect().crossXPoint());
	lstItems.position(rect().crossYPoint());

	mBaseArea = Rectangle<int>::Create(position(), NAS2D::Vector{mRect.width, btnDown.size().y});
}


/**
 * Mouse button down event handler.
 */
void ComboBox::onMouseDown(EventHandler::MouseButton button, int x, int y)
{
	if (!enabled() || !visible() || !hasFocus()) { return; }

	if (button != EventHandler::MouseButton::BUTTON_LEFT) { return; }

	if (mBaseArea.contains(Point{x, y}))
	{
		lstItems.visible(!lstItems.visible());
		if (lstItems.visible())
		{
			mRect.height = mRect.height + lstItems.size().y;
		}
		else
		{
			mRect = mBaseArea;
		}
	}
	else if (!lstItems.rect().contains(Point{x, y}))
	{
		lstItems.visible(false);
		mRect = mBaseArea;
	}
}


void ComboBox::onMouseWheel(int /*x*/, int /*y*/)
{

}


void ComboBox::clearSelection()
{
	lstItems.clearSelection();
	txtField.clear();
}


/**
 * ListBox selection changed event handler.
 */
void ComboBox::lstItemsSelectionChanged()
{
	txtField.text(lstItems.selectionText());
	lstItems.visible(false);
	mRect = mBaseArea;
	mSelectionChanged();
}


/**
 * Sets the maximum number of items to display before showing a scroll bar.
 */
void ComboBox::maxDisplayItems(std::size_t count)
{
	mMaxDisplayItems = count;
	
	if (count < constants::MINIMUM_DISPLAY_ITEMS)
	{
		mMaxDisplayItems = constants::MINIMUM_DISPLAY_ITEMS;
	}
}


/**
 * Adds an item to the list.
 */
void ComboBox::addItem(const std::string& item, int tag)
{
	lstItems.addItem(item, tag);

	if (lstItems.count() > mMaxDisplayItems) { return; }
	lstItems.height(static_cast<int>(lstItems.count() * lstItems.lineHeight()));

	lstItems.clearSelection();
}


/**
 * Gets the text of the selection.
 */
const std::string& ComboBox::selectionText() const
{
	return lstItems.selectionText();
}


/**
 * Gets the tag value of the selected item.
 */
int ComboBox::selectionTag() const
{
	return lstItems.selectionTag();
}


void ComboBox::currentSelection(std::size_t index) {
	lstItems.currentSelection(index);
	text(lstItems.selectionText());
	mSelectionChanged();
}

/**
 * 
 */
void ComboBox::update()
{
	txtField.update();
	btnDown.update();
	lstItems.update();
}

void ComboBox::text(const std::string& text) {
	txtField.text(text);
	txtField.textChanged();
	lstItems.setSelectionByName(txtField.text());
	mSelectionChanged();
}

const std::string& ComboBox::text() const {
	return txtField.text();
}
