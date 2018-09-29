/*
 * Copyright 2010-2016 OpenXcom Developers.
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
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <SDL_gfxPrimitives.h>
#include "Map.h"
#include "Camera.h"
#include "BattlescapeState.h"
#include "AbortMissionState.h"
#include "TileEngine.h"
#include "ActionMenuState.h"
#include "UnitInfoState.h"
#include "InventoryState.h"
#include "AlienInventoryState.h"
#include "Pathfinding.h"
#include "BattlescapeGame.h"
#include "WarningMessage.h"
#include "InfoboxState.h"
#include "DebriefingState.h"
#include "MiniMapState.h"
#include "BattlescapeGenerator.h"
#include "BriefingState.h"
#include "../lodepng.h"
#include "../fmath.h"
#include "../Geoscape/SelectMusicTrackState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Screen.h"
#include "../Engine/Sound.h"
#include "../Engine/Action.h"
#include "../Engine/Script.h"
#include "../Engine/Logger.h"
#include "../Engine/Timer.h"
#include "../Engine/CrossPlatform.h"
#include "../Interface/Cursor.h"
#include "../Interface/Text.h"
#include "../Interface/Bar.h"
#include "../Interface/BattlescapeButton.h"
#include "../Interface/NumberText.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/PauseState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/SaveGameState.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleUfo.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Tile.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/BattleItem.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Savegame/Ufo.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleStartingCondition.h"
#include <algorithm>

namespace OpenXcom
{

/**
 * Initializes all the elements in the Battlescape screen.
 * @param game Pointer to the core game.
 */
BattlescapeState::BattlescapeState() : _reserve(0), _firstInit(true), _paletteResetNeeded(false), _paletteResetRequested(false), _isMouseScrolling(false), _isMouseScrolled(false), _xBeforeMouseScrolling(0), _yBeforeMouseScrolling(0), _totalMouseMoveX(0), _totalMouseMoveY(0), _mouseMovedOverThreshold(0), _swipeFromSoldier(false), _multiGestureProcess(false), _mouseOverIcons(false), _autosave(false), _numberOfDirectlyVisibleUnits(0), _numberOfEnemiesTotal(0), _numberOfEnemiesTotalPlusWounded(0)
{
	std::fill_n(_visibleUnit, 10, (BattleUnit*)(0));

	const int screenWidth = Options::baseXResolution;
	const int screenHeight = Options::baseYResolution;
	const int iconsWidth = _game->getMod()->getInterface("battlescape")->getElement("icons")->w;
	const int iconsHeight = _game->getMod()->getInterface("battlescape")->getElement("icons")->h;
	const int visibleMapHeight = screenHeight - iconsHeight;
	_mouseOverIcons = false;
	const int x = screenWidth/2 - iconsWidth/2;
	const int y = screenHeight - iconsHeight;

	_indicatorTextColor = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->color;
	_indicatorGreen = _game->getMod()->getInterface("battlescape")->getElement("squadsightUnits")->color;
	_indicatorBlue = _game->getMod()->getInterface("battlescape")->getElement("woundedUnits")->color;
	_indicatorPurple = _game->getMod()->getInterface("battlescape")->getElement("passingOutUnits")->color;

	_twoHandedRed = _game->getMod()->getInterface("battlescape")->getElement("twoHandedRed")->color;
	_twoHandedGreen = _game->getMod()->getInterface("battlescape")->getElement("twoHandedGreen")->color;

	_tooltipDefaultColor = _game->getMod()->getInterface("battlescape")->getElement("textTooltip")->color;

	_medikitRed = _game->getMod()->getInterface("battlescape")->getElement("medikitRed")->color;
	_medikitGreen = _game->getMod()->getInterface("battlescape")->getElement("medikitGreen")->color;
	_medikitBlue = _game->getMod()->getInterface("battlescape")->getElement("medikitBlue")->color;
	_medikitOrange = _game->getMod()->getInterface("battlescape")->getElement("medikitOrange")->color;

	// Create buttonbar - this should be on the centerbottom of the screen
	_icons = new InteractiveSurface(iconsWidth, iconsHeight, x, y);

	// Create the battlemap view
	// the actual map height is the total height minus the height of the buttonbar
	_map = new Map(_game, screenWidth, screenHeight, 0, 0, visibleMapHeight);

	_numLayers = new NumberText(3, 5, x + 232, y + 6);
	_rank = new Surface(26, 23, x + 107, y + 33);

	// Create buttons
	_btnUnitUp = new BattlescapeButton(32, 16, x + 48, y);
	_btnUnitDown = new BattlescapeButton(32, 16, x + 48, y + 16);
	_btnMapUp = new BattlescapeButton(32, 16, x + 80, y);
	_btnMapDown = new BattlescapeButton(32, 16, x + 80, y + 16);
	_btnShowMap = new BattlescapeButton(32, 16, x + 112, y);
	_btnKneel = new BattlescapeButton(32, 16, x + 112, y + 16);
	_btnInventory = new BattlescapeButton(32, 16, x + 144, y);
	_btnCenter = new BattlescapeButton(32, 16, x + 144, y + 16);
	_btnNextSoldier = new BattlescapeButton(32, 16, x + 176, y);
	_btnNextStop = new BattlescapeButton(32, 16, x + 176, y + 16);
	_btnShowLayers = new BattlescapeButton(32, 16, x + 208, y);
	_btnHelp = new BattlescapeButton(32, 16, x + 208, y + 16);
	_btnEndTurn = new BattlescapeButton(32, 16, x + 240, y);
	_btnAbort = new BattlescapeButton(32, 16, x + 240, y + 16);
	_btnStats = new InteractiveSurface(164, 23, x + 107, y + 33);
	_btnReserveNone = new BattlescapeButton(17, 11, x + 60, y + 33);
	_btnReserveSnap = new BattlescapeButton(17, 11, x + 78, y + 33);
	_btnReserveAimed = new BattlescapeButton(17, 11, x + 60, y + 45);
	_btnReserveAuto = new BattlescapeButton(17, 11, x + 78, y + 45);
	_btnReserveKneel = new BattlescapeButton(10, 23, x + 96, y + 33);
	_btnZeroTUs = new BattlescapeButton(10, 23, x + 49, y + 33);
	_btnLeftHandItem = new InteractiveSurface(32, 48, x + 8, y + 4);
	_btnRightHandItem = new InteractiveSurface(32, 48, x + 280, y + 4);
	_numAmmoLeft.reserve(RuleItem::AmmoSlotMax);
	_numAmmoRight.reserve(RuleItem::AmmoSlotMax);
	for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
	{
		_numAmmoLeft.push_back(new NumberText(30, 5, x + 8, y + 4 + 6 * slot));
		_numAmmoRight.push_back(new NumberText(30, 5, x + 280, y + 4 + 6 * slot));
	}
	_numMedikitLeft.reserve(RuleItem::MedikitSlots);
	_numMedikitRight.reserve(RuleItem::MedikitSlots);
	for (int slot = 0; slot < RuleItem::MedikitSlots; ++slot)
	{
		_numMedikitLeft.push_back(new NumberText(30, 5, x + 9, y + 32 + 7 * slot));
		_numMedikitRight.push_back(new NumberText(30, 5, x + 281, y + 32 + 7 * slot));
	}
	_numTwoHandedIndicatorLeft = new NumberText(10, 5, x + 36, y + 46);
	_numTwoHandedIndicatorRight = new NumberText(10, 5, x + 308, y + 46);
	const int visibleUnitX = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->x;
	const int visibleUnitY = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->y;
#ifdef __MOBILE__
	_leftWpnActive = new Surface(36, 52, x + 6, y + 2);
	_rightWpnActive = new Surface(36, 52, x + 278, y + 2);
	
#endif
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[i] = new InteractiveSurface(15, 12, x + visibleUnitX, y + visibleUnitY - (i * 13));
		_numVisibleUnit[i] = new NumberText(15, 12, _btnVisibleUnit[i]->getX() + 6 , _btnVisibleUnit[i]->getY() + 4);
	}
	_numVisibleUnit[9]->setX(_numVisibleUnit[9]->getX() - 2); // center number 10
	_warning = new WarningMessage(224, 24, x + 48, y + 32);
	_btnLaunch = new BattlescapeButton(32, 24, screenWidth - 32, 0); // we need screenWidth, because that is independent of the black bars on the screen
	_btnLaunch->setVisible(false);
	_btnPsi = new BattlescapeButton(32, 24, screenWidth - 32, 25); // we need screenWidth, because that is independent of the black bars on the screen
	_btnPsi->setVisible(false);
	_btnSpecial = new BattlescapeButton(32, 24, screenWidth - 32, 25); // we need screenWidth, because that is independent of the black bars on the screen
	_btnSpecial->setVisible(false);

	// Create soldier stats summary
	_rankTiny = new Surface(7, 7, x + 135, y + 33);
	_txtName = new Text(136, 10, x + 135, y + 32);

	_numTimeUnits = new NumberText(15, 5, x + 136, y + 42);
	_barTimeUnits = new Bar(102, 3, x + 170, y + 41);

	_numEnergy = new NumberText(15, 5, x + 154, y + 42);
	_barEnergy = new Bar(102, 3, x + 170, y + 45);

	_numHealth = new NumberText(15, 5, x + 136, y + 50);
	_barHealth= new Bar(102, 3, x + 170, y + 49);

	_numMorale = new NumberText(15, 5, x + 154, y + 50);
	_barMorale = new Bar(102, 3, x + 170, y + 53);

	_txtDebug = new Text(300, 10, 20, 0);
	_txtTooltip = new Text(300, 10, x + 2, y - 10);

	// Palette transformations
	std::string startingConditionType = _game->getSavedGame()->getSavedBattle()->getStartingConditionType();
	RuleStartingCondition *startingCondition = _game->getMod()->getStartingCondition(startingConditionType);
	if (startingCondition)
	{
		for (auto change : *startingCondition->getPaletteTransformations())
		{
			Palette *origPal = _game->getMod()->getPalette(change.first, false);
			Palette *newPal = _game->getMod()->getPalette(change.second, false);
			if (origPal && newPal)
			{
				origPal->copyFrom(newPal);
				_paletteResetNeeded = true;
			}
		}
	}

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	if (_game->getMod()->getInterface("battlescape")->getElement("pathfinding"))
	{
		Element *pathing = _game->getMod()->getInterface("battlescape")->getElement("pathfinding");

		Pathfinding::green = pathing->color;
		Pathfinding::yellow = pathing->color2;
		Pathfinding::red = pathing->border;
	}

	add(_map);
	add(_icons);

	// Add in custom reserve buttons
	Surface *icons = _game->getMod()->getSurface("ICONS.PCK");
	if (_game->getMod()->getSurface("TFTDReserve", false))
	{
		Surface *tftdIcons = _game->getMod()->getSurface("TFTDReserve");
		tftdIcons->setX(48);
		tftdIcons->setY(176);
		tftdIcons->blit(icons);
	}

	// there is some cropping going on here, because the icons image is 320x200 while we only need the bottom of it.
	SDL_Rect *r = icons->getCrop();
	r->x = 0;
	r->y = 200 - iconsHeight;
	r->w = iconsWidth;
	r->h = iconsHeight;
	// we need to blit the icons before we add the battlescape buttons, as they copy the underlying parent surface.
	icons->blit(_icons);

	// this is a hack to fix the single transparent pixel on TFTD's icon panel.
	if (_game->getMod()->getInterface("battlescape")->getElement("icons")->TFTDMode)
	{
		_icons->setPixel(46, 44, 8);
	}

	add(_rank, "rank", "battlescape", _icons);
	add(_rankTiny, "rank", "battlescape", _icons);
	add(_btnUnitUp, "buttonUnitUp", "battlescape", _icons);
	add(_btnUnitDown, "buttonUnitDown", "battlescape", _icons);
	add(_btnMapUp, "buttonMapUp", "battlescape", _icons);
	add(_btnMapDown, "buttonMapDown", "battlescape", _icons);
	add(_btnShowMap, "buttonShowMap", "battlescape", _icons);
	add(_btnKneel, "buttonKneel", "battlescape", _icons);
	add(_btnInventory, "buttonInventory", "battlescape", _icons);
	add(_btnCenter, "buttonCenter", "battlescape", _icons);
	add(_btnNextSoldier, "buttonNextSoldier", "battlescape", _icons);
	add(_btnNextStop, "buttonNextStop", "battlescape", _icons);
	add(_btnShowLayers, "buttonShowLayers", "battlescape", _icons);
	add(_numLayers, "numLayers", "battlescape", _icons);
	add(_btnHelp, "buttonHelp", "battlescape", _icons);
	add(_btnEndTurn, "buttonEndTurn", "battlescape", _icons);
	add(_btnAbort, "buttonAbort", "battlescape", _icons);
	add(_btnStats, "buttonStats", "battlescape", _icons);
	add(_txtName, "textName", "battlescape", _icons);
	// need to do this here, because of TFTD
	if (_game->getMod()->getSurface("AvatarBackground", false))
	{
		// put tiny rank icon where name used to be
		_rankTiny->setX(_txtName->getX());
		_rankTiny->setY(_txtName->getY() + 1);
		// move name more to the right
		_txtName->setWidth(_txtName->getWidth() - 8);
		_txtName->setX(_txtName->getX() + 8);
	}
	add(_numTimeUnits, "numTUs", "battlescape", _icons);
	add(_numEnergy, "numEnergy", "battlescape", _icons);
	add(_numHealth, "numHealth", "battlescape", _icons);
	add(_numMorale, "numMorale", "battlescape", _icons);
	add(_barTimeUnits, "barTUs", "battlescape", _icons);
	add(_barEnergy, "barEnergy", "battlescape", _icons);
	add(_barHealth, "barHealth", "battlescape", _icons);
	add(_barMorale, "barMorale", "battlescape", _icons);
	add(_btnReserveNone, "buttonReserveNone", "battlescape", _icons);
	add(_btnReserveSnap, "buttonReserveSnap", "battlescape", _icons);
	add(_btnReserveAimed, "buttonReserveAimed", "battlescape", _icons);
	add(_btnReserveAuto, "buttonReserveAuto", "battlescape", _icons);
	add(_btnReserveKneel, "buttonReserveKneel", "battlescape", _icons);
	add(_btnZeroTUs, "buttonZeroTUs", "battlescape", _icons);
#ifdef __MOBILE__
	add(_leftWpnActive);
	add(_rightWpnActive);
	Surface *wpnActive = _game->getMod()->getSurface("WpnActive");
	if (wpnActive)
	{
		wpnActive->blit(_leftWpnActive);
		wpnActive->blit(_rightWpnActive);
		_leftWpnActive->setVisible(false);
		_rightWpnActive->setVisible(false);
	}
	else
	{
		_leftWpnActive->setHidden(true);
		_rightWpnActive->setHidden(true);
	}
#endif
	add(_btnLeftHandItem, "buttonLeftHand", "battlescape", _icons);
	add(_btnRightHandItem, "buttonRightHand", "battlescape", _icons);
	for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
	{
		add(_numAmmoLeft[slot], "numAmmoLeft", "battlescape", _icons);
		add(_numAmmoRight[slot], "numAmmoRight", "battlescape", _icons);
	}
	for (int slot = 0; slot < RuleItem::MedikitSlots; ++slot)
	{
		add(_numMedikitLeft[slot], "numMedikitLeft", "battlescape", _icons);
		add(_numMedikitRight[slot], "numMedikitRight", "battlescape", _icons);
	}
	add(_numTwoHandedIndicatorLeft, "numTwoHandedIndicatorLeft", "battlescape", _icons);
	add(_numTwoHandedIndicatorRight, "numTwoHandedIndicatorRight", "battlescape", _icons);
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		add(_btnVisibleUnit[i]);
		add(_numVisibleUnit[i]);
	}
	add(_warning, "warning", "battlescape", _icons);
	add(_txtDebug);
	add(_txtTooltip, "textTooltip", "battlescape", _icons);
	add(_btnLaunch);
	_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(0)->blit(_btnLaunch);
	add(_btnPsi);
	_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(1)->blit(_btnPsi);
	add(_btnSpecial);
	_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(1)->blit(_btnSpecial); // use psi button for default

	// Set up objects
	_save = _game->getSavedGame()->getSavedBattle();
	_map->init();
	_map->onMouseOver((ActionHandler)&BattlescapeState::mapOver);
	_map->onMousePress((ActionHandler)&BattlescapeState::mapPress);
	_map->onMouseRelease((ActionHandler)&BattlescapeState::mapRelease);
	_map->onMouseClick((ActionHandler)&BattlescapeState::mapClick, 0);
	_map->onMouseIn((ActionHandler)&BattlescapeState::mapIn);
	_map->onMultiGesture((ActionHandler)&BattlescapeState::multiGesture);
	_map->onFingerMotion((ActionHandler)&BattlescapeState::fingerMotion);
	_map->onKeyboardPress((ActionHandler)&BattlescapeState::mapKey, 0);

	_numLayers->setColor(Palette::blockOffset(1)-2);
	_numLayers->setValue(1);

	for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
	{
		_numAmmoLeft[slot]->setValue(999);
		_numAmmoRight[slot]->setValue(999);
	}
	for (int slot = 0; slot < RuleItem::MedikitSlots; ++slot)
	{
		_numMedikitLeft[slot]->setValue(999);
		_numMedikitRight[slot]->setValue(999);
	}
	_numTwoHandedIndicatorLeft->setValue(2);
	_numTwoHandedIndicatorRight->setValue(2);

	_icons->onMouseIn((ActionHandler)&BattlescapeState::mouseInIcons);
	_icons->onMouseOut((ActionHandler)&BattlescapeState::mouseOutIcons);

	_btnUnitUp->onMouseClick((ActionHandler)&BattlescapeState::btnUnitUpClick);
	_btnUnitUp->setTooltip("STR_UNIT_LEVEL_ABOVE");
	_btnUnitUp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnUnitUp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnUnitDown->onMouseClick((ActionHandler)&BattlescapeState::btnUnitDownClick);
	_btnUnitDown->setTooltip("STR_UNIT_LEVEL_BELOW");
	_btnUnitDown->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnUnitDown->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnMapUp->onMouseClick((ActionHandler)&BattlescapeState::btnMapUpClick);
	_btnMapUp->onKeyboardPress((ActionHandler)&BattlescapeState::btnMapUpClick, Options::keyBattleLevelUp);
	_btnMapUp->setTooltip("STR_VIEW_LEVEL_ABOVE");
	_btnMapUp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnMapUp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnMapDown->onMouseClick((ActionHandler)&BattlescapeState::btnMapDownClick);
	_btnMapDown->onKeyboardPress((ActionHandler)&BattlescapeState::btnMapDownClick, Options::keyBattleLevelDown);
	_btnMapDown->setTooltip("STR_VIEW_LEVEL_BELOW");
	_btnMapDown->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnMapDown->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnShowMap->onMouseClick((ActionHandler)&BattlescapeState::btnShowMapClick);
	_btnShowMap->onKeyboardPress((ActionHandler)&BattlescapeState::btnShowMapClick, Options::keyBattleMap);
	_btnShowMap->setTooltip("STR_MINIMAP");
	_btnShowMap->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnShowMap->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnKneel->onMouseClick((ActionHandler)&BattlescapeState::btnKneelClick);
	_btnKneel->onKeyboardPress((ActionHandler)&BattlescapeState::btnKneelClick, Options::keyBattleKneel);
	_btnKneel->setTooltip("STR_KNEEL");
	_btnKneel->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnKneel->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnKneel->allowToggleInversion();

	_btnInventory->onMouseClick((ActionHandler)&BattlescapeState::btnInventoryClick);
	_btnInventory->onKeyboardPress((ActionHandler)&BattlescapeState::btnInventoryClick, Options::keyBattleInventory);
	_btnInventory->setTooltip("STR_INVENTORY");
	_btnInventory->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnInventory->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnCenter->onMouseClick((ActionHandler)&BattlescapeState::btnCenterClick);
	_btnCenter->onKeyboardPress((ActionHandler)&BattlescapeState::btnCenterClick, Options::keyBattleCenterUnit);
	_btnCenter->setTooltip("STR_CENTER_SELECTED_UNIT");
	_btnCenter->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnCenter->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnNextSoldier->onMouseClick((ActionHandler)&BattlescapeState::btnNextSoldierClick, SDL_BUTTON_LEFT);
	_btnNextSoldier->onMouseClick((ActionHandler)&BattlescapeState::btnPrevSoldierClick, SDL_BUTTON_RIGHT);
	_btnNextSoldier->onKeyboardPress((ActionHandler)&BattlescapeState::btnNextSoldierClick, Options::keyBattleNextUnit);
	_btnNextSoldier->onKeyboardPress((ActionHandler)&BattlescapeState::btnPrevSoldierClick, Options::keyBattlePrevUnit);
	_btnNextSoldier->setTooltip("STR_NEXT_UNIT");
	_btnNextSoldier->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnNextSoldier->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnNextStop->onMouseClick((ActionHandler)&BattlescapeState::btnNextStopClick);
	_btnNextStop->onKeyboardPress((ActionHandler)&BattlescapeState::btnNextStopClick, Options::keyBattleDeselectUnit);
	_btnNextStop->setTooltip("STR_DESELECT_UNIT");
	_btnNextStop->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnNextStop->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnShowLayers->onMouseClick((ActionHandler)&BattlescapeState::btnShowLayersClick);
	_btnShowLayers->setTooltip("STR_MULTI_LEVEL_VIEW");
	_btnShowLayers->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnShowLayers->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnShowLayers->onKeyboardPress((ActionHandler)&BattlescapeState::btnUfopaediaClick, Options::keyGeoUfopedia);

	_btnHelp->onMouseClick((ActionHandler)&BattlescapeState::btnHelpClick);
	_btnHelp->onKeyboardPress((ActionHandler)&BattlescapeState::btnHelpClick, Options::keyBattleOptions);
	_btnHelp->setTooltip("STR_OPTIONS");
	_btnHelp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnHelp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnEndTurn->onMouseClick((ActionHandler)&BattlescapeState::btnEndTurnClick);
	_btnEndTurn->onKeyboardPress((ActionHandler)&BattlescapeState::btnEndTurnClick, Options::keyBattleEndTurn);
	_btnEndTurn->setTooltip("STR_END_TURN");
	_btnEndTurn->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipInEndTurn);
	_btnEndTurn->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnAbort->onMouseClick((ActionHandler)&BattlescapeState::btnAbortClick);
	_btnAbort->onKeyboardPress((ActionHandler)&BattlescapeState::btnAbortClick, Options::keyBattleAbort);
	_btnAbort->setTooltip("STR_ABORT_MISSION");
	_btnAbort->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnAbort->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnStats->onMouseClick((ActionHandler)&BattlescapeState::btnStatsClick);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnStatsClick, Options::keyBattleStats);
	_btnStats->setTooltip("STR_UNIT_STATS");
	_btnStats->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnStats->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnLeftHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnLeftHandItemClick);
	_btnLeftHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnLeftHandItemClick, SDL_BUTTON_MIDDLE);
	_btnLeftHandItem->onKeyboardPress((ActionHandler)&BattlescapeState::btnLeftHandItemClick, Options::keyBattleUseLeftHand);
	_btnLeftHandItem->setTooltip("STR_USE_LEFT_HAND");
	_btnLeftHandItem->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipInExtraLeftHand);
	_btnLeftHandItem->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnRightHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnRightHandItemClick);
	_btnRightHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnRightHandItemClick, SDL_BUTTON_MIDDLE);
	_btnRightHandItem->onKeyboardPress((ActionHandler)&BattlescapeState::btnRightHandItemClick, Options::keyBattleUseRightHand);
	_btnRightHandItem->setTooltip("STR_USE_RIGHT_HAND");
	_btnRightHandItem->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipInExtraRightHand);
	_btnRightHandItem->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveNone->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveNone->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveNone);
	_btnReserveNone->setTooltip("STR_DONT_RESERVE_TIME_UNITS");
	_btnReserveNone->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveNone->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveSnap->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveSnap->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveSnap);
	_btnReserveSnap->setTooltip("STR_RESERVE_TIME_UNITS_FOR_SNAP_SHOT");
	_btnReserveSnap->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveSnap->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveAimed->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveAimed->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveAimed);
	_btnReserveAimed->setTooltip("STR_RESERVE_TIME_UNITS_FOR_AIMED_SHOT");
	_btnReserveAimed->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveAimed->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveAuto->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveAuto->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveAuto);
	_btnReserveAuto->setTooltip("STR_RESERVE_TIME_UNITS_FOR_AUTO_SHOT");
	_btnReserveAuto->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveAuto->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveKneel->onMouseClick((ActionHandler)&BattlescapeState::btnReserveKneelClick);
	_btnReserveKneel->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveKneelClick, Options::keyBattleReserveKneel);
	_btnReserveKneel->setTooltip("STR_RESERVE_TIME_UNITS_FOR_KNEEL");
	_btnReserveKneel->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveKneel->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnReserveKneel->allowToggleInversion();

	_btnZeroTUs->onMouseClick((ActionHandler)&BattlescapeState::btnZeroTUsClick, SDL_BUTTON_RIGHT);
#ifdef __MOBILE__
	_btnZeroTUs->onMousePress((ActionHandler)&BattlescapeState::btnZeroTUsPress);
	_btnZeroTUs->onMouseRelease((ActionHandler)&BattlescapeState::btnZeroTUsRelease);
#endif
	_btnZeroTUs->onKeyboardPress((ActionHandler)&BattlescapeState::btnZeroTUsClick, Options::keyBattleZeroTUs);
	_btnZeroTUs->setTooltip("STR_EXPEND_ALL_TIME_UNITS");
	_btnZeroTUs->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnZeroTUs->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnZeroTUs->allowClickInversion();

	// shortcuts without a specific button
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnReloadClick, Options::keyBattleReload);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnSelectMusicTrackClick, Options::keySelectMusicTrack);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnPersonalLightingClick, Options::keyBattlePersonalLighting);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnNightVisionClick, Options::keyNightVisionToggle);
	if (Options::autoNightVision)
	{
		// during the night
		if (_save->getGlobalShade() > _game->getMod()->getMaxDarknessToSeeUnits())
		{
			// turn personal lighting off
			_save->getTileEngine()->togglePersonalLighting();
			// turn night vision on
			_map->toggleNightVision();
		}
	}

	SDL_Keycode buttons[] = {Options::keyBattleCenterEnemy1,
						Options::keyBattleCenterEnemy2,
						Options::keyBattleCenterEnemy3,
						Options::keyBattleCenterEnemy4,
						Options::keyBattleCenterEnemy5,
						Options::keyBattleCenterEnemy6,
						Options::keyBattleCenterEnemy7,
						Options::keyBattleCenterEnemy8,
						Options::keyBattleCenterEnemy9,
						Options::keyBattleCenterEnemy10};
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		std::ostringstream tooltip;
		_btnVisibleUnit[i]->onMousePress((ActionHandler)&BattlescapeState::consumeEvent);
		_btnVisibleUnit[i]->onMouseRelease((ActionHandler)&BattlescapeState::consumeEvent);
		_btnVisibleUnit[i]->onMouseClick((ActionHandler)&BattlescapeState::btnVisibleUnitClick);
		_btnVisibleUnit[i]->onKeyboardPress((ActionHandler)&BattlescapeState::btnVisibleUnitClick, buttons[i]);
		tooltip << "STR_CENTER_ON_ENEMY_" << (i+1);
		_txtVisibleUnitTooltip[i] = tooltip.str();
		_btnVisibleUnit[i]->setTooltip(_txtVisibleUnitTooltip[i]);
		_btnVisibleUnit[i]->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
		_btnVisibleUnit[i]->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
		_numVisibleUnit[i]->setColor(_indicatorTextColor);
		_numVisibleUnit[i]->setValue(i+1);
	}
	_txtVisibleUnitTooltip[VISIBLE_MAX] = "STR_CENTER_ON_WOUNDED_FRIEND";
	_txtVisibleUnitTooltip[VISIBLE_MAX+1] = "STR_CENTER_ON_SHOCKED_FRIEND";

	_warning->setColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color2);
	_warning->setTextColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color);
	_btnLaunch->onMouseClick((ActionHandler)&BattlescapeState::btnLaunchClick);
	_btnPsi->onMouseClick((ActionHandler)&BattlescapeState::btnPsiClick);

	_btnSpecial->onMouseClick((ActionHandler)&BattlescapeState::btnSpecialClick);
	_btnSpecial->onMouseClick((ActionHandler)&BattlescapeState::btnSpecialClick, SDL_BUTTON_MIDDLE);
	_btnSpecial->onKeyboardPress((ActionHandler)&BattlescapeState::btnSpecialClick, Options::keyBattleUseSpecial);
	_btnSpecial->setTooltip("STR_USE_SPECIAL_ITEM");
	_btnSpecial->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipInExtraSpecial);
	_btnSpecial->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_txtName->setHighContrast(true);

	_barTimeUnits->setScale(1.0);
	_barEnergy->setScale(1.0);
	_barHealth->setScale(1.0);
	_barMorale->setScale(1.0);

	_txtDebug->setColor(Palette::blockOffset(8));
	_txtDebug->setHighContrast(true);

	_txtTooltip->setHighContrast(true);

	_btnReserveNone->setGroup(&_reserve);
	_btnReserveSnap->setGroup(&_reserve);
	_btnReserveAimed->setGroup(&_reserve);
	_btnReserveAuto->setGroup(&_reserve);

	// Set music
	if (!Options::playBriefingMusicDuringEquipment)
	{
		if (_save->getMusic() == "")
		{
			_game->getMod()->playMusic("GMTACTIC");
		}
		else
		{
			_game->getMod()->playMusic(_save->getMusic());
		}
	}

	_animTimer = new Timer(DEFAULT_ANIM_SPEED, true);
	_animTimer->onTimer((StateHandler)&BattlescapeState::animate);

	_gameTimer = new Timer(DEFAULT_ANIM_SPEED, true);
	_gameTimer->onTimer((StateHandler)&BattlescapeState::handleState);
	
#ifdef __MOBILE__
	_longPressTimer = new Timer(Options::longPressDuration, false);
	_longPressTimer->onTimer((StateHandler)&BattlescapeState::mapLongPress);
#endif

	_battleGame = new BattlescapeGame(_save, this);

	_barHealthColor = _barHealth->getColor();

	_firstInit = true;
	_isMouseScrolling = false;
	_isMouseScrolled = false;
	_currentTooltip = "";
}


/**
 * Deletes the battlescapestate.
 */
BattlescapeState::~BattlescapeState()
{
#ifdef __MOBILE__
	delete _longPressTimer;
#endif
	delete _animTimer;
	delete _gameTimer;
	delete _battleGame;

	resetPalettes();
}

void BattlescapeState::resetPalettes()
{
	if (_paletteResetNeeded)
	{
		for (auto origPal : _game->getMod()->getPalettes())
		{
			if (origPal.first.find("PAL_") == 0)
			{
				std::string backupName = "BACKUP_" + origPal.first;
				Palette *backupPal = _game->getMod()->getPalette(backupName, false);
				if (backupPal)
				{
					origPal.second->copyFrom(backupPal);
				}
			}
		}
		_paletteResetNeeded = false;
	}
}

/**
 * Initializes the battlescapestate.
 */
void BattlescapeState::init()
{
	if (_paletteResetRequested)
	{
		_paletteResetRequested = false;

		resetPalettes();
		_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);
		for (auto surface : _surfaces)
		{
			surface->setPalette(_palette);
		}
	}

	if (_save->getAmbientSound() != -1)
	{
		_game->getMod()->getSoundByDepth(_save->getDepth(), _save->getAmbientSound())->loop();
		_game->setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	}

	State::init();
	_animTimer->start();
	_gameTimer->start();
	_map->setFocus(true);
	_map->draw();
	_battleGame->init();
	updateSoldierInfo();

	switch (_save->getTUReserved())
	{
	case BA_SNAPSHOT:
		_reserve = _btnReserveSnap;
		break;
	case BA_AIMEDSHOT:
		_reserve = _btnReserveAimed;
		break;
	case BA_AUTOSHOT:
		_reserve = _btnReserveAuto;
		break;
	default:
		_reserve = _btnReserveNone;
		break;
	}
	if (_firstInit)
	{
		// Set music
		if (Options::playBriefingMusicDuringEquipment)
		{
			if (_save->getMusic() == "")
			{
				_game->getMod()->playMusic("GMTACTIC");
			}
			else
			{
				_game->getMod()->playMusic(_save->getMusic());
			}
		}

		if (!playableUnitSelected())
		{
			selectNextPlayerUnit();
		}
		if (playableUnitSelected())
		{
			_battleGame->setupCursor();
			_map->getCamera()->centerOnPosition(_save->getSelectedUnit()->getPosition());
		}
		_firstInit = false;
		_btnReserveNone->setGroup(&_reserve);
		_btnReserveSnap->setGroup(&_reserve);
		_btnReserveAimed->setGroup(&_reserve);
		_btnReserveAuto->setGroup(&_reserve);
	}
	_txtTooltip->setText(L"");
	_btnReserveKneel->toggle(_save->getKneelReserved());
	_battleGame->setKneelReserved(_save->getKneelReserved());
	if (_autosave)
	{
		_autosave = false;
		if (_game->getSavedGame()->isIronman())
		{
			_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_IRONMAN, _palette));
		}
		else if (Options::autosave)
		{
			_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_AUTO_BATTLESCAPE, _palette));
		}
	}
}

/**
 * Runs the timers and handles popups.
 */
void BattlescapeState::think()
{
	static bool popped = false;

	if (_gameTimer->isRunning())
	{
		if (_popups.empty())
		{
			State::think();
			_battleGame->think();
			_animTimer->think(this, 0);
			_gameTimer->think(this, 0);
#ifdef __MOBILE__
			_longPressTimer->think(this, 0);
#endif
			if (popped)
			{
				_battleGame->handleNonTargetAction();
				popped = false;
			}
		}
		else
		{
			// Handle popups
			_game->pushState(*_popups.begin());
			_popups.erase(_popups.begin());
			popped = true;
			return;
		}
	}
}

/**
 * Processes any mouse moving over the map.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapOver(Action *action)
{

	if (_swipeFromSoldier || _multiGestureProcess)
		return;

	if (_isMouseScrolling && action->getDetails()->type == SDL_MOUSEMOTION)
	{
		// The following is the workaround for a rare problem where sometimes
		// the mouse-release event is missed for any reason.
		// (checking: is the dragScroll-mouse-button still pressed?)
		// However if the SDL is also missed the release event, then it is to no avail :(
		if ((CrossPlatform::getPointerState(0,0)&SDL_BUTTON(Options::battleDragScrollButton)) == 0)
		{ // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
			return;
		}

		_isMouseScrolled = true;

#ifndef __MOBILE__
		if (Options::touchEnabled == false)
		{
			// Set the mouse cursor back
			SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
			//SDL_WarpMouseInWindow(NULL, _game->getScreen()->getWidth() / 2, _game->getScreen()->getHeight() / 2 - _map->getIconHeight() / 2);
			SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
		}
#endif

		// Check the threshold
		_totalMouseMoveX += action->getDetails()->motion.xrel;
		_totalMouseMoveY += action->getDetails()->motion.yrel;
		if (!_mouseMovedOverThreshold)
		{
			_mouseMovedOverThreshold = ((std::abs(_totalMouseMoveX) > Options::dragScrollPixelTolerance) || (std::abs(_totalMouseMoveY) > Options::dragScrollPixelTolerance));
		}

		// Scrolling
		if (Options::battleDragScrollInvert)
		{
			_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			int scrollX = -(int)((double)_totalMouseMoveX / action->getXScale());
			int scrollY = -(int)((double)_totalMouseMoveY / action->getYScale());
			Position delta2 = _map->getCamera()->getMapOffset();
			_map->getCamera()->scrollXY(scrollX, scrollY, true);
			delta2 = _map->getCamera()->getMapOffset() - delta2;

			// Keep the limits...
			if (scrollX != delta2.x || scrollY != delta2.y)
			{
				_totalMouseMoveX = -(int) (delta2.x * action->getXScale());
				_totalMouseMoveY = -(int) (delta2.y * action->getYScale());
			}

			if (Options::touchEnabled == false)
			{
				action->getDetails()->motion.x = _xBeforeMouseScrolling;
				action->getDetails()->motion.y = _yBeforeMouseScrolling;
			}
			_map->setCursorType(CT_NONE);
		}
		else
		{
			Position delta = _map->getCamera()->getMapOffset();
			_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			int scrollX = (int)((double)_totalMouseMoveX / action->getXScale());
			int scrollY = (int)((double)_totalMouseMoveY / action->getYScale());
			Position delta2 = _map->getCamera()->getMapOffset();
			_map->getCamera()->scrollXY(scrollX, scrollY, true);
			delta2 = _map->getCamera()->getMapOffset() - delta2;
			delta = _map->getCamera()->getMapOffset() - delta;

			// Keep the limits...
			if (scrollX != delta2.x || scrollY != delta2.y)
			{
				_totalMouseMoveX = (int) (delta2.x * action->getXScale());
				_totalMouseMoveY = (int) (delta2.y * action->getYScale());
			}

			int barWidth = _game->getScreen()->getCursorLeftBlackBand();
			int barHeight = _game->getScreen()->getCursorTopBlackBand();
			int cursorX = _cursorPosition.x + Round(delta.x * action->getXScale());
			int cursorY = _cursorPosition.y + Round(delta.y * action->getYScale());
			_cursorPosition.x = std::min(_game->getScreen()->getWidth() - barWidth - (int)(Round(action->getXScale())), std::max(barWidth, cursorX));
			_cursorPosition.y = std::min(_game->getScreen()->getHeight() - barHeight - (int)(Round(action->getYScale())), std::max(barHeight, cursorY));

			if (Options::touchEnabled == false)
			{
				action->getDetails()->motion.x = _cursorPosition.x;
				action->getDetails()->motion.y = _cursorPosition.y;
			}
		}

		// We don't want to look the mouse-cursor jumping :)
		_game->getCursor()->handle(action);
	}
}

/**
 * Processes any presses on the map.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapPress(Action *action)
{
	// don't handle mouseclicks over the buttons (it overlaps with map surface)
	if (_mouseOverIcons) return;

	if (action->getDetails()->button.button == Options::battleDragScrollButton)
	{
		_isMouseScrolling = true;
		_isMouseScrolled = false;
		CrossPlatform::getPointerState(&_xBeforeMouseScrolling, &_yBeforeMouseScrolling);
		_mapOffsetBeforeMouseScrolling = _map->getCamera()->getMapOffset();
		if (!Options::battleDragScrollInvert && _cursorPosition.z == 0)
		{
			_cursorPosition.x = action->getDetails()->button.x;
			_cursorPosition.y = action->getDetails()->button.y;
			// the Z is irrelevant to our mouse position, but we can use it as a boolean to check if the position is set or not
			_cursorPosition.z = 1;
		}
		_totalMouseMoveX = 0; _totalMouseMoveY = 0;
		_mouseMovedOverThreshold = false;
		_mouseScrollingStartTime = SDL_GetTicks();
	}

#ifdef __MOBILE__
	_longPressTimer->onTimer((StateHandler)&BattlescapeState::mapLongPress);
	_longPressTimer->start();
	Position pos;
	/* Avoid null pointer dereference */
	BattleUnit *selectedUnit = _save->getSelectedUnit();
	if (selectedUnit != NULL)
	{
		_map->getSelectorPosition(&pos);
		if (pos == selectedUnit->getPosition())
		{
			_swipeFromSoldier = true;
		}
		else
		{
			_swipeFromSoldier = false;
		}
	}
	/* Avoid null pointer in the mapRelease function */
	else
	{
		_swipeFromSoldier = false;
	}
#endif
}

/**
 * Turns the soldier in the swipe direction.
 * @param action Pointer to an action.
 */

void BattlescapeState::mapRelease(Action *action)
{
	_multiGestureProcess = false;
#ifdef __MOBILE__
	_longPressTimer->stop();
	// Do nothing if this method of turning is disabled
	if (!Options::swipeToTurn)
	{
		return;
	}
	Position pos;
	_map->getSelectorPosition(&pos);
	if (_swipeFromSoldier)
	{
		if (pos != _save->getSelectedUnit()->getPosition())
			_battleGame->secondaryAction(pos);
		_swipeFromSoldier = false;
	}
#endif
}


#ifdef __MOBILE__
/**
* Handles long presses to turn the soldier.
*/
void BattlescapeState::mapLongPress()
{
	// Stop the timer first.
	_longPressTimer->stop();
	// If the event fired while the camera was moving,
	// then we don't want to handle it.
	if (_mouseMovedOverThreshold)
	{
		return;
	}

	Position pos;
	_map->getSelectorPosition(&pos);
	_battleGame->longPressAction(pos);
}
#endif

/**
 * Processes any clicks on the map to
 * command units.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapClick(Action *action)
{
	// The following is the workaround for a rare problem where sometimes
	// the mouse-release event is missed for any reason.
	// However if the SDL is also missed the release event, then it is to no avail :(
	// (this part handles the release if it is missed and now an other button is used)
	if (_isMouseScrolling)
	{
		if (action->getDetails()->button.button != Options::battleDragScrollButton
		&& (CrossPlatform::getPointerState(0,0)&SDL_BUTTON(Options::battleDragScrollButton)) == 0)
		{   // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
		}
	}

	// DragScroll-Button release: release mouse-scroll-mode
	if (_isMouseScrolling)
	{
		// While scrolling, other buttons are ineffective
		if (action->getDetails()->button.button == Options::battleDragScrollButton)
		{
			_isMouseScrolling = false;
			stopScrolling(action);
		}
		else
		{
			return;
		}
		// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
		if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
		{
			_isMouseScrolled = false;
			stopScrolling(action);
		}
		if (_isMouseScrolled) return;
	}

	// right-click aborts walking state
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		if (_battleGame->cancelCurrentAction())
		{
			return;
		}
	}

	// don't handle mouseclicks over the buttons (it overlaps with map surface)
	if (_mouseOverIcons) return;


	// don't accept leftclicks if there is no cursor or there is an action busy
	if (_map->getCursorType() == CT_NONE || _battleGame->isBusy()) return;

	Position pos;
	_map->getSelectorPosition(&pos);

	if (_save->getDebugMode())
	{
		std::wostringstream ss;
		ss << L"Clicked " << pos;
		debug(ss.str());
	}

	if (_save->getTile(pos) != 0) // don't allow to click into void
	{
		if ((action->getDetails()->button.button == SDL_BUTTON_RIGHT || (action->getDetails()->button.button == SDL_BUTTON_LEFT && (SDL_GetModState() & KMOD_ALT) != 0)) && playableUnitSelected())
		{
			_battleGame->secondaryAction(pos);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		{
			_battleGame->primaryAction(pos);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_MIDDLE)
		{
			_battleGame->cancelCurrentAction();
			BattleUnit *bu = _save->selectUnit(pos);
			if (bu && (bu->getVisible() || _save->getDebugMode()))
			{
				_game->pushState(new AlienInventoryState(bu));
			}
		}
	}
}

/**
 * Handles mouse entering the map surface.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapIn(Action *)
{
	_isMouseScrolling = false;
	_map->setButtonsPressed(Options::battleDragScrollButton, false);
}

void BattlescapeState::fingerMotion(Action *action)
{
	//don't scroll if we swipe from soldier, which is used to substitute right
	//click in android
	if (_swipeFromSoldier)
		return;
}

/**
 * Handles multi-finger gestures on the battlefield.
 * Currently it is used for changing elevation.
 * @param action Pointer to an action.
 */

void BattlescapeState::multiGesture(Action *action)
{
	//Log(LOG_INFO) << "Multigesture processing!";
	const double levelThreshold = 0.1; //Just an arbitrary number
	static double delta;
	static double prevPointY = 0;
	if (!_multiGestureProcess)
	{
		_multiGestureProcess = true;
		delta = 0;
		prevPointY = action->getDetails()->mgesture.y;
#ifdef __MOBILE__
		_longPressTimer->stop();
#endif
		return;
	}
	delta += action->getDetails()->mgesture.y - prevPointY;
	prevPointY = action->getDetails()->mgesture.y;
	if (std::abs(delta) > levelThreshold)
	{
		if (delta > 0) 
		{
		      btnMapUpClick(action);
		}
		else
		{
		      btnMapDownClick(action);
		}
		delta = 0;
	}
}

void BattlescapeState::mapKey(Action *action)
{
	if (action->getDetails()->key.keysym.scancode == SDL_SCANCODE_AC_BACK)
	{
		_battleGame->cancelCurrentAction();
	}
}

/**
 * Moves the selected unit up.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnUnitUpClick(Action *)
{
	if (playableUnitSelected() && _save->getPathfinding()->validateUpDown(_save->getSelectedUnit(), _save->getSelectedUnit()->getPosition(), Pathfinding::DIR_UP))
	{
		_battleGame->cancelCurrentAction();
		_battleGame->moveUpDown(_save->getSelectedUnit(), Pathfinding::DIR_UP);
	}
}

/**
 * Moves the selected unit down.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnUnitDownClick(Action *)
{
	if (playableUnitSelected() && _save->getPathfinding()->validateUpDown(_save->getSelectedUnit(), _save->getSelectedUnit()->getPosition(), Pathfinding::DIR_DOWN))
	{
		_battleGame->cancelCurrentAction();
		_battleGame->moveUpDown(_save->getSelectedUnit(), Pathfinding::DIR_DOWN);
	}
}

/**
 * Shows the next map layer.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnMapUpClick(Action *)
{
	if (_save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		_map->getCamera()->up();
}

/**
 * Shows the previous map layer.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnMapDownClick(Action *)
{
	if (_save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		_map->getCamera()->down();
}

/**
 * Shows the minimap.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnShowMapClick(Action *)
{
	//MiniMapState
	if (allowButtons())
		_game->pushState (new MiniMapState (_map->getCamera(), _save));
}

void BattlescapeState::toggleKneelButton(BattleUnit* unit)
{
	if (_btnKneel->isTFTDMode())
	{
		_btnKneel->toggle(unit && unit->isKneeled());
	}
	else
	{
		_game->getMod()->getSurfaceSet("KneelButton")->getFrame((unit && unit->isKneeled()) ? 1 : 0)->blit(_btnKneel);
	}
}

/**
 * Toggles the current unit's kneel/standup status.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnKneelClick(Action *)
{
	if (allowButtons())
	{
		BattleUnit *bu = _save->getSelectedUnit();
		if (bu)
		{
			_battleGame->kneel(bu);
			toggleKneelButton(bu);

			// update any path preview when unit kneels
			if (_battleGame->getPathfinding()->isPathPreviewed())
			{
				_battleGame->getPathfinding()->calculate(_battleGame->getCurrentAction()->actor, _battleGame->getCurrentAction()->target);
				_battleGame->getPathfinding()->removePreview();
				_battleGame->getPathfinding()->previewPath();
			}
		}
	}
}

/**
 * Goes to the soldier info screen.
 * Additionally resets TUs for current side in debug mode.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnInventoryClick(Action *)
{
	if (_save->getDebugMode())
	{
		for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
			if ((*i)->getFaction() == _save->getSide())
				(*i)->prepareNewTurn();
		updateSoldierInfo();
	}
	if (playableUnitSelected()
		&& (_save->getSelectedUnit()->hasInventory() || _save->getDebugMode()))
	{
		// clean up the waypoints
		if (_battleGame->getCurrentAction()->type == BA_LAUNCH)
		{
			_battleGame->getCurrentAction()->waypoints.clear();
			_battleGame->getMap()->getWaypoints()->clear();
			showLaunchButton(false);
		}

		_battleGame->getPathfinding()->removePreview();
		_battleGame->cancelCurrentAction(true);

		_game->pushState(new InventoryState(!_save->getDebugMode(), this, 0));
	}
}

/**
 * Centers on the currently selected soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnCenterClick(Action *)
{
	if (playableUnitSelected())
	{
		_map->getCamera()->centerOnPosition(_save->getSelectedUnit()->getPosition());
		_map->refreshSelectorPosition();
	}
}

/**
 * Selects the next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnNextSoldierClick(Action *)
{
	if (allowButtons())
	{
		selectNextPlayerUnit(true, false);
		_map->refreshSelectorPosition();
	}
}

/**
 * Disables reselection of the current soldier and selects the next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnNextStopClick(Action *)
{
	if (allowButtons())
		selectNextPlayerUnit(true, true);
}

/**
 * Selects next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPrevSoldierClick(Action *)
{
	if (allowButtons())
	{
		selectPreviousPlayerUnit(true);
		_map->refreshSelectorPosition();
	}
}

/**
 * Selects the next soldier.
 * @param checkReselect When true, don't select a unit that has been previously flagged.
 * @param setReselect When true, flag the current unit first.
 * @param checkInventory When true, don't select a unit that has no inventory.
 */
void BattlescapeState::selectNextPlayerUnit(bool checkReselect, bool setReselect, bool checkInventory, bool checkFOV)
{
	if (allowButtons())
	{
		if (_battleGame->getCurrentAction()->type != BA_NONE) return;
		BattleUnit *unit = _save->selectNextPlayerUnit(checkReselect, setReselect, checkInventory);
		updateSoldierInfo(checkFOV);
		if (unit) _map->getCamera()->centerOnPosition(unit->getPosition());
		_battleGame->cancelCurrentAction();
		_battleGame->getCurrentAction()->actor = unit;
		_battleGame->setupCursor();
	}
}

/**
 * Selects the previous soldier.
 * @param checkReselect When true, don't select a unit that has been previously flagged.
 * @param setReselect When true, flag the current unit first.
 * @param checkInventory When true, don't select a unit that has no inventory.
 */
void BattlescapeState::selectPreviousPlayerUnit(bool checkReselect, bool setReselect, bool checkInventory)
{
	if (allowButtons())
	{
		if (_battleGame->getCurrentAction()->type != BA_NONE) return;
		BattleUnit *unit = _save->selectPreviousPlayerUnit(checkReselect, setReselect, checkInventory);
		updateSoldierInfo();
		if (unit) _map->getCamera()->centerOnPosition(unit->getPosition());
		_battleGame->cancelCurrentAction();
		_battleGame->getCurrentAction()->actor = unit;
		_battleGame->setupCursor();
	}
}

/**
 * Shows/hides all map layers.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnShowLayersClick(Action *)
{
	_numLayers->setValue(_map->getCamera()->toggleShowAllLayers());
}

/**
 * Opens the Ufopaedia.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnUfopaediaClick(Action *)
{
	if (allowButtons())
	{
		Ufopaedia::open(_game);
	}
}

/**
 * Shows options.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnHelpClick(Action *)
{
	if (allowButtons(true))
		_game->pushState(new PauseState(OPT_BATTLESCAPE));
}

/**
 * Requests the end of turn. This will add a 0 to the end of the state queue,
 * so all ongoing actions, like explosions are finished first before really switching turn.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnEndTurnClick(Action *)
{
	if (allowButtons())
	{
		_txtTooltip->setText(L"");
		_battleGame->requestEndTurn(false);
	}
}

/**
 * Aborts the game.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnAbortClick(Action *)
{
	if (allowButtons())
		_game->pushState(new AbortMissionState(_save, this));
}

/**
 * Shows the selected soldier's info.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnStatsClick(Action *action)
{
	if (playableUnitSelected())
	{
		bool b = true;
		if (SCROLL_TRIGGER == Options::battleEdgeScroll &&
			SDL_MOUSEBUTTONUP == action->getDetails()->type && SDL_BUTTON_LEFT == action->getDetails()->button.button)
		{
			int posX = action->getXMouse();
			int posY = action->getYMouse();
			if ((posX < (Camera::SCROLL_BORDER * action->getXScale()) && posX > 0)
				|| (posX > (_map->getWidth() - Camera::SCROLL_BORDER) * action->getXScale())
				|| (posY < (Camera::SCROLL_BORDER * action->getYScale()) && posY > 0)
				|| (posY > (_map->getHeight() - Camera::SCROLL_BORDER) * action->getYScale()))
				// To avoid handling this event as a click
				// on the stats button when the mouse is on the scroll-border
				b = false;
		}
		// clean up the waypoints
		if (_battleGame->getCurrentAction()->type == BA_LAUNCH)
		{
			_battleGame->getCurrentAction()->waypoints.clear();
			_battleGame->getMap()->getWaypoints()->clear();
			showLaunchButton(false);
		}

		_battleGame->cancelCurrentAction(true);

		if (b) popup(new UnitInfoState(_save->getSelectedUnit(), this, false, false));
	}
}

/**
 * Shows an action popup menu. When clicked, creates the action.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnLeftHandItemClick(Action *action)
{
	if (playableUnitSelected())
	{
		// concession for touch devices:
		// click on the item to cancel action, and don't pop up a menu to select a new one
		// TODO: wrap this in an IFDEF ?
		if (_battleGame->getCurrentAction()->targeting)
		{
			_battleGame->cancelCurrentAction();
#ifdef __MOBILE__
			if (!_battleGame->getCurrentAction()->targeting)
			{
				_leftWpnActive->setVisible(false);
				_rightWpnActive->setVisible(false);
			}
#endif
			return;
		}

		_battleGame->cancelCurrentAction();

		_save->getSelectedUnit()->setActiveHand("STR_LEFT_HAND");
		_map->draw();
		BattleItem *leftHandItem = _save->getSelectedUnit()->getLeftHandWeapon();
		if (!leftHandItem)
		{
			auto typesToCheck = {BT_MELEE, BT_PSIAMP, BT_FIREARM, BT_MEDIKIT, BT_SCANNER, BT_MINDPROBE};
			for (auto &type : typesToCheck)
			{
				leftHandItem = _save->getSelectedUnit()->getSpecialWeapon(type);
				if (leftHandItem && leftHandItem->getRules()->isSpecialUsingEmptyHand())
				{
					break;
				}
				leftHandItem = 0;
			}
		}
		bool middleClick = action->getDetails()->button.button == SDL_BUTTON_MIDDLE;
		handleItemClick(leftHandItem, middleClick);
	}
}

/**
 * Shows an action popup menu. When clicked, create the action.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnRightHandItemClick(Action *action)
{
	if (playableUnitSelected())
	{
		// concession for touch devices:
		// click on the item to cancel action, and don't pop up a menu to select a new one
		// TODO: wrap this in an IFDEF ?
		if (_battleGame->getCurrentAction()->targeting)
		{
			_battleGame->cancelCurrentAction();
#ifdef __MOBILE__
			if (!_battleGame->getCurrentAction()->targeting)
			{
				_leftWpnActive->setVisible(false);
				_rightWpnActive->setVisible(false);
			}
#endif
			return;
		}

		_battleGame->cancelCurrentAction();

		_save->getSelectedUnit()->setActiveHand("STR_RIGHT_HAND");
		_map->draw();
		BattleItem *rightHandItem = _save->getSelectedUnit()->getRightHandWeapon();
		if (!rightHandItem)
		{
			auto typesToCheck = {BT_MELEE, BT_PSIAMP, BT_FIREARM, BT_MEDIKIT, BT_SCANNER, BT_MINDPROBE};
			for (auto &type : typesToCheck)
			{
				rightHandItem = _save->getSelectedUnit()->getSpecialWeapon(type);
				if (rightHandItem && rightHandItem->getRules()->isSpecialUsingEmptyHand())
				{
					break;
				}
				rightHandItem = 0;
			}
		}
		bool middleClick = action->getDetails()->button.button == SDL_BUTTON_MIDDLE;
		handleItemClick(rightHandItem, middleClick);
	}
}

/**
 * Centers on the unit corresponding to this button.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnVisibleUnitClick(Action *action)
{
	int btnID = -1;

	// got to find out which button was pressed
	for (int i = 0; i < VISIBLE_MAX && btnID == -1; ++i)
	{
		if (action->getSender() == _btnVisibleUnit[i])
		{
			btnID = i;
		}
	}

	if (btnID != -1)
	{
		_map->getCamera()->centerOnPosition(_visibleUnit[btnID]->getPosition());
	}

	action->getDetails()->type = SDL_FIRSTEVENT; // consume the event
#ifdef __MOBILE__
	_longPressTimer->stop();
#endif
}

/**
 * Launches the blaster bomb.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnLaunchClick(Action *action)
{
	_battleGame->launchAction();
	action->getDetails()->type = SDL_FIRSTEVENT; // consume the event
#ifdef __MOBILE__
	// Seems we have an issue with a timer.
	_longPressTimer->stop();
#endif
}

/**
 * Uses psionics.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPsiClick(Action *action)
{
	_battleGame->psiButtonAction();
	action->getDetails()->type = SDL_FIRSTEVENT; // consume the event
#ifdef __MOBILE__
	_longPressTimer->stop();
#endif
}

/**
 * Shows action menu for special weapons.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnSpecialClick(Action *action)
{
	if (playableUnitSelected())
	{
		// concession for touch devices:
		// click on the item to cancel action, and don't pop up a menu to select a new one
		// TODO: wrap this in an IFDEF ?
		if (_battleGame->getCurrentAction()->targeting)
		{
			_battleGame->cancelCurrentAction();
			return;
		}

		_battleGame->cancelCurrentAction();

		BattleType type;
		BattleItem *specialItem = _save->getSelectedUnit()->getSpecialIconWeapon(type);
		if (!specialItem)
		{
			return;
		}

		_map->draw();
		bool middleClick = action->getDetails()->button.button == SDL_BUTTON_MIDDLE;
		handleItemClick(specialItem, middleClick);
	}
}

/**
 * Reserves time units.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReserveClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);

		if (_reserve == _btnReserveNone)
			_battleGame->setTUReserved(BA_NONE);
		else if (_reserve == _btnReserveSnap)
			_battleGame->setTUReserved(BA_SNAPSHOT);
		else if (_reserve == _btnReserveAimed)
			_battleGame->setTUReserved(BA_AIMEDSHOT);
		else if (_reserve == _btnReserveAuto)
			_battleGame->setTUReserved(BA_AUTOSHOT);

		// update any path preview
		if (_battleGame->getPathfinding()->isPathPreviewed())
		{
			_battleGame->getPathfinding()->removePreview();
			_battleGame->getPathfinding()->previewPath();
		}
	}
}

/**
 * Reloads the weapon in hand.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReloadClick(Action *)
{
	if (playableUnitSelected() && _save->getSelectedUnit()->reloadAmmo())
	{
		_game->getMod()->getSoundByDepth(_save->getDepth(), _save->getSelectedUnit()->getReloadSound())->play(-1, getMap()->getSoundAngle(_save->getSelectedUnit()->getPosition()));
		updateSoldierInfo();
	}
}

/**
 * Opens the jukebox.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnSelectMusicTrackClick(Action *)
{
	if (allowButtons())
	{
		_game->pushState(new SelectMusicTrackState(SMT_BATTLESCAPE));
	}
}

/**
 * Toggles soldier's personal lighting.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPersonalLightingClick(Action *)
{
	if (allowButtons())
		_save->getTileEngine()->togglePersonalLighting();
}

/**
 * Toggles night vision (purely cosmetic).
 * @param action Pointer to an action.
 */
void BattlescapeState::btnNightVisionClick(Action *action)
{
	if (allowButtons())
		_map->toggleNightVision();
}

/**
 * Determines whether a playable unit is selected. Normally only player side units can be selected, but in debug mode one can play with aliens too :)
 * Is used to see if stats can be displayed and action buttons will work.
 * @return Whether a playable unit is selected.
 */
bool BattlescapeState::playableUnitSelected()
{
	return _save->getSelectedUnit() != 0 && allowButtons();
}

/**
 * Draw hand item with ammo number.
 */
void BattlescapeState::drawItem(BattleItem* item, Surface* hand, std::vector<NumberText*> &ammoText, std::vector<NumberText*> &medikitText, NumberText *twoHandedText)
{
	hand->clear();
	for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
	{
		ammoText[slot]->setVisible(false);
	}
	for (int slot = 0; slot < RuleItem::MedikitSlots; ++slot)
	{
		medikitText[slot]->setVisible(false);
	}
	twoHandedText->setVisible(false);
	if (item)
	{
		const RuleItem *rule = item->getRules();
		rule->drawHandSprite(_game->getMod()->getSurfaceSet("BIGOBS.PCK"), hand, item, _save->getAnimFrame());
		for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
		{
			if (item->isAmmoVisibleForSlot(slot))
			{
				auto ammo = item->getAmmoForSlot(slot);
				if (!ammo)
				{
					ammoText[slot]->setVisible(true);
					ammoText[slot]->setValue(0);
				}
				else
				{
					ammoText[slot]->setVisible(true);
					ammoText[slot]->setValue(ammo->getAmmoQuantity());
				}
			}
		}
		if (Options::twoHandedIndicator)
		{
			twoHandedText->setVisible(rule->isTwoHanded());
			twoHandedText->setColor(rule->isBlockingBothHands() ? _twoHandedRed : _twoHandedGreen);
		}
		if (rule->getBattleType() == BT_MEDIKIT)
		{
			medikitText[0]->setVisible(true);
			medikitText[0]->setValue(item->getPainKillerQuantity());
			medikitText[1]->setVisible(true);
			medikitText[1]->setValue(item->getStimulantQuantity());
			medikitText[2]->setVisible(true);
			medikitText[2]->setValue(item->getHealQuantity());
		}

		// primed grenade indicator (static)
		/*
		if (item->getFuseTimer() >= 0)
		{
			Surface *tempSurface = _game->getMod()->getSurfaceSet("SCANG.DAT")->getFrame(6);
			tempSurface->setX((RuleInventory::HAND_W - rule->getInventoryWidth()) * RuleInventory::SLOT_W / 2);
			tempSurface->setY((RuleInventory::HAND_H - rule->getInventoryHeight()) * RuleInventory::SLOT_H / 2);
			tempSurface->blit(hand);
		}
		*/
		// primed grenade indicator (animated)
		if (item->getFuseTimer() >= 0)
		{
			const int Pulsate[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
			Surface *tempSurface = _game->getMod()->getSurfaceSet("SCANG.DAT")->getFrame(6);
			int x = (RuleInventory::HAND_W - rule->getInventoryWidth()) * RuleInventory::SLOT_W / 2;
			int y = (RuleInventory::HAND_H - rule->getInventoryHeight()) * RuleInventory::SLOT_H / 2;
			tempSurface->blitNShade(hand, hand->getX() + x, hand->getY() + y, Pulsate[_save->getAnimFrame() % 8], false, item->isFuseEnabled() ? 0 : 32);
		}
	}
}

/**
 * Draw both hands sprites.
 */
void BattlescapeState::drawHandsItems()
{
	BattleUnit *battleUnit = playableUnitSelected() ? _save->getSelectedUnit() : nullptr;
	drawItem(battleUnit ? battleUnit->getLeftHandWeapon() : nullptr, _btnLeftHandItem, _numAmmoLeft, _numMedikitLeft, _numTwoHandedIndicatorLeft);
	drawItem(battleUnit ? battleUnit->getRightHandWeapon() : nullptr, _btnRightHandItem, _numAmmoRight, _numMedikitRight, _numTwoHandedIndicatorRight);
}

/**
 * Updates a soldier's name/rank/tu/energy/health/morale.
 */
void BattlescapeState::updateSoldierInfo(bool checkFOV)
{
	BattleUnit *battleUnit = _save->getSelectedUnit();

	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[i]->setVisible(false);
		_numVisibleUnit[i]->setVisible(false);
		_visibleUnit[i] = 0;
	}

	bool playableUnit = playableUnitSelected();
	_rank->setVisible(playableUnit);
	_rankTiny->setVisible(playableUnit);
	_numTimeUnits->setVisible(playableUnit);
	_barTimeUnits->setVisible(playableUnit);
	_barTimeUnits->setVisible(playableUnit);
	_numEnergy->setVisible(playableUnit);
	_barEnergy->setVisible(playableUnit);
	_barEnergy->setVisible(playableUnit);
	_numHealth->setVisible(playableUnit);
	_barHealth->setVisible(playableUnit);
	_barHealth->setVisible(playableUnit);
	_numMorale->setVisible(playableUnit);
	_barMorale->setVisible(playableUnit);
	_barMorale->setVisible(playableUnit);
	_btnLeftHandItem->setVisible(playableUnit);
	_btnRightHandItem->setVisible(playableUnit);
	for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
	{
		_numAmmoLeft[slot]->setVisible(playableUnit);
		_numAmmoRight[slot]->setVisible(playableUnit);
	}
	if (!playableUnit)
	{
#ifdef __MOBILE__
		_leftWpnActive->setVisible(false);
		_rightWpnActive->setVisible(false);
#endif
		_txtName->setText(L"");
		showPsiButton(false);
		showSpecialButton(false);
		toggleKneelButton(0);
		return;
	}

	_txtName->setText(battleUnit->getName(_game->getLanguage(), false));
	Soldier *soldier = battleUnit->getGeoscapeSoldier();
	if (soldier != 0)
	{
		// presence of custom background determines what should happen
		Surface *customBg = _game->getMod()->getSurface("AvatarBackground", false);
		if (customBg == 0)
		{
			// show rank (vanilla behaviour)
			SurfaceSet *texture = _game->getMod()->getSurfaceSet("SMOKE.PCK");
			texture->getFrame(soldier->getRankSpriteBattlescape())->blit(_rank);
		}
		else
		{
			// show tiny rank (modded)
			SurfaceSet *texture = _game->getMod()->getSurfaceSet("TinyRanks");
			Surface *spr = texture->getFrame(soldier->getRankSpriteTiny());
			if (spr)
			{
				spr->blit(_rankTiny);
			}

			// use custom background (modded)
			customBg->blit(_rank);

			// show avatar
			std::string look = soldier->getArmor()->getSpriteInventory();
			if (!soldier->getRules()->getArmorForAvatar().empty())
			{
				look = _game->getMod()->getArmor(soldier->getRules()->getArmorForAvatar())->getSpriteInventory();
			}
			const std::string gender = soldier->getGender() == GENDER_MALE ? "M" : "F";
			std::stringstream ss;
			Surface *surf = 0;

			for (int i = 0; i <= 4; ++i)
			{
				ss.str("");
				ss << look;
				ss << gender;
				ss << (int)soldier->getLook() + (soldier->getLookVariant() & (15 >> i)) * 4;
				ss << ".SPK";
				surf = _game->getMod()->getSurface(ss.str(), false);
				if (surf)
				{
					break;
				}
			}
			if (!surf)
			{
				ss.str("");
				ss << look;
				ss << ".SPK";
				surf = _game->getMod()->getSurface(ss.str(), true);
			}

			// crop
			surf->getCrop()->x = soldier->getRules()->getAvatarOffsetX();
			surf->getCrop()->y = soldier->getRules()->getAvatarOffsetY();
			surf->getCrop()->w = 26;
			surf->getCrop()->h = 23;

			surf->blit(_rank);

			// reset crop
			surf->getCrop()->x = 0;
			surf->getCrop()->y = 0;
			surf->getCrop()->w = surf->getWidth();
			surf->getCrop()->h = surf->getHeight();
		}
	}
	else
	{
		_rank->clear();
		_rankTiny->clear();
	}
	_numTimeUnits->setValue(battleUnit->getTimeUnits());
	_barTimeUnits->setMax(battleUnit->getBaseStats()->tu);
	_barTimeUnits->setValue(battleUnit->getTimeUnits());
	_numEnergy->setValue(battleUnit->getEnergy());
	_barEnergy->setMax(battleUnit->getBaseStats()->stamina);
	_barEnergy->setValue(battleUnit->getEnergy());
	_numHealth->setValue(battleUnit->getHealth());
	_barHealth->setMax(battleUnit->getBaseStats()->health);
	_barHealth->setValue(battleUnit->getHealth());
	_barHealth->setValue2(battleUnit->getStunlevel());
	_numMorale->setValue(battleUnit->getMorale());
	_barMorale->setMax(100);
	_barMorale->setValue(battleUnit->getMorale());

	toggleKneelButton(battleUnit);

	drawHandsItems();

#ifdef __MOBILE__
	if (_battleGame->getCurrentAction()->targeting)
	{
		BattleItem *currentItem = _battleGame->getCurrentAction()->weapon;
		BattleItem *leftHandItem = battleUnit->getItem("STR_LEFT_HAND");
		BattleItem *rightHandItem = battleUnit->getItem("STR_RIGHT_HAND");
		if (currentItem == leftHandItem)
		{
			_leftWpnActive->setVisible(true);
		}
		else if (currentItem == rightHandItem)
		{
			_rightWpnActive->setVisible(true);
		}
	}
	else
	{
		_leftWpnActive->setVisible(false);
		_rightWpnActive->setVisible(false);
	}
#endif

	if (checkFOV)
	{
		_save->getTileEngine()->calculateFOV(_save->getSelectedUnit());
	}

	// go through all units visible to the selected soldier (or other unit, e.g. mind-controlled enemy)
	int j = 0;
	for (std::vector<BattleUnit*>::iterator i = battleUnit->getVisibleUnits()->begin(); i != battleUnit->getVisibleUnits()->end() && j < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[j]->setTooltip(_txtVisibleUnitTooltip[j]);
		_btnVisibleUnit[j]->setVisible(true);
		_numVisibleUnit[j]->setVisible(true);
		_visibleUnit[j] = (*i);
		++j;
	}

	// remember where red indicators turn green
	_numberOfDirectlyVisibleUnits = j;

	// go through all units on the map
	for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end() && j < VISIBLE_MAX; ++i)
	{
		// check if they are hostile and visible (by any friendly unit)
		if ((*i)->getOriginalFaction() == FACTION_HOSTILE && !(*i)->isOut() && (*i)->getVisible())
		{
			bool alreadyShown = false;
			// check if they are not already shown (e.g. because we see them directly)
			for (std::vector<BattleUnit*>::iterator k = battleUnit->getVisibleUnits()->begin(); k != battleUnit->getVisibleUnits()->end(); ++k)
			{
				if ((*i)->getId() == (*k)->getId())
				{
					alreadyShown = true;
				}
			}
			if (!alreadyShown)
			{
				_btnVisibleUnit[j]->setTooltip(_txtVisibleUnitTooltip[j]);
				_btnVisibleUnit[j]->setVisible(true);
				_numVisibleUnit[j]->setVisible(true);
				_visibleUnit[j] = (*i);
				++j;
			}
		}
	}

	// remember where green indicators turn blue
	_numberOfEnemiesTotal = j;

	if (Options::bleedingIndicator)
	{
		// go through all wounded units under player's control (incl. unconscious)
		for (std::vector<BattleUnit*>::iterator i = _battleGame->getSave()->getUnits()->begin(); i != _battleGame->getSave()->getUnits()->end() && j < VISIBLE_MAX; ++i)
		{
			if ((*i)->getFaction() == FACTION_PLAYER && (*i)->getStatus() != STATUS_DEAD && (*i)->getFatalWounds() > 0)
			{
				_btnVisibleUnit[j]->setTooltip(_txtVisibleUnitTooltip[VISIBLE_MAX]);
				_btnVisibleUnit[j]->setVisible(true);
				_numVisibleUnit[j]->setVisible(true);
				_visibleUnit[j] = (*i);
				++j;
			}
		}
	}

	// remember where blue indicators turn purple
	_numberOfEnemiesTotalPlusWounded = j;

	if (Options::knockOutIndicator)
	{
		// go through all units under player's control (excl. unconscious)
		for (std::vector<BattleUnit*>::iterator i = _battleGame->getSave()->getUnits()->begin(); i != _battleGame->getSave()->getUnits()->end() && j < VISIBLE_MAX; ++i)
		{
			if ((*i)->getFaction() == FACTION_PLAYER && !((*i)->isOut()) && (*i)->getHealth() > 0)
			{
				if ((*i)->getStunlevel() * 100 / (*i)->getHealth() >= 75)
				{
					_btnVisibleUnit[j]->setTooltip(_txtVisibleUnitTooltip[VISIBLE_MAX+1]);
					_btnVisibleUnit[j]->setVisible(true);
					_numVisibleUnit[j]->setVisible(true);
					_visibleUnit[j] = (*i);
					++j;
				}
			}
		}
	}

	showPsiButton(battleUnit->getSpecialWeapon(BT_PSIAMP) != 0);
	BattleType type = BT_NONE;
	BattleItem *specialWeapon = battleUnit->getSpecialIconWeapon(type); // updates type!
	if (specialWeapon && type != BT_NONE && type != BT_AMMO && type != BT_GRENADE && type != BT_PROXIMITYGRENADE && type != BT_FLARE && type != BT_CORPSE)
	{
		showPsiButton(false);
		showSpecialButton(true, specialWeapon->getRules()->getSpecialIconSprite());
	}
	else
	{
		showSpecialButton(false);
	}
}

/**
 * Shifts the red colors of the visible unit buttons backgrounds.
 */
void BattlescapeState::blinkVisibleUnitButtons()
{
	static int delta = 1, color = 32;

	for (int i = 0; i < VISIBLE_MAX;  ++i)
	{
		if (_btnVisibleUnit[i]->getVisible() == true)
		{
			_btnVisibleUnit[i]->drawRect(0, 0, 15, 12, 15);
			int bgColor = i < _numberOfDirectlyVisibleUnits ? color : i < _numberOfEnemiesTotal ? _indicatorGreen : i < _numberOfEnemiesTotalPlusWounded ? _indicatorBlue : _indicatorPurple;
			_btnVisibleUnit[i]->drawRect(1, 1, 13, 10, bgColor);
		}
	}

	if (color == 44) delta = -2;
	if (color == 32) delta = 1;

	color += delta;
}

/**
 * Shifts the colors of the health bar when unit has fatal wounds.
 */
void BattlescapeState::blinkHealthBar()
{
	static Uint8 color = 0, maxcolor = 3, step = 0;

	step = 1 - step;	// 1, 0, 1, 0, ...
	BattleUnit *bu = _save->getSelectedUnit();
	if (step == 0 || bu == 0 || !_barHealth->getVisible()) return;

	if (++color > maxcolor) color = maxcolor - 3;

	for (int i = 0; i < 6; i++)
	{
		if (bu->getFatalWound(i) > 0)
		{
			_barHealth->setColor(_barHealthColor + color);
			return;
		}
	}
	if (_barHealth->getColor() != _barHealthColor) // avoid redrawing if we don't have to
		_barHealth->setColor(_barHealthColor);
}

/**
 * Popups a context sensitive list of actions the user can choose from.
 * Some actions result in a change of gamestate.
 * @param item Item the user clicked on (righthand/lefthand)
 * @param middleClick was it a middle click?
 */
void BattlescapeState::handleItemClick(BattleItem *item, bool middleClick)
{
	// make sure there is an item, and the battlescape is in an idle state
	if (item && !_battleGame->isBusy())
	{
		if (middleClick)
		{
			std::string articleId = item->getRules()->getType();
			Ufopaedia::openArticle(_game, articleId);
		}
		else
		{
			_battleGame->getCurrentAction()->weapon = item;
#ifdef __MOBILE__
            BattleUnit *battleUnit = _save->getSelectedUnit();
            if (battleUnit)
            {
                BattleItem *leftHandItem = battleUnit->getItem("STR_LEFT_HAND");
                BattleItem *rightHandItem = battleUnit->getItem("STR_RIGHT_HAND");
                if (item == leftHandItem)
                {
                    _leftWpnActive->setVisible(true);
                }
                else
                {
                    _leftWpnActive->setVisible(false);
                }
                if (item == rightHandItem)
                {
                    _rightWpnActive->setVisible(true);
                }
                else
                {
                    _rightWpnActive->setVisible(false);
                }
            }
#endif
			popup(new ActionMenuState(_battleGame->getCurrentAction(), _icons->getX(), _icons->getY() + 16));
		}
	}
}

/**
 * Animates map objects on the map, also smoke,fire, ...
 */
void BattlescapeState::animate()
{
	_map->animate(!_battleGame->isBusy());

	blinkVisibleUnitButtons();
	blinkHealthBar();
	drawHandsItems();
}

/**
 * Handles the battle game state.
 */
void BattlescapeState::handleState()
{
	_battleGame->handleState();
}

/**
 * Sets the timer interval for think() calls of the state.
 * @param interval An interval in ms.
 */
void BattlescapeState::setStateInterval(Uint32 interval)
{
	_gameTimer->setInterval(interval);
}

/**
 * Gets pointer to the game. Some states need this info.
 * @return Pointer to game.
 */
Game *BattlescapeState::getGame() const
{
	return _game;
}

/**
 * Gets pointer to the map. Some states need this info.
 * @return Pointer to map.
 */
Map *BattlescapeState::getMap() const
{
	return _map;
}

/**
 * Shows a debug message in the topleft corner.
 * @param message Debug message.
 */
void BattlescapeState::debug(const std::wstring &message)
{
	if (_save->getDebugMode())
	{
		_txtDebug->setText(message);
	}
}

/**
* Shows a bug hunt message in the topleft corner.
*/
void BattlescapeState::bugHuntMessage()
{
	if (_save->getBughuntMode())
	{
		_txtDebug->setText(tr("STR_BUG_HUNT_ACTIVATED"));
	}
}

/**
 * Shows a warning message.
 * @param message Warning message.
 */
void BattlescapeState::warning(const std::string &message)
{
	_warning->showMessage(tr(message));
}

/**
 * Gets melee damage preview.
 * @param actor Selected unit.
 * @param weapon Weapon to use for calculation.
 */
std::wstring BattlescapeState::getMeleeDamagePreview(BattleUnit *actor, BattleItem *weapon) const
{
	if (!weapon)
		return L"";

	bool discovered = false;
	if (_game->getSavedGame()->getMonthsPassed() == -1)
	{
		discovered = true; // new battle mode
	}
	else
	{
		ArticleDefinition *article = _game->getMod()->getUfopaediaArticle(weapon->getRules()->getType(), false);
		if (article && Ufopaedia::isArticleAvailable(_game->getSavedGame(), article))
		{
			discovered = true; // pedia article unlocked
		}
	}

	std::wostringstream ss;
	if (discovered)
	{
		int totalDamage = 0;
		const RuleDamageType *dmgType;
		if (weapon->getRules()->getBattleType() == BT_MELEE)
		{
			totalDamage += weapon->getRules()->getPowerBonus(actor);
			dmgType = weapon->getRules()->getDamageType();
		}
		else
		{
			totalDamage += weapon->getRules()->getMeleeBonus(actor);
			dmgType = weapon->getRules()->getMeleeType();
		}

		ss << tr(weapon->getRules()->getType());
		ss << L"\n";
		ss << dmgType->getRandomDamage(totalDamage, 1);
		ss << L"-";
		ss << dmgType->getRandomDamage(totalDamage, 2);
		if (dmgType->RandomType == DRT_UFO_WITH_TWO_DICE)
			ss << L"*";
	}
	else
	{
		ss << tr(weapon->getRules()->getType());
		ss << L"\n?-?";
	}

	return ss.str();
}

/**
 * Takes care of any events from the core game engine.
 * @param action Pointer to an action.
 */
inline void BattlescapeState::handle(Action *action)
{
	if (!_firstInit)
	{
		if (_game->getCursor()->getVisible() || ((action->getDetails()->type == SDL_MOUSEBUTTONDOWN || action->getDetails()->type == SDL_MOUSEBUTTONUP) && action->getDetails()->button.button == SDL_BUTTON_RIGHT))
		{
			State::handle(action);

			if (Options::touchEnabled == false && _isMouseScrolling && !Options::battleDragScrollInvert)
			{
				_map->setSelectorPosition((_cursorPosition.x - _game->getScreen()->getCursorLeftBlackBand()) / action->getXScale(), (_cursorPosition.y - _game->getScreen()->getCursorTopBlackBand()) / action->getYScale());
			}

			if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
			{
				if (action->getDetails()->button.button == SDL_BUTTON_X1)
				{
					btnNextSoldierClick(action);
				}
				else if (action->getDetails()->button.button == SDL_BUTTON_X2)
				{
					btnPrevSoldierClick(action);
				}
			}

			if (action->getDetails()->type == SDL_KEYDOWN)
			{
				SDL_Keycode key = action->getDetails()->key.keysym.sym;
				bool ctrlPressed = (SDL_GetModState() & KMOD_CTRL) != 0;
				bool shiftPressed = (SDL_GetModState() & KMOD_SHIFT) != 0;

				// "ctrl-b" - reopen briefing
				if (key == SDLK_b && ctrlPressed)
				{
					_game->pushState(new BriefingState(0, 0, true));
				}
				// "ctrl-h" - show hit log
				else if (key == SDLK_h && ctrlPressed)
				{
					_game->pushState(new InfoboxState(_save->hitLog.str()));
				}
				// "ctrl-Home" - reset default palettes
				else if (key == SDLK_HOME && ctrlPressed)
				{
					_paletteResetRequested = true;
					init();
				}
				// "ctrl-End" - toggle max brightness
				else if (key == SDLK_END && ctrlPressed)
				{
					_map->toggleMaxBrightnessVision();
				}
				// "ctrl-e" - experience log
				else if (key == SDLK_e && ctrlPressed)
				{
					std::wostringstream ss;
					ss << tr("STR_NO_EXPERIENCE_YET");
					ss << L"\n\n";
					bool first = true;
					for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
					{
						if ((*i)->getOriginalFaction() == FACTION_PLAYER && !(*i)->isOut())
						{
							if ((*i)->getGeoscapeSoldier() && !(*i)->hasGainedAnyExperience())
							{
								if (!first) ss << L", ";
								ss << (*i)->getName(_game->getLanguage());
								first = false;
							}
						}
					}
					_game->pushState(new InfoboxState(ss.str()));
				}
				// "ctrl-m" - melee damage preview
				else if (key == SDLK_m && ctrlPressed)
				{
					BattleUnit *actor = _save->getSelectedUnit();
					if (actor)
					{
						BattleItem *leftWeapon = actor->getLeftHandWeapon();

						BattleItem *rightWeapon = actor->getRightHandWeapon();

						BattleItem *specialWeapon = 0;
						auto typesToCheck = {BT_MELEE, BT_PSIAMP, BT_FIREARM, BT_MEDIKIT, BT_SCANNER, BT_MINDPROBE};
						for (auto &type : typesToCheck)
						{
							specialWeapon = actor->getSpecialWeapon(type);
							if (specialWeapon && specialWeapon->getRules()->isSpecialUsingEmptyHand())
							{
								break;
							}
							specialWeapon = 0;
						}

						BattleType type;
						BattleItem *anotherSpecialWeapon = actor->getSpecialIconWeapon(type);
						if (anotherSpecialWeapon && anotherSpecialWeapon == specialWeapon)
						{
							anotherSpecialWeapon = 0;
						}

						std::wostringstream ss;
						bool first = true;
						if (leftWeapon)
						{
							ss << getMeleeDamagePreview(actor, leftWeapon);
							first = false;
						}
						if (rightWeapon)
						{
							if (!first) ss << L"\n\n";
							ss << getMeleeDamagePreview(actor, rightWeapon);
							first = false;
						}
						if (specialWeapon)
						{
							if (!first) ss << L"\n\n";
							ss << getMeleeDamagePreview(actor, specialWeapon);
							first = false;
						}
						if (anotherSpecialWeapon)
						{
							if (!first) ss << L"\n\n";
							ss << getMeleeDamagePreview(actor, anotherSpecialWeapon);
							first = false;
						}

						_game->pushState(new InfoboxState(ss.str()));
					}
				}
				if (Options::debug)
				{
					// "ctrl-d" - enable debug mode
					if (key == SDLK_d && ctrlPressed)
					{
						_save->setDebugMode();
						debug(L"Debug Mode");
					}
					// "ctrl-v" - reset tile visibility
					else if (_save->getDebugMode() && key == SDLK_v && ctrlPressed)
					{
						debug(L"Resetting tile visibility");
						_save->resetTiles();
					}
					else if (_save->getDebugMode() && (key == SDLK_k || key == SDLK_j) && ctrlPressed)
					{
						bool stunOnly = (key == SDLK_j);
						if (shiftPressed)
						{
							// kill (ctrl-shift-k) or stun (ctrl-shift-j) just a single unit (under the cursor)
							Position newPos;
							_map->getSelectorPosition(&newPos);
							Tile *tile = _save->getTile(newPos);
							if (tile)
							{
								BattleUnit *unit = tile->getUnit();
								if (unit && !unit->isOut())
								{
									debug(L"Bingo!");
									unit->damage(Position(0, 0, 0), 1000, _game->getMod()->getDamageType(stunOnly ? DT_STUN : DT_AP), _save, {});
								}
							}
						}
						else
						{
							if (stunOnly)
							{
								// "ctrl-j" - stun all aliens
								debug(L"Deploying Celine Dion album");
							}
							else
							{
								// "ctrl-k" - kill all aliens
								debug(L"Influenza bacterium dispersed");
							}
							for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
							{
								if ((*i)->getOriginalFaction() == FACTION_HOSTILE && !(*i)->isOut())
								{
									(*i)->damage(Position(0, 0, 0), 1000, _game->getMod()->getDamageType(stunOnly ? DT_STUN : DT_AP), _save, { });
								}
							}
						}
						_save->getBattleGame()->checkForCasualties(nullptr, BattleActionAttack{}, true, false);
						_save->getBattleGame()->handleState();
					}
					// "ctrl-w" - warp unit
					else if (_save->getDebugMode() && key == SDLK_w && ctrlPressed)
					{
						BattleUnit *unit = _save->getSelectedUnit();
						if (unit && unit->getArmor()->getSize() < 2)
						{
							Position newPos;
							_map->getSelectorPosition(&newPos);
							Tile *tile = _save->getTile(newPos);
							if (tile)
							{
								debug(L"Beam me up Scotty");
								_save->getPathfinding()->removePreview();

								unit->getTile()->setUnit(0);
								unit->setPosition(newPos);
								tile->setUnit(unit);
								_save->getTileEngine()->calculateLighting(LL_UNITS);
								_save->getBattleGame()->handleState();
							}
						}
					}
					// f11 - voxel map dump
					else if (key == SDLK_F11)
					{
						saveVoxelMap();
					}
					// f9 - ai
					else if (key == SDLK_F9 && Options::traceAI)
					{
						saveAIMap();
					}
				}
				// quick save and quick load
				if (!_game->getSavedGame()->isIronman())
				{
					if (key == Options::keyQuickSave)
					{
						_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_QUICK, _palette));
					}
					else if (key == Options::keyQuickLoad)
					{
						_game->pushState(new LoadGameState(OPT_BATTLESCAPE, SAVE_QUICK, _palette));
					}
				}

				// voxel view dump
				if (key == Options::keyBattleVoxelView)
				{
					saveVoxelView();
				}
			}
		}
	}
}

/**
 * Saves a map as used by the AI.
 */
void BattlescapeState::saveAIMap()
{
	Uint32 start = SDL_GetTicks();
	BattleUnit *unit = _save->getSelectedUnit();
	if (!unit) return;

	int w = _save->getMapSizeX();
	int h = _save->getMapSizeY();

	int expMax = 0;

	SDL_Surface *img = SDL_CreateRGBSurface(0, w * 8, h * 8, 24, 0xff, 0xff00, 0xff0000, 0);
	Log(LOG_INFO) << "unit = " << unit->getId();
	memset(img->pixels, 0, img->pitch * img->h);

	Position tilePos(unit->getPosition());
	SDL_Rect r;
	r.h = 8;
	r.w = 8;

	for (int y = 0; y < h; ++y)
	{
		tilePos.y = y;
		for (int x = 0; x < w; ++x)
		{
			tilePos.x = x;
			Tile *t = _save->getTile(tilePos);

			if (!t) continue;
			if (!t->isDiscovered(2)) continue;

		}
	}

	for (int y = 0; y < h; ++y)
	{
		tilePos.y = y;
		for (int x = 0; x < w; ++x)
		{
			tilePos.x = x;
			Tile *t = _save->getTile(tilePos);

			if (!t) continue;
			if (!t->isDiscovered(2)) continue;

			r.x = x * r.w;
			r.y = y * r.h;

			if (t->getTUCost(O_FLOOR, MT_FLY) != 255 && t->getTUCost(O_OBJECT, MT_FLY) != 255)
			{
				SDL_FillRect(img, &r, SDL_MapRGB(img->format, 255, 0, 0x20));
				characterRGBA(img, r.x, r.y,'*' , 0x7f, 0x7f, 0x7f, 0x7f);
			} else
			{
				if (!t->getUnit()) SDL_FillRect(img, &r, SDL_MapRGB(img->format, 0x50, 0x50, 0x50)); // gray for blocked tile
			}

			for (int z = tilePos.z; z >= 0; --z)
			{
				Position pos(tilePos.x, tilePos.y, z);
				t = _save->getTile(pos);
				BattleUnit *wat = t->getUnit();
				if (wat)
				{
					switch(wat->getFaction())
					{
					case FACTION_HOSTILE:
						// #4080C0 is Volutar Blue
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'a' : 'A', 0x40, 0x80, 0xC0, 0xff);
						break;
					case FACTION_PLAYER:
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'x' : 'X', 255, 255, 127, 0xff);
						break;
					case FACTION_NEUTRAL:
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'c' : 'C', 255, 127, 127, 0xff);
						break;
					}
					break;
				}
				pos.z--;
				if (z > 0 && !t->hasNoFloor(_save->getTile(pos))) break; // no seeing through floors
			}

			if (t->getMapData(O_NORTHWALL) && t->getMapData(O_NORTHWALL)->getTUCost(MT_FLY) == 255)
			{
				lineRGBA(img, r.x, r.y, r.x+r.w, r.y, 0x50, 0x50, 0x50, 255);
			}

			if (t->getMapData(O_WESTWALL) && t->getMapData(O_WESTWALL)->getTUCost(MT_FLY) == 255)
			{
				lineRGBA(img, r.x, r.y, r.x, r.y+r.h, 0x50, 0x50, 0x50, 255);
			}
		}
	}

	std::ostringstream ss;

	ss.str("");
	ss << "z = " << tilePos.z;
	stringRGBA(img, 12, 12, ss.str().c_str(), 0, 0, 0, 0x7f);

	int i = 0;
	do
	{
		ss.str("");
		ss << Options::getMasterUserFolder() << "AIExposure" << std::setfill('0') << std::setw(3) << i << ".png";
		i++;
	}
	while (CrossPlatform::fileExists(ss.str()));


	unsigned error = lodepng::encode(ss.str(), (const unsigned char*)img->pixels, img->w, img->h, LCT_RGB);
	if (error)
	{
		Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
	}

	SDL_FreeSurface(img);

	Log(LOG_INFO) << "saveAIMap() completed in " << SDL_GetTicks() - start << "ms.";
}

/**
 * Saves a first-person voxel view of the battlescape.
 */
void BattlescapeState::saveVoxelView()
{
	static const unsigned char pal[30]=
	//			ground		west wall	north wall		object		enem unit						xcom unit	neutr unit
	{0,0,0, 224,224,224,  192,224,255,  255,224,192, 128,255,128, 192,0,255,  0,0,0, 255,255,255,  224,192,0,  255,64,128 };

	BattleUnit * bu = _save->getSelectedUnit();
	if (bu==0) return; //no unit selected
	std::vector<Position> _trajectory;

	double ang_x,ang_y;
	bool black;
	Tile *tile = 0;
	std::ostringstream ss;
	std::vector<unsigned char> image;
	int test;
	Position originVoxel = getBattleGame()->getTileEngine()->getSightOriginVoxel(bu);

	Position targetVoxel,hitPos;
	double dist = 0;
	bool _debug = _save->getDebugMode();
	double dir = ((double)bu->getDirection()+4)/4*M_PI;
	image.clear();
	for (int y = -256+32; y < 256+32; ++y)
	{
		ang_y = (((double)y)/640*M_PI+M_PI/2);
		for (int x = -256; x < 256; ++x)
		{
			ang_x = ((double)x/1024)*M_PI+dir;

			targetVoxel.x=originVoxel.x + (int)(-sin(ang_x)*1024*sin(ang_y));
			targetVoxel.y=originVoxel.y + (int)(cos(ang_x)*1024*sin(ang_y));
			targetVoxel.z=originVoxel.z + (int)(cos(ang_y)*1024);

			_trajectory.clear();
			test = _save->getTileEngine()->calculateLine(originVoxel, targetVoxel, false, &_trajectory, bu, true, !_debug) +1;
			black = true;
			if (test!=0 && test!=6)
			{
				tile = _save->getTile(Position(_trajectory.at(0).x/16, _trajectory.at(0).y/16, _trajectory.at(0).z/24));
				if (_debug
					|| (tile->isDiscovered(0) && test == 2)
					|| (tile->isDiscovered(1) && test == 3)
					|| (tile->isDiscovered(2) && (test == 1 || test == 4))
					|| test==5
					)
				{
					if (test==5)
					{
						if (tile->getUnit())
						{
							if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
							else
							if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
						}
						else
						{
							tile = _save->getTile(Position(_trajectory.at(0).x/16, _trajectory.at(0).y/16, _trajectory.at(0).z/24-1));
							if (tile && tile->getUnit())
							{
								if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
								else
								if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
							}
						}
					}
					hitPos = Position(_trajectory.at(0).x, _trajectory.at(0).y, _trajectory.at(0).z);
					dist = sqrt((double)((hitPos.x-originVoxel.x)*(hitPos.x-originVoxel.x)
						+ (hitPos.y-originVoxel.y)*(hitPos.y-originVoxel.y)
						+ (hitPos.z-originVoxel.z)*(hitPos.z-originVoxel.z)) );
					black = false;
				}
			}

			if (black)
			{
				dist = 0;
			}
			else
			{
				if (dist>1000) dist=1000;
				if (dist<1) dist=1;
				dist=(1000-(log(dist))*140)/700;//140

				if (hitPos.x%16==15)
				{
					dist*=0.9;
				}
				if (hitPos.y%16==15)
				{
					dist*=0.9;
				}
				if (hitPos.z%24==23)
				{
					dist*=0.9;
				}
				if (dist > 1) dist = 1;
				if (tile) dist *= (16 - (double)tile->getShade())/16;
			}

			image.push_back((int)((double)(pal[test*3+0])*dist));
			image.push_back((int)((double)(pal[test*3+1])*dist));
			image.push_back((int)((double)(pal[test*3+2])*dist));
		}
	}

	int i = 0;
	do
	{
		ss.str("");
		ss << Options::getMasterUserFolder() << "fpslook" << std::setfill('0') << std::setw(3) << i << ".png";
		i++;
	}
	while (CrossPlatform::fileExists(ss.str()));


	unsigned error = lodepng::encode(ss.str(), image, 512, 512, LCT_RGB);
	if (error)
	{
		Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
	}

	return;
}

/**
 * Saves each layer of voxels on the bettlescape as a png.
 */
void BattlescapeState::saveVoxelMap()
{
	std::ostringstream ss;
	std::vector<unsigned char> image;
	static const unsigned char pal[30]=
	{255,255,255, 224,224,224,  128,160,255,  255,160,128, 128,255,128, 192,0,255,  255,255,255, 255,255,255,  224,192,0,  255,64,128 };

	Tile *tile;

	for (int z = 0; z < _save->getMapSizeZ()*12; ++z)
	{
		image.clear();

		for (int y = 0; y < _save->getMapSizeY()*16; ++y)
		{
			for (int x = 0; x < _save->getMapSizeX()*16; ++x)
			{
				int test = _save->getTileEngine()->voxelCheck(Position(x,y,z*2),0,0) +1;
				float dist=1;
				if (x%16==15)
				{
					dist*=0.9f;
				}
				if (y%16==15)
				{
					dist*=0.9f;
				}

				if (test == V_OUTOFBOUNDS)
				{
					tile = _save->getTile(Position(x/16, y/16, z/12));
					if (tile->getUnit())
					{
						if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
						else
						if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
					}
					else
					{
						tile = _save->getTile(Position(x/16, y/16, z/12-1));
						if (tile && tile->getUnit())
						{
							if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
							else
							if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
						}
					}
				}

				image.push_back((int)((float)pal[test*3+0]*dist));
				image.push_back((int)((float)pal[test*3+1]*dist));
				image.push_back((int)((float)pal[test*3+2]*dist));
			}
		}

		ss.str("");
		ss << Options::getMasterUserFolder() << "voxel" << std::setfill('0') << std::setw(2) << z << ".png";

		unsigned error = lodepng::encode(ss.str(), image, _save->getMapSizeX()*16, _save->getMapSizeY()*16, LCT_RGB);
		if (error)
		{
			Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
		}
	}
	return;
}

/**
 * Adds a new popup window to the queue
 * (this prevents popups from overlapping).
 * @param state Pointer to popup state.
 */
void BattlescapeState::popup(State *state)
{
	_popups.push_back(state);
}

/**
 * Finishes up the current battle, shuts down the battlescape
 * and presents the debriefing screen for the mission.
 * @param abort Was the mission aborted?
 * @param inExitArea Number of soldiers in the exit area OR number of survivors when battle finished due to either all aliens or objective being destroyed.
 */
void BattlescapeState::finishBattle(bool abort, int inExitArea)
{
	while (!_game->isState(this))
	{
		_game->popState();
	}
	_game->getCursor()->setVisible(true);
	if (_save->getAmbientSound() != -1)
	{
		_game->getMod()->getSoundByDepth(0, _save->getAmbientSound())->stopLoop();
	}

	_battleGame->removeSummonedPlayerUnits();

	AlienDeployment *ruleDeploy = _game->getMod()->getDeployment(_save->getMissionType());
	if (!ruleDeploy)
	{
		for (std::vector<Ufo*>::iterator ufo =_game->getSavedGame()->getUfos()->begin(); ufo != _game->getSavedGame()->getUfos()->end(); ++ufo)
		{
			if ((*ufo)->isInBattlescape())
			{
				ruleDeploy = _game->getMod()->getDeployment((*ufo)->getRules()->getType());
				break;
			}
		}
	}
	std::string nextStage;
	if (ruleDeploy)
	{
		nextStage = ruleDeploy->getNextStage();
	}

	if (!nextStage.empty() && inExitArea)
	{
		// if there is a next mission stage + we have people in exit area OR we killed all aliens, load the next stage
		_popups.clear();
		_save->setMissionType(nextStage);
		BattlescapeGenerator bgen = BattlescapeGenerator(_game);
		bgen.nextStage();
		_game->popState();
		_game->pushState(new BriefingState(0, 0));
	}
	else
	{
		_popups.clear();
		_animTimer->stop();
		_gameTimer->stop();
		_game->popState();
		_game->pushState(new DebriefingState);
		std::string cutscene;
		if (ruleDeploy)
		{
			if (abort)
			{
				cutscene = ruleDeploy->getAbortCutscene();
			}
			else if (inExitArea == 0)
			{
				cutscene = ruleDeploy->getLoseCutscene();
			}
			else
			{
				cutscene = ruleDeploy->getWinCutscene();
			}
		}
		if (!cutscene.empty())
		{
			// if cutscene is "wingame" or "losegame", then the DebriefingState
			// pushed above will get popped without being shown.  otherwise
			// it will get shown after the cutscene.
			_game->pushState(new CutsceneState(cutscene));

			if (cutscene == CutsceneState::WIN_GAME)
			{
				_game->getSavedGame()->setEnding(END_WIN);
			}
			else if (cutscene == CutsceneState::LOSE_GAME)
			{
				_game->getSavedGame()->setEnding(END_LOSE);				
			}
			// Autosave if game is over
			if (_game->getSavedGame()->getEnding() != END_NONE && _game->getSavedGame()->isIronman())
			{
				_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_IRONMAN, _palette));
			}
		}
	}
}

/**
 * Shows the launch button.
 * @param show Show launch button?
 */
void BattlescapeState::showLaunchButton(bool show)
{
	_btnLaunch->setVisible(show);
}

/**
 * Shows the PSI button.
 * @param show Show PSI button?
 */
void BattlescapeState::showPsiButton(bool show)
{
	_btnPsi->setVisible(show);
}

/**
 * Shows the special button.
 * @param show Show special button?
 */
void BattlescapeState::showSpecialButton(bool show, int sprite)
{
	if (show)
	{
		_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(sprite)->blit(_btnSpecial);
	}
	_btnSpecial->setVisible(show);
}

/**
 * Clears mouse-scrolling state (isMouseScrolling).
 */
void BattlescapeState::clearMouseScrollingState()
{
	_isMouseScrolling = false;
}

/**
 * Returns a pointer to the battlegame, in case we need its functions.
 */
BattlescapeGame *BattlescapeState::getBattleGame()
{
	return _battleGame;
}

/**
 * Handler for the mouse moving over the icons, disabling the tile selection cube.
 * @param action Pointer to an action.
 */
void BattlescapeState::mouseInIcons(Action *)
{
	_mouseOverIcons = true;
}

/**
 * Handler for the mouse going out of the icons, enabling the tile selection cube.
 * @param action Pointer to an action.
 */
void BattlescapeState::mouseOutIcons(Action *)
{
	_mouseOverIcons = false;
}

/**
 * Checks if the mouse is over the icons.
 * @return True, if the mouse is over the icons.
 */
bool BattlescapeState::getMouseOverIcons() const
{
	return _mouseOverIcons;
}

/**
 * Determines whether the player is allowed to press buttons.
 * Buttons are disabled in the middle of a shot, during the alien turn,
 * and while a player's units are panicking.
 * The save button is an exception as we want to still be able to save if something
 * goes wrong during the alien turn, and submit the save file for dissection.
 * @param allowSaving True, if the help button was clicked.
 * @return True if the player can still press buttons.
 */
bool BattlescapeState::allowButtons(bool allowSaving) const
{
	return ((allowSaving || _save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		&& (_battleGame->getPanicHandled() || _firstInit )
		&& (_map->getProjectile() == 0));
}

/**
 * Reserves time units for kneeling.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReserveKneelClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);
		_battleGame->setKneelReserved(!_battleGame->getKneelReserved());

		_btnReserveKneel->toggle(_battleGame->getKneelReserved());

		// update any path preview
		if (_battleGame->getPathfinding()->isPathPreviewed())
		{
			_battleGame->getPathfinding()->removePreview();
			_battleGame->getPathfinding()->previewPath();
		}
	}
}

/**
 * Removes all time units.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnZeroTUsClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);
		if (_battleGame->getSave()->getSelectedUnit())
		{
			_battleGame->getSave()->getSelectedUnit()->setTimeUnits(0);
			updateSoldierInfo();
		}
	}
}

#ifdef __MOBILE__
void BattlescapeState::btnZeroTUsPress(Action *action)
{
	_longPressTimer->onTimer((StateHandler)&BattlescapeState::btnZeroTUsLongPress);
	_longPressTimer->start();
}

void BattlescapeState::btnZeroTUsRelease(Action *action)
{
	_longPressTimer->stop();
}

void BattlescapeState::btnZeroTUsLongPress()
{
	_longPressTimer->stop();
	SDL_Event ev;
	ev.type = SDL_MOUSEBUTTONDOWN;
	ev.button.button = SDL_BUTTON_LEFT;
	Action a = Action(&ev, 0.0, 0.0, 0, 0);
	a.setSender(_btnZeroTUs);
	btnZeroTUsClick(&a);
}
#endif

/**
* Shows a tooltip with extra information (used for medikit-type equipment).
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipInExtra(Action *action, bool leftHand, bool special)
{
	if (allowButtons() && Options::battleTooltips)
	{
		// no one selected... do normal tooltip
		if(!playableUnitSelected())
		{
			_currentTooltip = action->getSender()->getTooltip();
			_txtTooltip->setText(tr(_currentTooltip));
			return;
		}

		BattleUnit *selectedUnit = _save->getSelectedUnit();
		BattleItem *weapon;
		if (leftHand)
		{
			weapon = selectedUnit->getItem("STR_LEFT_HAND");
		}
		else if (special)
		{
			BattleType type;
			weapon = selectedUnit->getSpecialIconWeapon(type);
		}
		else
		{
			weapon = selectedUnit->getItem("STR_RIGHT_HAND");
		}

		// no weapon selected... do normal tooltip
		if(!weapon)
		{
			_currentTooltip = action->getSender()->getTooltip();
			_txtTooltip->setText(tr(_currentTooltip));
			return;
		}

		// find the target unit
		if (weapon->getRules()->getBattleType() == BT_MEDIKIT)
		{
			BattleUnit *targetUnit = 0;
			TileEngine *tileEngine = _game->getSavedGame()->getSavedBattle()->getTileEngine();
			const std::vector<BattleUnit*> *units = _game->getSavedGame()->getSavedBattle()->getUnits();

			// search for target on the ground
			bool onGround = false;
			for (std::vector<BattleUnit*>::const_iterator i = units->begin(); i != units->end() && !targetUnit; ++i)
			{
				// we can heal a unit that is at the same position, unconscious and healable(=woundable)
				if ((*i)->getPosition() == selectedUnit->getPosition() && *i != selectedUnit && (*i)->getStatus() == STATUS_UNCONSCIOUS && (*i)->isWoundable())
				{
					targetUnit = *i;
					onGround = true;
				}
			}

			// search for target in front of the selected unit
			if (!targetUnit)
			{
				Position dest;
				if (tileEngine->validMeleeRange(
					selectedUnit->getPosition(),
					selectedUnit->getDirection(),
					selectedUnit,
					0, &dest, false))
				{
					Tile *tile = _game->getSavedGame()->getSavedBattle()->getTile(dest);
					if (tile != 0 && tile->getUnit() && tile->getUnit()->isWoundable())
					{
						targetUnit = tile->getUnit();
					}
				}
			}

			_currentTooltip = action->getSender()->getTooltip();
			std::wostringstream tooltipExtra;
			tooltipExtra << tr(_currentTooltip);

			// target unit found
			if (targetUnit)
			{
				if (targetUnit->getOriginalFaction() == FACTION_HOSTILE) {
					_txtTooltip->setColor(Palette::blockOffset(_medikitRed));
					tooltipExtra << tr("STR_TARGET_ENEMY");
				} else if (targetUnit->getOriginalFaction() == FACTION_NEUTRAL) {
					_txtTooltip->setColor(Palette::blockOffset(_medikitOrange));
					tooltipExtra << tr("STR_TARGET_NEUTRAL");
				} else if (targetUnit->getOriginalFaction() == FACTION_PLAYER) {
					_txtTooltip->setColor(Palette::blockOffset(_medikitGreen));
					tooltipExtra << tr("STR_TARGET_FRIEND");
				}
				if (onGround) tooltipExtra << tr("STR_TARGET_ON_THE_GROUND");
				_txtTooltip->setText(tooltipExtra.str());
			}
			else
			{
				// target unit not found => selected unit is the target (if self-heal is possible)
				if (weapon->getRules()->getAllowSelfHeal())
				{
					targetUnit = selectedUnit;
					_txtTooltip->setColor(Palette::blockOffset(_medikitBlue));
					tooltipExtra << tr("STR_TARGET_YOURSELF");
					if (onGround) tooltipExtra << tr("STR_TARGET_ON_THE_GROUND");
					_txtTooltip->setText(tooltipExtra.str());
				}
				else
				{
					// cannot use the weapon (medi-kit) on anyone
					_currentTooltip = action->getSender()->getTooltip();
					_txtTooltip->setText(tr(_currentTooltip));
				}
			}
		}
		else 
		{
			// weapon is not of medi-kit battle type
			_currentTooltip = action->getSender()->getTooltip();
			_txtTooltip->setText(tr(_currentTooltip));
		}
	}
}

/**
* Shows a tooltip with extra information (used for medikit-type equipment).
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipInExtraLeftHand(Action *action)
{
	txtTooltipInExtra(action, true);
}

/**
* Shows a tooltip with extra information (used for medikit-type equipment).
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipInExtraRightHand(Action *action)
{
	txtTooltipInExtra(action, false);
}

/**
* Shows a tooltip with extra information (used for medikit-type equipment).
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipInExtraSpecial(Action *action)
{
	txtTooltipInExtra(action, false, true);
}

/**
* Shows a tooltip for the End Turn button.
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipInEndTurn(Action *action)
{
	if (allowButtons() && Options::battleTooltips)
	{
		_currentTooltip = action->getSender()->getTooltip();

		std::wostringstream ss;
		ss << tr(_currentTooltip);
		ss << L" ";
		ss << _save->getTurn();
		_txtTooltip->setText(ss.str().c_str());
	}
}

/**
* Shows a tooltip for the appropriate button.
* @param action Pointer to an action.
*/
void BattlescapeState::txtTooltipIn(Action *action)
{
	if (allowButtons() && Options::battleTooltips)
	{
		_currentTooltip = action->getSender()->getTooltip();
		_txtTooltip->setText(tr(_currentTooltip));
	}
}

/**
 * Clears the tooltip text.
 * @param action Pointer to an action.
 */
void BattlescapeState::txtTooltipOut(Action *action)
{
	// reset color
	_txtTooltip->setColor(_tooltipDefaultColor);

	if (allowButtons() && Options::battleTooltips)
	{
		if (_currentTooltip == action->getSender()->getTooltip())
		{
			_txtTooltip->setText(L"");
		}
	}
}

/**
 * Updates the scale.
 * @param dX delta of X;
 * @param dY delta of Y;
 */
void BattlescapeState::resize(int &dX, int &dY)
{
	dX = Options::baseXResolution;
	dY = Options::baseYResolution;
	int divisor = 1;
	double pixelRatioY = 1.0;

	if (Options::nonSquarePixelRatio)
	{
		pixelRatioY = 1.2;
	}
	switch (Options::battlescapeScale)
	{
	case SCALE_SCREEN_DIV_3:
		divisor = 3;
		break;
	case SCALE_SCREEN_DIV_2:
		divisor = 2;
		break;
	case SCALE_SCREEN:
		break;
	default:
		dX = 0;
		dY = 0;
		return;
	}

	Options::baseXResolution = std::max(Screen::ORIGINAL_WIDTH, Options::displayWidth / divisor);
	Options::baseYResolution = std::max(Screen::ORIGINAL_HEIGHT, (int)(Options::displayHeight / pixelRatioY / divisor));

	dX = Options::baseXResolution - dX;
	dY = Options::baseYResolution - dY;
	_map->setWidth(Options::baseXResolution);
	_map->setHeight(Options::baseYResolution);
	_map->getCamera()->resize();
	_map->getCamera()->jumpXY(dX/2, dY/2);

	for (std::vector<Surface*>::const_iterator i = _surfaces.begin(); i != _surfaces.end(); ++i)
	{
		if (*i != _map && (*i) != _btnPsi && *i != _btnLaunch && *i != _txtDebug)
		{
			(*i)->setX((*i)->getX() + dX / 2);
			(*i)->setY((*i)->getY() + dY);
		}
		else if (*i != _map && *i != _txtDebug)
		{
			(*i)->setX((*i)->getX() + dX);
		}
	}
}

bool BattlescapeState::hasScrolled() const
{
	return _hasScrolled;
}

/**
 * Move the mouse back to where it started after we finish drag scrolling.
 * @param action Pointer to an action.
 */
void BattlescapeState::stopScrolling(Action *action)
{
#ifndef __MOBILE__
	if (Options::battleDragScrollInvert)
	{
		/* FIXME: Mouse warping still doesn't work as intended */
		SDL_WarpMouseInWindow(NULL, _xBeforeMouseScrolling, _yBeforeMouseScrolling);
		action->setMouseAction(_xBeforeMouseScrolling, _yBeforeMouseScrolling, _map->getX(), _map->getY());
		_battleGame->setupCursor();
		if (_battleGame->getCurrentAction()->actor == 0 && (_save->getSide() == FACTION_PLAYER || _save->getDebugMode()))
		{
			getMap()->setCursorType(CT_NORMAL);
		}
	}
	else
	{
		/* FIXME: Mouse warping still doesn't work as intended */
		SDL_WarpMouseInWindow(NULL, _cursorPosition.x, _cursorPosition.y);
		action->setMouseAction(_cursorPosition.x/action->getXScale(), _cursorPosition.y/action->getYScale(), _game->getScreen()->getSurface()->getX(), _game->getScreen()->getSurface()->getY());
		_map->setSelectorPosition(_cursorPosition.x / action->getXScale(), _cursorPosition.y / action->getYScale());
	}
#endif
	// reset our "mouse position stored" flag
	_cursorPosition.z = 0;
}

/**
 * Autosave the game the next time the battlescape is displayed.
 */
void BattlescapeState::autosave()
{
	_autosave = true;
}

void BattlescapeState::consumeEvent(Action *action)
{
	action->getDetails()->type = SDL_FIRSTEVENT;
}

}
