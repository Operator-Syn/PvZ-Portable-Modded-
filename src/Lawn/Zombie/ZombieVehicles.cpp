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

void Zombie::ZombieCatapultFire(Plant* thePlant)
{
	float aOriginX = mPosX + 113.0f;
	float aOriginY = mPosY - 44.0f;
	int aTargetX;
	if (thePlant)
	{
		aTargetX = thePlant->mX;
	}
	else
	{
		aTargetX = mPosX - 300.0f;
	}

	mApp->PlayFoley(FoleyType::FOLEY_BASKETBALL);

	Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_BASKETBALL);
	if (aProjectile != nullptr)
	{
		aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
		aProjectile->mSourceZombieType = mZombieType;
	}
	if (aProjectile == nullptr)
		return;
	float aRangeX = aOriginX - aTargetX - 20.0f;
	if (aRangeX < 40.0f)
	{
		aRangeX = 40.0f;
	}
	aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
	if (thePlant)
	{
		aProjectile->mCobTargetX = thePlant->mX + thePlant->mWidth / 2.0f - aProjectile->mWidth / 2.0f;
		aProjectile->mCobTargetRow = thePlant->mRow;
	}
	aProjectile->mVelX = -aRangeX / 120.0f;
	aProjectile->mVelY = 0.0f;
	aProjectile->mVelZ = -7.0f;
	aProjectile->mAccZ = 0.115f;
}

int Zombie::FindCatapultTargets(Plant** theTargets, int theMaxTargets)
{
	if (theTargets == nullptr || theMaxTargets <= 0)
		return 0;

	Plant* aCandidates[CLASSIC_GRID_SIZE_X * MAX_GRID_SIZE_Y]{};
	int aCandidateCount = 0;

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (mX < aPlant->mX + 100 || aPlant->NotOnGround() || aPlant->IsSpiky())
			continue;

		Plant* aTarget = mBoard->GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_CATAPULT_ORDER);
		if (aTarget == nullptr || aTarget->NotOnGround() || aTarget->IsSpiky() ||
			std::find(std::begin(aCandidates), std::begin(aCandidates) + aCandidateCount, aTarget) != std::begin(aCandidates) + aCandidateCount)
			continue;
		if (aCandidateCount < static_cast<int>(std::size(aCandidates)))
			aCandidates[aCandidateCount++] = aTarget;
	}

	std::sort(std::begin(aCandidates), std::begin(aCandidates) + aCandidateCount,
		[this](const Plant* theLeft, const Plant* theRight)
		{
			int aLeftRowDistance = std::abs(theLeft->mRow - mRow);
			int aRightRowDistance = std::abs(theRight->mRow - mRow);
			if (aLeftRowDistance != aRightRowDistance)
				return aLeftRowDistance < aRightRowDistance;
			if (theLeft->mPlantCol != theRight->mPlantCol)
				return theLeft->mPlantCol > theRight->mPlantCol;
			return theLeft->mRow < theRight->mRow;
		});

	int aTargetCount = 0;
	bool aSelectedRows[MAX_GRID_SIZE_Y]{};
	for (int i = 0; i < aCandidateCount && aTargetCount < theMaxTargets; ++i)
	{
		Plant* aCandidate = aCandidates[i];
		if (aSelectedRows[aCandidate->mRow])
			continue;
		theTargets[aTargetCount++] = aCandidate;
		aSelectedRows[aCandidate->mRow] = true;
	}
	for (int i = 0; i < aCandidateCount && aTargetCount < theMaxTargets; ++i)
	{
		Plant* aCandidate = aCandidates[i];
		if (std::find(theTargets, theTargets + aTargetCount, aCandidate) == theTargets + aTargetCount)
			theTargets[aTargetCount++] = aCandidate;
	}
	return aTargetCount;
}

void Zombie::UpdateZombieCatapult()
{
	const Rect aZombieRect = GetZombieRect();
	// On extended lawns, bring the complete vehicle inside the viewport with
	// room for the throwing arm, rather than stopping at 25% visibility.
	const bool aCanStartAttacking = mPosX <= 650 || (mApp->mWidth > BOARD_WIDTH &&
		aZombieRect.mX >= 0 && aZombieRect.mX + aZombieRect.mWidth <= mApp->mWidth - 32 && EffectedByDamage(1U));
	if (!aCanStartAttacking && (mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING ||
		mZombiePhase == ZombiePhase::PHASE_CATAPULT_RELOADING))
	{
		// Resume entry for a vehicle saved while parked at the former edge threshold.
		mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
		PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 0, 5.5f);
	}
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL)
	{
		Plant* aTargets[3]{};
		if (FindCatapultTargets(aTargets, 3) > 0 && aCanStartAttacking && mSummonCounter > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_CATAPULT_LAUNCHING;
			mPhaseCounter = 250;
			PlayZombieReanim("anim_shoot", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.545f))
		{
			Plant* aTargets[3]{};
			int aTargetCount = FindCatapultTargets(aTargets, 3);
			for (int i = 0; i < aTargetCount; ++i)
				ZombieCatapultFire(aTargets[i]);
		}
		if (aBodyReanim->mLoopCount > 0)
		{
			mSummonCounter--;
			if (mSummonCounter == 4)
			{
				ReanimShowTrack("Zombie_catapult_basketball", RENDER_GROUP_HIDDEN);
			}
			else if (mSummonCounter == 3)
			{
				ReanimShowTrack("Zombie_catapult_basketball2", RENDER_GROUP_HIDDEN);
			}
			else if (mSummonCounter == 2)
			{
				ReanimShowTrack("Zombie_catapult_basketball3", RENDER_GROUP_HIDDEN);
			}
			else if (mSummonCounter == 1)
			{
				ReanimShowTrack("Zombie_catapult_basketball4", RENDER_GROUP_HIDDEN);
			}

			if (mSummonCounter == 0)
			{
				PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 20, 6.0f);
				mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			}
			else
			{
				PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
				mZombiePhase = ZombiePhase::PHASE_CATAPULT_RELOADING;
			}
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_CATAPULT_RELOADING && mPhaseCounter == 0)
	{
		Plant* aTargets[3]{};
		if (FindCatapultTargets(aTargets, 3) > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_CATAPULT_LAUNCHING;
			mPhaseCounter = 250;
			PlayZombieReanim("anim_shoot", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		}
		else
		{
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 20, 6.0f);
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
		}
	}
}

void Zombie::BobsledCrash()
{
	mAltitude = 0.0f;
	mZombieRect = Rect(36, 0, 42, 115);
	mZombiePhase = ZombiePhase::PHASE_BOBSLED_CRASHING;
	mPhaseCounter = BOBSLED_CRASH_TIME;
	StartWalkAnim(0);

	Reanimation* aLeaderReanim = mApp->ReanimationGet(mBodyReanimID);
	for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
	{
		Zombie* aFollowerZombie = mBoard->ZombieGet(mFollowerZombieID[i]);
		aFollowerZombie->mZombiePhase = ZombiePhase::PHASE_BOBSLED_CRASHING;
		aFollowerZombie->mPhaseCounter = BOBSLED_CRASH_TIME;
		aFollowerZombie->mPosY = GetPosYBasedOnRow(mRow);
		aFollowerZombie->mAltitude = 0.0f;
		aFollowerZombie->StartWalkAnim(0);

		Reanimation* aFollowerReanim = mApp->ReanimationGet(aFollowerZombie->mBodyReanimID);
		if (aFollowerReanim)
		{
			aFollowerZombie->mVelX = mVelX;
			aFollowerReanim->mAnimTime = RandRangeFloat(0.0f, 1.0f);
			aFollowerReanim->mAnimRate = aLeaderReanim->mAnimRate;
		}
	}
}

void Zombie::UpdateZombieBobsled()
{
	if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
	{
		if (mPhaseCounter == 0)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			if (GetBobsledPosition() == 0)
			{
				for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
				{
					Zombie* aZombie = mBoard->ZombieGet(mFollowerZombieID[i]);
					aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
					mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
					aZombie->PickRandomSpeed();
				}
				PickRandomSpeed();
			}
		}
		return;
	}

	if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_SLIDING)
	{
		if (mPhaseCounter == 0)
		{
			mZombiePhase = ZombiePhase::PHASE_BOBSLED_BOARDING;
			PlayZombieReanim("anim_jump", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 20.0f);
			if (GetBobsledPosition() == 0 && mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			{
				for (int i = 0; i < 3; i++)
				{
					Zombie* aPassenger = mBoard->AddZombieInRow(ZombieType::ZOMBIE_IMP, mRow, mFromWave);
					if (aPassenger == nullptr)
						break;

					aPassenger->mPosX = mPosX - 133.0f - i * 18.0f;
					aPassenger->mPosY = GetPosYBasedOnRow(mRow);
					aPassenger->SetRow(mRow);
					aPassenger->mVariant = false;
					aPassenger->mAltitude = 88.0f;
					aPassenger->mRenderOrder = mRenderOrder + 1;
					aPassenger->mZombiePhase = ZombiePhase::PHASE_IMP_GETTING_THROWN;
					aPassenger->mVelX = 3.0f;
					aPassenger->mVelZ = 0.5f * (90.0f / aPassenger->mVelX) * THOWN_ZOMBIE_GRAVITY;
				}
			}
		}
	}
	else
	{
		if (mZombiePhase != ZombiePhase::PHASE_BOBSLED_BOARDING)
			return;

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		int aCounter = aBodyReanim->mAnimTime * 50.0f;
		int aPosition = GetBobsledPosition();
		if (aPosition == 1 || aPosition == 3)
		{
			mAltitude = PvzpAnimateCurveFloat(0, 50, aCounter, 8.0f, 18.0f, PvzpCurves::CURVE_LINEAR);
		}
		else
		{
			mAltitude = PvzpAnimateCurveFloat(0, 50, aCounter, -9.0f, 18.0f, PvzpCurves::CURVE_LINEAR);
		}
	}

	if (mBoard->mZombieTierSunMoney < TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		mBoard->mIceTimer[mRow] = std::max(500, mBoard->mIceTimer[mRow]);
	if (mPosX + 10.0f < mBoard->mIceMinX[mRow] && GetBobsledPosition() == 0)
	{
		TakeDamage(6, 8U);
	}
}
