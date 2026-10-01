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

#include "PlantRules.h"

namespace PlantRules
{
bool IsFumeGloomStackType(SeedType theSeedType)
{
	return theSeedType == SeedType::SEED_FUMESHROOM || theSeedType == SeedType::SEED_GLOOMSHROOM;
}

bool IsMelonPultStackType(SeedType theSeedType)
{
	return theSeedType == SeedType::SEED_MELONPULT || theSeedType == SeedType::SEED_WINTERMELON;
}

bool IsMagnetStackType(SeedType theSeedType)
{
	return theSeedType == SeedType::SEED_MAGNETSHROOM || theSeedType == SeedType::SEED_GOLD_MAGNET ||
		theSeedType == SeedType::SEED_SUN_MAGNET;
}

bool IsAutomaticPult(SeedType theSeedType)
{
	return theSeedType == SeedType::SEED_CABBAGEPULT || theSeedType == SeedType::SEED_KERNELPULT ||
		theSeedType == SeedType::SEED_MELONPULT || theSeedType == SeedType::SEED_WINTERMELON;
}
}
