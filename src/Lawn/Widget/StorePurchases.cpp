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

StoreScreen::~StoreScreen() = default;

StoreItem StoreScreen::GetStoreItemType(int theSpotIndex)
{
	if (mPage < NUM_STORE_PAGES && theSpotIndex < MAX_PAGE_SPOTS)
	{
		if (mPage == STORE_PAGE_SLOT_UPGRADES && theSpotIndex == 6 && mApp->IsTrialStageLocked())
		{
			return STORE_ITEM_PVZ;
		}
		return gStoreItemSpots[mPage][theSpotIndex];
	}

	PVZP_ASSERT(false);
	return STORE_ITEM_INVALID;
}

bool StoreScreen::IsFullVersionOnly(StoreItem theStoreItem)
{
	if (!mApp->IsTrialStageLocked())
		return false;

	if (theStoreItem == STORE_ITEM_PACKET_UPGRADE && mApp->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] >= 2)
		return true;

	return theStoreItem == STORE_ITEM_PLANT_TWINSUNFLOWER;
}

bool StoreScreen::IsPottedPlant(StoreItem theStoreItem)
{
	return theStoreItem == STORE_ITEM_POTTED_MARIGOLD_1 || theStoreItem == STORE_ITEM_POTTED_MARIGOLD_2 || theStoreItem == STORE_ITEM_POTTED_MARIGOLD_3;
}

bool StoreScreen::IsComingSoon(StoreItem theStoreItem)
{
	if (IsFullVersionOnly(theStoreItem))
		return true;
	else if (theStoreItem == STORE_ITEM_WHEEL_BARROW)
		return !mApp->mPlayerInfo->mPurchases[STORE_ITEM_MUSHROOM_GARDEN] && !mApp->mPlayerInfo->mPurchases[STORE_ITEM_AQUARIUM_GARDEN];
	else if (IsPottedPlant(theStoreItem))
		return !mApp->HasFinishedAdventure();
	else if (theStoreItem == STORE_ITEM_TREE_FOOD)
		return !mApp->mPlayerInfo->mPurchases[STORE_ITEM_TREE_OF_WISDOM] || mApp->mPlayerInfo->mPurchases[STORE_ITEM_TREE_FOOD] < PURCHASE_COUNT_OFFSET;
	return false;
}

bool StoreScreen::IsItemSoldOut(StoreItem theStoreItem)
{
	PlayerInfo* aPlayer = mApp->mPlayerInfo;
	if (theStoreItem == STORE_ITEM_INVALID)
		return false;
	else if (theStoreItem == STORE_ITEM_PACKET_UPGRADE)
		return aPlayer->mPurchases[STORE_ITEM_PACKET_UPGRADE] >= 4;
	else if (theStoreItem == STORE_ITEM_FERTILIZER || theStoreItem == STORE_ITEM_BUG_SPRAY)
		return aPlayer->mPurchases[theStoreItem] > PURCHASE_COUNT_OFFSET + 15;
	else if (theStoreItem == STORE_ITEM_TREE_FOOD)
		return aPlayer->mPurchases[STORE_ITEM_TREE_FOOD] >= PURCHASE_COUNT_OFFSET + 10;
	else if (theStoreItem == STORE_ITEM_BONUS_LAWN_MOWER)
		return aPlayer->mPurchases[STORE_ITEM_BONUS_LAWN_MOWER] >= 2;
	else if (IsPottedPlant(theStoreItem))
		return mApp->mZenGarden->IsZenGardenFull(true) || aPlayer->mPurchases[theStoreItem] == static_cast<uint32_t>(GetCurrentDaysSince2000(mApp->GetNowTime()));
	else return aPlayer->mPurchases[theStoreItem];

	unreachable();
}

bool StoreScreen::IsItemUnavailable(StoreItem theStoreItem)
{
	if (mEasyBuyingCheat)
		return false;

	/*
    if (mApp->HasFinishedAdventure())
        return true;

    bool aTrialStageLocked = mApp->IsTrialStageLocked();
    int aCurrentLevel = mApp->mPlayerInfo->mLevel;
    if (theStoreItem == STORE_ITEM_ROOF_CLEANER)
    {
        return aTrialStageLocked || aCurrentLevel < 42;
    }
    else if (theStoreItem == STORE_ITEM_PLANT_GLOOMSHROOM || theStoreItem == STORE_ITEM_PLANT_CATTAIL)
    {
        return aTrialStageLocked || aCurrentLevel < 35;
    }
    else if (theStoreItem == STORE_ITEM_PLANT_SPIKEROCK || theStoreItem == STORE_ITEM_PLANT_GOLD_MAGNET)
    {
        return aCurrentLevel < 41;
    }

    return
        theStoreItem != STORE_ITEM_PLANT_WINTERMELON &&
        theStoreItem != STORE_ITEM_PLANT_COBCANNON &&
        theStoreItem != STORE_ITEM_PLANT_IMITATER &&
        theStoreItem != STORE_ITEM_FIRSTAID;
    */

	if (theStoreItem == STORE_ITEM_ROOF_CLEANER)
	{
		return mApp->IsTrialStageLocked() || (!mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() < 42);
	}
	if (theStoreItem == STORE_ITEM_PLANT_GLOOMSHROOM)
	{
		return mApp->IsTrialStageLocked() || (!mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() < 35);
	}
	if (theStoreItem == STORE_ITEM_PLANT_CATTAIL)
	{
		return mApp->IsTrialStageLocked() || (!mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() < 35);
	}
	if (theStoreItem == STORE_ITEM_PLANT_SPIKEROCK)
	{
		return !mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() < 41;
	}
	if (theStoreItem == STORE_ITEM_PLANT_GOLD_MAGNET)
	{
		return !mApp->HasFinishedAdventure() && mApp->mPlayerInfo->GetLevel() < 41;
	}
	if (theStoreItem == STORE_ITEM_PLANT_WINTERMELON || theStoreItem == STORE_ITEM_PLANT_COBCANNON ||
		theStoreItem == STORE_ITEM_PLANT_IMITATER || theStoreItem == STORE_ITEM_FIRSTAID)
	{
		return !mApp->HasFinishedAdventure();
	}
	return false;
}

int StoreScreen::GetItemCost(StoreItem theStoreItem)
{
	if (theStoreItem == STORE_ITEM_BONUS_LAWN_MOWER)    return gLawnApp->mPlayerInfo->mPurchases[STORE_ITEM_BONUS_LAWN_MOWER] ? 500 : 200;
	switch (theStoreItem)
	{
	case STORE_ITEM_PLANT_GATLINGPEA:                   return 500;
	case STORE_ITEM_PLANT_TWINSUNFLOWER:                return 500;
	case STORE_ITEM_PLANT_GLOOMSHROOM:                  return 750;
	case STORE_ITEM_PLANT_CATTAIL:                      return 1000;
	case STORE_ITEM_PLANT_WINTERMELON:                  return 1000;
	case STORE_ITEM_PLANT_GOLD_MAGNET:                  return 300;
	case STORE_ITEM_PLANT_SPIKEROCK:                    return 750;
	case STORE_ITEM_PLANT_COBCANNON:                    return 2000;
	case STORE_ITEM_PLANT_IMITATER:                     return 3000;
	case STORE_ITEM_POTTED_MARIGOLD_1:                  return 250;
	case STORE_ITEM_POTTED_MARIGOLD_2:                  return 250;
	case STORE_ITEM_POTTED_MARIGOLD_3:                  return 250;
	case STORE_ITEM_GOLD_WATERINGCAN:                   return 1000;
	case STORE_ITEM_FERTILIZER:                         return 75;
	case STORE_ITEM_BUG_SPRAY:                          return 100;
	case STORE_ITEM_PHONOGRAPH:                         return 1500;
	case STORE_ITEM_GARDENING_GLOVE:                    return 100;
	case STORE_ITEM_MUSHROOM_GARDEN:                    return 3000;
	case STORE_ITEM_WHEEL_BARROW:                       return 20;
	case STORE_ITEM_STINKY_THE_SNAIL:                   return 300;
	case STORE_ITEM_PACKET_UPGRADE:
	{
		int aPurchase = gLawnApp->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE];
		return aPurchase == 0 ? 75 : aPurchase == 1 ? 500 : aPurchase == 2 ? 2000 : 8000;
	}
	case STORE_ITEM_POOL_CLEANER:                       return 100;
	case STORE_ITEM_ROOF_CLEANER:                       return 300;
	case STORE_ITEM_RAKE:                               return 20;
	case STORE_ITEM_AQUARIUM_GARDEN:                    return 3000;
	case STORE_ITEM_TREE_OF_WISDOM:                     return 1000;
	case STORE_ITEM_TREE_FOOD:                          return 250;
	case STORE_ITEM_FIRSTAID:                           return 200;
	default: PVZP_ASSERT(false);                              return 0;
	}
}

bool StoreScreen::CanAffordItem(StoreItem theStoreItem)
{
	return mApp->mPlayerInfo->mCoins >= GetItemCost(theStoreItem);
}

void StoreScreen::PurchaseItem(StoreItem theStoreItem)
{
	mApp->SetCursor(CURSOR_POINTER);
	mBubbleCountDown = 0;
	mApp->CrazyDaveStopTalking();
	if (!CanAffordItem(theStoreItem))
	{
		// the localization key names for this dialog are wrong
		Dialog* aDialog = mApp->DoDialog(DIALOG_NOT_ENOUGH_MONEY, true,
			mApp->GetString("NOT_ENOUGH_MONEY", "Not enough money"),
			mApp->GetString("CANNOT_AFFORD_ITEM",
				"You can't afford this item yet. Earn more coins by killing zombies!"),
			"[DIALOG_BUTTON_OK]", BUTTONS_FOOTER);
		mWaitForDialog = true;
		aDialog->WaitForResult(true);
		mWaitForDialog = false;
	}
	else
	{
		LawnDialog* aComfirmDialog = (LawnDialog*)mApp->DoDialog(
			DIALOG_STORE_PURCHASE,
			true,
			mApp->GetString("BUY_ITEM_HEADER", "Buy this item?"),
			mApp->GetString("BUY_ITEM", "Are you sure you want to buy this item?"),
			"",
			BUTTONS_YES_NO
		);
		aComfirmDialog->mLawnYesButton->SetLabel("[DIALOG_BUTTON_YES]");
		aComfirmDialog->mLawnNoButton->SetLabel("[DIALOG_BUTTON_NO]");

		mWaitForDialog = true;
		int aComfirmResult = aComfirmDialog->WaitForResult(true);
		mWaitForDialog = false;

		if (aComfirmResult == ID_OK)
		{
			mApp->mPlayerInfo->AddCoins(-GetItemCost(theStoreItem));
			if (theStoreItem == STORE_ITEM_PACKET_UPGRADE)
			{
				++mApp->mPlayerInfo->mPurchases[theStoreItem];
				std::string aDialogLines = mApp->GetFormattedString(
					"NOW_YOU_CAN_CHOOSE_X_SEEDS", "Now you can choose to take %d seeds with you per level!",
					6 + mApp->mPlayerInfo->mPurchases[theStoreItem]);
				Dialog* aDialog = mApp->DoDialog(DIALOG_UPGRADED, true, mApp->GetString("MORE_SLOTS", "More slots!"), aDialogLines, "[DIALOG_BUTTON_OK]", BUTTONS_FOOTER);

				mWaitForDialog = true;
				aDialog->WaitForResult(true);
				mWaitForDialog = false;

				if (mApp->mBoard)
				{
					mApp->mBoard->mSeedBank->UpdateWidth();
				}
			}
			else if (theStoreItem == STORE_ITEM_BONUS_LAWN_MOWER)
			{
				mApp->mPlayerInfo->mPurchases[theStoreItem]++;
			}
			else if (theStoreItem == STORE_ITEM_RAKE)
			{
				mApp->mPlayerInfo->mPurchases[theStoreItem] = 3;
			}
			else if (theStoreItem == STORE_ITEM_STINKY_THE_SNAIL)
			{
				uint32_t aTime = static_cast<uint32_t>(mApp->GetNowTime());
				if (aTime == 0) aTime = 1;
				mApp->mPlayerInfo->mPurchases[theStoreItem] = aTime;
			}
			else if (theStoreItem == STORE_ITEM_FERTILIZER || theStoreItem == STORE_ITEM_BUG_SPRAY)
			{
				if (mApp->mPlayerInfo->mPurchases[theStoreItem] < PURCHASE_COUNT_OFFSET)
				{
					mApp->mPlayerInfo->mPurchases[theStoreItem] = PURCHASE_COUNT_OFFSET;
				}
				mApp->mPlayerInfo->mPurchases[theStoreItem] += 5;
			}
			else if (theStoreItem == STORE_ITEM_TREE_FOOD)
			{
				if (mApp->mPlayerInfo->mPurchases[theStoreItem] < PURCHASE_COUNT_OFFSET)
				{
					mApp->mPlayerInfo->mPurchases[theStoreItem] = PURCHASE_COUNT_OFFSET;
				}
				mApp->mPlayerInfo->mPurchases[theStoreItem]++;
			}
			else if (theStoreItem == STORE_ITEM_TREE_OF_WISDOM)
			{
				mApp->mPlayerInfo->mPurchases[theStoreItem] = 1;
				mApp->mPlayerInfo->mChallengeRecords[GAMEMODE_TREE_OF_WISDOM - GAMEMODE_SURVIVAL_NORMAL_STAGE_1] = 1;

				LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(
					DIALOG_STORE_PURCHASE,
					true,
					"[VISIT_TREE_HEADER]",
					"[VISIT_TREE_BODY]",
					"",
					BUTTONS_YES_NO
				);
				aDialog->mLawnYesButton->SetLabel("[DIALOG_BUTTON_YES]");
				aDialog->mLawnNoButton->SetLabel("[DIALOG_BUTTON_NO]");

				mWaitForDialog = true;
				int aResult = aDialog->WaitForResult(true);
				mWaitForDialog = false;

				if (aResult == ID_OK)
				{
					mGoToTreeNow = true;
					mResult = aResult;
				}
			}
			else if (IsPottedPlant(theStoreItem))
			{
				mApp->mZenGarden->AddPottedPlant(&mPottedPlantSpecs);
				mPottedPlantSpecs.InitializePottedPlant(SEED_MARIGOLD);
				mPottedPlantSpecs.mDrawVariation = (DrawVariation)RandRangeInt(VARIATION_MARIGOLD_WHITE, VARIATION_MARIGOLD_LIGHT_GREEN);
				mApp->mPlayerInfo->mPurchases[theStoreItem] = GetCurrentDaysSince2000(mApp->GetNowTime());
			}
			else
			{
				PVZP_ASSERT(theStoreItem >= STORE_ITEM_PLANT_GATLINGPEA && theStoreItem < (StoreItem)MAX_PURCHASES);
				mApp->mPlayerInfo->mPurchases[theStoreItem] = 1;
			}

			if (theStoreItem == STORE_ITEM_FIRSTAID)
			{
				SetBubbleText(3400, 800, false);
			}

			if (mApp->mSeedChooserScreen)
			{
				mApp->mSeedChooserScreen->UpdateAfterPurchase();
			}

			// Only give the achievement if the player bought a plant and has all plants purchased
			bool aGiveAchievement = theStoreItem >= STORE_ITEM_PLANT_GATLINGPEA && theStoreItem <= STORE_ITEM_PLANT_IMITATER;
			if (aGiveAchievement) {
				for (int aSeedType = SeedType::SEED_GATLINGPEA; aSeedType <= SeedType::SEED_IMITATER; aSeedType++) {
					if (!mApp->HasSeedType(SeedType(aSeedType)))
						aGiveAchievement = false;
				}
			}

			if (aGiveAchievement) {
				ReportAchievement::GiveAchievement(mApp, Morticulturalist, aGiveAchievement);
				SetBubbleText(4000, 800, false);
			}

			mApp->WriteCurrentUserConfig();
		}
	}
}
