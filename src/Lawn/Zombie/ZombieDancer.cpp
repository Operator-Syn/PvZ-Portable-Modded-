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

ZombieID Zombie::SummonBackupDancer(int theRow, int thePosX)
{
	if (!mBoard->RowCanHaveZombieType(theRow, ZombieType::ZOMBIE_BACKUP_DANCER))
		return ZombieID::ZOMBIEID_NULL;

	Zombie* aZombie = mBoard->AddZombie(ZombieType::ZOMBIE_BACKUP_DANCER, mFromWave);
	if (aZombie == nullptr)
		return ZombieID::ZOMBIEID_NULL;
	aZombie->mSpawnedByZombieRain = mSpawnedByZombieRain;

	aZombie->mPosX = thePosX;
	aZombie->mPosY = GetPosYBasedOnRow(theRow);
	aZombie->SetRow(theRow);
	aZombie->mX = static_cast<int>(aZombie->mPosX);
	aZombie->mY = static_cast<int>(aZombie->mPosY);

	aZombie->mAltitude = ZOMBIE_BACKUP_DANCER_RISE_HEIGHT;
	aZombie->mZombiePhase = ZombiePhase::PHASE_DANCER_RISING;
	aZombie->mPhaseCounter = 150;
	aZombie->mRelatedZombieID = mBoard->ZombieGetID(this);

	aZombie->SetAnimRate(0.0f);
	aZombie->mMindControlled = mMindControlled;

	int aParticleX = static_cast<int>(aZombie->mPosX) + 60;
	int aParticleY = static_cast<int>(aZombie->mPosY) + 110;
	if (aZombie->IsOnHighGround())
	{
		aParticleY -= HIGH_GROUND_HEIGHT;
	}
	int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
	mApp->AddPvzpParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_DANCER_RISE);
	mApp->PlayFoley(FoleyType::FOLEY_GRAVESTONE_RUMBLE);

	return mBoard->ZombieGetID(aZombie);
}

void Zombie::SummonBackupDancers()
{
	if (!mHasHead)
		return;

	for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
	{
		if (mBoard->ZombieTryToGet(mFollowerZombieID[i]) == nullptr)
		{
			int aRow = 0, aPosX = 0;
			switch (i)
			{
			case 0:     aRow = mRow - 1;    aPosX = mPosX;          break;
			case 1:     aRow = mRow + 1;    aPosX = mPosX;          break;
			case 2:     aRow = mRow;        aPosX = mPosX - 100;    break;
			case 3:     aRow = mRow;        aPosX = mPosX + 100;    break;
			default:    PVZP_ASSERT(false);                               break;
			}

			mFollowerZombieID[i] = SummonBackupDancer(aRow, aPosX);
		}
	}
}

bool Zombie::NeedsMoreBackupDancers()
{
	for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
	{
		if (mBoard->ZombieTryToGet(mFollowerZombieID[i]) == nullptr)
		{
			if (i == 0 && !mBoard->RowCanHaveZombieType(mRow - 1, ZombieType::ZOMBIE_BACKUP_DANCER))
			{
				continue;
			}

			if (i == 1 && !mBoard->RowCanHaveZombieType(mRow + 1, ZombieType::ZOMBIE_BACKUP_DANCER))
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

void Zombie::UpdateZombieBackupDancer()
{
	if (mIsEating)
		return;

	if (mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
	{
		mAltitude = PvzpAnimateCurve(150, 0, mPhaseCounter, ZOMBIE_BACKUP_DANCER_RISE_HEIGHT, 0, PvzpCurves::CURVE_LINEAR);

		if (mPhaseCounter != 0)
			return;

		if (IsOnHighGround())
		{
			mAltitude = HIGH_GROUND_HEIGHT;
		}
	}

	ZombiePhase aDancerPhase = GetDancerPhase();
	if (aDancerPhase != mZombiePhase)
	{
		switch (aDancerPhase)
		{
		case ZombiePhase::PHASE_DANCER_DANCING_LEFT:
			mZombiePhase = aDancerPhase;
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 10, 0.0f);
			break;

		case ZombiePhase::PHASE_DANCER_WALK_TO_RAISE:
			mZombiePhase = aDancerPhase;
			PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 10, 18.0f);
			mApp->ReanimationTryToGet(mBodyReanimID)->mAnimTime = 0.6f;
			break;

		case ZombiePhase::PHASE_DANCER_RAISE_LEFT_1:
		case ZombiePhase::PHASE_DANCER_RAISE_LEFT_2:
			mZombiePhase = aDancerPhase;
			PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 10, 18.0f);
			break;
		default:
			break;
		}
	}
}

void Zombie::UpdateZombieDancer()
{
	if (mIsEating)
		return;

	if (mSummonCounter > 0)
	{
		mSummonCounter--;
		if (mSummonCounter == 0)
		{
			if (GetDancerFrame() == 12 && mHasHead && mPosX < 700.0f)
			{
				mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT;
				PlayZombieReanim("anim_point", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
			}
			else
			{
				mSummonCounter = 1;
			}
		}
	}

	if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
	{
		if (mHasHead && mPhaseCounter == 0)
		{
			mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS;
			PlayZombieReanim("anim_point", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
			PickRandomSpeed();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS || mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT)
	{
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS && mBoard->CountZombiesOnScreen() <= 15)
			{
				mApp->PlayFoley(FoleyType::FOLEY_DANCER);
			}

			SummonBackupDancers();
			mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD;
			mPhaseCounter = 200;
		}
	}
	else
	{
		if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD)
		{
			if (mPhaseCounter != 0)
				return;

			mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_LEFT;
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
		}

		ZombiePhase aDancerPhase = GetDancerPhase();
		if (aDancerPhase != mZombiePhase)
		{
			switch (aDancerPhase)
			{
			case ZombiePhase::PHASE_DANCER_DANCING_LEFT:
				mZombiePhase = aDancerPhase;
				PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 10, 0.0f);
				break;

			case ZombiePhase::PHASE_DANCER_WALK_TO_RAISE:
				mZombiePhase = aDancerPhase;
				PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 10, 18.0f);
				mApp->ReanimationTryToGet(mBodyReanimID)->mAnimTime = 0.6f;
				break;

			case ZombiePhase::PHASE_DANCER_RAISE_LEFT_1:
			case ZombiePhase::PHASE_DANCER_RAISE_LEFT_2:
				mZombiePhase = aDancerPhase;
				PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 10, 18.0f);
				break;
			default:
				break;
			}
		}

		if (mHasHead && mSummonCounter == 0 && NeedsMoreBackupDancers())
		{
			mSummonCounter = 100;
		}
	}
}
