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

Zombie::Zombie()
{
}

void Zombie::ZombieInitialize(int theRow, ZombieType theType, bool theVariant, Zombie* theParentZombie, int theFromWave)
{
	PVZP_ASSERT(theType >= 0 && theType <= ZombieType::NUM_ZOMBIE_TYPES);

	int aZombatarRecordIndex = -1;
	if (theType == ZombieType::ZOMBIE_FLAG && mBoard)
	{
		PlayerInfo* aPlayerInfo = mApp->mPlayerInfo;
		if (aPlayerInfo && !aPlayerInfo->mZombatarData.empty())
		{
			int aCount = static_cast<int>(aPlayerInfo->mZombatarData.size() / ZOMBATAR_RECORD_SIZE);
			if (aCount > 0)
				aZombatarRecordIndex = Rand(aCount);
		}
	}

	mFromWave = theFromWave;
	mContinuousHealthRemainder = 0.0f;
	mSniperWoundCounter = 0;
	mSniperDotRemainder = 0;
	mRow = theRow;
	mPosX = mApp->mWidth - 20 + Rand(ZOMBIE_START_RANDOM_OFFSET);
	mPosY = GetPosYBasedOnRow(theRow);
	mVelX = 0.0f;
	mVelZ = 0.0f;
	mWidth = 120;
	mHeight = 120;
	mFrame = 0;
	mPrevFrame = 0;
	mZombieType = theType;
	mVariant = theVariant;
	mIsEating = false;
	mJustGotShotCounter = 0;
	mShieldJustGotShotCounter = 0;
	mShieldRecoilCounter = 0;
	mChilledCounter = 0;
	mIceTrapCounter = 0;
	mButteredCounter = 0;
	mMindControlled = false;
	mBlowingAway = false;
	mBloverKnockbackDistanceRemaining = 0;
	mEphraimStaggerCounter = 0;
	mEphraimKnockbackDistanceRemaining = 0;
	mHasHead = true;
	mHasArm = true;
	mHasObject = false;
	mInPool = false;
	mSpawnedByZombieRain = false;
	mDolphinFirstLeapComplete = false;
	mThreeMillionSunDurabilityApplied = false;
	mOnHighGround = false;
	mHelmType = HelmType::HELMTYPE_NONE;
	mShieldType = ShieldType::SHIELDTYPE_NONE;
	mYuckyFace = false;
	mYuckyFaceCounter = 0;
	mAnimCounter = 0;
	mGroanCounter = RandRangeInt(300, 400);
	mAnimTicksPerFrame = 12;
	mAnimFrames = 12;
	mZombieAge = 0;
	mTargetCol = -1;
	mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
	mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
	mPhaseCounter = 0;
	mHitUmbrella = false;
	mDroppedLoot = false;
	mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
	mZombieRect = Rect(36, 0, 42, 115);
	mZombieAttackRect = Rect(50, 0, 20, 115);
	mPlayingSong = false;
	mZombieFade = -1;
	mFlatTires = false;
	mScaleZombie = 1.0f;
	mUseLadderCol = -1;
	mShieldHealth = 0;
	mHelmHealth = 0;
	mTierBucketArmorHealth = 0;
	mTierBucketArmorMaxHealth = 0;
	mAltitude = 0.0f;
	mFlyingHealth = 0;
	mOriginalAnimRate = 0.0f;
	mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	mSummonCounter = 0;
	mBossStompCounter = -1;
	mBossBungeeCounter = -1;
	mBossHeadCounter = -1;
	mBodyReanimID = ReanimationID::REANIMATIONID_NULL;
	mTargetPlantID = PlantID::PLANTID_NULL;
	mFirstIgnoredSpikyPlantID = PlantID::PLANTID_NULL;
	mBossMode = 0;
	mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
	mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
	mTargetRow = -1;
	mFireballRow = -1;
	mIsFireBall = false;
	mMoweredReanimID = ReanimationID::REANIMATIONID_NULL;
	mZombatarHeadReanimID = ReanimationID::REANIMATIONID_NULL;
	mLastPortalX = -1;
	for (int i = 0; i < MAX_ZOMBIE_FOLLOWERS; i++)
	{
		mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
	}
	if (mBoard && mBoard->IsFlagWave(mFromWave))
	{
		mPosX += 40.0f;
	}
	PickRandomSpeed();
	mBodyHealth = 270;

	const ZombieDefinition& aZombieDef = GetZombieDefinition(mZombieType);
	RenderLayer aRenderLayer = RenderLayer::RENDER_LAYER_ZOMBIE;
	int aRenderOffset = 4;
	if (aZombieDef.mReanimationType != ReanimationType::REANIM_NONE)
	{
		LoadReanim(aZombieDef.mReanimationType);
	}

	switch (theType)
	{
	case ZombieType::ZOMBIE_NORMAL:
		LoadPlainZombieReanim();
		break;

	case ZombieType::ZOMBIE_DUCKY_TUBE:
		LoadPlainZombieReanim();
		break;

	case ZombieType::ZOMBIE_TRAFFIC_CONE:
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_cone", RENDER_GROUP_NORMAL);
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		mHelmType = HelmType::HELMTYPE_TRAFFIC_CONE;
		mHelmHealth = 370;
		break;

	case ZombieType::ZOMBIE_PAIL:
	case ZombieType::ZOMBIE_BULWARK_BUCKET:
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_bucket", RENDER_GROUP_NORMAL);
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		mHelmType = HelmType::HELMTYPE_PAIL;
		mHelmHealth = 1100;
		break;

	case ZombieType::ZOMBIE_DOOR:
		mShieldType = ShieldType::SHIELDTYPE_DOOR;
		mShieldHealth = 1100;
		LoadPlainZombieReanim();
		AttachShield();
		break;

	case ZombieType::ZOMBIE_YETI:
		mBodyHealth = 1350;
		mPhaseCounter = RandRangeInt(1500, 2000);
		mHasObject = true;
		mZombieAttackRect = Rect(20, 0, 50, 115);
		break;

	case ZombieType::ZOMBIE_LADDER:
		mBodyHealth = 500;
		mShieldHealth = 500;
		mShieldType = ShieldType::SHIELDTYPE_LADDER;
		mZombieAttackRect = Rect(10, 0, 50, 115);
		if (IsOnBoard())
		{
			mZombiePhase = ZombiePhase::PHASE_LADDER_CARRYING;
			StartWalkAnim(0);
		}
		AttachShield();
		break;

	case ZombieType::ZOMBIE_BUNGEE:
	{
		mBodyHealth = 450;
		mAnimFrames = 4;
		mAltitude = BUNGEE_ZOMBIE_HEIGHT + RandRangeInt(0, 150);
		mVelX = 0.0f;

		if (IsOnBoard())
		{
			PickBungeeZombieTarget(-1);

			if (mDead)
			{
				return;
			}

			mZombiePhase = ZombiePhase::PHASE_BUNGEE_DIVING;
		}
		else
		{
			mZombiePhase = ZombiePhase::PHASE_BUNGEE_CUTSCENE;
			mPhaseCounter = RandRangeInt(0, 200);
		}

		PlayZombieReanim("anim_drop", ReanimLoopType::REANIM_LOOP, 0, 24.0f);
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_bungi_rightarm_lower2", RENDER_GROUP_ARMS);
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_bungi_rightarm_hand2", RENDER_GROUP_ARMS);
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_bungi_leftarm_lower2", RENDER_GROUP_ARMS);
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_bungi_leftarm_hand2", RENDER_GROUP_ARMS);
		aBodyReanim->SetTruncateDisappearingFrames(nullptr, false);

		aRenderLayer = RenderLayer::RENDER_LAYER_GRAVE_STONE;
		aRenderOffset = 7;
		mZombieRect = Rect(-20, 22, 110, 94);
		mZombieAttackRect = Rect(0, 0, 0, 0);
		mVariant = false;
		break;
	}

	case ZombieType::ZOMBIE_FOOTBALL:
		mZombieRect = Rect(50, 0, 57, 115);
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		mHelmType = HelmType::HELMTYPE_FOOTBALL;
		mHelmHealth = 1400;
		mAnimTicksPerFrame = 6;
		mVariant = false;
		break;

	case ZombieType::ZOMBIE_DIGGER:
	{
		mHelmType = HelmType::HELMTYPE_DIGGER;
		mHelmHealth = 100;
		mVariant = false;
		mHasObject = true;
		mZombieRect = Rect(50, 0, 28, 115);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		aBodyReanim->SetTruncateDisappearingFrames(nullptr, false);

		if (!IsOnBoard())
		{
			mZombiePhase = ZombiePhase::PHASE_DIGGER_CUTSCENE;
		}
		else
		{
			mZombiePhase = ZombiePhase::PHASE_DIGGER_TUNNELING;
			AddAttachedParticle(60, 100, ParticleEffect::PARTICLE_DIGGER_TUNNEL);
			aRenderOffset = 7;
			PlayZombieReanim("anim_dig", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
			PickRandomSpeed();
		}

		break;
	}

	case ZombieType::ZOMBIE_POLEVAULTER:
		mBodyHealth = 500;
		mAnimTicksPerFrame = 6;
		mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT;
		mHasObject = true;
		mVariant = false;
		mPosX = mApp->mWidth + 70 + Rand(10);
		if (IsOnBoard())
		{
			PlayZombieReanim("anim_run", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
			PickRandomSpeed();
		}
		if (mApp->IsWallnutBowlingLevel())
		{
			mZombieAttackRect = Rect(-229, 0, 270, 115);
		}
		else
		{
			mZombieAttackRect = Rect(-29, 0, 70, 115);
		}
		break;

	case ZombieType::ZOMBIE_DOLPHIN_RIDER:
		mBodyHealth = 500;
		mAnimTicksPerFrame = 6;
		mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING;
		mVariant = false;
		if (IsOnBoard())
		{
			PlayZombieReanim("anim_walkdolphin", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
			PickRandomSpeed();
		}
		SetupWaterTrack("zombie_dolphinrider_whitewater");
		SetupWaterTrack("zombie_dolphinrider_dolphininwater");
		break;

	case ZombieType::ZOMBIE_GARGANTUAR:
	case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
	case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
	{
		mWidth = 180;
		mHeight = 180;
		mBodyHealth = 3000;
		mAnimFrames = 24;
		mAnimTicksPerFrame = 8;
		mPosX = mApp->mWidth + 45 + Rand(10);
		mZombieRect = Rect(-17, -38, 125, 154);
		mZombieAttackRect = Rect(-30, -38, 89, 154);
		mVariant = false;
		aRenderOffset = 8;
		mHasObject = mZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR;

		int aPoleHit = Rand(100);
		int aPoleVariant;
		if (!IsOnBoard() || mBoard->mLevel == 48)
		{
			aPoleVariant = 0;
		}
		else
		{
			aPoleVariant = aPoleHit < 10 ? 2 : aPoleHit < 35 ? 1 : 0;
		}

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aPoleVariant == 2)
		{
			aBodyReanim->SetImageOverride("Zombie_gargantuar_telephonepole", IMAGE_REANIM_ZOMBIE_GARGANTUAR_ZOMBIE);
		}
		else if (aPoleVariant == 1)
		{
			aBodyReanim->SetImageOverride("Zombie_gargantuar_telephonepole", IMAGE_REANIM_ZOMBIE_GARGANTUAR_DUCKXING);
		}

		if (mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD_REDEYE);
			mBodyHealth = 6000;
		}
		else if (mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
		{
			mBodyHealth = 4500;
		}

		break;
	}

	case ZombieType::ZOMBIE_ZAMBONI:
		mBodyHealth = 1350;
		mAnimFrames = 2;
		mAnimTicksPerFrame = 8;
		mPosX = mApp->mWidth + Rand(10);
		aRenderOffset = 8;
		PlayZombieReanim("anim_drive", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		mZombieRect = Rect(0, -13, 153, 140);
		mZombieAttackRect = Rect(10, -13, 133, 140);
		mVariant = false;
		break;

	case ZombieType::ZOMBIE_CATAPULT:
		mBodyHealth = 850;
		mPosX = mApp->mWidth + 25 + Rand(10);
		mSummonCounter = 20;
		if (IsOnBoard())
		{
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 0, 5.5f);
		}
		else
		{
			PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 8.0f);
		}
		mZombieRect = Rect(0, -13, 153, 140);
		mZombieAttackRect = Rect(10, -13, 133, 140);
		mVariant = false;
		break;

	case ZombieType::ZOMBIE_SNORKEL:
		mZombieRect = Rect(12, 0, 62, 115);
		mZombieAttackRect = Rect(-5, 0, 55, 115);
		SetupWaterTrack("Zombie_snorkle_whitewater");
		SetupWaterTrack("Zombie_snorkle_whitewater2");
		mVariant = false;
		mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
		break;

	case ZombieType::ZOMBIE_JACK_IN_THE_BOX:
	{
		mBodyHealth = 500;
		mAnimTicksPerFrame = 6;

		int aDistance = 450 + Rand(300);
		if (Rand(20) == 0)  // chance of an early explosion
		{
			aDistance /= 3;
		}
		mPhaseCounter = static_cast<int>(aDistance / mVelX) * ZOMBIE_LIMP_SPEED_FACTOR;
		mZombieAttackRect = Rect(20, 0, 50, 115);

		if (mApp->IsScaryPotterLevel())
		{
			mPhaseCounter = 10;
		}
		if (IsOnBoard())
		{
			mZombiePhase = ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING;
		}

		break;
	}

	case ZombieType::ZOMBIE_BOBSLED:
	{
		aRenderOffset = 3;

		if (theParentZombie)
		{
			int aPosition = 0;
			while (aPosition < NUM_BOBSLED_FOLLOWERS && theParentZombie->mFollowerZombieID[aPosition] != ZombieID::ZOMBIEID_NULL)
			{
				aPosition++;
			}
			PVZP_ASSERT(aPosition < 3);
			theParentZombie->mFollowerZombieID[aPosition] = mBoard->ZombieGetID(this);
			mRelatedZombieID = mBoard->ZombieGetID(theParentZombie);

			mPosX = theParentZombie->mPosX + (aPosition + 1) * 50;
			if (aPosition == 0)
			{
				aRenderOffset = 1;
				mAltitude = 9.0f;
			}
			else if (aPosition == 1)
			{
				aRenderOffset = 2;
				mAltitude = -7.0f;
			}
			else
			{
				aRenderOffset = 0;
				mAltitude = 9.0f;
			}
		}
		else
		{
			mPosX = mApp->mWidth + 80;
			mZombieRect = Rect(-50, 0, 275, 115);
			mHelmType = HelmType::HELMTYPE_BOBSLED;
			mHelmHealth = 300;
			mAltitude = -10.0f;
		}

		mVelX = 0.6f;
		mZombiePhase = ZombiePhase::PHASE_BOBSLED_SLIDING;
		mPhaseCounter = mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? MILLION_SUN_BOBSLED_SLIDE_TICKS : 500;
		mVariant = false;

		if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			PlayZombieReanim("anim_jump", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 20.0f);
			mApp->ReanimationGet(mBodyReanimID)->mAnimTime = 1.0f;
			mAltitude = 18.0f;
		}
		else if (IsOnBoard())
		{
			PlayZombieReanim("anim_push", ReanimLoopType::REANIM_LOOP, 0, 30.0f);
		}

		break;
	}

	case ZombieType::ZOMBIE_FLAG:
	{
		mHasObject = true;
		LoadPlainZombieReanim();

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		Reanimation* aFlagReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FLAG);
		aFlagReanim->PlayReanim("Zombie_flag", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aFlagReanim);
		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Zombie_flaghand");
		AttachReanim(aTrackInstance->mAttachmentID, aFlagReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		SetupZombatarFlagReanim(aZombatarRecordIndex);

			mPosX = mApp->mWidth;
		break;
	}

	case ZombieType::ZOMBIE_POGO:
		mVariant = false;
		mZombiePhase = ZombiePhase::PHASE_POGO_BOUNCING;
		mPhaseCounter = Rand(POGO_BOUNCE_TIME) + 1;
		mHasObject = true;
		mBodyHealth = 500;
		mZombieAttackRect = Rect(10, 0, 30, 115);
		PlayZombieReanim("anim_pogo", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 40.0f);
		mApp->ReanimationGet(mBodyReanimID)->mAnimTime = 1.0f;
		break;

	case ZombieType::ZOMBIE_NEWSPAPER:
		mZombieAttackRect = Rect(20, 0, 50, 115);
		mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_READING;
		mShieldType = ShieldType::SHIELDTYPE_NEWSPAPER;
		mShieldHealth = 150;
		mVariant = false;
		AttachShield();
		break;

	case ZombieType::ZOMBIE_BALLOON:
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		aBodyReanim->SetTruncateDisappearingFrames(nullptr, false);

		if (IsOnBoard())
		{
			mAltitude = 25.0f;
			mZombiePhase = ZombiePhase::PHASE_BALLOON_FLYING;
			PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, aBodyReanim->mAnimRate);
		}
		else
		{
			float aAnimRate = RandRangeFloat(8.0f, 10.0f);
			SetAnimRate(aAnimRate);
		}

		Reanimation* aPropellerReanim = mApp->AddReanimation(0.0f, 0.0f, 0, aZombieDef.mReanimationType);
		aPropellerReanim->SetFramesForLayer("Propeller");
		aPropellerReanim->mLoopType = ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME;
		aPropellerReanim->AttachToAnotherReanimation(aBodyReanim, "hat");

		mFlyingHealth = 20;
		mZombieRect = Rect(36, 30, 42, 115);
		mZombieAttackRect = Rect(20, 30, 50, 115);
		mVariant = false;
		break;
	}

	case ZombieType::ZOMBIE_DANCER:
		mScaleZombie = 0.8f;
		if (!IsOnBoard())
		{
			PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		}
		else
		{
			mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_IN;
			mVelX = 0.5f;
			mPhaseCounter = 300 + Rand(12);
			PlayZombieReanim("anim_moonwalk", ReanimLoopType::REANIM_LOOP, 0, 24.0f);
		}
		mBodyHealth = 500;
		mVariant = false;
		break;

	case ZombieType::ZOMBIE_BACKUP_DANCER:
		mScaleZombie = 0.8f;
		if (!IsOnBoard())
		{
			PlayZombieReanim("anim_armraise", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		}
		mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_LEFT;
		mVariant = false;
		break;

	case ZombieType::ZOMBIE_IMP:
		if (!IsOnBoard())
		{
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		}
		if (mApp->IsIZombieLevel())
		{
			mBodyHealth = 70;
		}
		break;

	case ZombieType::ZOMBIE_BOSS:
		mPosX = 0.0f;
		mPosY = 0.0f;
		mZombieRect = Rect(700, 80, 90, 430);
		mZombieAttackRect = Rect(0, 0, 0, 0);
		aRenderLayer = RenderLayer::RENDER_LAYER_TOP;
		mBodyHealth = mApp->IsAdventureMode() ? 40000 : 60000;
		if (IsOnBoard())
		{
			PlayZombieReanim("anim_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
			mSummonCounter = 500;
			mBossHeadCounter = 5000;
			mZombiePhase = ZombiePhase::PHASE_BOSS_ENTER;
		}
		else
		{
			PlayZombieReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		}
		BossSetupReanim();
		break;

	case ZombieType::ZOMBIE_PEA_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head2", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (IsOnBoard())
		{
			aBodyReanim->SetFramesForLayer("anim_walk2");
		}

		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("anim_head1");
		aTrackInstance->mImageOverride = IMAGE_BLANK;
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_PEASHOOTER);
		aHeadReanim->PlayReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 65.0f, -5.0f, 0.2f, -1.0f, 1.0f);

		mPhaseCounter = 150;
		mVariant = false;
		break;
	}

	case ZombieType::ZOMBIE_WALLNUT_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("Zombie_tie", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Zombie_body");
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT);
		aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 50.0f, 0.0f, 0.2f, -0.8f, 0.8f);

		mHelmType = HelmType::HELMTYPE_WALLNUT;
		mHelmHealth = 1100;
		mVariant = false;
		break;
	}

	case ZombieType::ZOMBIE_TALLNUT_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("Zombie_tie", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Zombie_body");
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TALLNUT);
		aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 37.0f, 0.0f, 0.2f, -0.8f, 0.8f);

		mHelmType = HelmType::HELMTYPE_TALLNUT;
		mHelmHealth = 2200;
		mVariant = false;
		mPosX += 30.0f;
		break;
	}

	case ZombieType::ZOMBIE_JALAPENO_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("Zombie_tie", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Zombie_body");
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_JALAPENO);
		aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 55.0f, -5.0f, 0.2f, -1.0f, 1.0f);

		mVariant = false;
		mBodyHealth = 500;
		int aDistance = 275 + Rand(175);
		mPhaseCounter = static_cast<int>(aDistance / mVelX) * ZOMBIE_LIMP_SPEED_FACTOR;
		break;
	}

	case ZombieType::ZOMBIE_GATLING_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head2", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (IsOnBoard())
		{
			aBodyReanim->SetFramesForLayer("anim_walk2");
		}

		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("anim_head1");
		aTrackInstance->mImageOverride = IMAGE_BLANK;
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_GATLINGPEA);
		aHeadReanim->PlayReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 65.0f, -5.0f, 0.2f, -1.0f, 1.0f);

		mPhaseCounter = 150;
		mVariant = false;
		break;
	}

	case ZombieType::ZOMBIE_SQUASH_HEAD:
	{
		LoadPlainZombieReanim();
		ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_head2", RENDER_GROUP_HIDDEN);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (IsOnBoard())
		{
			aBodyReanim->SetFramesForLayer("anim_walk2");
		}

		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("anim_head1");
		aTrackInstance->mImageOverride = IMAGE_BLANK;
		Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_SQUASH);
		aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		aBodyReanim->mFrameBasePose = 0;
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, 55.0f, -15.0f, 0.2f, -0.75f, 0.75f);

		mZombiePhase = ZombiePhase::PHASE_SQUASH_PRE_LAUNCH;
		mVariant = false;
		break;
	}
	case ZombieType::ZOMBIE_CACHED_POLEVAULTER_WITH_POLE:
	case ZombieType::NUM_ZOMBIE_TYPES:
	case ZombieType::NUM_CACHED_ZOMBIE_TYPES:
	case ZombieType::ZOMBIE_INVALID:
		break;
	}

	if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
	{
		float aAnimRate = RandRangeFloat(8.0f, 10.0f);
		PlayZombieReanim("anim_aquarium_swim", ReanimLoopType::REANIM_LOOP, 0, aAnimRate);

		mZombieHeight = ZombieHeight::HEIGHT_ZOMBIQUARIUM;
		mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
		mPhaseCounter = 200;
		mBodyHealth = 200;
		mSummonCounter = RandRangeInt(200, 400);
	}

	if (mApp->IsLittleTroubleLevel() && (IsOnBoard() || theFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE))
	{
		mScaleZombie = 0.5f;
		mBodyHealth /= 4;
		mHelmHealth /= 4;
		mShieldHealth /= 4;
		mFlyingHealth /= 4;
	}

	UpdateAnimSpeed();
	if (mVariant)
	{
		ReanimShowPrefix("anim_tongue", RENDER_GROUP_NORMAL);
	}

	mBodyMaxHealth = mBodyHealth;
	mHelmMaxHealth = mHelmHealth;
	mShieldMaxHealth = mShieldHealth;
	mFlyingMaxHealth = mFlyingHealth;
	mDead = false;
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY);
	mRenderOrder = Board::MakeRenderOrder(aRenderLayer, mRow, aRenderOffset);
	if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
	{
		mBodyMaxHealth = 300;
	}
	if (IsOnBoard() && mBoard->mZombieStrengthTier > 0)
		mBoard->ApplyZombieStrengthTierToZombie(this, 0, mBoard->mZombieStrengthTier);
	if (IsOnBoard())
		mBoard->EnsureZombieTierBucketArmor(this);

	if (IsOnBoard())
	{
		PlayZombieAppearSound();
		StartZombieSound();
	}

	UpdateReanim();
}

void Zombie::UpdateZombieJackInTheBox()
{
	if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING)
	{
		if (mPhaseCounter <= 0 && mHasHead)
		{
			mPhaseCounter = 110;
			mZombiePhase = ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING;

			StopZombieSound();
			mApp->PlaySample(SOUND_BOING);
			PlayZombieReanim("anim_pop", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 28.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING)
	{
		if (mPhaseCounter == 80)
		{
			mApp->PlayFoley(FoleyType::FOLEY_JACK_SURPRISE);
		}

		if (mPhaseCounter <= 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);

			int aPosX = mX + mWidth / 2;
			int aPosY = mY + mHeight / 2;
			if (mMindControlled)
			{
				mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, JACK_IN_THE_BOX_ZOMBIE_RADIUS, 1, true, 127);
			}
			else
			{
				mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, JACK_IN_THE_BOX_ZOMBIE_RADIUS, 1, true, 255);
				mBoard->KillAllPlantsInRadius(aPosX, aPosY, JACK_IN_THE_BOX_PLANT_RADIUS, this);
			}

			mApp->AddPvzpParticle(aPosX, aPosY, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_JACKEXPLODE);
			mBoard->ShakeBoard(4, -6);
			DieNoLoot();

			if (mApp->IsScaryPotterLevel())
			{
				mBoard->mChallenge->ScaryPotterJackExplode(aPosX, aPosY);
			}
		}
	}
}

bool Zombie::IsOnBoard()
{
	if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE || mFromWave == Zombie::ZOMBIE_WAVE_UI)
	{
		return false;
	}

	PVZP_ASSERT(mBoard);
	return true;
}

void Zombie::UpdateBurn()
{
	mPhaseCounter--;
	if (mPhaseCounter == 0)
	{
		DieWithLoot();
	}
}

void Zombie::Update()
{
	PVZP_ASSERT(!mDead);

	mZombieAge++;
	bool doUpdate = false;
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		doUpdate = true;
	}
	else if (IsOnBoard() && mBoard->mCutScene->ShouldRunUpsellBoard())
	{
		doUpdate = true;
	}
	else if (mApp->mGameScene == GameScenes::SCENE_PLAYING || !IsOnBoard() || mFromWave == Zombie::ZOMBIE_WAVE_WINNER)
	{
		doUpdate = true;
	}

	if (doUpdate)
	{
		UpdateSniperWound();
		if (mDead)
			return;
		if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
		{
			UpdateBurn();
		}
		else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
		{
			UpdateMowered();
		}
		else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
		{
			UpdateDeath();
			UpdateZombieWalking();
		}
		else
		{
			if (mPhaseCounter > 0 && !IsImmobilizied())
			{
				mPhaseCounter--;
			}

			if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
			{
				if (mBoard->mCutScene->ShowZombieWalking())
				{
					UpdateZombieChimney();
					UpdateZombieWalking();

				}
			}
			else if (IsOnBoard())
			{
				UpdatePlaying();
			}

			if (mZombieType == ZombieType::ZOMBIE_BUNGEE && mBloverKnockbackDistanceRemaining <= 0)
			{
				UpdateZombieBungee();
			}
			if (mZombieType == ZombieType::ZOMBIE_POGO && mBloverKnockbackDistanceRemaining <= 0 && mEphraimStaggerCounter <= 0 &&
				!(mSpawnedByZombieRain && mZombieHeight == ZombieHeight::HEIGHT_FALLING))
			{
				UpdateZombiePogo();
			}

			Animate();
		}

		mJustGotShotCounter--;
		if (mShieldJustGotShotCounter > 0)
		{
			mShieldJustGotShotCounter--;
		}
		if (mShieldRecoilCounter > 0)
		{
			mShieldRecoilCounter--;
		}
		if (mZombieFade > 0)
		{
			mZombieFade--;
			if (mZombieFade == 0)
			{
				DieNoLoot();
			}
		}

		mX = static_cast<int>(mPosX);
		mY = static_cast<int>(mPosY);

		AttachmentUpdateAndMove(mAttachmentID, mPosX, mPosY);
		UpdateReanim();
	}
}

void Zombie::UpdateClimbingLadder()
{
	float aDistOffGround = mAltitude;
	if (mOnHighGround)
	{
		aDistOffGround -= HIGH_GROUND_HEIGHT;
	}
	int aLadderOriginX = mBoard->PixelToGridXKeepOnBoard(mX + 5 + aDistOffGround * 0.5f, mY);
	if (mBoard->GetLadderAt(aLadderOriginX, mRow) == nullptr)
	{
		mZombieHeight = ZombieHeight::HEIGHT_FALLING;
		return;
	}

	mAltitude += 0.8f;
	if (mVelX < 0.5f)
	{
		mPosX -= 0.5f;
	}

	float aTargetHeight = 90.0f;
	if (mOnHighGround)
	{
		aTargetHeight += HIGH_GROUND_HEIGHT;
	}
	if (mAltitude >= aTargetHeight)
	{
		mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	}
}

void Zombie::UpdateActions()
{
	if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER)
	{
		UpdateClimbingLadder();
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
	{
		UpdateZombiquarium();
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL || mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL || mInPool)
	{
		UpdateZombiePool();
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND || mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
	{
		UpdateZombieHighGround();
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_FALLING)
	{
		UpdateZombieFalling();
	}
	if (mSpawnedByZombieRain && mZombieHeight != ZombieHeight::HEIGHT_FALLING && !mInPool && !IsFlying() &&
		Zombie::ZombieTypeCanGoInPool(mZombieType) && mBoard != nullptr &&
		mBoard->IsPoolSquare(mBoard->PixelToGridXKeepOnBoard(static_cast<int>(mPosX + mWidth / 2), static_cast<int>(mPosY)), mRow))
	{
		if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
		{
			mInPool = true;
			mZombiePhase = ZombiePhase::PHASE_DOLPHIN_RIDING;
			mZombieAttackRect = Rect(-29, 0, 70, 115);
			PlayZombieReanim("anim_ride", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
			PoolSplash(true);
		}
		else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
		{
			mInPool = true;
			mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL;
			PlayZombieReanim("anim_swim", ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
			PoolSplash(true);
		}
		else
		{
			mInPool = true;
			mZombieHeight = ZombieHeight::HEIGHT_IN_TO_POOL;
			PoolSplash(true);
		}
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY)
	{
		UpdateZombieChimney();
	}

	if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		UpdateZombiePolevaulter();
	}
	if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		UpdateZombieCatapult();
	}
	if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
	{
		UpdateZombieDolphinRider();
	}
	if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		UpdateZombieSnorkel();
	}
	if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		UpdateZombieFlyer();
	}
	if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
	{
		UpdateZombieNewspaper();
	}
	if (mZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		UpdateZombieDigger();
	}
	if (mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
	{
		UpdateZombieJackInTheBox();
	}
	if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
		mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
	{
		UpdateZombieGargantuar();
	}
	if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		UpdateZombieBobsled();
	}
	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		UpdateZamboni();
	}
	if (mZombieType == ZombieType::ZOMBIE_LADDER)
	{
		UpdateLadder();
	}
	if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		UpdateYeti();
	}
	if (mZombieType == ZombieType::ZOMBIE_DANCER)
	{
		UpdateZombieDancer();
	}
	if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		UpdateZombieBackupDancer();
	}
	if (mZombieType == ZombieType::ZOMBIE_IMP ||
		((mZombieType == ZombieType::ZOMBIE_PAIL || mZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET) &&
			mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN))
	{
		UpdateZombieThrown();
	}
	if (mZombieType == ZombieType::ZOMBIE_PEA_HEAD)
	{
		UpdateZombiePeaHead();
	}
	if (mZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD)
	{
		UpdateZombieJalapenoHead();
	}
	if (mZombieType == ZombieType::ZOMBIE_GATLING_HEAD)
	{
		UpdateZombieGatlingHead();
	}
	if (mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD)
	{
		UpdateZombieSquashHead();
	}
}

void Zombie::CheckForBoardEdge()
{
	if (IsWalkingBackwards() && mPosX > 850.0f)
	{
		DieNoLoot();
		return;
	}

	int aEdgeX = BOARD_EDGE;
	if (ZombieRules::IsGargantuarType(mZombieType) || mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		aEdgeX = -150;
	}
	else if (mZombieType == ZombieType::ZOMBIE_CATAPULT || mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		aEdgeX = -175;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER || mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		aEdgeX = -130;
	}

	if (mX <= aEdgeX && mHasHead)
	{
		if (mApp->IsIZombieLevel())
		{
			DieNoLoot();
		}
		else
		{
			mBoard->ZombiesWon(this);
		}
	}
	if (mX <= aEdgeX + 70 && !mHasHead)
	{
		TakeDamage(1800, 9U);
	}
}

void Zombie::UpdatePlaying()
{
	mBoard->EnsureZombieTierBucketArmor(this);
	PVZP_ASSERT(mBodyHealth > 0 || mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING);
	bool aDolphinButterException = ZombieEffects::CanDolphinBeButterStunned(this);
	if (mBoard->mZombieTierSunMoney >= TWO_AND_HALF_MILLION_SUN_THRESHOLD &&
		mZombieType != ZombieType::ZOMBIE_BUNGEE && !aDolphinButterException && mButteredCounter > 0)
	{
		mButteredCounter = 0;
		RemoveButter();
	}
	if (((ZombieRules::IsGargantuarType(mZombieType) || ZombieRules::IsBulwarkType(mZombieType)) &&
		mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD) ||
		(ZombieRules::IsConeOrBucketZombie(mZombieType) && mBoard->mZombieTierSunMoney >= TWO_MILLION_SUN_THRESHOLD) ||
		mBoard->mZombieTierSunMoney >= THREE_AND_HALF_MILLION_SUN_THRESHOLD)
	{
		bool aWasChilled = mChilledCounter > 0;
		mChilledCounter = 0;
		if (mIceTrapCounter > 0)
			RemoveIceTrap();
		else if (aWasChilled)
			UpdateAnimSpeed();
		if (mButteredCounter > 0 && mZombieType != ZombieType::ZOMBIE_BUNGEE && !aDolphinButterException)
		{
			mButteredCounter = 0;
			RemoveButter();
		}
	}

	mGroanCounter--;
	int aZombiesCount = mBoard->mZombies.mSize;
	if (mGroanCounter == 0 && Rand(aZombiesCount) == 0 && mHasHead && mZombieType != ZombieType::ZOMBIE_BOSS && !mBoard->HasLevelAwardDropped())
	{
		float aPitch = 0.0f;
		if (mApp->IsLittleTroubleLevel())
		{
			aPitch = RandRangeFloat(40.0f, 50.0f);
		}

		if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR)
		{
			mApp->PlayFoley(FoleyType::FOLEY_LOW_GROAN);
		}
		else if (mVariant)
		{
			mApp->PlayFoleyPitch(FoleyType::FOLEY_BRAINS, aPitch);
		}
		else if (mApp->mSukhbirMode)
		{
			mApp->PlayFoleyPitch(FoleyType::FOLEY_SUKHBIR, aPitch);
		}
		else
		{
			mApp->PlayFoleyPitch(FoleyType::FOLEY_GROAN, aPitch);
		}

		mGroanCounter = Rand(1000) + 500;
	}

	if (mIceTrapCounter > 0)
	{
		mIceTrapCounter--;
		if (mIceTrapCounter == 0)
		{
			RemoveIceTrap();
			AddAttachedParticle(75, 106, ParticleEffect::PARTICLE_ICE_TRAP_RELEASE);
		}
	}
	if (mChilledCounter > 0)
	{
		mChilledCounter--;
		if (mChilledCounter == 0)
		{
			UpdateAnimSpeed();
		}
	}
	if (mButteredCounter > 0)
	{
		bool aIsGargantuar = ZombieRules::IsGargantuarType(mZombieType);
		if (mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR ||
			(mBoard != nullptr && mBoard->mZombieStrengthTier >= 3))
		{
			if (aIsGargantuar)
			{
				mButteredCounter = 0;
				RemoveButter();
			}
			else
			{
				mButteredCounter = std::min(mButteredCounter, 200);
			}
		}
		if (mButteredCounter > 0 && --mButteredCounter == 0)
		{
			RemoveButter();
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_BALLOON &&
		mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING && mFlyingHealth <= 0)
	{
		// A save can retain the flight phase after its balloon was destroyed.
		LandFlyer(0U);
		if (mDead)
			return;
	}
	// Clear legacy javelin control effects on popped Balloons before their
	// early returns can suspend the landing and one-shot pop animation.
	if (mZombieType == ZombieType::ZOMBIE_BALLOON &&
		(mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING || mZombieHeight == ZombieHeight::HEIGHT_FALLING) &&
		(mEphraimKnockbackDistanceRemaining != 0 || mEphraimStaggerCounter > 0))
	{
		mEphraimKnockbackDistanceRemaining = 0;
		mEphraimStaggerCounter = 0;
		UpdateAnimSpeed();
	}
	if (mZombieType == ZombieType::ZOMBIE_BALLOON && mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING)
	{
		// Restore a saved zero playback rate even when no stagger remains.
		mOriginalAnimRate = 24.0f;
		UpdateAnimSpeed(); // Retain genuine ice/butter pauses and chilled speed.
		if (mAltitude > (mOnHighGround ? HIGH_GROUND_HEIGHT : 0))
			mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	}
	if (mEphraimKnockbackDistanceRemaining != 0)
	{
		// Finish the push before counting down the stop at its destination.
		UpdateZombiePosition();
		return;
	}
	if (mEphraimStaggerCounter > 0)
	{
		if (--mEphraimStaggerCounter > 0)
			return;
		UpdateAnimSpeed();
	}

	if (mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
	{
		if (mBloverKnockbackDistanceRemaining > 0)
			UpdateZombiePosition();
		else
			UpdateZombieRiseFromGrave();
		return;
	}

	if (mBloverKnockbackDistanceRemaining > 0)
	{
		UpdateZombiePosition();
	}
	else if (!IsImmobilizied())
	{
		UpdateActions();
		UpdateZombiePosition();
		CheckIfPreyCaught();
		CheckForPool();
		CheckForHighGround();
		CheckForBoardEdge();
	}

	if (mZombieType == ZombieType::ZOMBIE_BOSS && mBloverKnockbackDistanceRemaining <= 0)
	{
		UpdateBoss();
	}

	if (!IsDeadOrDying() && mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
	{
		bool isDying = !mHasHead;
		if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
		{
			if (mBodyHealth < 200)
			{
				isDying = true;
			}
		}

		if (isDying)
		{
			int aDamage = 1;
			if (mZombieType == ZombieType::ZOMBIE_YETI)
			{
				aDamage = 10;
			}
			if (mBodyMaxHealth >= 500)
			{
				aDamage = 3;
			}

			if (Rand(5) == 0)
			{
				TakeDamage(aDamage, 9U);
			}
		}
	}
}

Zombie::~Zombie()
{
	AttachmentDie(mAttachmentID);
	StopZombieSound();
}
