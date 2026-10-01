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

#include "ZombieStrengthRules.h"
#include "SunThresholds.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ZombieStrengthRules
{
struct ZombieStrengthTier
{
	int mSunThreshold;
	int mHealthMultiplier;
};

// Keep toughness thresholds and multipliers together: extend this table when adding future sun tiers.
constexpr std::array<ZombieStrengthTier, 10> ZOMBIE_STRENGTH_TIERS = {{
	{ 0, 1 },
	{ 100000, 5 },
	{ 200000, 10 },
	{ 500000, 8 },
	{ 700000, 12 },
	{ 800000, 12 },
	{ 1000000, 18 }, // 1.5x the 800k tier; armor uses the same multiplier.
	{ 2000000, 41 }, // 1.5x the former 27x tier; armor and type modifiers use the same scaling.
	{ TWO_AND_HALF_MILLION_SUN_THRESHOLD, 62 }, // Advances toughness and armor to the former 5m tier; all zombies receive armor.
	{ FIVE_MILLION_SUN_THRESHOLD, 62 },
}};


constexpr int QUADRATIC_RESISTANCE_SUN_THRESHOLD = 900000;
constexpr int QUADRATIC_RESISTANCE_BASE_TIER_INDEX = 5; // 800k tier immediately below resistance activation.
constexpr int QUADRATIC_RESISTANCE_BASE_CAP = 4;
constexpr int QUADRATIC_RESISTANCE_CAP_STEP = 2;

int ZombieStrengthTierForSun(int theSunAmount)
{
	int aTier = 0;
	for (size_t i = 1; i < ZOMBIE_STRENGTH_TIERS.size(); i++)
	{
		if (theSunAmount < ZOMBIE_STRENGTH_TIERS[i].mSunThreshold)
			break;
		aTier = static_cast<int>(i);
	}
	return aTier;
}

int ZombieStrengthSunForTier(int theTier)
{
	const int aClampedTier = std::clamp(theTier, 0, static_cast<int>(ZOMBIE_STRENGTH_TIERS.size()) - 1);
	return ZOMBIE_STRENGTH_TIERS[aClampedTier].mSunThreshold;
}

int ZombieHealthMultiplierForTier(int theTier)
{
	const int aClampedTier = std::clamp(theTier, 0, static_cast<int>(ZOMBIE_STRENGTH_TIERS.size()) - 1);
	return ZOMBIE_STRENGTH_TIERS[aClampedTier].mHealthMultiplier;
}

int ZombieHealthMultiplierForZombie(ZombieType theType, ZombiePhase thePhase, int theTier)
{
	const int aMultiplier = ZombieHealthMultiplierForTier(theTier);
	if ((theType == ZombieType::ZOMBIE_TRAFFIC_CONE || theType == ZombieType::ZOMBIE_PAIL ||
			theType == ZombieType::ZOMBIE_BULWARK_BUCKET) &&
		theTier >= TWO_MILLION_ZOMBIE_TIER_INDEX)
		return aMultiplier * 2;
	if (theType == ZombieType::ZOMBIE_NEWSPAPER &&
		(thePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
			thePhase == ZombiePhase::PHASE_NEWSPAPER_MAD) &&
		theTier >= TWO_MILLION_ZOMBIE_TIER_INDEX)
		return aMultiplier * 2;
	if ((theType == ZombieType::ZOMBIE_LADDER ||
		theType == ZombieType::ZOMBIE_IMP) && theTier >= 4)
		return aMultiplier * 3 / 2;
	return aMultiplier;
}

int QuadraticDamageMultiplier(int theSunAmount, int theTier, bool theExempt, int theTargetCount)
{
	const int aFullMultiplier = std::max(1, theTargetCount);
	if (theSunAmount < QUADRATIC_RESISTANCE_SUN_THRESHOLD ||
		theExempt)
		return aFullMultiplier;

	// Resistance is deliberately parameterized from the 800k effective HP baseline. Raise the cap
	// by two for every doubling of the global tier HP multiplier as toughness tiers are added.
	const int aBaselineHealth = ZombieHealthMultiplierForTier(QUADRATIC_RESISTANCE_BASE_TIER_INDEX);
	const int aCurrentHealth = ZombieHealthMultiplierForTier(theTier);
	int aCap = QUADRATIC_RESISTANCE_BASE_CAP;
	for (int64_t aScaledHealth = static_cast<int64_t>(aBaselineHealth) * 2;
		aCurrentHealth >= aScaledHealth; aScaledHealth *= 2)
		aCap += QUADRATIC_RESISTANCE_CAP_STEP;

	const int aDiminishedMultiplier = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(aFullMultiplier))));
	return std::min(aDiminishedMultiplier, aCap);
}
}
