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

#include "../ConstEnums.h"
#include <vector>

class Board;
class Plant;

namespace BoardPlanting
{
struct CoffeeBeanColumnPlan
{
	std::vector<PlantID> mAffectedPlants;
	std::vector<PlantID> mPlanterns;
	int mPlanternTargetLaneCount = 0;
};


Plant* FindTopUpgradeablePlant(Board* theBoard, int theGridX, int theGridY, SeedType theUpgrade, SeedType theBase = SEED_NONE);
CoffeeBeanColumnPlan BuildCoffeeBeanColumnPlan(Board* theBoard, int theColumn);
bool IsCoffeeBeanChargeableByPlant(Board* theBoard);
int64_t CoffeeBeanColumnSunCost(Board* theBoard, const CoffeeBeanColumnPlan& thePlan);
bool CoffeeBeanIsInChosenSeedBank(Board* theBoard);
void AutomaticallyCoffeeBeanPlanterns(Board* theBoard);
}
