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
#include "../Modes/ZenGarden.h"
#include "BoardInclude.h"
#include "../Entities/LawnCommon.h"
#include "../System/Music.h"
#include "../System/SaveGame.h"
#include "../Widget/LawnDialog.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/TypingCheck.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/AwardScreen.h"
#include "../../PvzpLib/Trail.h"
#include "../Widget/ChallengeScreen.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "../Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "../Zombie/ZombieStrengthRules.h"
#include "../Plant/PlantRules.h"
#include "../Rules/PlantingRules.h"





GridItem* Board::GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY)
{
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY && aGridItem->mGridItemType == theGridItemType)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

GridItem* Board::GetRake()
{
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_RAKE)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

GridItem* Board::GetCraterAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_CRATER, theGridX, theGridY);
}

GridItem* Board::GetGraveStoneAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_GRAVESTONE, theGridX, theGridY);
}

GridItem* Board::GetLadderAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_LADDER, theGridX, theGridY);
}

GridItem* Board::GetScaryPotAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_SCARY_POT, theGridX, theGridY);
}

GridItem* Board::GetZenToolAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_ZEN_TOOL, theGridX, theGridY);
}

bool Board::CanAddGraveStoneAt(int theGridX, int theGridY)
{
	if (mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_GRASS && mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		return false;
	}

	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY)
		{
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_CRATER ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
				return false;
		}
	}
	return true;
}

GridItem* Board::AddALadder(int theGridX, int theGridY)
{
	GridItem* aLadder = mGridItems.DataArrayAlloc();
	aLadder->mGridItemType = GridItemType::GRIDITEM_LADDER;
	aLadder->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, theGridY, 800);
	aLadder->mGridX = theGridX;
	aLadder->mGridY = theGridY;
	return aLadder;
}

GridItem* Board::AddACrater(int theGridX, int theGridY)
{
	GridItem* aCrater = mGridItems.DataArrayAlloc();
	aCrater->mGridItemType = GridItemType::GRIDITEM_CRATER;
	aCrater->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, theGridY, 1);
	aCrater->mGridX = theGridX;
	aCrater->mGridY = theGridY;
	return aCrater;
}

GridItem* Board::AddAGraveStone(int theGridX, int theGridY)
{
	GridItem* aGraveStone = mGridItems.DataArrayAlloc();
	aGraveStone->mGridItemType = GridItemType::GRIDITEM_GRAVESTONE;
	aGraveStone->mGridItemCounter = -Rand(50);
	aGraveStone->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, theGridY, 3);
	aGraveStone->mGridX = theGridX;
	aGraveStone->mGridY = theGridY;
	return aGraveStone;
}

void Board::AddGraveStones(int theGridX, int theCount, MTRand& theLevelRNG)
{
	PVZP_ASSERT(theCount <= MAX_GRID_SIZE_Y);

	// Clamp theCount to the number of squares that can hold a grave stone, otherwise the loop below would never terminate
	//GridItem* aGridItem = nullptr;
	//bool aAllowGraveStone[MAX_GRID_SIZE_Y] = { false };
	int aGridAllowGraveStonesCount = 0;
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		if (CanAddGraveStoneAt(theGridX, y))
		{
			aGridAllowGraveStonesCount++;
		}
	}
	theCount = std::min(theCount, aGridAllowGraveStonesCount);

	int i = 0;
	while (i < theCount)
	{
		int aGridY = theLevelRNG.Next((unsigned long)MAX_GRID_SIZE_Y);
		//if (aAllowGraveStone[aGridY])
		//{
		//	aAllowGraveStone[aGridY] = false;
		//	GridItem* aGraveStone = AddAGraveStone(theGridX, aGridY);
		//	++i;
		//}
		// re-check each time instead of a cached allowance array, which could go stale if AddAGraveStone() changes
		if (CanAddGraveStoneAt(theGridX, aGridY))
		{
			AddAGraveStone(theGridX, aGridY);
			++i;
		}
	}
}

bool Board::IsPoolSquare(int theGridX, int theGridY)
{
	if (theGridX >= 0 && theGridX < GetNumPlayableColumns() && theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y)
	{
		return mGridSquareType[theGridX][theGridY] == GridSquareType::GRIDSQUARE_POOL;
	}
	return false;
}

Plant* Board::NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	Plant* aPlant = mPlants.DataArrayAlloc();
	aPlant->mIsOnBoard = true;
	aPlant->PlantInitialize(theGridX, theGridY, theSeedType, theImitaterType);
	return aPlant;
}

void Board::DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant)
{
	int aXPos = GridToPixelX(theGridX, theGridY) + 41;
	int aYPos = GridToPixelY(theGridX, theGridY) + 74;
	if (thePlant)
	{
		if (thePlant->mSeedType == SeedType::SEED_LILYPAD)
		{
			aYPos += 15;
		}
		else if (thePlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			aYPos += 30;
		}
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
	{
		mApp->PlayFoley(FoleyType::FOLEY_CERAMIC);
		return;
	}
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		return;
	}
	if (Plant::IsFlying(thePlant->mSeedType))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		return;
	}

	if (IsPoolSquare(theGridX, theGridY))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		mApp->AddPvzpParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING_POOL);
	}
	else
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		mApp->AddPvzpParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING);
	}
}

Plant* Board::AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	Plant* aPlantToFuse = GetTopPlantAt(theGridX, theGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
	bool aFuseChomperNut = aPlantToFuse != nullptr &&
		((theSeedType == SeedType::SEED_CHOMPER && aPlantToFuse->mSeedType == SeedType::SEED_TALLNUT) ||
		 (theSeedType == SeedType::SEED_TALLNUT && aPlantToFuse->mSeedType == SeedType::SEED_CHOMPER));
	int aFusedHealth = 0;
	int aFusedMaxHealth = 0;
	PlantState aFusedChomperState = PlantState::STATE_READY;
	int aFusedChomperCountdown = 0;
	ZombieID aFusedTargetZombieID = ZombieID::ZOMBIEID_NULL;
	if (aFuseChomperNut)
	{
		int aIncomingMaxHealth = theSeedType == SeedType::SEED_TALLNUT
			? (mTallNutOverdriveActive ? 10000 : 8000) : 500;
		aFusedHealth = aPlantToFuse->mPlantHealth + aIncomingMaxHealth;
		aFusedMaxHealth = aPlantToFuse->mPlantMaxHealth + aIncomingMaxHealth;
		if (aPlantToFuse->IsChomper())
		{
			aFusedChomperState = aPlantToFuse->mState;
			aFusedChomperCountdown = aPlantToFuse->mStateCountdown;
			aFusedTargetZombieID = aPlantToFuse->mTargetZombieID;
		}
		aPlantToFuse->Die();
		theSeedType = SeedType::SEED_CHOMPERNUT;
		theImitaterType = SeedType::SEED_NONE;
	}
	Plant* aPlant = NewPlant(theGridX, theGridY, theSeedType, theImitaterType);
	if (aFuseChomperNut)
	{
		aPlant->mPlantHealth = aFusedHealth;
		aPlant->mPlantMaxHealth = aFusedMaxHealth;
		aPlant->mState = aFusedChomperState;
		aPlant->mStateCountdown = aFusedChomperCountdown;
		aPlant->mTargetZombieID = aFusedTargetZombieID;
		if (aFusedChomperState == PlantState::STATE_CHOMPER_BITING || aFusedChomperState == PlantState::STATE_CHOMPER_BITING_MISSED)
			aPlant->PlayBodyReanim("anim_bite", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
		else if (aFusedChomperState == PlantState::STATE_CHOMPER_DIGESTING || aFusedChomperState == PlantState::STATE_CHOMPER_BITING_GOT_ONE)
			aPlant->PlayBodyReanim("anim_chew", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		else if (aFusedChomperState == PlantState::STATE_CHOMPER_SWALLOWING)
			aPlant->PlayBodyReanim("anim_swallow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
	}
	DoPlantingEffects(theGridX, theGridY, aPlant);
	mChallenge->PlantAdded(aPlant);

	int aSunPlantsCount = CountPlantByType(SeedType::SEED_SUNSHROOM) + CountPlantByType(SeedType::SEED_SUNFLOWER);
	if (aSunPlantsCount > mMaxSunPlants)
	{
		mMaxSunPlants = aSunPlantsCount;  //mMaxSunPlants = max(aSunPlantsCount, mMaxSunPlants);
	}

	if (theSeedType == SeedType::SEED_PEASHOOTER ||
		theSeedType == SeedType::SEED_SNOWPEA ||
		theSeedType == SeedType::SEED_REPEATER ||
		theSeedType == SeedType::SEED_THREEPEATER ||
		theSeedType == SeedType::SEED_SPLITPEA ||
		theSeedType == SeedType::SEED_GATLINGPEA)
	{
		mPeaShooterUsed = true;
	}
	if (theSeedType == SeedType::SEED_CABBAGEPULT ||
		theSeedType == SeedType::SEED_KERNELPULT ||
		theSeedType == SeedType::SEED_MELONPULT ||
		theSeedType == SeedType::SEED_WINTERMELON)
	{
		mCatapultPlantsUsed = true;
	}

	bool aIsFungi = Plant::IsFungus(theSeedType);
	if (!Plant::IsFlying(theSeedType) && !aIsFungi) {
		mMushroomAndCoffeeBeansOnly = false;
	}
	if (aIsFungi) {
		mMushroomsUsed = true;
	}

	return aPlant;
}

Plant* Board::GetPumpkinAt(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mPlantCol == theGridX && aPlant->mRow == theGridY && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			return aPlant;
		}
	}
	return nullptr;
}

Plant* Board::GetFlowerPotAt(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mPlantCol == theGridX && aPlant->mRow == theGridY && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			return aPlant;
		}
	}
	return nullptr;
}

void Board::GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn)
{
	thePlantOnLawn->mUnderPlant = nullptr;
	thePlantOnLawn->mPumpkinPlant = nullptr;
	thePlantOnLawn->mFlyingPlant = nullptr;
	thePlantOnLawn->mNormalPlant = nullptr;
	thePlantOnLawn->mSunflowerCount = 0;
	thePlantOnLawn->mFumeGloomCount = 0;
	thePlantOnLawn->mCatTailCount = 0;
	thePlantOnLawn->mMelonPultCount = 0;
	thePlantOnLawn->mSunMagnetPlant = nullptr;
	thePlantOnLawn->mMagnetCount = 0;

	if (theGridX < 0 || theGridX >= GetNumPlayableColumns() || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return;

	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow != theGridY)
		{
			continue;
		}
		SeedType aSeedType = aPlant->mSeedType;
		if (aSeedType == SeedType::SEED_IMITATER && aPlant->mImitaterType != SeedType::SEED_NONE)
		{
			aSeedType = aPlant->mImitaterType;
		}

		if (aSeedType == SeedType::SEED_COBCANNON)
		{
			if (aPlant->mPlantCol < theGridX - 1 || aPlant->mPlantCol > theGridX)
			{
				continue;
			}
		}
		else
		{
			if (aPlant->mPlantCol != theGridX)
			{
				continue;
			}
		}
		if (aPlant->NotOnGround())
		{
			continue;
		}

		if (Plant::IsFlying(aSeedType))
		{
			PVZP_ASSERT(!thePlantOnLawn->mFlyingPlant);
			thePlantOnLawn->mFlyingPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_FLOWERPOT || (aSeedType == SeedType::SEED_LILYPAD && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN))
		{
			PVZP_ASSERT(!thePlantOnLawn->mUnderPlant);
			thePlantOnLawn->mUnderPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			PVZP_ASSERT(!thePlantOnLawn->mPumpkinPlant);
			thePlantOnLawn->mPumpkinPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_TWINSUNFLOWER)
		{
			++thePlantOnLawn->mSunflowerCount;
			if (thePlantOnLawn->mNormalPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mNormalPlant))
				thePlantOnLawn->mNormalPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_MAGNETSHROOM || aSeedType == SeedType::SEED_GOLD_MAGNET ||
			aSeedType == SeedType::SEED_SUN_MAGNET)
		{
			++thePlantOnLawn->mMagnetCount;
			if (thePlantOnLawn->mNormalPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mNormalPlant))
				thePlantOnLawn->mNormalPlant = aPlant;
			if (aSeedType == SeedType::SEED_SUN_MAGNET && (thePlantOnLawn->mSunMagnetPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mSunMagnetPlant)))
				thePlantOnLawn->mSunMagnetPlant = aPlant;
		}
		else if (PlantRules::IsFumeGloomStackType(aSeedType))
		{
			++thePlantOnLawn->mFumeGloomCount;
			if (thePlantOnLawn->mNormalPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mNormalPlant))
				thePlantOnLawn->mNormalPlant = aPlant;
		}
		else if (aSeedType == SeedType::SEED_CATTAIL)
		{
			++thePlantOnLawn->mCatTailCount;
			if (thePlantOnLawn->mNormalPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mNormalPlant))
				thePlantOnLawn->mNormalPlant = aPlant;
		}
		else if (PlantRules::IsMelonPultStackType(aSeedType))
		{
			++thePlantOnLawn->mMelonPultCount;
			if (thePlantOnLawn->mNormalPlant == nullptr ||
				mPlants.DataArrayGetID(aPlant) > mPlants.DataArrayGetID(thePlantOnLawn->mNormalPlant))
				thePlantOnLawn->mNormalPlant = aPlant;
		}
		else
		{
			PVZP_ASSERT(!thePlantOnLawn->mNormalPlant);
			thePlantOnLawn->mNormalPlant = aPlant;
		}
	}
}

Plant* Board::GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority)
{
	if (theGridX < 0 || theGridX >= GetNumPlayableColumns() || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return nullptr;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return nullptr;

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);

	switch (thePriority)
	{
	case PlantPriority::TOPPLANT_EATING_ORDER:
		if (aPlantOnLawn.mPumpkinPlant)							return aPlantOnLawn.mPumpkinPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_DIGGING_ORDER:
		if (aPlantOnLawn.mNormalPlant)							return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_BUNGEE_ORDER:
	case PlantPriority::TOPPLANT_CATAPULT_ORDER:
	case PlantPriority::TOPPLANT_ANY:
		if (aPlantOnLawn.mFlyingPlant)							return aPlantOnLawn.mFlyingPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else if (aPlantOnLawn.mPumpkinPlant)					return aPlantOnLawn.mPumpkinPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_ZEN_TOOL_ORDER:
		if (aPlantOnLawn.mFlyingPlant)							return aPlantOnLawn.mFlyingPlant;
		else if (aPlantOnLawn.mPumpkinPlant)					return aPlantOnLawn.mPumpkinPlant;
		else if (aPlantOnLawn.mNormalPlant)						return aPlantOnLawn.mNormalPlant;
		else													return aPlantOnLawn.mUnderPlant;
	case PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION:			return aPlantOnLawn.mNormalPlant;
	case PlantPriority::TOPPLANT_ONLY_FLYING:					return aPlantOnLawn.mFlyingPlant;
	case PlantPriority::TOPPLANT_ONLY_PUMPKIN:					return aPlantOnLawn.mPumpkinPlant;
	case PlantPriority::TOPPLANT_ONLY_UNDER_PLANT:				return aPlantOnLawn.mUnderPlant;
	default:													PVZP_ASSERT(false);
	}
	unreachable();
}

int Board::CountSunFlowers()
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->MakesSun())
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountPlantByType(SeedType theSeedType)
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == theSeedType ||
			(theSeedType == SeedType::SEED_CHOMPER && aPlant->mSeedType == SeedType::SEED_CHOMPERNUT) ||
			(theSeedType == SeedType::SEED_TALLNUT && aPlant->mSeedType == SeedType::SEED_CHOMPERNUT))
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountEmptyPotsOrLilies(SeedType theSeedType)
{
	int aCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == theSeedType && !GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION))
		{
			aCount++;
		}
	}
	return aCount;
}

bool Board::IsValidCobCannonSpotHelper(int theGridX, int theGridY)
{
	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (aPlantOnLawn.mPumpkinPlant)
		return false;

	if (aPlantOnLawn.mNormalPlant && aPlantOnLawn.mNormalPlant->mSeedType == SeedType::SEED_KERNELPULT)
		return true;

	return mApp->mEasyPlantingCheat && CanPlantAt(theGridX, theGridY, SeedType::SEED_KERNELPULT) == PlantingReason::PLANTING_OK;
}

bool Board::IsValidCobCannonSpot(int theGridX, int theGridY)
{
	if (!IsValidCobCannonSpotHelper(theGridX, theGridY) || !IsValidCobCannonSpotHelper(theGridX + 1, theGridY))
		return false;

	return !GetFlowerPotAt(theGridX, theGridY) == !GetFlowerPotAt(theGridX + 1, theGridY);
}

bool Board::HasValidCobCannonSpot()
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_KERNELPULT && IsValidCobCannonSpot(aPlant->mPlantCol, aPlant->mRow))
		{
			return true;
		}
	}
	return false;
}

Projectile* Board::AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType)
{
	if (mProjectiles.mSize >= mProjectiles.mMaxSize)
	{
		PvzpAssertFailed("mProjectiles.mSize < mProjectiles.mMaxSize", __FILE__, __LINE__,
			"Projectile pool exhausted: live {}/{}, high-water slots {}, free-list head {}",
			mProjectiles.mSize, mProjectiles.mMaxSize, mProjectiles.mMaxUsedCount, mProjectiles.mFreeListHead);
		std::abort();
	}
	Projectile* aProjectile = mProjectiles.DataArrayAlloc();
	if (aProjectile == nullptr)
		return nullptr;
	aProjectile->ProjectileInitialize(theX, theY, theRenderOrder, theRow, theProjectileType);
	return aProjectile;
}

bool Board::IsIceAt(int theGridX, int theGridY)
{
	PVZP_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mIceTimer[theGridY] == 0 || mIceMinX[theGridY] > 750)
		return false;

	return theGridX >= PixelToGridXKeepOnBoard(mIceMinX[theGridY] + 12, 0);
}











PlantingReason Board::CanPlantAt(int theGridX, int theGridY, SeedType theSeedType)
{
	PlantingReason aCellReason = PlantingRules::ValidateCell(theGridX, theGridY, GetNumPlayableColumns(), MAX_GRID_SIZE_Y, theSeedType);
	if (aCellReason != PlantingReason::PLANTING_OK)
		return aCellReason;

	PlantingReason aReason = mChallenge->CanPlantAt(theGridX, theGridY, theSeedType);
	if (aReason != PlantingReason::PLANTING_OK || Challenge::IsZombieSeedType(theSeedType))
	{
		return aReason;
	}

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (aPlantOnLawn.mUnderPlant || aPlantOnLawn.mPumpkinPlant || aPlantOnLawn.mFlyingPlant || aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		if (mApp->mZenGarden->mGardenType == GARDEN_AQUARIUM && !Plant::IsAquatic(theSeedType))
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}

		return PlantingReason::PLANTING_OK;
	}

	bool aHasGrave = GetGraveStoneAt(theGridX, theGridY);
	if (theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		if (aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}

		return aHasGrave ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_ONLY_ON_GRAVES;
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		if (aPlantOnLawn.mFlyingPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		BoardPlanting::CoffeeBeanColumnPlan aPlan = BoardPlanting::BuildCoffeeBeanColumnPlan(this, theGridX);
		bool aClickedPlantIsEligible = aPlantOnLawn.mSunMagnetPlant != nullptr;
		if (aPlantOnLawn.mNormalPlant != nullptr)
		{
			Plant* aClickedPlant = aPlantOnLawn.mNormalPlant;
			aClickedPlantIsEligible = aClickedPlantIsEligible ||
				(aClickedPlant->mIsAsleep && aClickedPlant->mWakeUpCounter == 0 &&
				 aClickedPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE);
			PlantID aClickedPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(aClickedPlant));
			aClickedPlantIsEligible = aClickedPlantIsEligible ||
				std::find(aPlan.mPlanterns.begin(), aPlan.mPlanterns.end(), aClickedPlantID) != aPlan.mPlanterns.end();
		}
		if (!aClickedPlantIsEligible)
		{
			if (aPlantOnLawn.mNormalPlant != nullptr && aPlantOnLawn.mNormalPlant->mSeedType == SeedType::SEED_PLANTERN)
				return PlantingReason::PLANTING_NOT_HERE;
			return PlantingReason::PLANTING_NEEDS_SLEEPING;
		}
		int64_t aRequiredSun = BoardPlanting::CoffeeBeanColumnSunCost(this, aPlan);
		if (aRequiredSun > std::numeric_limits<int>::max() || !CanTakeSunMoney(static_cast<int>(aRequiredSun)))
			return PlantingReason::PLANTING_NOT_HERE;

		return PlantingReason::PLANTING_OK;
	}
	if (aHasGrave)
	{
		return Plant::IsFlying(theSeedType) ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_ON_GRAVE;
	}

	Plant* aUnderPlant = aPlantOnLawn.mUnderPlant;
	bool aHasLilypad, aHasFlowerPot;
	if (!aUnderPlant || aUnderPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
	{
		aHasLilypad = false;
		aHasFlowerPot = false;
	}
	else
	{
		aHasLilypad = aUnderPlant->mSeedType == SeedType::SEED_LILYPAD;
		aHasFlowerPot = aUnderPlant->mSeedType == SeedType::SEED_FLOWERPOT;
	}
	if (GetCraterAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_ON_CRATER;
	}
	if (GetScaryPotAt(theGridX, theGridY) || IsIceAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	GridSquareType aGridSquare = mGridSquareType[theGridX][theGridY];
	if (aGridSquare == GridSquareType::GRIDSQUARE_DIRT || aGridSquare == GridSquareType::GRIDSQUARE_NONE)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	Plant* aNormalPlant = aPlantOnLawn.mNormalPlant;
	if (aNormalPlant != nullptr &&
		((theSeedType == SeedType::SEED_CHOMPER && aNormalPlant->mSeedType == SeedType::SEED_TALLNUT) ||
		 (theSeedType == SeedType::SEED_TALLNUT && aNormalPlant->mSeedType == SeedType::SEED_CHOMPER)))
	{
		return aNormalPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE
			? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	{
		if (theSeedType == SeedType::SEED_CATTAIL && aPlantOnLawn.mCatTailCount > 0)
			return aPlantOnLawn.mCatTailCount < 5 ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_HERE;
		if (PlantRules::IsMelonPultStackType(theSeedType) && aPlantOnLawn.mMelonPultCount > 0)
		{
			if (theSeedType == SeedType::SEED_WINTERMELON && BoardPlanting::FindTopUpgradeablePlant(this, theGridX, theGridY, SEED_WINTERMELON, SEED_MELONPULT) != nullptr)
				return PlantingReason::PLANTING_OK;
			return aPlantOnLawn.mMelonPultCount < 5 ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_HERE;
		}
	}
	if (theSeedType == SeedType::SEED_LILYPAD || theSeedType == SeedType::SEED_TANGLEKELP || theSeedType == SeedType::SEED_SEASHROOM)
	{
		if (!IsPoolSquare(theGridX, theGridY))
		{
			return PlantingReason::PLANTING_ONLY_IN_POOL;
		}

		return (aNormalPlant || aUnderPlant) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	if (Plant::IsFlying(theSeedType))
	{
		return aPlantOnLawn.mFlyingPlant ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (aGridSquare == GridSquareType::GRIDSQUARE_POOL || StageHasRoof() || aUnderPlant)
		{
			return PlantingReason::PLANTING_NEEDS_GROUND;
		}
	}
	// non-aquatic plants need a lily pad on water, but a pumpkin shell may go on a cattail
	Plant* aPumpkinPlant = aPlantOnLawn.mPumpkinPlant;
	if (aGridSquare == GridSquareType::GRIDSQUARE_POOL && !aHasLilypad && theSeedType != SeedType::SEED_CATTAIL)
	{
		if (!aNormalPlant || aNormalPlant->mSeedType != SeedType::SEED_CATTAIL || theSeedType != SeedType::SEED_PUMPKINSHELL)
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}
	}
	if (theSeedType == SeedType::SEED_FLOWERPOT)
	{
		return (aNormalPlant || aUnderPlant || aPumpkinPlant) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	if (StageHasRoof() && !aHasFlowerPot)
	{
		return PlantingReason::PLANTING_NEEDS_POT;
	}
	bool aAidPurchased = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FIRSTAID] > 0;
	if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		if (aNormalPlant && aNormalPlant->mSeedType == SeedType::SEED_COBCANNON)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		if (!aPumpkinPlant)
		{
			return PlantingReason::PLANTING_OK;
		}
		// wall-nut first aid on a damaged pumpkin
		if (aAidPurchased && aPumpkinPlant->mPlantHealth < aPumpkinPlant->mPlantMaxHealth * 2 / 3 &&
			aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL && aPumpkinPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}
	if (aHasLilypad && theSeedType == SeedType::SEED_POTATOMINE)
	{
		return PlantingReason::PLANTING_ONLY_ON_GROUND;
	}

	if (aUnderPlant)
	{
		if (theSeedType == SeedType::SEED_CATTAIL)
		{
			if (aNormalPlant || aPlantOnLawn.mFlyingPlant)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
			if (aUnderPlant->IsUpgradableTo(theSeedType) && aUnderPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_OK;
			}
			if (Plant::IsUpgrade(theSeedType))
			{
				return PlantingReason::PLANTING_NEEDS_UPGRADE;
			}
		}
		else
		{
			if (aUnderPlant->mSeedType == SeedType::SEED_IMITATER)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
		}
	}

	if (aNormalPlant)
	{
		bool aIsFumeGloomStack = PlantRules::IsFumeGloomStackType(aNormalPlant->mSeedType);
		if (aIsFumeGloomStack &&
			PlantRules::IsFumeGloomStackType(theSeedType))
		{
			if (theSeedType == SeedType::SEED_GLOOMSHROOM &&
				BoardPlanting::FindTopUpgradeablePlant(this, theGridX, theGridY, SEED_GLOOMSHROOM) != nullptr)
				return PlantingReason::PLANTING_OK;
			return aPlantOnLawn.mFumeGloomCount < 5 ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_HERE;
		}

		bool aIsSunflowerStack = aNormalPlant->mSeedType == SeedType::SEED_SUNFLOWER || aNormalPlant->mSeedType == SeedType::SEED_TWINSUNFLOWER;
		bool aWantsSunflowerLayer = theSeedType == SeedType::SEED_SUNFLOWER || theSeedType == SeedType::SEED_TWINSUNFLOWER;
		if (aIsSunflowerStack && aWantsSunflowerLayer)
		{
			if (theSeedType == SeedType::SEED_TWINSUNFLOWER && BoardPlanting::FindTopUpgradeablePlant(this, theGridX, theGridY, SEED_TWINSUNFLOWER, SEED_SUNFLOWER) != nullptr)
				return PlantingReason::PLANTING_OK;
			return aPlantOnLawn.mSunflowerCount < 5 ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_HERE;
		}
		if (PlantRules::IsMagnetStackType(aNormalPlant->mSeedType) && PlantRules::IsMagnetStackType(theSeedType))
		{
			if (theSeedType == SeedType::SEED_GOLD_MAGNET && BoardPlanting::FindTopUpgradeablePlant(this, theGridX, theGridY, SEED_GOLD_MAGNET, SEED_MAGNETSHROOM) != nullptr)
				return PlantingReason::PLANTING_OK;
			return aPlantOnLawn.mMagnetCount < 5 ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_HERE;
		}
		if (aNormalPlant->IsUpgradableTo(theSeedType) && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}
		if (Plant::IsUpgrade(theSeedType))
		{
			return PlantingReason::PLANTING_NEEDS_UPGRADE;
		}

		// wall-nut first aid
		if ((theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_TALLNUT) && aAidPurchased)
		{
			if (aNormalPlant->mPlantHealth < aNormalPlant->mPlantMaxHealth * 2 / 3 &&
				aNormalPlant->mSeedType == theSeedType && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_OK;
			}
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}

	// the easy planting cheat skips the upgrade requirement
	if (!mApp->mEasyPlantingCheat && Plant::IsUpgrade(theSeedType))
	{
		return PlantingReason::PLANTING_NEEDS_UPGRADE;
	}
	if (theSeedType == SeedType::SEED_COBCANNON && !IsValidCobCannonSpot(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NEEDS_UPGRADE;
	}
	else if (theSeedType == SeedType::SEED_CATTAIL && aGridSquare != GridSquareType::GRIDSQUARE_POOL)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}

	return PlantingReason::PLANTING_OK;
}
