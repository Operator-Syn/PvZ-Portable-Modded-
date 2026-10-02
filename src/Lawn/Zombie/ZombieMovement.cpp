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

void Zombie::PickRandomSpeed()
{
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
	{
		mVelX = 0.3f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
	{
		if (mApp->IsIZombieLevel())
		{
			mVelX = 0.23f;
		}
		else
		{
			mVelX = 0.12f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_IMP && mApp->IsIZombieLevel())
	{
		mVelX = 0.9f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_YETI_RUNNING)
	{
		mVelX = 0.8f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		mVelX = 0.4f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
		mZombieType == ZombieType::ZOMBIE_POGO || mZombieType == ZombieType::ZOMBIE_FLAG)
	{
		mVelX = 0.45f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING || mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
		mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_SNORKEL || mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
	{
		mVelX = RandRangeFloat(0.66f, 0.68f);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING || mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD)
	{
		mVelX = RandRangeFloat(0.79f, 0.81f);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN)
	{
		mVelX = RandRangeFloat(0.89f, 0.91f);
	}
	else
	{
		mVelX = RandRangeFloat(0.23f, 0.37f);
		if (mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
			mVelX *= 1.2f;
		if (mVelX < 0.3f)
		{
			mAnimTicksPerFrame = 12;
		}
		else
		{
			mAnimTicksPerFrame = 15;
		}
	}

	UpdateAnimSpeed();
}

void Zombie::PogoBreak(unsigned int theDamageFlags)
{
	if (!mHasObject)
		return;

	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
	{
		//ZombieDrawPosition aDrawPos;
		//GetDrawPos(aDrawPos);

		float aPosX, aPosY;
		GetTrackPosition("Zombie_pogo_stick", aPosX, aPosY);
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_POGO);
		OverrideParticleScale(aParticle);
	}

	PVZP_ASSERT(mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_BURNED && !mDead);

	mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
	StartWalkAnim(0);
	mZombieRect = Rect(36, 17, 42, 115);
	mZombieAttackRect = Rect(20, 17, 50, 115);
	mShieldHealth = 0;
	mShieldType = ShieldType::SHIELDTYPE_NONE;
	mHasObject = false;
}

bool Zombie::IsBouncingPogo()
{
	return mZombiePhase >= ZombiePhase::PHASE_POGO_BOUNCING && mZombiePhase <= ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_7;
}

void Zombie::UpdateZombiePogo()
{
	if (IsDeadOrDying() || IsImmobilizied() || !IsBouncingPogo() || mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY)
		return;

	float aHeight = 40.0f;
	if (mZombiePhase >= ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1 && mZombiePhase <= ZombiePhase::PHASE_POGO_HIGH_BOUNCE_6)
	{
		aHeight = 50.0f + 20.0f * (mZombiePhase - ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2)
	{
		aHeight = 90.0f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_7)
	{
		aHeight = 170.0f;
	}
	mAltitude = PvzpAnimateCurveFloat(POGO_BOUNCE_TIME, 0, mPhaseCounter, 9.0f, aHeight + 9.0f, PvzpCurves::CURVE_BOUNCE_SLOW_MIDDLE);
	mFrame = std::clamp(static_cast<int>(3 - mAltitude / 3), 0, 3);

	if (mPhaseCounter == 7)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		aBodyReanim->mAnimTime = 0.0f;
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	}
	if (IsOnBoard() && mPhaseCounter == 5)
	{
		mApp->PlayFoley(FoleyType::FOLEY_POGO_ZOMBIE);
	}

	if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND)
	{
		mAltitude += HIGH_GROUND_HEIGHT;
		mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
	}
	else if (mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
	{
		mOnHighGround = false;
		mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
	}
	else if (mOnHighGround)
	{
		mAltitude += HIGH_GROUND_HEIGHT;
	}

	if (mZombiePhase == ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2 && mPhaseCounter == 70)
	{
		Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
		if (aPlant && aPlant->IsTallNut())
		{
			mApp->PlayFoley(FoleyType::FOLEY_BONK);
			mApp->AddPvzpParticle(aPlant->mX + 60, aPlant->mY - 20, mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK);

			mShieldType = ShieldType::SHIELDTYPE_NONE;
			PogoBreak(0U);
			return;
		}
	}

	if (mPhaseCounter != 0)
		return;

	Plant* aPlant = nullptr;
	if (IsOnBoard())
	{
		aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
	}
	if (aPlant == nullptr)
	{
		mZombiePhase = ZombiePhase::PHASE_POGO_BOUNCING;

		PickRandomSpeed();
		mPhaseCounter = POGO_BOUNCE_TIME;
		return;
	}

	if (mZombiePhase == ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1)
	{
		mZombiePhase = ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2;
		mVelX = (mX - aPlant->mX + 60) / static_cast<float>(POGO_BOUNCE_TIME);
		mPhaseCounter = POGO_BOUNCE_TIME;
	}
	else
	{
		mZombiePhase = ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1;
		mVelX = 0.0f;
		mPhaseCounter = POGO_BOUNCE_TIME;
	}
}

void Zombie::LandFlyer(unsigned int theDamageFlags)
{
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)) && mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING)
	{
		mApp->PlaySample(SOUND_BALLOON_POP);
		mZombiePhase = ZombiePhase::PHASE_BALLOON_POPPING;
		PlayZombieReanim("anim_pop", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
	}

	if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
	{
		DieWithLoot();
	}
	else
	{
		mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	}
}

void Zombie::UpdateZombieFlyer()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY && mPosX < 720.0f)
	{
		mAltitude -= 0.1f;
		if (mAltitude < -35.0f)
		{
			LandFlyer(0U);
		}
	}

	if (mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_BALLOON_WALKING;
			StartWalkAnim(0);
		}
	}

	if (mApp->IsIZombieLevel() && mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING && mBoard->mChallenge->IZombieGetBrainTarget(this))
	{
		LandFlyer(0U);
	}
}

void Zombie::UpdateZombieNewspaper()
{
	if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_MAD;
			if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead)
			{
				mApp->PlayFoley(FoleyType::FOLEY_NEWSPAPER_RARRGH);
			}

			StartWalkAnim(20);
			aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_PAPER_MADHEAD);
		}
	}
}

void Zombie::UpdateZombiePolevaulter()
{
	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT && mHasHead && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
	{
		Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
		if (aPlant)
		{
			if (mBoard->GetLadderAt(aPlant->mPlantCol, aPlant->mRow))
			{
				float aPlantX = mBoard->GridToPixelX(aPlant->mPlantCol, aPlant->mRow) + 40;
				if (aPlantX > mPosX && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL && mUseLadderCol != aPlant->mPlantCol)
				{
					mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
					mUseLadderCol = aPlant->mPlantCol;
				}
				return;
			}

			mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_IN_VAULT;
			PlayZombieReanim("anim_jump", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);

			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
			float aAnimDuration = aBodyReanim->mFrameCount / aBodyReanim->mAnimRate * 100.0f;
			int aJumpDistance = mX - aPlant->mX - 80;
			if (mApp->IsWallnutBowlingLevel())
			{
				aJumpDistance = 0;
			}
			mVelX = aJumpDistance / aAnimDuration;
			mHasObject = false;
		}

		if (mApp->IsIZombieLevel() && mBoard->mChallenge->IZombieGetBrainTarget(this))
		{
			mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
			StartWalkAnim(0);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

		bool aJumpEnds = false;
		if (aBodyReanim->mAnimTime > 0.6f && aBodyReanim->mAnimTime <= 0.7f)
		{
			Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
			if (aPlant && aPlant->IsTallNut())
			{
				mApp->PlayFoley(FoleyType::FOLEY_BONK);
				aJumpEnds = true;
				mApp->AddPvzpParticle(aPlant->mX + 60, aPlant->mY - 20, mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK);

				mZombieHeight = ZombieHeight::HEIGHT_FALLING;
				mPosX = aPlant->mX;
				mPosY -= 30.0f;
			}
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			aJumpEnds = true;
			mPosX -= 150.0f;
		}
		if (aBodyReanim->ShouldTriggerTimedEvent(0.2f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_GRASSSTEP);
		}
		if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_POLEVAULT);
		}

		if (aJumpEnds)
		{
			mX = static_cast<int>(mPosX);
			mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
			mZombieAttackRect = Rect(50, 0, 20, 115);

			StartWalkAnim(0);
		}
		else
		{
			float aOldPosX = mPosX;
			mPosX -= 150.0f * aBodyReanim->mAnimTime;
			mPosY = GetPosYBasedOnRow(mRow);
			mPosX = aOldPosX;
		}
	}
}

void Zombie::DiggerLoseAxe()
{
	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
	{
		mZombiePhase = ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE;
		mPhaseCounter = 200;
		SetAnimRate(0.0f);
		UpdateAnimSpeed();
		AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_DIGGER_TUNNEL, nullptr);
		StopZombieSound();
	}

	mHasObject = false;
	ReanimShowTrack("Zombie_digger_pickaxe", RENDER_GROUP_HIDDEN);
	ReanimShowTrack("Zombie_digger_dirt", RENDER_GROUP_HIDDEN);
}

void Zombie::UpdateZombieDigger()
{
	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
	{
		if (mPosX < 10.0f)
		{
			mAltitude = -120.0f;
			mZombiePhase = ZombiePhase::PHASE_DIGGER_RISING;
			mPhaseCounter = 130;
			PlayZombieReanim("anim_drill", ReanimLoopType::REANIM_LOOP, 0, 20.0f);

			mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
			mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
			AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_DIGGER_TUNNEL, nullptr);
			StopZombieSound();

			mApp->AddPvzpParticle(mPosX + 60.0f, mPosY + 118.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_DIGGER_RISE);
			Reanimation* aDirtReanim = mApp->AddReanimation(mPosX + 13.0f, mPosY + 97.0f, mRenderOrder + 1, ReanimationType::REANIM_DIGGER_DIRT);
			aDirtReanim->mAnimRate = 24.0f;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING)
	{
		if (mPhaseCounter > 40)
		{
			mAltitude = PvzpAnimateCurve(130, 40, mPhaseCounter, -120, 20, PvzpCurves::CURVE_EASE_OUT);
		}
		else
		{
			mAltitude = PvzpAnimateCurve(30, 0, mPhaseCounter, 20, 0, PvzpCurves::CURVE_EASE_IN);
		}

		if (mPhaseCounter == 30)
		{
			PlayZombieReanim("anim_landing", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
		}

		if (mPhaseCounter == 0)
		{
			mAltitude = 0.0f;
			mZombiePhase = ZombiePhase::PHASE_DIGGER_STUNNED;
			PlayZombieReanim("anim_dizzy", ReanimLoopType::REANIM_LOOP, 10, 12.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE)
	{
		if (mPhaseCounter == 150)
		{
			AddAttachedReanim(23, 93, ReanimationType::REANIM_ZOMBIE_SURPRISE);
		}

		if (mPhaseCounter == 0)
		{
			mAltitude = -120.f;
			mZombiePhase = ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE;
			mPhaseCounter = 130;
			PlayZombieReanim("anim_landing", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);

			mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
			mApp->AddPvzpParticle(mPosX + 60.0f, mPosY + 118.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_DIGGER_RISE);
			Reanimation* aDirtReanim = mApp->AddReanimation(mPosX + 13.0f, mPosY + 97.0f, mRenderOrder + 1, ReanimationType::REANIM_DIGGER_DIRT);
			aDirtReanim->mAnimRate = 24.0f;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE)
	{
		if (mPhaseCounter > 40)
		{
			mAltitude = PvzpAnimateCurve(130, 40, mPhaseCounter, -120, 20, PvzpCurves::CURVE_EASE_OUT);
		}
		else
		{
			mAltitude = PvzpAnimateCurve(30, 0, mPhaseCounter, 20, 0, PvzpCurves::CURVE_EASE_IN);
		}

		if (mPhaseCounter == 30)
		{
			PlayZombieReanim("anim_landing", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
		}

		if (mPhaseCounter == 0)
		{
			mAltitude = 0.0f;
			mZombiePhase = ZombiePhase::PHASE_DIGGER_WALKING_WITHOUT_AXE;
			StartWalkAnim(20);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 1)
		{
			mZombiePhase = ZombiePhase::PHASE_DIGGER_WALKING;
			StartWalkAnim(20);
		}
	}
}

void Zombie::UpdateZombieRiseFromGrave()
{
	if (mInPool)
	{
		mAltitude = PvzpAnimateCurve(50, 0, mPhaseCounter, -150, -40, PvzpCurves::CURVE_LINEAR) * mScaleZombie;
	}
	else
	{
		mAltitude = PvzpAnimateCurve(50, 0, mPhaseCounter, -200, 0, PvzpCurves::CURVE_LINEAR);
	}

	if (mPhaseCounter == 0)
	{
		mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;

		if (IsOnHighGround())
		{
			mAltitude = HIGH_GROUND_HEIGHT;
		}

		if (mInPool)
		{
			ReanimIgnoreClipRect("Zombie_duckytube", true);
			ReanimIgnoreClipRect("Zombie_whitewater", true);
			ReanimIgnoreClipRect("Zombie_outerarm_hand", true);
			ReanimIgnoreClipRect("Zombie_innerarm3", true);
		}
	}
}

void Zombie::DragUnder()
{
	mZombieHeight = ZombieHeight::HEIGHT_DRAGGED_UNDER;
	StopEating();
	ReanimReenableClipping();
}

bool Zombie::ZombiquariumFindClosestBrain()
{
	if (mBoard->HasLevelAwardDropped() || mBodyHealth > 150)
		return false;

	GridItem* aBrainClosest = nullptr;
	float aDistanceClosest = 0.0f;
	for (GridItem* aGridItem : mBoard->mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_BRAIN && aGridItem->mGridItemCounter >= 15)
		{
			float aDistance = Distance2D(aGridItem->mPosX + 15.0f, aGridItem->mPosY + 15.0f, mPosX + 50.0f, mPosY + 40.0f);
			if (aBrainClosest == nullptr || aDistance < aDistanceClosest)
			{
				aDistanceClosest = aDistance;
				aBrainClosest = aGridItem;
			}
		}
	}

	if (aBrainClosest)
	{
		if (aDistanceClosest < 50.0f)
		{
			aBrainClosest->GridItemDie();
			mApp->PlayFoley(FoleyType::FOLEY_SLURP);

			mBodyHealth += 200;
			mBodyHealth = std::min(mBodyHealth, mBodyMaxHealth);

			PlayZombieReanim("anim_aquarium_bite", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
			mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BITE;
			mPhaseCounter = 200;
			return false;
		}

		float aRangeY = aBrainClosest->mPosY + 15.0f - (mPosY + 40.0f);
		float aRangeX = aBrainClosest->mPosX + 15.0f - (mPosX + 50.0f);
		mVelZ = atan2(aRangeY, aRangeX);
		if (mVelZ < 0.0f)
		{
			mVelZ += PI * 2;
		}

		mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL;
		return true;
	}

	return false;
}

void Zombie::UpdateZombiquarium()
{
	if (IsDeadOrDying())
		return;

	//float& num2 = mVelX; // unused
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BITE)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			float aAnimRate = RandRangeFloat(8.0f, 10.0f);
			PlayZombieReanim("anim_aquarium_swim", ReanimLoopType::REANIM_LOOP, 20, aAnimRate);

			mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
			mPhaseCounter = 100;
		}
	}
	else if (!ZombiquariumFindClosestBrain() && mPhaseCounter == 0)
	{
		int aPhaseHit = Rand(7);
		if (aPhaseHit <= 4)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL;
			mVelZ = RandRangeFloat(0.0f, PI * 2);
			mPhaseCounter = RandRangeInt(300, 1000);
			aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		}
		//else if (aPhaseHit == 4)
		//{
		//    mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
		//    mVelZ = PI * 1.5f;
		//    mPhaseCounter = RandRangeInt(300, 1000);
		//    aBodyReanim->mAnimRate = RandRangeFloat(8.0f, 10.0f);
		//}
		else if (aPhaseHit == 5)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH;
			mVelZ = 0.0f;
			mPhaseCounter = RandRangeInt(300, 1000);
			aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		}
		else
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH;
			mVelZ = PI;
			mPhaseCounter = RandRangeInt(300, 1000);
			aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		}
	}

	float aVelX = cos(mVelZ);
	float aVelY = sin(mVelZ);
	bool aIsOutOfBounds = false;
	if (mPosX < 0.0f && aVelX < 0.0f)
	{
		aIsOutOfBounds = true;
	}
	else if (mPosX > 680.0f && aVelX > 0.0f)
	{
		aIsOutOfBounds = true;
	}
	else if (mPosY < 100.0f && aVelY < 0.0f)
	{
		aIsOutOfBounds = true;
	}
	else if (mPosY > 400.0f && aVelY > 0.0f)
	{
		aIsOutOfBounds = true;
	}

	float aMaxSpeed = 0.5f;
	if (aIsOutOfBounds)
	{
		aMaxSpeed = mVelX * 0.3f;
		mPhaseCounter = std::min(100, mPhaseCounter);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL)
	{
		aMaxSpeed = 0.5f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH)
	{
		if (mPosX < 200.0f && aVelX < 0.0f)
		{
			mVelZ = 0.0f;
		}

		if (mPosX > 550.0f && aVelX > 0.0f)
		{
			mVelZ = PI;
		}

		aMaxSpeed = 0.3f;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT || mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BITE)
	{
		aMaxSpeed = 0.05f;
	}

	mVelX = std::min(aMaxSpeed, mVelX + 0.01f);
	aVelX *= mVelX;
	aVelY *= mVelX;
	mPosX += aVelX;
	mPosY += aVelY;

	if (!mBoard->HasLevelAwardDropped())
	{
		if (mSummonCounter > 0)
		{
			mSummonCounter--;
			if (mSummonCounter == 0)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(mX + 50, mY + 40, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
				mSummonCounter = RandRangeInt(1000, 1500);
			}
		}

		if (mZombieAge % 100 == 0)
		{
			TakeDamage(10, 8U);
			if (IsDeadOrDying())
			{
				mApp->PlaySample(SOUND_ZOMBAQUARIUM_DIE);
			}
		}
	}
}

void Zombie::UpdateZombieHighGround()
{
	if (mZombieType == ZombieType::ZOMBIE_POGO)
		return;

	if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND)
	{
		mAltitude++;
		if (mAltitude >= HIGH_GROUND_HEIGHT)
		{
			mAltitude = HIGH_GROUND_HEIGHT;
			mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
		}
	}
	else if (mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
	{
		mAltitude--;
		if (mAltitude <= 0.0f)
		{
			mAltitude = 0.0f;
			mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
			mOnHighGround = false;
		}
	}
}

void Zombie::UpdateZombieFalling()
{
	mAltitude--;
	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
	{
		mAltitude--;
	}

	int aGroundHeight = 0;
	if (IsOnHighGround())
	{
		aGroundHeight = HIGH_GROUND_HEIGHT;
	}
	if (mAltitude <= aGroundHeight)
	{
		mAltitude = aGroundHeight;
		mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
	}
}

float Zombie::ZombieTargetLeadX(float theTime)
{
	float aSpeed = mVelX * ZombieRules::ZombieStrengthSpeedMultiplier(mBoard ? mBoard->mZombieStrengthTier : 0);
	if (mChilledCounter > 0)
	{
		aSpeed *= CHILLED_SPEED_FACTOR;
	}
	if (IsWalkingBackwards())
	{
		aSpeed = -aSpeed;
	}
	if (ZombieNotWalking())
	{
		aSpeed = 0.0f;
	}

	Rect aZombieRect = GetZombieRect();
	float aCurrentPosX = aZombieRect.mX + aZombieRect.mWidth / 2;
	float aDisplacementX = aSpeed * theTime;
	return aCurrentPosX - aDisplacementX;
}

bool Zombie::ZombieNotWalking()
{
	if (mIsEating || IsImmobilizied())
	{
		return true;
	}

	if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
		mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
		mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_THROWING ||
		mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_SMASHING ||
		mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING ||
		mZombiePhase == ZombiePhase::PHASE_CATAPULT_RELOADING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
		mZombiePhase == ZombiePhase::PHASE_IMP_LANDING ||
		mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING ||
		mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY ||
		mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED ||
		mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM ||
		mZombieType == ZombieType::ZOMBIE_BUNGEE ||
		mZombieType == ZombieType::ZOMBIE_BOSS ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RAISE_LEFT_1 ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_WALK_TO_RAISE ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RAISE_LEFT_2)
	{
		return true;
	}

	if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		Zombie* aLeader = nullptr;
		if (mZombieType == ZombieType::ZOMBIE_DANCER)
		{
			aLeader = this;
		}
		else
		{
			aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
		}

		if (aLeader)
		{
			if (aLeader->IsImmobilizied() || aLeader->mIsEating)
			{
				return true;
			}

			for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
			{
				Zombie* aDancer = mBoard->ZombieTryToGet(aLeader->mFollowerZombieID[i]);
				if (aDancer && (aDancer->IsImmobilizied() || aDancer->mIsEating))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void Zombie::UpdateZamboni()
{
	if (mPosX > 400.0f && !mFlatTires)
	{
		mVelX = PvzpAnimateCurveFloat(700, 300, mPosX, 0.25f, 0.05f, PvzpCurves::CURVE_LINEAR);
	}
	else if (mFlatTires && mVelX > 0.0005f)
	{
		mVelX -= 0.0005f;
	}

	int anIceX = mPosX + 118;
	if (mBoard->StageHasRoof())
	{
		anIceX = std::max(anIceX, 500);
	}
	else
	{
		anIceX = std::max(anIceX, 25);
	}
	mBoard->mIceMinX[mRow] = std::min(mBoard->mIceMinX[mRow], anIceX);
	if (anIceX < mApp->mWidth)
	{
		mBoard->mIceTimer[mRow] = 3000;
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA)
		{
			mBoard->mIceTimer[mRow] = INT_MAX;
		}
	}
}

void Zombie::UpdateYeti()
{
	if (mMindControlled || !mHasHead || IsDeadOrDying())
		return;

	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL && mPhaseCounter == 0)
	{
		mZombiePhase = ZombiePhase::PHASE_YETI_RUNNING;
		mHasObject = false;
		PickRandomSpeed();
	}
}

void Zombie::UpdateLadder()
{
	if (mMindControlled || !mHasHead || IsDeadOrDying())
		return;

	if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
	{
		if (FindPlantTarget(ZombieAttackType::ATTACKTYPE_LADDER))
		{
			StopEating();
			mZombiePhase = ZombiePhase::PHASE_LADDER_PLACING;
			PlayZombieReanim("anim_placeladder", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_LADDER);
			if (aPlant)
			{
				mBoard->AddALadder(aPlant->mPlantCol, aPlant->mRow);
				mApp->PlaySample(SOUND_LADDER_ZOMBIE);
				mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
				mUseLadderCol = aPlant->mPlantCol;
				DetachShield();
			}
			else
			{
				mZombiePhase = ZombiePhase::PHASE_LADDER_CARRYING;
				StartWalkAnim(0);
			}
		}
	}
}

void Zombie::UpdateZombieWalking()
{
	if (ZombieNotWalking() || mBloverKnockbackDistanceRemaining > 0 || mBlowingAway ||
		(mEphraimStaggerCounter > 0 && !IsDeadOrDying()))
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		float aSpeed;
		if (IsBouncingPogo() || mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
			mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL || mZombieType == ZombieType::ZOMBIE_CATAPULT)
		{
			aSpeed = mVelX;
			if (IsMovingAtChilledSpeed())
			{
				aSpeed *= CHILLED_SPEED_FACTOR;
			}
		}
		else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
			IsBobsledTeamWithSled() || mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
		{
			aSpeed = mVelX;
		}
		else if (aBodyReanim->TrackExists("_ground"))
		{
			aSpeed = aBodyReanim->GetTrackVelocity("_ground") * mScaleZombie;
		}
		else
		{
			aSpeed = mVelX;
			if (IsMovingAtChilledSpeed())
			{
				aSpeed *= CHILLED_SPEED_FACTOR;
			}
		}
		aSpeed *= ZombieRules::ZombieStrengthSpeedMultiplier(mBoard ? mBoard->mZombieStrengthTier : 0);

		if (IsWalkingBackwards() || mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
		{
			mPosX += aSpeed;
		}
		else
		{
			mPosX -= aSpeed;
		}

		if (mZombieType == ZombieType::ZOMBIE_FOOTBALL && mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
		{
			if (aBodyReanim->ShouldTriggerTimedEvent(0.03f))
			{
				mApp->AddPvzpParticle(mX + 81, mY + 106, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT);
			}
			if (aBodyReanim->ShouldTriggerTimedEvent(0.61f))
			{
				mApp->AddPvzpParticle(mX + 87, mY + 110, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT);
			}
		}
		if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
		{
			if (aBodyReanim->ShouldTriggerTimedEvent(0.16f))
			{
				mApp->AddPvzpParticle(mX + 81, mY + 106, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT);
			}
			if (aBodyReanim->ShouldTriggerTimedEvent(0.67f))
			{
				mApp->AddPvzpParticle(mX + 87, mY + 110, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT);
			}
		}
	}
	else
	{
		bool doWalk = false;
		if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
			mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
			mZombieType == ZombieType::ZOMBIE_DANCER ||
			mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
			mZombieType == ZombieType::ZOMBIE_BOBSLED ||
			mZombieType == ZombieType::ZOMBIE_POGO ||
			mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
			mZombieType == ZombieType::ZOMBIE_BALLOON)
		{
			doWalk = true;
		}
		else if (mZombieType == ZombieType::ZOMBIE_SNORKEL && mInPool)
		{
			doWalk = true;
		}
		else if (mFrame >= 0 && mFrame <= 2)
		{
			doWalk = true;
		}
		else if (mFrame >= 6 && mFrame <= 8)
		{
			doWalk = true;
		}

		if (doWalk)
		{
			float aSpeed = mVelX;
			if (IsMovingAtChilledSpeed())
			{
				aSpeed *= CHILLED_SPEED_FACTOR;
			}
			aSpeed *= ZombieRules::ZombieStrengthSpeedMultiplier(mBoard ? mBoard->mZombieStrengthTier : 0);

			if (IsWalkingBackwards())
			{
				mPosX += aSpeed;
			}
			else
			{
				mPosX -= aSpeed;
			}
		}
	}
}

Plant* Zombie::IsStandingOnSpikeweed()
{
	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
		return nullptr;

	Rect aZombieRect = GetZombieRect();

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow == mRow && aPlant->IsSpiky() && !aPlant->NotOnGround() && (!mOnHighGround || aPlant->IsOnHighGround()))
		{
			Rect aPlantAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
			if (GetRectOverlap(aPlantAttackRect, aZombieRect) > 0)
			{
				return aPlant;
			}
		}
	}

	return nullptr;
}

void Zombie::CheckForZombieStep()
{
	if ((mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT) && !mFlatTires)
	{
		CheckSquish(ZombieAttackType::ATTACKTYPE_DRIVE_OVER);
	}
}

void Zombie::UpdateZombiePosition()
{
	if (mBlowingAway)
	{
		mPosX += 10.0f;
		if (mX > 850)
		{
			DieWithLoot();
		}
		return;
	}

	if (mBloverKnockbackDistanceRemaining > 0)
	{
		int aKnockbackStep = std::min(2, mBloverKnockbackDistanceRemaining);
		mPosX += static_cast<float>(aKnockbackStep);
		mBloverKnockbackDistanceRemaining -= aKnockbackStep;
		return;
	}

	if (mEphraimKnockbackDistanceRemaining != 0)
	{
		const int aDistance = std::abs(mEphraimKnockbackDistanceRemaining);
		// Like Blover, advance every tick; ease the last eight pixels to a stop.
		const int aStep = (mEphraimKnockbackDistanceRemaining < 0 ? -1 : 1) *
			std::min(aDistance, aDistance > 8 ? 2 : 1);
		mPosX += static_cast<float>(aStep);
		mEphraimKnockbackDistanceRemaining -= aStep;
		return;
	}

	if (mZombieType == ZombieType::ZOMBIE_BUNGEE || mZombieType == ZombieType::ZOMBIE_BOSS ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE || mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
		return;

	UpdateZombieWalking();
	CheckForZombieStep();

	if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
	{
		float aDesiredY = GetPosYBasedOnRow(mRow);
		if (mPosY < aDesiredY)
		{
			mPosY += std::min(aDesiredY - mPosY, 1.0f);
		}
		else if (mPosY > aDesiredY)
		{
			mPosY -= std::min(mPosY - aDesiredY, 1.0f);
		}
	}
}

float Zombie::GetPosYBasedOnRow(int theRow)
{
	if (!IsOnBoard())
		return 0.0f;

	if (IsOnHighGround())
	{
		if (mAltitude < HIGH_GROUND_HEIGHT)
		{
			mZombieHeight = ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND;
		}
		mOnHighGround = true;
	}

	float aPosY = mBoard->GetPosYBasedOnRow(mPosX + 40.0f, theRow) - 30.0f;
	if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		aPosY -= 30.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		aPosY -= 16.0f;
	}

	return aPosY;
}

void Zombie::SetRow(int theRow)
{
	PVZP_ASSERT(theRow >= 0 && theRow <= MAX_GRID_SIZE_Y);

	mRow = theRow;
	mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ZOMBIE, mRow, 4);
}

void Zombie::RiseFromGrave(int theCol, int theRow)
{
	PVZP_ASSERT(mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL);

	mPosX = mBoard->GridToPixelX(theCol, mRow) - 25;
	mPosY = GetPosYBasedOnRow(theRow);
	SetRow(theRow);
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY);
	mAltitude = CLIP_HEIGHT_OFF;
	mZombiePhase = ZombiePhase::PHASE_RISING_FROM_GRAVE;
	mPhaseCounter = 150;

	if (mBoard->StageHasPool())
	{
		mAltitude = -150.0f;
		mInPool = true;
		mPhaseCounter = 50;
		mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;

		StartWalkAnim(0);
		ReanimIgnoreClipRect("Zombie_duckytube", false);
		ReanimIgnoreClipRect("Zombie_whitewater", false);
		ReanimIgnoreClipRect("Zombie_outerarm_hand", false);
		ReanimIgnoreClipRect("Zombie_innerarm3", false);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED);
		OverrideParticleScale(aParticle);

		if (mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE && aParticle)
		{
			aBodyReanim->AttachParticleToTrack("anim_cone", aParticle, 37.0f, 20.0f);
		}
		else if (mZombieType == ZombieType::ZOMBIE_PAIL && aParticle)
		{
			aBodyReanim->AttachParticleToTrack("anim_bucket", aParticle, 37.0f, 20.0f);
		}
		else if (aParticle)
		{
			aBodyReanim->AttachParticleToTrack("anim_head1", aParticle, 30.0f, 20.0f);
		}

		PvzpParticleSystem* aParticle2 = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED);
		if (aParticle2)
		{
			OverrideParticleScale(aParticle2);
			aBodyReanim->AttachParticleToTrack("Zombie_outerarm_upper", aParticle2, 5.0f, 5.0f);
		}

		PvzpParticleSystem* aParticle3 = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED);
		if (aParticle3)
		{
			OverrideParticleScale(aParticle3);
			aBodyReanim->AttachParticleToTrack("Zombie_duckytube", aParticle3, 77.0f, 20.0f);
		}

		PoolSplash(false);
	}
	else
	{
		int aParticleX = mPosX + 60;
		int aParticleY = mPosY + 110;
		if (IsOnHighGround())
		{
			aParticleY -= HIGH_GROUND_HEIGHT;
		}

		int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
		if (mApp->IsWhackAZombieLevel())
		{
			mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
			mApp->AddPvzpParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_WHACK_A_ZOMBIE_RISE);
		}
		else
		{
			mApp->PlayFoley(FoleyType::FOLEY_GRAVESTONE_RUMBLE);
			mApp->AddPvzpParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_RISE);
		}
	}
}

bool Zombie::IsZombotany(ZombieType theZombieType)
{
	return
		theZombieType == ZombieType::ZOMBIE_PEA_HEAD ||
		theZombieType == ZombieType::ZOMBIE_WALLNUT_HEAD ||
		theZombieType == ZombieType::ZOMBIE_TALLNUT_HEAD ||
		theZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD ||
		theZombieType == ZombieType::ZOMBIE_GATLING_HEAD ||
		theZombieType == ZombieType::ZOMBIE_SQUASH_HEAD;
}

bool Zombie::ZombieTypeCanGoInPool(ZombieType theZombieType)
{
	return
		theZombieType == ZombieType::ZOMBIE_NORMAL ||
		theZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE ||
		theZombieType == ZombieType::ZOMBIE_PAIL ||
		theZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET ||
		theZombieType == ZombieType::ZOMBIE_FLAG ||
		theZombieType == ZombieType::ZOMBIE_SNORKEL ||
		theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
		theZombieType == ZombieType::ZOMBIE_PEA_HEAD ||
		theZombieType == ZombieType::ZOMBIE_WALLNUT_HEAD ||
		theZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD ||
		theZombieType == ZombieType::ZOMBIE_GATLING_HEAD ||
		theZombieType == ZombieType::ZOMBIE_TALLNUT_HEAD;
}

bool Zombie::ZombieTypeCanGoOnHighGround(ZombieType theZombieType)
{
	return theZombieType != ZombieType::ZOMBIE_ZAMBONI && theZombieType != ZombieType::ZOMBIE_BOBSLED;
}

void Zombie::UpdateZombieChimney()
{
	if (mBoard->mBackground == BackgroundType::BACKGROUND_5_ROOF || mBoard->mBackground == BackgroundType::BACKGROUND_6_BOSS)
	{
		mAltitude = PvzpAnimateCurve(4000, 5000, mBoard->mCutScene->mCutsceneTime, 200, 0, PvzpCurves::CURVE_EASE_IN);
	}
}

void Zombie::WalkIntoHouse()
{
	AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, nullptr);
	mFromWave = Zombie::ZOMBIE_WAVE_WINNER;
	ReanimReenableClipping();

	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
	{
		mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
		StartWalkAnim(0);
	}

	if (mBoard->mBackground == BackgroundType::BACKGROUND_1_DAY || mBoard->mBackground == BackgroundType::BACKGROUND_2_NIGHT ||
		mBoard->mBackground == BackgroundType::BACKGROUND_3_POOL || mBoard->mBackground == BackgroundType::BACKGROUND_4_FOG)
	{
		mPosY = 290.0f;
		mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ZOMBIE, 2, 0);

		if (ZombieRules::IsGargantuarType(mZombieType))
		{
			mPosY += 30.0f;
		}
		else if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
		{
			mPosX += 35.0f;
		}
		else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
		{
			mPosY += 15.0f;
		}

		if (mBoard->StageHasPool())
		{
			if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
			{
				mPosX -= 10.0f;
			}
			else
			{
				mPosX -= 80.0f;
			}
		}
	}
	else if (mBoard->mBackground == BackgroundType::BACKGROUND_5_ROOF || mBoard->mBackground == BackgroundType::BACKGROUND_6_BOSS)
	{
		mPosX = -180.0f;
		mPosY = 250.0f;
		mZombieHeight = ZombieHeight::HEIGHT_IN_TO_CHIMNEY;
		mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 0, 2);

		if (ZombieRules::IsGargantuarType(mZombieType))
		{
			mPosY += 5.0f;
		}
		else if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
		{
			mPosX -= 14.0f;
		}
		else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
		{
			mPosX -= 28.0f;
		}

		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim && aBodyReanim->TrackExists("anim_idle") && mZombieType != ZombieType::ZOMBIE_POLEVAULTER)
		{
			PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		}
	}
}
