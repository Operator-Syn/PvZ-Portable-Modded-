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
#include "PlantHealing.h"
#include "../Rules/TargetingRules.h"

bool Plant::MakesSun()
{
	return mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_TWINSUNFLOWER ||
		mSeedType == SeedType::SEED_SUNSHROOM || mSeedType == SeedType::SEED_PLANTERN ||
		mSeedType == SeedType::SEED_SERRA_BISHOP;
}

void Plant::UpdateProductionPlant()
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING || mApp->mSeedChooserScreen != nullptr)
		return;
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

	if (mSeedType == SeedType::SEED_SERRA_BISHOP)
	{
		auto FindHealingTarget = [&]() -> Plant*
		{
			Plant* aBest = nullptr;
			for (Plant* aPlant : mBoard->mPlants)
			{
				if (!PlantHealing::PlantCanRegenerate(aPlant) || aPlant->NotOnGround() ||
					!aPlant->IsInPlay() || aPlant->mPlantHealth >= aPlant->mPlantMaxHealth)
					continue;
				if (aBest == nullptr || static_cast<int64_t>(aPlant->mPlantHealth) * aBest->mPlantMaxHealth <
					static_cast<int64_t>(aBest->mPlantHealth) * aPlant->mPlantMaxHealth)
					aBest = aPlant;
			}
			return aBest;
		};
		if (mSerraHealCooldown > 0)
			--mSerraHealCooldown;
		// Migrate the earlier Sunflower cadence without restarting a running cast.
		if (mLaunchRate != SERRA_PRODUCTION_RATE_TICKS)
		{
			mLaunchRate = SERRA_PRODUCTION_RATE_TICKS;
			mLaunchCounter = std::clamp(mLaunchCounter, 0, mLaunchRate);
		}
		if (--mLaunchCounter <= 0)
		{
			mLaunchCounter = RandRangeInt(mLaunchRate - SERRA_PRODUCTION_JITTER_TICKS, mLaunchRate);
			mSerraSunPending = true;
		}
		if (mShootingCounter == 0 &&
			(mSerraSunPending || (mSerraHealCooldown == 0 && mBoard->mSunMoney >= SERRA_HEAL_SUN_PER_HP &&
				FindHealingTarget() != nullptr)))
		{
			// Randomly choose a complete source sequence, like the other custom plants.
			mAnimPing = RandRangeInt(0, 1) != 0;
			mShootingCounter = SERRA_SEQUENCE_DURATION_TICKS[mAnimPing ? 1 : 0];
			mFrame = 0;
			Plant* aRequestedTarget = mSerraHealCooldown == 0 && mBoard->mSunMoney >= SERRA_HEAL_SUN_PER_HP ? FindHealingTarget() : nullptr;
			PvzpLogLn("[healing] tick={} event=serra_cast_start source_id={} source_row={} source_col={} source_hp={} source_max_hp={} "
				"sequence={} sun_pending={} healing_requested={} requested_target_id={} sun_balance={} release_after_ticks={}",
				mBoard->mMainCounter, mBoard->mPlants.DataArrayGetID(this), mRow, mPlantCol, mPlantHealth, mPlantMaxHealth,
				mAnimPing ? "critical" : "staff", mSerraSunPending, aRequestedTarget != nullptr,
				aRequestedTarget != nullptr ? mBoard->mPlants.DataArrayGetID(aRequestedTarget) : 0U,
				mBoard->mSunMoney, SERRA_SUN_RELEASE_TICKS[mAnimPing ? 1 : 0]);
		}
		if (mShootingCounter > 0)
		{
			const int aSet = mAnimPing ? 1 : 0;
			const int aElapsed = SERRA_SEQUENCE_DURATION_TICKS[aSet] - mShootingCounter;
			if (aElapsed == SERRA_SUN_RELEASE_TICKS[aSet])
			{
				const int aProducedSuns = mSerraSunPending ? (aSet == 1 ? 5 : 1) : 0;
				if (mSerraSunPending)
				{
					mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
					for (int aSun = 0; aSun < (aSet == 1 ? 5 : 1); aSun++)
						AddFlagScaledSun(CoinType::COIN_SUN);
					mSerraSunPending = false;
				}
				// Resolve priority at the release pose so dead or restored targets are skipped.
				Plant* aTarget = mSerraHealCooldown == 0 ? FindHealingTarget() : nullptr;
				if (aTarget != nullptr)
				{
					const int aSunBefore = mBoard->mSunMoney;
					const int aAffordableHP = std::max(0, aSunBefore) / SERRA_HEAL_SUN_PER_HP;
					PlantHealing::HealingAudit anAudit;
					const int aHealed = PlantHealing::HealPlant(mBoard, aTarget, aTarget->mPlantMaxHealth * 0.05f,
						aAffordableHP, &anAudit);
					bool aPaymentSucceeded = true;
					if (aHealed > 0)
					{
						aPaymentSucceeded = mBoard->TakeSunMoney(anAudit.mPaidBaseHealing * SERRA_HEAL_SUN_PER_HP);
						aTarget->mSerraBlessingTicksRemaining = SERRA_BLESSING_DURATION_TICKS;
						mSerraHealCooldown = SERRA_HEAL_COOLDOWN_TICKS;
					}
					PvzpLogLn("[healing] tick={} event=serra_release status={} sequence={} source=\"{}\" source_id={} source_seed={} source_row={} source_col={} "
						"target=\"{}\" target_id={} target_seed={} target_row={} target_col={} target_x={} target_y={} hp_before={} hp_after={} hp_max={} "
						"hp_percent_before={:.2f} hp_percent_after={:.2f} base={:.3f} modified={:.3f} requested_with_carry={:.3f} healed={} "
						"overflow={:.3f} unaffordable={:.3f} carry_before={:.3f} carry_after={:.3f} affordable_hp={} sun_per_hp={} "
						"paid_base_hp={} bonus_hp={} sun_cost={} sun_spent={} sun_before={} sun_after={} payment_succeeded={} practice_exempt={} suns_produced={} blessing_ticks={} blessing_asset_loaded={}",
						mBoard->mMainCounter, aHealed > 0 ? "healed" : (aAffordableHP == 0 ? "insufficient_sun" : "fractional_only"),
						aSet == 1 ? "critical" : "staff", GetNameString(mSeedType, mImitaterType), mBoard->mPlants.DataArrayGetID(this), static_cast<int>(mSeedType), mRow, mPlantCol,
						GetNameString(aTarget->mSeedType, aTarget->mImitaterType), mBoard->mPlants.DataArrayGetID(aTarget), static_cast<int>(aTarget->mSeedType),
						aTarget->mRow, aTarget->mPlantCol, aTarget->mX, aTarget->mY, anAudit.mHealthBefore, anAudit.mHealthAfter, anAudit.mMaxHealth,
						100.0f * anAudit.mHealthBefore / anAudit.mMaxHealth, 100.0f * anAudit.mHealthAfter / anAudit.mMaxHealth,
						anAudit.mBaseAmount, anAudit.mModifiedAmount, anAudit.mRequestedAmount, aHealed, anAudit.mOverflow, anAudit.mUnaffordable,
						anAudit.mRemainderBefore, anAudit.mRemainderAfter, aAffordableHP, SERRA_HEAL_SUN_PER_HP,
						anAudit.mPaidBaseHealing, anAudit.mBonusHealing, anAudit.mPaidBaseHealing * SERRA_HEAL_SUN_PER_HP,
						aSunBefore - mBoard->mSunMoney, aSunBefore, mBoard->mSunMoney, aPaymentSucceeded,
						mApp->mGameMode == GameMode::GAMEMODE_PLANT_PRACTICE, aProducedSuns, aTarget->mSerraBlessingTicksRemaining, gSerraDivineBlessing != nullptr);
				}
				else
				{
					PvzpLogLn("[healing] tick={} event=serra_release status={} source=\"{}\" source_id={} source_row={} source_col={} sequence={} cooldown={} sun_balance={} suns_produced={}",
						mBoard->mMainCounter, mSerraHealCooldown > 0 ? "cooldown" : "no_injured_target", GetNameString(mSeedType, mImitaterType),
						mBoard->mPlants.DataArrayGetID(this), mRow, mPlantCol, aSet == 1 ? "critical" : "staff", mSerraHealCooldown, mBoard->mSunMoney, aProducedSuns);
				}
			}
			if (--mShootingCounter == 0)
				mFrame = 0;
		}
		return;
	}

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
