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

#include "PlantingRules.h"

PlantingReason PlantingRules::ValidateCell(int theColumn, int theRow, int theColumnCount, int theRowCount, SeedType theSeedType)
{
	if (theSeedType == SEED_CHOMPERNUT || theColumn < 0 || theColumn >= theColumnCount ||
		theRow < 0 || theRow >= theRowCount)
		return PLANTING_NOT_HERE;
	return PLANTING_OK;
}
