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

#include "../Entities/Coin.h"
#include "Plant.h"
#include "../Board/Board.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Modes/ZenGarden.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../GameConstants.h"
#include <array>
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "../Rules/TargetingRules.h"

bool Plant::MakesSun()
{
	return mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_TWINSUNFLOWER ||
		mSeedType == SeedType::SEED_SUNSHROOM || mSeedType == SeedType::SEED_PLANTERN;
}

void Plant::UpdateProductionPlant()
{
	if (!IsInPlay() || mApp->IsIZombieLevel() || mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;
	if (mSeedType == SeedType::SEED_PLANTERN && mBoard->mSunMoney >= FIVE_MILLION_SUN_THRESHOLD)
		return;

	if (mBoard->HasLevelAwardDropped())
		return;

	if (mSeedType == SeedType::SEED_MARIGOLD && mBoard->mCurrentWave == mBoard->mNumWaves)
	{
		if (mState != PlantState::STATE_MARIGOLD_ENDING)
		{
			mState = PlantState::STATE_MARIGOLD_ENDING;
			mStateCountdown = 6000;
		}
		else if (mStateCountdown <= 0)
			return;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND && mBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT)
		return;

	if (mSeedType == SeedType::SEED_TWINSUNFLOWER &&
		mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
	{
		int anAssaultInterval = TWIN_SUNFLOWER_ASSAULT_INTERVAL / 2;
		if (mTwinSunflowerBombCountdown > anAssaultInterval)
			mTwinSunflowerBombCountdown = anAssaultInterval;
		if (mTwinSunflowerBombCountdown > 0)
			--mTwinSunflowerBombCountdown;
		if (mTwinSunflowerBombCountdown <= 0)
		{
			mTwinSunflowerBombCountdown = anAssaultInterval;
			mBoard->TryLaunchTwinSunflowerSunBomb();
		}
	}
	else if (mSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		mTwinSunflowerBombCountdown = TWIN_SUNFLOWER_ASSAULT_INTERVAL;
	}

	mLaunchCounter--;
	if (mLaunchCounter <= 100)
	{
		int aFlashCountdown = PvzpAnimateCurve(100, 0, mLaunchCounter, 0, 100, PvzpCurves::CURVE_LINEAR);
		mEatenFlashCountdown = std::max(mEatenFlashCountdown, aFlashCountdown);
	}
	if (mLaunchCounter <= 0)
	{
		int aLaunchJitter = (mSeedType == SeedType::SEED_TWINSUNFLOWER || mSeedType == SeedType::SEED_PLANTERN) &&
			mBoard->mTwinSunflowerProductionOverdriveActive ? 75 : 150;
		mLaunchCounter = RandRangeInt(mLaunchRate - aLaunchJitter, mLaunchRate);
		if (mSeedType != SeedType::SEED_TWINSUNFLOWER ||
			mBoard->mSunMoney < TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
		auto AddFlagScaledSun = [&](CoinType theCoinType)
		{
			Coin* aCoin = mBoard->AddCoin(mX, mY, theCoinType, CoinMotion::COIN_MOTION_FROM_PLANT);
			if (!aCoin)
				return;
			int aBaseValue = aCoin->GetSunValue();
			int aScaledValue = mBoard->ScaleSunValueForCompletedFlags(aBaseValue);
			if (mSeedType == SeedType::SEED_PLANTERN &&
				mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
				aScaledValue = static_cast<int>(std::min<int64_t>(
					static_cast<int64_t>(aScaledValue) * 5, std::numeric_limits<int32_t>::max()));
			if (aScaledValue != aBaseValue)
				aCoin->mSunValueOverride = aScaledValue;
		};

		if (mSeedType == SeedType::SEED_SUNSHROOM)
		{
			if (mState == PlantState::STATE_SUNSHROOM_SMALL)
			{
				mBoard->AddCoin(mX, mY, CoinType::COIN_SMALLSUN, CoinMotion::COIN_MOTION_FROM_PLANT);
			}
			else
			{
				mBoard->AddCoin(mX, mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
			}
		}
		else if (mSeedType == SeedType::SEED_SUNFLOWER)
		{
			AddFlagScaledSun(CoinType::COIN_SUN);
		}
		else if (mSeedType == SeedType::SEED_TWINSUNFLOWER &&
			mBoard->mSunMoney < TWIN_SUNFLOWER_PRODUCTION_STOP_SUN_THRESHOLD)
		{
			AddFlagScaledSun(CoinType::COIN_SUN_150);
			AddFlagScaledSun(CoinType::COIN_SUN_150);
			if (Sexy::Rand(4) == 0)
				AddFlagScaledSun(CoinType::COIN_SUN_500);
			if (mBoard->mSunMoney < TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD &&
				mBoard->mTwinSunflowerBombardmentOverdriveActive &&
				Sexy::Rand(100) < (mBoard->mTwinSunflowerHighOverdriveActive ? 30 : 10))
				mBoard->TryLaunchTwinSunflowerSunBomb();
		}
		else if (mSeedType == SeedType::SEED_PLANTERN)
		{
			CoinType aSunType = Sexy::Rand(4) == 0 ? CoinType::COIN_SUN_600 : CoinType::COIN_SUN_100;
			AddFlagScaledSun(aSunType);
		}
		else if (mSeedType == SeedType::SEED_MARIGOLD)
		{
			mBoard->AddCoin(mX, mY, (Sexy::Rand(100) < 10) ? CoinType::COIN_GOLD : CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
		}

		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME)
		{
			if (mSeedType == SeedType::SEED_SUNFLOWER)
			{
				AddFlagScaledSun(CoinType::COIN_SUN);
			}
			else if (mSeedType == SeedType::SEED_MARIGOLD)
			{
				mBoard->AddCoin(mX, mY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
		}
	}
}

void Plant::UpdateSunShroom()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_SUNSHROOM_SMALL)
	{
		if (mStateCountdown == 0)
		{
			PlayBodyReanim("anim_grow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
			mState = PlantState::STATE_SUNSHROOM_GROWING;
			mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
		}

		UpdateProductionPlant();
	}
	else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayBodyReanim("anim_bigidle", ReanimLoopType::REANIM_LOOP, 10, RandRangeFloat(12.0f, 15.0f));
			mState = PlantState::STATE_SUNSHROOM_BIG;
		}
	}
	else
	{
		UpdateProductionPlant();
	}
}
