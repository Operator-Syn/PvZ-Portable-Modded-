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

namespace SeedChooserOrder
{
// Stock slot 49 is SUN_MAGNET; custom plants occupy slots 50 through 52.
constexpr int EPHRAIM_INDEX = 50;
constexpr int SNIPER_FEMALE_INDEX = 51;
constexpr int SERRA_BISHOP_INDEX = 52;

inline SeedType SeedChooserTypeAtIndex(int theIndex)
{
	if (theIndex == SERRA_BISHOP_INDEX) return SeedType::SEED_SERRA_BISHOP;
	if (theIndex == SNIPER_FEMALE_INDEX) return SeedType::SEED_SNIPER_FEMALE;
	if (theIndex == EPHRAIM_INDEX) return SeedType::SEED_EPHRAIM;
	if (theIndex == 49) return SeedType::SEED_SUN_MAGNET;
	return static_cast<SeedType>(theIndex);
}

inline int SeedChooserIndexOf(SeedType theSeedType)
{
	if (theSeedType == SeedType::SEED_SERRA_BISHOP) return SERRA_BISHOP_INDEX;
	if (theSeedType == SeedType::SEED_SNIPER_FEMALE) return SNIPER_FEMALE_INDEX;
	if (theSeedType == SeedType::SEED_EPHRAIM) return EPHRAIM_INDEX;
	if (theSeedType == SeedType::SEED_SUN_MAGNET) return 49;
	return static_cast<int>(theSeedType);
}
}
