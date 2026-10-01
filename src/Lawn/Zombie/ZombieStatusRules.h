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

#pragma once

#include "../../ConstEnums.h"

namespace ZombieRules
{
bool IsGargantuarType(ZombieType theType);
bool IsBulwarkType(ZombieType theType);
bool IsConeOrBucketZombie(ZombieType theType);
float ZombieStrengthSpeedMultiplier(int theTier);

// Snapshot of the gameplay state needed for cold eligibility, with no asset or app dependency.
struct ColdState
{
	ZombieType mType = ZOMBIE_NORMAL;
	ZombiePhase mPhase = PHASE_ZOMBIE_NORMAL;
	int mSunAmount = 0;
	int mStrengthTier = 0;
	bool mHasSled = false;
	bool mSpawnedByRain = false;
	bool mSunTierInvulnerable = false;
	bool mDeadOrDying = false;
	bool mMindControlled = false;
	bool mFlying = false;
	bool mBouncingPogo = false;
};

bool CanBeChilled(const ColdState& theState);
bool CanBeFrozen(const ColdState& theState);
}
