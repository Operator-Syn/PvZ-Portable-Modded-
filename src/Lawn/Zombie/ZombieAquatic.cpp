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

bool Zombie::IsTanglekelpTarget()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_TANGLEKELP && aPlant->mTargetZombieID == mBoard->ZombieGetID(this))
		{
			return true;
		}
	}

	return false;
}

void Zombie::UpdateZombieDolphinRider()
{
	if (IsTangleKelpTarget())
		return;

	bool aBackwards = IsWalkingBackwards();
	if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING && !aBackwards)
	{
		if (mX > 700 && mX <= 720)
		{
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_INTO_POOL;
			PlayZombieReanim("anim_jumpinpool", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.56f))
		{
			Reanimation* aSplashReanim = mApp->AddReanimation(mX - 83, mY + 73, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
			aSplashReanim->OverrideScale(1.2f, 0.8f);
			mApp->AddPvzpParticle(mX - 46, mY + 115, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
			mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			mPosX -= 70.0f;
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_RIDING;
			mInPool = true;
			mZombieAttackRect = Rect(-29, 0, 70, 115);
			PlayZombieReanim("anim_ride", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
	{
		if (mX <= 10)
		{
			mAltitude = -40.0f;
			mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
			mZombieAttackRect = Rect(30, 0, 30, 115);
			mZombieRect = Rect(20, 0, 42, 115);
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN;

			PoolSplash(false);
			StartWalkAnim(0);
			return;
		}

		if (mHasHead && !IsTanglekelpTarget())
		{
			Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
			if (aPlant)
			{
				mApp->PlayFoley(FoleyType::FOLEY_DOLPHIN_BEFORE_JUMPING);
				mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);

				mVelX = 0.5f;
				mZombiePhase = ZombiePhase::PHASE_DOLPHIN_IN_JUMP;
				mPhaseCounter = DOLPHIN_JUMP_TIME;
				PlayZombieReanim("anim_dolphinjump", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 10.0f);
			}
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		mAltitude = PvzpAnimateCurveFloat(DOLPHIN_JUMP_TIME, 0, mPhaseCounter, 0.0f, 10.0f, PvzpCurves::CURVE_LINEAR);

		bool aJumpEnds = false;
		if (aBodyReanim->ShouldTriggerTimedEvent(0.3f))
		{
			Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
			if (aPlant && aPlant->HasTallNutDefense())
			{
				mApp->PlayFoley(FoleyType::FOLEY_BONK);
				aJumpEnds = true;
				mApp->AddPvzpParticle(aPlant->mX + 60, aPlant->mY - 20, mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK);

				mZombieHeight = ZombieHeight::HEIGHT_FALLING;
				mPosX = aPlant->mX + 25.0f;
				mAltitude = 30.0f;
			}
		}
		else if (aBodyReanim->ShouldTriggerTimedEvent(0.49f))
		{
			Reanimation* aSplashReanim = mApp->AddReanimation(mX - 63, mY + 73, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
			aSplashReanim->OverrideScale(1.2f, 0.8f);
			mApp->AddPvzpParticle(mX - 26, mY + 115, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
			mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
			mVelX = 0.0f;
		}
		else if (aBodyReanim->mLoopCount > 0)
		{
			aJumpEnds = true;
			mPosX -= 94.0f;
			mAltitude = 0.0f;
		}

		if (aJumpEnds)
		{
			mDolphinFirstLeapComplete = true;
			mZombieAttackRect = Rect(30, 0, 30, 115);
			mZombieRect = Rect(20, 0, 42, 115);
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL;
			StartWalkAnim(0);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL)
	{
		if ((mX <= 10 && !aBackwards) || (mX > 680 && aBackwards))
		{
			mAltitude = -40.0f;
			mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN;

			PoolSplash(false);
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
			PickRandomSpeed();
		}
	}
}

void Zombie::UpdateZombieSnorkel()
{
	bool aBackwards = IsWalkingBackwards();
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING && !aBackwards)
	{
		if (mX > 700 && mX <= 720)
		{
			mVelX = 0.2f;
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_INTO_POOL;
			PlayZombieReanim("anim_jumpinpool", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		mAltitude = PvzpAnimateCurveFloat(0, 1000, aBodyReanim->mAnimTime * 1000, 0.0f, 10.0f, PvzpCurves::CURVE_LINEAR);

		if (aBodyReanim->ShouldTriggerTimedEvent(0.83f))
		{
			Reanimation* aSplashReanim = mApp->AddReanimation(mX - 47, mY + 73, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
			aSplashReanim->OverrideScale(1.2f, 0.8f);
			mApp->AddPvzpParticle(mX - 10, mY + 115, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
			mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL;
			mInPool = true;
			PlayZombieReanim("anim_swim", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
	{
		if (!mHasHead)
		{
			DieNoLoot();
		}
		else if (mX <= 25 && !aBackwards)
		{
			mAltitude = -90.0f;
			mPosX -= 15.0f;
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
			mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;

			PoolSplash(false);
			StartWalkAnim(0);
		}
		else if (mX > 640 && aBackwards)
		{
			mAltitude = -90.0f;
			mPosX += 15.0f;
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
			mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;

			PoolSplash(false);
			StartWalkAnim(0);
		}
		else if (mIsEating)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_UP_TO_EAT;
			PlayZombieReanim("anim_uptoeat", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (!mIsEating)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT;
			PlayZombieReanim("anim_uptoeat", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, -24.0f);
		}
		else if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_EATING_IN_POOL;
			PlayZombieReanim("anim_eat", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_EATING_IN_POOL)
	{
		if (!mIsEating)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT;
			PlayZombieReanim("anim_uptoeat", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, -24.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL;
			PlayZombieReanim("anim_swim", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 0.0f);
			PickRandomSpeed();
		}
	}
}

void Zombie::UpdateZombiePool()
{
	if (mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
	{
		mAltitude++;
		if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
		{
			mAltitude++;
		}

		if (mAltitude >= 0.0f)
		{
			mAltitude = 0.0f;
			mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
			mInPool = false;
		}
	}
	else if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL)
	{
		mAltitude--;
		int aDepth = -40 * mScaleZombie;
		if (mAltitude <= aDepth)
		{
			mAltitude = aDepth;
			mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
			StartWalkAnim(0);
		}
	}
	else if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
	{
		mAltitude--;
	}
}
