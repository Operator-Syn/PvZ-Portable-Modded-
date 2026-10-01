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

int Board::GetQuadraticZombieDamageMultiplier(const Zombie* theZombie, int theTargetCount) const
{
	return ZombieStrengthRules::QuadraticDamageMultiplier(mZombieTierSunMoney, mZombieStrengthTier,
		theZombie == nullptr || theZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE, theTargetCount);
}

void Board::ApplyZombieStrengthTierToZombie(Zombie* theZombie, int theFromTier, int theToTier)
{
	if (theZombie == nullptr || theFromTier == theToTier)
		return;

	int aOldMultiplier = ZombieStrengthRules::ZombieHealthMultiplierForZombie(theZombie->mZombieType, theZombie->mZombiePhase, theFromTier);
	int aNewMultiplier = ZombieStrengthRules::ZombieHealthMultiplierForZombie(theZombie->mZombieType, theZombie->mZombiePhase, theToTier);
	bool anImpArmorAdded = false;
	if (theZombie->mZombieType == ZombieType::ZOMBIE_IMP && theFromTier < 4 && theToTier >= 4 &&
		theZombie->mHelmType == HelmType::HELMTYPE_NONE)
	{
		theZombie->mHelmType = HelmType::HELMTYPE_PAIL;
		theZombie->mHelmHealth = 1100;
		theZombie->mHelmMaxHealth = 1100;
		anImpArmorAdded = true;
		theZombie->mChilledCounter = 0;
		if (theZombie->mIceTrapCounter > 0)
			theZombie->RemoveIceTrap();
		if (theZombie->mButteredCounter > 0)
		{
			theZombie->mButteredCounter = 0;
			theZombie->RemoveButter();
		}
	}
	if (theToTier >= 3 && theZombie->mButteredCounter > 0)
	{
		bool anIsGargantuar = theZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR ||
			theZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
			theZombie->mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR;
		if (anIsGargantuar)
		{
			theZombie->mButteredCounter = 0;
			theZombie->RemoveButter();
		}
		else
		{
			theZombie->mButteredCounter = std::min(theZombie->mButteredCounter, 200);
		}
	}
	bool anImpArmorRemoved = theZombie->mZombieType == ZombieType::ZOMBIE_IMP && theFromTier >= 4 && theToTier < 4 &&
		theZombie->mHelmType == HelmType::HELMTYPE_PAIL;
	bool aHasTierBucketArmor = theZombie->mTierBucketArmorMaxHealth > 0 || theZombie->mTierBucketArmorHealth > 0;
	bool aBucketArmorAdded = theZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && !aHasTierBucketArmor &&
		((theFromTier < 5 && theToTier >= 5 && theZombie->mHelmType != HelmType::HELMTYPE_PAIL) ||
		theToTier >= ZombieStrengthRules::TWO_AND_HALF_MILLION_ZOMBIE_TIER_INDEX);
	bool aUniversalArmorRemoved = theFromTier >= ZombieStrengthRules::TWO_AND_HALF_MILLION_ZOMBIE_TIER_INDEX &&
		theToTier < ZombieStrengthRules::TWO_AND_HALF_MILLION_ZOMBIE_TIER_INDEX &&
		theZombie->mHelmType == HelmType::HELMTYPE_PAIL;
	bool aBucketArmorRemoved = (theZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && theFromTier >= 5 && theToTier < 5) ||
		aUniversalArmorRemoved;
	auto aScaleHealth = [aOldMultiplier, aNewMultiplier](int32_t& theHealth)
	{
		if (theHealth <= 0)
			return;
		int64_t aScaledHealth = (static_cast<int64_t>(theHealth) * aNewMultiplier + aOldMultiplier / 2) / aOldMultiplier;
		theHealth = static_cast<int32_t>(std::clamp<int64_t>(aScaledHealth, 1, std::numeric_limits<int32_t>::max()));
	};
	aScaleHealth(theZombie->mBodyHealth);
	aScaleHealth(theZombie->mBodyMaxHealth);
	if (anImpArmorAdded)
	{
		auto aScaleNewArmor = [aNewMultiplier](int32_t& theHealth)
		{
			int64_t aScaledHealth = static_cast<int64_t>(theHealth) * aNewMultiplier;
			theHealth = static_cast<int32_t>(std::clamp<int64_t>(aScaledHealth, 1, std::numeric_limits<int32_t>::max()));
		};
		aScaleNewArmor(theZombie->mHelmHealth);
		aScaleNewArmor(theZombie->mHelmMaxHealth);
	}
	else if (anImpArmorRemoved)
	{
		theZombie->mHelmType = HelmType::HELMTYPE_NONE;
		theZombie->mHelmHealth = 0;
		theZombie->mHelmMaxHealth = 0;
	}
	else
	{
		aScaleHealth(theZombie->mHelmHealth);
		aScaleHealth(theZombie->mHelmMaxHealth);
	}
	aScaleHealth(theZombie->mShieldHealth);
	aScaleHealth(theZombie->mShieldMaxHealth);
	aScaleHealth(theZombie->mFlyingHealth);
	aScaleHealth(theZombie->mFlyingMaxHealth);
	if (aBucketArmorAdded)
	{
		theZombie->mTierBucketArmorMaxHealth = 1100 * aNewMultiplier;
		theZombie->mTierBucketArmorHealth = theZombie->mTierBucketArmorMaxHealth;
	}
	else if (aBucketArmorRemoved)
	{
		theZombie->mTierBucketArmorHealth = 0;
		theZombie->mTierBucketArmorMaxHealth = 0;
	}
	else if (theFromTier >= 5 && theToTier >= 5)
	{
		aScaleHealth(theZombie->mTierBucketArmorHealth);
		aScaleHealth(theZombie->mTierBucketArmorMaxHealth);
	}
	if (theZombie->IsSunTierInvulnerable())
	{
		bool aRemovedEffect = theZombie->mChilledCounter > 0 || theZombie->mIceTrapCounter > 0 ||
			theZombie->mButteredCounter > 0;
		theZombie->mChilledCounter = 0;
		if (theZombie->mIceTrapCounter > 0)
			theZombie->RemoveIceTrap();
		if (theZombie->mButteredCounter > 0)
		{
			theZombie->mButteredCounter = 0;
			theZombie->RemoveButter();
		}
		if (aRemovedEffect)
			theZombie->UpdateAnimSpeed();
	}
	ApplyThreeMillionSunDurabilityToZombie(theZombie, mZombieTierSunMoney >= THREE_MILLION_SUN_THRESHOLD);
}

void Board::ApplyThreeMillionSunDurabilityToZombie(Zombie* theZombie, bool theApply)
{
	if (theZombie == nullptr || theZombie->mThreeMillionSunDurabilityApplied == theApply)
		return;

	theZombie->mThreeMillionSunDurabilityApplied = theApply;
	if (theZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
		return;

	int aOldMultiplier = theApply ? 2 : 3;
	int aNewMultiplier = theApply ? 3 : 2;
	auto aScaleHealth = [aOldMultiplier, aNewMultiplier](int32_t& theHealth)
	{
		if (theHealth <= 0)
			return;
		int64_t aScaledHealth = (static_cast<int64_t>(theHealth) * aNewMultiplier + aOldMultiplier / 2) / aOldMultiplier;
		theHealth = static_cast<int32_t>(std::clamp<int64_t>(aScaledHealth, 1, std::numeric_limits<int32_t>::max()));
	};
	aScaleHealth(theZombie->mBodyHealth);
	aScaleHealth(theZombie->mBodyMaxHealth);
	aScaleHealth(theZombie->mHelmHealth);
	aScaleHealth(theZombie->mHelmMaxHealth);
	aScaleHealth(theZombie->mShieldHealth);
	aScaleHealth(theZombie->mShieldMaxHealth);
	aScaleHealth(theZombie->mFlyingHealth);
	aScaleHealth(theZombie->mFlyingMaxHealth);
	aScaleHealth(theZombie->mTierBucketArmorHealth);
	aScaleHealth(theZombie->mTierBucketArmorMaxHealth);
}

int Board::GetZombieExplosiveDamage() const
{
	constexpr int aBaseExplosionDamage = 1800;
	const int aHealthMultiplier = ZombieStrengthRules::ZombieHealthMultiplierForTier(mZombieStrengthTier);
	constexpr int aMaximumDamageReductionPercent = 75;
	constexpr int aMinimumExplosionDamage = aBaseExplosionDamage * (100 - aMaximumDamageReductionPercent) / 100;
	return std::max(aMinimumExplosionDamage,
		static_cast<int>(std::lround(aBaseExplosionDamage / std::sqrt(static_cast<double>(aHealthMultiplier)))));
}
