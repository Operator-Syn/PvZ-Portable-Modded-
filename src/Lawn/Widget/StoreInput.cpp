/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <time.h>
#include "../Coin.h"
#include "../Board.h"
#include "../Plant.h"
#include "../LawnCommon.h"
#include "LawnDialog.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../ZenGarden.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "SeedChooserScreen.h"
#include "../../GameConstants.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "misc/Debug.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "widget/WidgetManager.h"
#include "AchievementsScreen.h"
#include <algorithm>


#include "StoreCatalog.h"

void StoreScreen::UpdateMouse()
{
	mMouseOverItem = STORE_ITEM_INVALID;
	if (mStoreTime < 120 || mBubbleClickToContinue || mHatchTimer > 0 || mWaitForDialog) return;
	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX, aMouseY = mApp->mWidgetManager->mLastMouseY - mY;
	bool aShowFinger = false;
	for (int aItemPos = 0; aItemPos < MAX_PAGE_SPOTS; aItemPos++)
	{
		StoreItem aItemType = GetStoreItemType(aItemPos);
		if (aItemType != STORE_ITEM_INVALID && !IsItemUnavailable(aItemType))
		{
			int aItemX, aItemY;
			GetStorePosition(aItemPos, aItemX, aItemY);
			if (Rect(aItemX, aItemY, 50, 87).Contains(aMouseX, aMouseY))
			{
				mMouseOverItem = aItemType;
				int aMessageIndex = -1;
				switch (aItemType)
				{
				case STORE_ITEM_PLANT_GATLINGPEA:       aMessageIndex = 2000;                           break;
				case STORE_ITEM_PLANT_TWINSUNFLOWER:    aMessageIndex = 2001;                           break;
				case STORE_ITEM_PLANT_GLOOMSHROOM:      aMessageIndex = 2002;                           break;
				case STORE_ITEM_PLANT_CATTAIL:          aMessageIndex = 2003;                           break;
				case STORE_ITEM_PLANT_WINTERMELON:      aMessageIndex = 2004;                           break;
				case STORE_ITEM_PLANT_GOLD_MAGNET:      aMessageIndex = 2005;                           break;
				case STORE_ITEM_PLANT_SPIKEROCK:        aMessageIndex = 2006;                           break;
				case STORE_ITEM_PLANT_COBCANNON:        aMessageIndex = 2007;                           break;
				case STORE_ITEM_PLANT_IMITATER:         aMessageIndex = 2008;                           break;
				case STORE_ITEM_BONUS_LAWN_MOWER:       aMessageIndex = 2009;                           break;
				case STORE_ITEM_POTTED_MARIGOLD_1:
				case STORE_ITEM_POTTED_MARIGOLD_2:
				case STORE_ITEM_POTTED_MARIGOLD_3:      aMessageIndex = 2010;                           break;
				case STORE_ITEM_GOLD_WATERINGCAN:       aMessageIndex = 2019;                           break;
				case STORE_ITEM_FERTILIZER:             aMessageIndex = 2020;                           break;
				case STORE_ITEM_BUG_SPRAY:              aMessageIndex = 2022;                           break;
				case STORE_ITEM_PHONOGRAPH:             aMessageIndex = 2021;                           break;
				case STORE_ITEM_GARDENING_GLOVE:        aMessageIndex = 2023;                           break;
				case STORE_ITEM_MUSHROOM_GARDEN:        aMessageIndex = 2032;                           break;
				case STORE_ITEM_WHEEL_BARROW:           aMessageIndex = 2024;                           break;
				case STORE_ITEM_STINKY_THE_SNAIL:       aMessageIndex = 2025;                           break;
				case STORE_ITEM_PACKET_UPGRADE:
					aMessageIndex = std::clamp(static_cast<int>(mApp->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE]) + 2011, 2011, 2014);
					break;
				case STORE_ITEM_POOL_CLEANER:           aMessageIndex = 2026;                           break;
				case STORE_ITEM_ROOF_CLEANER:           aMessageIndex = 2027;                           break;
				case STORE_ITEM_RAKE:                   aMessageIndex = 2028;                           break;
				case STORE_ITEM_AQUARIUM_GARDEN:        aMessageIndex = 2029;                           break;
				case STORE_ITEM_CHOCOLATE:                                                              break;
				case STORE_ITEM_TREE_OF_WISDOM:         aMessageIndex = 2030;                           break;
				case STORE_ITEM_TREE_FOOD:              aMessageIndex = 2031;                           break;
				case STORE_ITEM_FIRSTAID:               aMessageIndex = 2033;                           break;
				case STORE_ITEM_PVZ:                    aMessageIndex = 2034;                           break;
				default:                                PVZP_ASSERT(false);                              break;
				}
				if (mApp->mCrazyDaveMessageIndex != aMessageIndex)
					SetBubbleText(aMessageIndex, 100, false);
				else mBubbleCountDown = 100;
				if (IsFullVersionOnly(aItemType) || (!IsItemSoldOut(aItemType) && !IsItemUnavailable(aItemType) && !IsComingSoon(aItemType)))
					aShowFinger = true;
				break;
			}
		}
	}

	mApp->SetCursor(mBackButton->mIsOver || mPrevButton->mIsOver || mNextButton->mIsOver || aShowFinger ? CURSOR_HAND : CURSOR_POINTER);
}

void StoreScreen::ButtonPress(int theId)
{
	if (theId != StoreScreen::StoreScreen_Prev && theId != StoreScreen::StoreScreen_Next)
		mApp->PlaySample(Sexy::SOUND_BUTTONCLICK);
}

void StoreScreen::ButtonDepress(int theId)
{
	if (theId == StoreScreen::StoreScreen_Back)
		mResult = 1000;
	else if (theId == StoreScreen::StoreScreen_Prev || theId == StoreScreen::StoreScreen_Next)
	{
		mHatchTimer = 50;
		mApp->PlaySample(Sexy::SOUND_HATCHBACK_CLOSE);
		mBubbleCountDown = 0;
		mApp->CrazyDaveStopTalking();
		EnableButtons(false);
		do
		{
			if (theId == StoreScreen::StoreScreen_Prev)
			{
				mPage = (StorePages)(mPage - 1);
				if (mPage < STORE_PAGE_SLOT_UPGRADES)
				{
					mPage = STORE_PAGE_ZEN2;
				}
			}
			else
			{
				mPage = (StorePages)(mPage + 1);
				if (mPage >= NUM_STORE_PAGES)
				{
					mPage = STORE_PAGE_SLOT_UPGRADES;
				}
			}
		} while (!IsPageShown(mPage));
	}
}

void StoreScreen::KeyDown(KeyCode theKey)
{
	if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		ButtonDepress(StoreScreen::StoreScreen_Back);
		return;
	}

	if (mBubbleClickToContinue && (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN))
	{
		AdvanceCrazyDaveDialog();
		return;
	}

	Dialog::KeyDown(theKey);
}

void StoreScreen::MouseDown(int x, int y, [[maybe_unused]] int theClickCount)
{
	if (mBubbleClickToContinue)
	{
		AdvanceCrazyDaveDialog();
		return;
	}
	if (!CanInteractWithButtons()) return;
	for (int aItemPos = 0; aItemPos < MAX_PAGE_SPOTS; aItemPos++)
	{
		StoreItem aItemType = GetStoreItemType(aItemPos);
		if (aItemType == STORE_ITEM_INVALID) continue;
		int aItemX, aItemY;
		GetStorePosition(aItemPos, aItemX, aItemY);
		if (Rect(aItemX, aItemY, 50, 87).Contains(x, y))
		{
			if (IsFullVersionOnly(aItemType))
			{
				mWaitForDialog = true;
				mApp->LawnMessageBox(DIALOG_MESSAGE, "[GET_FULL_VERSION_TITLE]", "[FULL_VERSION_TO_BUY]", "[DIALOG_BUTTON_OK]", "", BUTTONS_FOOTER);
				mWaitForDialog = false;
			}
			else if (aItemType == STORE_ITEM_PVZ)
			{
				mWaitForDialog = true;
				mApp->LawnMessageBox(
					DIALOG_MESSAGE, "[BUY_PVZ_TITLE]", "[BUY_PVZ_BODY]", "[GET_FULL_VERSION_YES_BUTTON]", "[GET_FULL_VERSION_NO_BUTTON]", BUTTONS_YES_NO);
				mWaitForDialog = false;
			}
			else if(!IsItemSoldOut(aItemType) && !IsItemUnavailable(aItemType) && !IsComingSoon(aItemType))
				PurchaseItem(aItemType);
			break;
		}
	}
}

void StoreScreen::EnableButtons(bool theEnable)
{
	if (mEasyBuyingCheat || IsPageShown(STORE_PAGE_PLANT_UPGRADES) || !theEnable)
	{
		mNextButton->mMouseVisible = theEnable;
		mNextButton->SetDisabled(!theEnable);
		mPrevButton->mMouseVisible = theEnable;
		mPrevButton->SetDisabled(!theEnable);
	}
	mBackButton->mMouseVisible = theEnable;
	mBackButton->SetDisabled(!theEnable);
}
