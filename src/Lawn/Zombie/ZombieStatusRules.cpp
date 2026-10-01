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

#include "ZombieStatusRules.h"
#include "../Rules/SunThresholds.h"

namespace ZombieRules
{
float ZombieStrengthSpeedMultiplier(int theTier)
{
	return theTier > 0 ? 1.5f : 1.0f;
}

bool IsGargantuarType(ZombieType theType)
{
	return theType == ZombieType::ZOMBIE_GARGANTUAR || theType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
		theType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR;
}

bool IsBulwarkType(ZombieType theType)
{
	return theType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR || theType == ZombieType::ZOMBIE_BULWARK_BUCKET;
}

bool IsConeOrBucketZombie(ZombieType theType)
{
	return theType == ZombieType::ZOMBIE_TRAFFIC_CONE || theType == ZombieType::ZOMBIE_PAIL ||
		theType == ZombieType::ZOMBIE_BULWARK_BUCKET;
}

bool CanBeChilled(const ColdState& theState)
{
	if (theState.mType == ZombieType::ZOMBIE_ZAMBONI || theState.mHasSled)
		return false;
	if (!theState.mSpawnedByRain)
	{
		if (theState.mSunTierInvulnerable)
			return false;
		if (theState.mSunAmount >= THREE_AND_HALF_MILLION_SUN_THRESHOLD)
			return false;
		if ((IsGargantuarType(theState.mType) || IsBulwarkType(theState.mType)) &&
			theState.mSunAmount >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			return false;
		if (IsConeOrBucketZombie(theState.mType) &&
			theState.mSunAmount >= TWO_MILLION_SUN_THRESHOLD)
			return false;
		if (theState.mType == ZombieType::ZOMBIE_IMP && theState.mStrengthTier >= 4)
			return false;
	}

	if (theState.mDeadOrDying)
		return false;

	if (theState.mPhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
		theState.mPhase == ZombiePhase::PHASE_DIGGER_RISING ||
		theState.mPhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		theState.mPhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		theState.mPhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		theState.mPhase == ZombiePhase::PHASE_DANCER_RISING)
		return false;

	if (theState.mMindControlled)
		return false;

	return
		theState.mType != ZombieType::ZOMBIE_BOSS ||
		theState.mPhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT ||
		theState.mPhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT ||
		theState.mPhase == ZombiePhase::PHASE_BOSS_HEAD_SPIT;
}

bool CanBeFrozen(const ColdState& theState)
{
	if (!CanBeChilled(theState))
		return false;

	if (theState.mPhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		theState.mPhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
		theState.mPhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
		theState.mPhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
		theState.mFlying ||
		theState.mPhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
		theState.mPhase == ZombiePhase::PHASE_IMP_LANDING ||
		theState.mPhase == ZombiePhase::PHASE_BOBSLED_CRASHING ||
		theState.mPhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
		theState.mPhase == ZombiePhase::PHASE_SQUASH_RISING ||
		theState.mPhase == ZombiePhase::PHASE_SQUASH_FALLING ||
		theState.mPhase == ZombiePhase::PHASE_SQUASH_DONE_FALLING ||
		theState.mBouncingPogo)
		return false;

	return theState.mType != ZombieType::ZOMBIE_BUNGEE || theState.mPhase == ZombiePhase::PHASE_BUNGEE_AT_BOTTOM;
}
}
