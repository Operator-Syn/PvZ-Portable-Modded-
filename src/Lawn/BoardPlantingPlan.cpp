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
#include "ZenGarden.h"
#include "BoardInclude.h"
#include "LawnCommon.h"
#include "System/Music.h"
#include "System/SaveGame.h"
#include "Widget/LawnDialog.h"
#include "System/PlayerInfo.h"
#include "System/PoolEffect.h"
#include "System/TypingCheck.h"
#include "Widget/StoreScreen.h"
#include "Widget/AwardScreen.h"
#include "../PvzpLib/Trail.h"
#include "Widget/ChallengeScreen.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"

namespace BoardPlanting
{
Plant* FindTopUpgradeablePlant(Board* theBoard, int theGridX, int theGridY, SeedType theUpgrade, SeedType theBase)
{
	Plant* aTarget = nullptr;
	for (Plant* aPlant : theBoard->mPlants)
	{
		if (aPlant->mDead || aPlant->mPlantCol != theGridX || aPlant->mRow != theGridY || aPlant->NotOnGround() ||
			aPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE ||
			(theBase != SEED_NONE && aPlant->mSeedType != theBase) || !aPlant->IsUpgradableTo(theUpgrade))
			continue;
		if (aTarget == nullptr || theBoard->mPlants.DataArrayGetID(aPlant) > theBoard->mPlants.DataArrayGetID(aTarget))
			aTarget = aPlant;
	}
	return aTarget;
}

CoffeeBeanColumnPlan BuildCoffeeBeanColumnPlan(Board* theBoard, int theColumn)
{
	CoffeeBeanColumnPlan aPlan;
	for (int aRow = 0; aRow < theBoard->GetNumPlayableRows(); aRow++)
	{
		bool aLaneHasTarget = false;
		for (Zombie* aZombie : theBoard->mZombies)
		{
			if (!aZombie->mDead && !aZombie->IsDeadOrDying() && aZombie->mRow == aRow && aZombie->EffectedByDamage(127U))
			{
				aLaneHasTarget = true;
				break;
			}
		}
		aPlan.mPlanternTargetLaneCount += aLaneHasTarget ? 1 : 0;
	}

	std::vector<PlantID> aEligiblePlanterns;
	for (Plant* aPlant : theBoard->mPlants)
	{
		if (aPlant->mDead || !aPlant->IsOnBoard() || aPlant->mSquished || aPlant->mPlantCol != theColumn ||
			aPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			continue;

		PlantID aPlantID = static_cast<PlantID>(theBoard->mPlants.DataArrayGetID(aPlant));
		if (aPlant->mSeedType == SeedType::SEED_SUN_MAGNET ||
			(aPlant->mIsAsleep && aPlant->mWakeUpCounter == 0))
		{
			aPlan.mAffectedPlants.push_back(aPlantID);
		}
		else if (aPlant->mSeedType == SeedType::SEED_PLANTERN && aPlan.mPlanternTargetLaneCount > 0)
		{
			aEligiblePlanterns.push_back(aPlantID);
		}
	}

	uint64_t aRequiredProjectileCount = static_cast<uint64_t>(aEligiblePlanterns.size()) * aPlan.mPlanternTargetLaneCount;
	uint64_t aAvailableProjectileCount = theBoard->mProjectiles.mMaxSize - theBoard->mProjectiles.mSize;
	if (aRequiredProjectileCount <= aAvailableProjectileCount)
	{
		aPlan.mPlanterns = std::move(aEligiblePlanterns);
		aPlan.mAffectedPlants.insert(aPlan.mAffectedPlants.end(), aPlan.mPlanterns.begin(), aPlan.mPlanterns.end());
	}
	return aPlan;
}

bool IsCoffeeBeanChargeableByPlant(Board* theBoard)
{
	return theBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
		!theBoard->mApp->mEasyPlantingCheat && !theBoard->HasConveyorBeltSeedBank();
}

int64_t CoffeeBeanColumnSunCost(Board* theBoard, const CoffeeBeanColumnPlan& thePlan)
{
	if (thePlan.mAffectedPlants.empty())
		return std::numeric_limits<int64_t>::max();

	int64_t aSunCost = 0;
	if (IsCoffeeBeanChargeableByPlant(theBoard))
		aSunCost += static_cast<int64_t>(thePlan.mAffectedPlants.size()) *
			theBoard->GetCurrentPlantCost(SeedType::SEED_INSTANT_COFFEE, SeedType::SEED_NONE);
	int aSunCostPerLane = theBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 1000 : 500;
	aSunCost += static_cast<int64_t>(thePlan.mPlanterns.size()) * thePlan.mPlanternTargetLaneCount * aSunCostPerLane;
	return aSunCost;
}

bool CoffeeBeanIsInChosenSeedBank(Board* theBoard)
{
	if (theBoard->mSeedBank == nullptr || theBoard->HasConveyorBeltSeedBank() || theBoard->mApp->IsSlotMachineLevel())
		return false;
	for (int i = 0; i < theBoard->mSeedBank->mNumPackets; i++)
	{
		const SeedPacket& aPacket = theBoard->mSeedBank->mSeedPackets[i];
		if (aPacket.mPacketType == SeedType::SEED_INSTANT_COFFEE ||
			(aPacket.mPacketType == SeedType::SEED_IMITATER && aPacket.mImitaterType == SeedType::SEED_INSTANT_COFFEE))
			return true;
	}
	return false;
}

void AutomaticallyCoffeeBeanPlanterns(Board* theBoard)
{
	int64_t aRemainingSun = static_cast<int64_t>(theBoard->mSunMoney) + theBoard->CountSunBeingCollected();
	int64_t aShadowSunMoney = theBoard->mSunMoney;
	int aBeanCost = theBoard->mApp->mEasyPlantingCheat ? 0 :
		theBoard->GetCurrentPlantCost(SeedType::SEED_INSTANT_COFFEE, SeedType::SEED_NONE);
	for (int aColumn = 0; aColumn < theBoard->GetNumPlayableColumns(); aColumn++)
	{
		CoffeeBeanColumnPlan aPlan = BuildCoffeeBeanColumnPlan(theBoard, aColumn);
		if (aPlan.mPlanterns.empty() || aPlan.mPlanternTargetLaneCount <= 0)
			continue;

		std::vector<int> aPlanternCosts;
		int64_t aColumnAvailableSun = aRemainingSun;
		int64_t aColumnShadowSunMoney = aShadowSunMoney;
		for (size_t i = 0; i < aPlan.mPlanterns.size(); i++)
		{
			int aSunCostPerLane = aColumnShadowSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 1000 : 500;
			int64_t aPlanternCost = static_cast<int64_t>(aBeanCost) +
				static_cast<int64_t>(aPlan.mPlanternTargetLaneCount) * aSunCostPerLane;
			if (aPlanternCost <= 0 || aColumnAvailableSun < aPlanternCost ||
				aPlanternCost > std::numeric_limits<int>::max())
				break;
			aPlanternCosts.push_back(static_cast<int>(aPlanternCost));
			aColumnAvailableSun -= aPlanternCost;
			aColumnShadowSunMoney -= aPlanternCost;
		}
		if (aPlanternCosts.empty())
			continue;

		int aFiredPlanternCount = 0;
		int64_t aFiredSunCost = 0;
		for (size_t i = 0; i < aPlanternCosts.size(); i++)
		{
			Plant* aPlantern = theBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(aPlan.mPlanterns[i]));
			if (aPlantern == nullptr || aPlantern->mDead || !aPlantern->PlanternCoffeeBeanVolley(true))
				break;
			++aFiredPlanternCount;
			aFiredSunCost += aPlanternCosts[i];
		}
		if (aFiredPlanternCount > 0 && aFiredSunCost <= std::numeric_limits<int>::max() &&
			theBoard->CanTakeSunMoney(static_cast<int>(aFiredSunCost)) &&
			theBoard->TakeSunMoney(static_cast<int>(aFiredSunCost)))
		{
			aRemainingSun -= aFiredSunCost;
			aShadowSunMoney = theBoard->mSunMoney;
		}
	}
}

}
