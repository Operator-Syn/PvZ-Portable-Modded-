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

#include <time.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <SDL.h>
#include <format>
#include "../Modes/ZenGarden.h"
#include "BoardInclude.h"
#include "../Entities/LawnCommon.h"
#include "../System/Music.h"
#include "../System/SaveGame.h"
#include "../Widget/LawnDialog.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/TypingCheck.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/AwardScreen.h"
#include "../../PvzpLib/Trail.h"
#include "../Widget/ChallengeScreen.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "../Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "../Zombie/ZombieStrengthRules.h"
#include "../Plant/PlantRules.h"

void Board::DisplayAdvice(std::string_view theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		if (mHelpDisplayed[theHelpIndex])
			return;

		mHelpDisplayed[theHelpIndex] = true;
	}

	mAdvice->SetLabel(theAdvice, theMessageStyle);
	mHelpIndex = theHelpIndex;
}

void Board::DisplayAdviceAgain(std::string_view theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		mHelpDisplayed[theHelpIndex] = false;
	}
	DisplayAdvice(theAdvice, theMessageStyle, theHelpIndex);
}

void Board::ClearAdviceImmediately()
{
	ClearAdvice(AdviceType::ADVICE_NONE);
	mAdvice->mDuration = 0;
}

void Board::ClearAdvice(AdviceType theHelpIndex)
{
	if (theHelpIndex == AdviceType::ADVICE_NONE || theHelpIndex == mHelpIndex)
	{
		mAdvice->ClearLabel();
		mHelpIndex = AdviceType::ADVICE_NONE;
	}
}

Coin* Board::AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion)
{
	Coin* aCoin = mCoins.DataArrayAlloc();
	aCoin->CoinInitialize(theX, theY, theCoinType, theCoinMotion);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
	{
		DisplayAdvice("[ADVICE_CLICK_ON_SUN]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_CLICK_ON_SUN);
	}
	return aCoin;
}

void Board::AddSunMoney(int theAmount)
{
	int64_t aNewSunMoney = static_cast<int64_t>(mSunMoney) + theAmount;
	mSunMoney = static_cast<int32_t>(std::clamp<int64_t>(aNewSunMoney, 0, std::numeric_limits<int32_t>::max()));
	mZombieTierSunMoney = std::max(mZombieTierSunMoney, mSunMoney);
	if (mSunMoney >= 8000)
		ReportAchievement::GiveAchievement(mApp, SunnyDays, true);
}

int Board::CountSunBeingCollected()
{
	int aCount = 0;
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mIsBeingCollected && aCoin->IsSun())
		{
			aCount += aCoin->GetSunValue();
		}
	}
	return aCount;
}

int Board::CountCoinsBeingCollected()
{
	int aCount = 0;
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mIsBeingCollected && aCoin->IsMoney())
		{
			aCount += aCoin->GetCoinValue(aCoin->mType);
		}
	}
	return aCount;
}

bool Board::TakeSunMoney(int theAmount)
{
	if (CanTakeSunMoney(theAmount))
	{
		mSunMoney -= theAmount;
		return true;
	}

	mApp->PlaySample(Sexy::SOUND_BUZZER);
	mOutOfMoneyCounter = 70;
	return false;
}

bool Board::CanTakeSunMoney(int theAmount)
{
	return theAmount >= 0 && static_cast<int64_t>(theAmount) <=
		static_cast<int64_t>(mSunMoney) + CountSunBeingCollected();
}

bool Board::TakeSunMoneyRate(float theSunPerSecond)
{
	if (theSunPerSecond <= 0.0f)
		return false;
	mContinuousSunCostRemainder += theSunPerSecond / 100.0f;
	int aWholeSunCost = static_cast<int>(mContinuousSunCostRemainder);
	if (aWholeSunCost == 0)
		return true;
	if (!CanTakeSunMoney(aWholeSunCost))
	{
		mContinuousSunCostRemainder = 0.0f;
		return false;
	}
	if (!TakeSunMoney(aWholeSunCost))
		return false;
	mContinuousSunCostRemainder -= aWholeSunCost;
	return true;
}

void Board::DropLootPiece(int thePosX, int thePosY, int theDropFactor)
{
	if (mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 22 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedMinigames && CountCoinByType(CoinType::COIN_PRESENT_MINIGAMES) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_MINIGAMES, CoinMotion::COIN_MOTION_COIN);
			return;
		}
		if (mLevel == 36 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedPuzzleMode && CountCoinByType(CoinType::COIN_PRESENT_PUZZLE_MODE) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_PUZZLE_MODE, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	int aDropHit = Rand(30000);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11 && !mDroppedFirstCoin && mCurrentWave > 5)
	{
		aDropHit = 1000;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
	{
		aDropHit *= 5;
	}

	if (mApp->IsWhackAZombieLevel())
	{
		int aSunChanceMin = 2500;
		int aSunChanceMax = mSunMoney > 500 ? 2800 : mSunMoney > 350 ? 3100 : mSunMoney > 200 ? 3700 : 5000;
		if (aDropHit >= aSunChanceMin * theDropFactor && aDropHit <= aSunChanceMax * theDropFactor)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
			AddCoin(thePosX - 20, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 40, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 60, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	if (mTotalSpawnedWaves > 70 && !mGoldMagnetOverdriveActive)
		return;

	int aPottedPlantChance;
	if (!mApp->mZenGarden->CanDropPottedPlantLoot())
	{
		aPottedPlantChance = 0;
	}
	else if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
	{
		aPottedPlantChance = 24;
	}
	else
	{
		aPottedPlantChance = mApp->IsSurvivalEndless(mApp->mGameMode) ? 3 : 12;
	}

	int aChocolateChance = aPottedPlantChance;
	if (mApp->mZenGarden->CanDropChocolate())
	{
		if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
		{
			aChocolateChance = aPottedPlantChance + 72;
		}
		else
		{
			aChocolateChance = aPottedPlantChance + (mApp->IsSurvivalEndless(mApp->mGameMode) ? 9 : 36);
		}
	}

	int aDiamondDropWeight = mGoldMagnetOverdriveActive ? 42 : 14;
	int aGoldDropWeight = mGoldMagnetOverdriveActive ? 708 : 236;
	int aDiamondChance = aChocolateChance + aDiamondDropWeight;
	int aGoldChance = aDiamondChance + aGoldDropWeight;
	int aSilverChance = aChocolateChance + 2500;

	CoinType aCoinType;
	if (aDropHit < aPottedPlantChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_PRESENT_PLANT;
	}
	else if (aDropHit < aChocolateChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_CHOCOLATE;
	}
	else if (aDropHit < aDiamondChance * theDropFactor)
	{
		aCoinType = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] < 1 ? CoinType::COIN_GOLD : CoinType::COIN_DIAMOND;
	}
	else if (aDropHit < aGoldChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_GOLD;
	}
	else if (aDropHit < aSilverChance * theDropFactor)
	{
		aCoinType = CoinType::COIN_SILVER;
	}
	else return;

	if (mApp->IsWallnutBowlingLevel() && Coin::IsMoney(aCoinType))
		return;

	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11)
	{
		int aMoney = Coin::GetCoinValue(CoinType::COIN_GOLD) * mLawnMowers.mSize;
		int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
		aMoney += mApp->mPlayerInfo->mCoins + CountCoinsBeingCollected();
		if (Coin::GetCoinValue(aCoinType) + aMoney >= aCost)
		{
			return;
		}
	}

	mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
	AddCoin(thePosX - 40, thePosY, aCoinType, CoinMotion::COIN_MOTION_COIN);
	mDroppedFirstCoin = true;
}

bool Board::CanDropLoot()
{
	return !mCutScene->ShouldRunUpsellBoard() && (!mApp->IsFirstTimeAdventureMode() || mLevel >= 11);
}

bool Board::PlantUsesAcceleratedPricing(SeedType theSeedType)
{
	return Plant::IsUpgrade(theSeedType) && mApp->IsSurvivalEndless(mApp->mGameMode);
}

int Board::GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType)
{
	int aCost = Plant::GetCost(theSeedType, theImitaterType);
	if (PlantUsesAcceleratedPricing(theSeedType))
	{
		aCost += CountPlantByType(theSeedType) * 50;
	}
	return aCost;
}
