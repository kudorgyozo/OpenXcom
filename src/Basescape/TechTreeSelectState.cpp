/*
 * Copyright 2010-2015 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <locale>
#include "TechTreeSelectState.h"
#include "TechTreeViewerState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"

namespace OpenXcom
{

/**
 * Initializes all the elements on the UI.
 * @param parent Pointer to parent state.
 */
TechTreeSelectState::TechTreeSelectState(TechTreeViewerState *parent) : _parent(parent)
{
	_screen = false;

	_window = new Window(this, 230, 140, 45, 32);
	_btnQuickSearch = new TextEdit(this, 48, 9, 219, 42);
	_txtTitle = new Text(182, 9, 53, 42);
	_lstTopics = new TextList(198, 88, 53, 54);
	_btnOk = new TextButton(206, 16, 57, 145);

	// Set palette
	setInterface("techTreeSelect");

	add(_window, "window", "techTreeSelect");
	add(_btnQuickSearch, "button", "techTreeSelect");
	add(_txtTitle, "text", "techTreeSelect");
	add(_btnOk, "button", "techTreeSelect");
	add(_lstTopics, "list", "techTreeSelect");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_AVAILABLE_TOPICS"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&TechTreeSelectState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TechTreeSelectState::btnOkClick, Options::keyCancel);

	_lstTopics->setColumns(1, 182);
	_lstTopics->setSelectable(true);
	_lstTopics->setBackground(_window);
	_lstTopics->setMargin(0);
	_lstTopics->setAlign(ALIGN_CENTER);
	_lstTopics->onMouseClick((ActionHandler)&TechTreeSelectState::onSelectTopic);

	_btnQuickSearch->setText(L""); // redraw
	_btnQuickSearch->onEnter((ActionHandler)&TechTreeSelectState::btnQuickSearchApply);
	_btnQuickSearch->setVisible(true);
	_btnQuickSearch->setFocus(true);

	_btnOk->onKeyboardRelease((ActionHandler)&TechTreeSelectState::btnQuickSearchToggle, Options::keyToggleQuickSearch);
}

TechTreeSelectState::~TechTreeSelectState()
{
}

/**
* Initializes the screen (fills the lists).
*/
void TechTreeSelectState::init()
{
	State::init();
	initLists();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TechTreeSelectState::btnOkClick(Action *)
{
	_game->popState();
}

/**
* Quick search toggle.
* @param action Pointer to an action.
*/
void TechTreeSelectState::btnQuickSearchToggle(Action *action)
{
	if (_btnQuickSearch->getVisible())
	{
		_btnQuickSearch->setText(L"");
		_btnQuickSearch->setVisible(false);
		btnQuickSearchApply(action);
	}
	else
	{
		_btnQuickSearch->setVisible(true);
		_btnQuickSearch->setFocus(true);
	}
}

/**
* Quick search.
* @param action Pointer to an action.
*/
void TechTreeSelectState::btnQuickSearchApply(Action *)
{
	initLists();
}

/**
* Shows the filtered topics.
*/
void TechTreeSelectState::initLists()
{
	std::locale myLocale = CrossPlatform::testLocale();
	std::wstring searchString = _btnQuickSearch->getText();
	CrossPlatform::upperCase(searchString, myLocale);

	_firstManufacturingTopicIndex = 0;
	_firstFacilitiesTopicIndex = 0;
	_availableTopics.clear();
	_lstTopics->clearList();

	if (searchString.length() < 3)
	{
		_lstTopics->addRow(1, tr("STR_QS_THREE_LETTERS_A").c_str());
		_lstTopics->addRow(1, tr("STR_QS_THREE_LETTERS_B").c_str());
		return;
	}

	int row = 0;
	const std::vector<std::string> &researchItems = _game->getMod()->getResearchList();
	for (std::vector<std::string>::const_iterator i = researchItems.begin(); i != researchItems.end(); ++i)
	{
		if (Options::techTreeViewerSpoilerProtection)
		{
			if (!_parent->isDiscoveredResearch(*i))
				continue;
		}
		std::wstring projectName = tr((*i));
		CrossPlatform::upperCase(projectName, myLocale);
		if (searchString == L"SHAZAM")
		{
			if (_parent->isDiscoveredResearch(*i))
			{
				continue;
			}
		}
		else if (projectName.find(searchString) == std::string::npos)
		{
			continue;
		}

		_availableTopics.push_back(*i);
		_lstTopics->addRow(1, tr((*i)).c_str());
		++row;
	}

	_firstManufacturingTopicIndex = row;

	const std::vector<std::string> &manufacturingItems = _game->getMod()->getManufactureList();
	for (std::vector<std::string>::const_iterator i = manufacturingItems.begin(); i != manufacturingItems.end(); ++i)
	{
		if (Options::techTreeViewerSpoilerProtection)
		{
			if (!_parent->isDiscoveredManufacture(*i))
				continue;
		}
		std::wstring projectName = tr((*i));
		CrossPlatform::upperCase(projectName, myLocale);
		if (searchString == L"SHAZAM")
		{
			if (_parent->isDiscoveredManufacture(*i))
			{
				continue;
			}
		}
		else if (projectName.find(searchString) == std::string::npos)
		{
			continue;
		}

		_availableTopics.push_back(*i);
		std::wostringstream ss;
		ss << tr((*i));
		ss << tr("STR_M_FLAG");
		_lstTopics->addRow(1, ss.str().c_str());
		++row;
	}

	_firstFacilitiesTopicIndex = row;

	const std::vector<std::string> &facilityItems = _game->getMod()->getBaseFacilitiesList();
	for (std::vector<std::string>::const_iterator i = facilityItems.begin(); i != facilityItems.end(); ++i)
	{
		if (Options::techTreeViewerSpoilerProtection)
		{
			if (!_parent->isDiscoveredFacility(*i))
				continue;
		}
		std::wstring facilityName = tr((*i));
		CrossPlatform::upperCase(facilityName, myLocale);
		if (searchString == L"SHAZAM")
		{
			if (_parent->isDiscoveredFacility(*i))
			{
				continue;
			}
		}
		else if (facilityName.find(searchString) == std::string::npos)
		{
			continue;
		}

		_availableTopics.push_back(*i);
		std::wostringstream ss;
		ss << tr((*i));
		ss << tr("STR_F_FLAG");
		_lstTopics->addRow(1, ss.str().c_str());
		++row;
	}

	_firstItemTopicIndex = row;

	const std::vector<std::string> &itemsList = _game->getMod()->getItemsList();
	for (std::vector<std::string>::const_iterator i = itemsList.begin(); i != itemsList.end(); ++i)
	{
		if (!_parent->isProtectedItem(*i))
		{
			// items that are not protected at all are irrelevant for the Tech Tree Viewer!
			continue;
		}
		if (Options::techTreeViewerSpoilerProtection)
		{
			if (!_parent->isProtectedAndDiscoveredItem(*i))
				continue;
		}
		std::wstring itemName = tr((*i));
		CrossPlatform::upperCase(itemName, myLocale);
		if (searchString == L"SHAZAM")
		{
			if (_parent->isProtectedAndDiscoveredItem(*i))
			{
				continue;
			}
		}
		else if (itemName.find(searchString) == std::string::npos)
		{
			continue;
		}

		_availableTopics.push_back(*i);
		std::wostringstream ss;
		ss << tr((*i));
		ss << tr("STR_I_FLAG");
		_lstTopics->addRow(1, ss.str().c_str());
		++row;
	}
}

/**
* Selects the topic.
* @param action Pointer to an action.
*/
void TechTreeSelectState::onSelectTopic(Action *)
{
	size_t index = _lstTopics->getSelectedRow();
	if (index >= _availableTopics.size())
		return;

	const std::string selectedTopic = _availableTopics[index];

	TTVMode topicType = TTV_RESEARCH;
	if (index >= _firstItemTopicIndex)
	{
		topicType = TTV_ITEMS;
	}
	else if (index >= _firstFacilitiesTopicIndex)
	{
		topicType = TTV_FACILITIES;
	}
	else if (index >= _firstManufacturingTopicIndex)
	{
		topicType = TTV_MANUFACTURING;
	}

	_parent->setSelectedTopic(selectedTopic, topicType);

	_game->popState();
}

}
