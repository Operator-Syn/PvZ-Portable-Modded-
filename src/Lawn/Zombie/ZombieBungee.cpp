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

#include <climits>
#include <algorithm>
#include <array>
#include <format>

#include "../Plant/Plant.h"
#include "../Board/Board.h"
#include "../../ConstEnums.h"
#include "Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Entities/LawnMower.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/PlayerInfo.h"
#include "../System/Zombatar.h"
#include "../System/Music.h"
#include "../Widget/AlmanacDialog.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

#include "ZombieConstants.h"

int Zombie::CountBungeesTargetingSunFlowers()
{
	int aCount = 0;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol != -1)
		{
			Plant* aPlant = mBoard->GetTopPlantAt(aZombie->mTargetCol, aZombie->mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
			if (aPlant && aPlant->MakesSun())
			{
				aCount++;
			}
		}
	}

	return aCount;
}

void Zombie::PickBungeeZombieTarget(int theColumn)
{
	bool aAllowSunFlowerTarget = true;
	if (CountBungeesTargetingSunFlowers() == mBoard->CountSunFlowers() - 1)
	{
		aAllowSunFlowerTarget = false;
	}

	PvzpWeightedGridArray aPicks[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	int aPickCount = 0;

	for (int x = 0; x < mBoard->GetNumPlayableColumns(); x++)
	{
		if (theColumn == -1 || theColumn == x)
		{
			for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
			{
				int aWeight = 1;
				if (mBoard->GetGraveStoneAt(x, y) || mBoard->mGridSquareType[x][y] == GridSquareType::GRIDSQUARE_DIRT)
				{
					continue;
				}

				Plant* aPlant = mBoard->GetTopPlantAt(x, y, PlantPriority::TOPPLANT_BUNGEE_ORDER);
				if (aPlant)
				{
					if (!aAllowSunFlowerTarget && aPlant->MakesSun())
					{
						continue;
					}

					if (aPlant->mSeedType == SeedType::SEED_GRAVEBUSTER || aPlant->mSeedType == SeedType::SEED_COBCANNON)
					{
						continue;
					}

					aWeight = 10000;
				}

				if (!mBoard->BungeeIsTargetingCell(x, y))
				{
					aPicks[aPickCount].mX = x;
					aPicks[aPickCount].mY = y;
					aPicks[aPickCount].mWeight = aWeight;
					aPickCount++;
				}
			}
		}
	}

	if (aPickCount == 0)
	{
		DieNoLoot();
		return;
	}

	PvzpWeightedGridArray* aGrid = PvzpPickFromWeightedGridArray(aPicks, aPickCount);
	mTargetCol = aGrid->mX;
	SetRow(aGrid->mY);
	mPosX = mBoard->GridToPixelX(mTargetCol, mRow);
	mPosY = GetPosYBasedOnRow(mRow);
}

void Zombie::BungeeDropZombie(Zombie* theDroppedZombie, int theGridX, int theGridY)
{
	mTargetCol = theGridX;
	SetRow(theGridY);
	mPosX = mBoard->GridToPixelX(mTargetCol, mRow);
	mPosY = GetPosYBasedOnRow(mRow);
	PlayZombieReanim("anim_raise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
	mRelatedZombieID = mBoard->ZombieGetID(theDroppedZombie);

	theDroppedZombie->mPosX = mPosX - 15.0f;
	theDroppedZombie->SetRow(theGridY);
	theDroppedZombie->mPosY = GetPosYBasedOnRow(theGridY);
	theDroppedZombie->mZombieHeight = ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED;
	theDroppedZombie->PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
	theDroppedZombie->mRenderOrder = mRenderOrder + 1;
}

void Zombie::BungeeStealTarget()
{
	PlayZombieReanim("anim_grab", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);

	Plant* aPlant = mBoard->GetTopPlantAt(mTargetCol, mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
	if (aPlant && !aPlant->NotOnGround())
	{
		PVZP_ASSERT(aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER);

		if (aPlant->mSeedType != SeedType::SEED_COBCANNON && aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER)
		{
			mTargetPlantID = (PlantID)mBoard->mPlants.DataArrayGetID(aPlant);
			aPlant->mOnBungeeState = PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE;
			mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
		}
	}
}

void Zombie::BungeeLiftTarget()
{
	PlayZombieReanim("anim_raise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);

	Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mTargetPlantID));
	if (aPlant == nullptr)
		return;

#ifdef DO_FIX_BUGS
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie != this && aZombie->mTargetPlantID == mTargetPlantID)
		{
			aZombie->mTargetPlantID = PlantID::PLANTID_NULL;  // fixes the IZ bungee sun-farming bug
		}
	}
#endif

	aPlant->mOnBungeeState = PlantOnBungeeState::RISING_WITH_BUNGEE;
	mApp->PlayFoley(FoleyType::FOLEY_FLOOP);

	Reanimation* aPlantReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
	if (aPlantReanim)
	{
		aPlantReanim->mAnimRate = 0.1f;
	}

	if (aPlant->mSeedType == SeedType::SEED_CATTAIL && mBoard->GetTopPlantAt(mTargetCol, mRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
	{
		mBoard->NewPlant(mTargetCol, mRow, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	}

	if (mApp->IsIZombieLevel())
	{
		mBoard->mChallenge->IZombiePlantDropRemainingSun(aPlant);
	}
}

void Zombie::BungeeLanding()
{
	if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING && mAltitude < 1500.0f && !mApp->IsFinalBossLevel())
	{
		mApp->PlayFoley(FoleyType::FOLEY_BUNGEE_SCREAM);
		mZombiePhase = ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING;
	}

	if (mAltitude > 40.0f)
		return;

	Plant* aPlant = mBoard->FindUmbrellaPlant(mTargetCol, mRow);
	if (aPlant)
	{
		mApp->PlaySample(SOUND_BOING);
		mApp->PlayFoley(FoleyType::FOLEY_UMBRELLA);

		aPlant->DoSpecial();

		mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
		mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1);
		mHitUmbrella = true;

		return;
	}

	if (mAltitude > 0.0f)
		return;

	mAltitude = 0.0f;
	Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
	if (aZombie)  // carrying an airdropped zombie: release it
	{
		aZombie->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
		aZombie->StartWalkAnim(0);

		mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
		mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
		PlayZombieReanim("anim_raise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
	}
	else  // otherwise start stealing the plant
	{
		mZombiePhase = ZombiePhase::PHASE_BUNGEE_AT_BOTTOM;
		mPhaseCounter = 300;
		PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 5, 24.0f);
		mApp->ReanimationGet(mBodyReanimID)->mAnimTime = 0.5f;
	}
}

void Zombie::UpdateZombieBungee()
{
	if (IsDeadOrDying() || IsImmobilizied())
		return;

	if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING || mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING)
	{
		float aOldAltitude = mAltitude;
		mAltitude -= 8.0f;
		if (mAltitude <= BUNGEE_ZOMBIE_HEIGHT - 404.0f && aOldAltitude > BUNGEE_ZOMBIE_HEIGHT - 404.0f && mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
		{
			mApp->PlayFoley(FoleyType::FOLEY_GRASSSTEP);  // sound of the target marker hitting the ground
		}

		BungeeLanding();
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_AT_BOTTOM)
	{
		if (mPhaseCounter <= 0)
		{
			BungeeStealTarget();
			mZombiePhase = ZombiePhase::PHASE_BUNGEE_GRABBING;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_GRABBING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			BungeeLiftTarget();
			mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_HIT_OUCHY)
	{
		if (mPhaseCounter <= 0)
		{
			DieWithLoot();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
	{
		mAltitude += 8.0f;
		if (mAltitude >= 600.0f)
		{
			DieNoLoot();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_CUTSCENE)
	{
		mAltitude = PvzpAnimateCurve(200, 0, mPhaseCounter, 40, 0, PvzpCurves::CURVE_SIN_WAVE);
		if (mPhaseCounter <= 0)
		{
			mPhaseCounter = 200;
		}
	}

	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY);
}
