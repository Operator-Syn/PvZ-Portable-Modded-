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
#include "../Entities/Coin.h"
#include "../Board/Board.h"
#include "../Plant/Plant.h"
#include "../Entities/LawnCommon.h"
#include "LawnDialog.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../Modes/ZenGarden.h"
#include "SeedPacket.h"
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

StoreScreenOverlay::StoreScreenOverlay(StoreScreen* theParent)
{
	mParent = theParent;
	mMouseVisible = false;
	mHasAlpha = true;
}

void StoreScreenOverlay::Draw(Graphics* g)
{
	mParent->DrawOverlay(g);
}

void StoreScreen::GetStorePosition(int theSpotIndex, int& thePosX, int& thePosY)
{
	if (theSpotIndex <= 3)
	{
		thePosX = STORESCREEN_ITEMOFFSET_1_X + STORESCREEN_ITEMSIZE * theSpotIndex;
		thePosY = STORESCREEN_ITEMOFFSET_1_Y;
	}
	else
	{
		thePosX = STORESCREEN_ITEMOFFSET_2_X + STORESCREEN_ITEMSIZE * (theSpotIndex - 4);
		thePosY = STORESCREEN_ITEMOFFSET_2_Y;
	}
}

void StoreScreen::DrawItemIcon(Graphics* g, int theItemPosition, StoreItem theItemType, bool theIsForHighlight)
{
	if (theIsForHighlight)
	{
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->SetColor(Color(255, 255, 255, 96));
		g->SetColorizeImages(true);
	}

	int aPosX, aPosY;
	GetStorePosition(theItemPosition, aPosX, aPosY);
	if (theItemType == STORE_ITEM_PACKET_UPGRADE)
	{
		g->SetColor(Color(255, 255, 255, 32));
		g->DrawImage(Sexy::IMAGE_STORE_PACKETUPGRADE, aPosX - 7, aPosY + 7);
		if (theIsForHighlight)
		{
			g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
			g->SetColorizeImages(false);
		}

		std::string aSlotText = PvzpReplaceNumberString("[STORE_UPGRADE_SLOTS]", "{SLOTS}", mApp->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] + 7);
		Rect aRect(aPosX, aPosY + 6, 55, 70);
		// STORE_USE_*_IMAGE_LABEL not checked: no localized image, always draw text.
		PvzpDrawStringWrapped(g, aSlotText, aRect, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	}
	else if (theItemType == STORE_ITEM_POOL_CLEANER)
	{
		g->DrawImage(Sexy::IMAGE_ICON_POOLCLEANER, aPosX + 1, aPosY + 7);
	}
	else if (theItemType == STORE_ITEM_RAKE)
	{
		g->DrawImage(Sexy::IMAGE_ICON_RAKE, aPosX - 5, aPosY + 10);
	}
	else if (theItemType == STORE_ITEM_ROOF_CLEANER)
	{
		g->DrawImage(Sexy::IMAGE_ICON_ROOFCLEANER, aPosX, aPosY + 28);
	}
	else if (theItemType == STORE_ITEM_PLANT_IMITATER)
	{
		g->DrawImage(Sexy::IMAGE_IMITATERSEED, aPosX, aPosY);
	}
	else if (theItemType == STORE_ITEM_MUSHROOM_GARDEN)
	{
		g->DrawImage(Sexy::IMAGE_STORE_MUSHROOMGARDENICON, aPosX - 8, aPosY + 2);
	}
	else if (theItemType == STORE_ITEM_AQUARIUM_GARDEN)
	{
		g->DrawImage(Sexy::IMAGE_STORE_AQUARIUMGARDENICON, aPosX - 8, aPosY + 2);
	}
	else if (theItemType == STORE_ITEM_TREE_OF_WISDOM)
	{
		g->DrawImage(Sexy::IMAGE_STORE_TREEOFWISDOMICON, aPosX - 8, aPosY + 2);
	}
	else if (theItemType == STORE_ITEM_FIRSTAID)
	{
		g->DrawImage(Sexy::IMAGE_STORE_FIRSTAIDWALLNUTICON, aPosX - 1, aPosY + 13);
	}
	else if (theItemType == STORE_ITEM_PVZ)
	{
		g->DrawImage(Sexy::IMAGE_STORE_PVZICON, aPosX, aPosY - 9);
	}
	else if (theItemType == STORE_ITEM_TREE_FOOD)
	{
		g->DrawImage(Sexy::IMAGE_TREEFOOD, aPosX - 8, aPosY - 2);
	}
	else if (theItemType == STORE_ITEM_STINKY_THE_SNAIL)
	{
		g->DrawImage(Sexy::IMAGE_REANIM_STINKY_TURN3, aPosX - 24, aPosY + 14);
	}
	else if (theItemType == STORE_ITEM_GOLD_WATERINGCAN)
	{
		g->DrawImage(Sexy::IMAGE_WATERINGCANGOLD, aPosX - 14, aPosY - 4);
	}
	else if (theItemType == STORE_ITEM_FERTILIZER)
	{
		g->DrawImage(Sexy::IMAGE_FERTILIZER, aPosX - 11, aPosY - 2);
		PvzpDrawString(g, "x5", aPosX + 56, aPosY + 62, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	}
	else if (theItemType == STORE_ITEM_PHONOGRAPH)
	{
		g->DrawImage(Sexy::IMAGE_PHONOGRAPH, aPosX - 12, aPosY + 3);
	}
	else if (theItemType == STORE_ITEM_BUG_SPRAY)
	{
		g->DrawImage(Sexy::IMAGE_BUG_SPRAY, aPosX - 12, aPosY + 3);
		PvzpDrawString(g, "x5", aPosX + 56, aPosY + 62, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	}
	else if (theItemType == STORE_ITEM_GARDENING_GLOVE)
	{
		g->DrawImage(Sexy::IMAGE_ZEN_GARDENGLOVE, aPosX - 12, aPosY + 3);
	}
	else if (theItemType == STORE_ITEM_WHEEL_BARROW)
	{
		g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aPosX - 12, aPosY + 3);
	}
	else if (IsPottedPlant(theItemType))
	{
		mApp->mZenGarden->DrawPottedPlantIcon(g, aPosX, aPosY, &mPottedPlantSpecs);
	}
	else
	{
		DrawSeedPacket(g, aPosX, aPosY, (SeedType)(theItemType + 40), SEED_NONE, 0, 255, false, false);
	}

	g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	g->SetColorizeImages(false);
}

void StoreScreen::DrawItem(Graphics* g, int theItemPosition, StoreItem theItemType)
{
	if (IsItemUnavailable(theItemType))
		return;

	DrawItemIcon(g, theItemPosition, theItemType, false);

	int aPosX, aPosY;
	GetStorePosition(theItemPosition, aPosX, aPosY);
	if (theItemType != STORE_ITEM_PVZ)
	{
		g->DrawImage(Sexy::IMAGE_STORE_PRICETAG, aPosX - 3, aPosY + 70);
		std::string aCostString = LawnApp::GetMoneyString(GetItemCost(theItemType));
		PvzpDrawString(g, aCostString, aPosX + 23, aPosY + 85, Sexy::FONT_BRIANNETOD12, Color::Black, DS_ALIGN_CENTER);
	}
	if (IsComingSoon(theItemType))
	{
		Rect aRect(aPosX, aPosY, 60, 70);
		if (theItemType == STORE_ITEM_PLANT_TWINSUNFLOWER || theItemType == STORE_ITEM_PACKET_UPGRADE)
		{
			aRect.mX -= 4;
		}
		// STORE_USE_*_IMAGE_LABEL not checked: no localized image, always draw text.
		PvzpDrawStringWrapped(g, "[COMING_SOON]", aRect, Sexy::FONT_HOUSEOFTERROR16, Color(255, 0, 0), DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	}
	else if (IsItemSoldOut(theItemType))
	{
		Rect aRect(aPosX, aPosY, 50, 70);
		// STORE_USE_*_IMAGE_LABEL not checked: no localized image, always draw text.
		PvzpDrawStringWrapped(g, "[SOLD_OUT]", aRect, Sexy::FONT_HOUSEOFTERROR16, Color(255, 0, 0), DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	}
	else if (mMouseOverItem == theItemType)
	{
		if (theItemType >= 0 && theItemType <= 8)
		{
			g->DrawImage(Sexy::IMAGE_SEEDPACKETFLASH, aPosX, aPosY);
		}
		else
		{
			DrawItemIcon(g, theItemPosition, theItemType, true);
		}
	}
}

void StoreScreen::Draw(Graphics* g)
{
	g->SetLinearBlend(true);
	mDrawnOnce = true;

	int aStoreSignPosY = PvzpAnimateCurve(50, 110, mStoreTime, -150, 0, CURVE_EASE_IN_OUT);
	if (mApp->IsNight())
	{
		g->DrawImage(Sexy::IMAGE_STORE_BACKGROUNDNIGHT, 0, 0);
	}
	else
	{
		g->DrawImage(Sexy::IMAGE_STORE_BACKGROUND, 0, 0);
	}

	if (!mHatchTimer && mHatchOpen)
	{
		g->DrawImage(Sexy::IMAGE_STORE_CAR, mShakeX + 196, mShakeY + 138);
		g->DrawImage(Sexy::IMAGE_STORE_HATCHBACKOPEN, mShakeX + 299, mShakeY);
		if (mApp->IsNight())
		{
			g->DrawImage(Sexy::IMAGE_STORE_CAR_NIGHT, mShakeX + 688, mShakeY + 193);
		}
	}
	else
	{
		g->DrawImage(Sexy::IMAGE_STORE_CARCLOSED, mShakeX + 196, mShakeY + 138);
		if (mApp->IsNight())
		{
			g->DrawImage(Sexy::IMAGE_STORE_CAR_NIGHT, mShakeX + 688, mShakeY + 193);
			g->DrawImage(Sexy::IMAGE_STORE_CARCLOSED_NIGHT, mShakeX + 337, mShakeY + 187);
		}
	}
	g->DrawImage(Sexy::IMAGE_STORE_SIGN, 285, aStoreSignPosY);

	Graphics gCrazyDave = Graphics(*g);
	gCrazyDave.mTransX -= 42.0f;
	gCrazyDave.mTransY += 68.0f;
	mApp->DrawCrazyDave(&gCrazyDave);

	if (!mHatchTimer && mHatchOpen)
	{
		for (int i = 0; i < MAX_PAGE_SPOTS; i++)
		{
			StoreItem aStoreItem = GetStoreItemType(i);
			if (aStoreItem != STORE_ITEM_INVALID)
			{
				DrawItem(g, i, aStoreItem);
			}
		}
	}

	g->DrawImage(Sexy::IMAGE_COINBANK, STORESCREEN_COINBANK_X, STORESCREEN_COINBANK_Y);
	g->SetColor(Color(180, 255, 90));
	g->SetFont(Sexy::FONT_CONTINUUMBOLD14);
	std::string aCoinLabel = mApp->GetMoneyString(mApp->mPlayerInfo->mCoins);
	g->DrawString(aCoinLabel, STORESCREEN_COINBANK_X + 116 - Sexy::FONT_CONTINUUMBOLD14->StringWidth(aCoinLabel), STORESCREEN_COINBANK_Y + 24);

	if (!mPrevButton->mDisabled)
	{
		int aNumPages = 0;
		for (StorePages aPage = STORE_PAGE_SLOT_UPGRADES; aPage < NUM_STORE_PAGES; aPage = (StorePages)(aPage + 1))
		{
			if (IsPageShown(aPage))
			{
				aNumPages++;
			}
		}

		std::string aPageString = PvzpReplaceNumberString(PvzpReplaceNumberString("[STORE_PAGE]", "{PAGE}", mPage + 1), "{NUM_PAGES}", aNumPages);
		PvzpDrawString(g, aPageString, STORESCREEN_PAGESTRING_X, STORESCREEN_PAGESTRING_Y, Sexy::FONT_BRIANNETOD12, Color(80, 80, 80), DS_ALIGN_CENTER);
	}
}

void StoreScreen::DrawOverlay(Graphics* g)
{
	for (Coin* aCoin : mCoins)
	{
		if (!aCoin->mDead)
		{
			aCoin->Draw(g);
		}
	}
}
