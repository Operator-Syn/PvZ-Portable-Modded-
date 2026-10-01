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

namespace ZombieStrengthRules
{
constexpr int TWO_MILLION_ZOMBIE_TIER_INDEX = 7;
constexpr int TWO_AND_HALF_MILLION_ZOMBIE_TIER_INDEX = 8;

int ZombieStrengthTierForSun(int theSunAmount);
int ZombieStrengthSunForTier(int theTier);
int ZombieHealthMultiplierForTier(int theTier);
int ZombieHealthMultiplierForZombie(ZombieType theType, ZombiePhase thePhase, int theTier);
int QuadraticDamageMultiplier(int theSunAmount, int theTier, bool theExempt, int theTargetCount);
}
