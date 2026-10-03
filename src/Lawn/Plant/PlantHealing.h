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

class Board;
class Plant;

namespace PlantHealing
{
struct HealingAudit
{
	float mBaseAmount = 0.0f;
	float mModifiedAmount = 0.0f;
	float mRequestedAmount = 0.0f;
	float mOverflow = 0.0f;
	float mUnaffordable = 0.0f;
	float mRemainderBefore = 0.0f;
	float mRemainderAfter = 0.0f;
	int mHealthBefore = 0;
	int mHealthAfter = 0;
	int mMaxHealth = 0;
	int mPaidBaseHealing = 0;
	int mBonusHealing = 0;
};

bool PlantCanRegenerate(Plant* thePlant);
void HealPlant(Board* theBoard, Plant* thePlant, float theBaseAmount);
// The bounded overload budgets base HP; healing bonuses do not consume the budget.
int HealPlant(Board* theBoard, Plant* thePlant, float theBaseAmount, int theMaxBaseHealing, HealingAudit* theAudit = nullptr);
void ApplyPlantHealthRate(Plant* thePlant, float theHealthPerSecond);
}
