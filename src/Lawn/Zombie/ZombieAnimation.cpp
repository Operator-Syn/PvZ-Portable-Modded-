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

static std::string ZombatarTrackName(const char* thePrefix, int theIndex)
{
	return std::format("{}{:02d}", thePrefix, theIndex);
}

void Zombie::SetupDoorArms(Reanimation* aReanim, bool theShow)
{
	int aArmGroup = RENDER_GROUP_NORMAL;
	int aDoorGroup = RENDER_GROUP_HIDDEN;
	if (theShow)
	{
		aArmGroup = RENDER_GROUP_HIDDEN;
		aDoorGroup = RENDER_GROUP_NORMAL;
	}

	aReanim->AssignRenderGroupToPrefix("Zombie_outerarm_hand", aArmGroup);
	aReanim->AssignRenderGroupToPrefix("Zombie_outerarm_lower", aArmGroup);
	aReanim->AssignRenderGroupToPrefix("Zombie_outerarm_upper", aArmGroup);
	aReanim->AssignRenderGroupToPrefix("anim_innerarm", aArmGroup);
	aReanim->AssignRenderGroupToPrefix("Zombie_outerarm_screendoor", aDoorGroup);
	aReanim->AssignRenderGroupToPrefix("Zombie_innerarm_screendoor", aDoorGroup);
	aReanim->AssignRenderGroupToPrefix("Zombie_innerarm_screendoor_hand", aDoorGroup);
}

void Zombie::SetupReanimLayers(Reanimation* aReanim, ZombieType theZombieType)
{
	aReanim->AssignRenderGroupToPrefix("anim_cone", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("anim_bucket", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("anim_screendoor", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("Zombie_flaghand", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("Zombie_duckytube", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("anim_tongue", RENDER_GROUP_HIDDEN);
	aReanim->AssignRenderGroupToPrefix("Zombie_mustache", RENDER_GROUP_HIDDEN);
	SetupDoorArms(aReanim, false);

	if (theZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE)
	{
		aReanim->AssignRenderGroupToPrefix("anim_cone", RENDER_GROUP_NORMAL);
		aReanim->AssignRenderGroupToPrefix("anim_hair", RENDER_GROUP_HIDDEN);
	}
	else if (theZombieType == ZombieType::ZOMBIE_PAIL || theZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET)
	{
		aReanim->AssignRenderGroupToPrefix("anim_bucket", RENDER_GROUP_NORMAL);
		aReanim->AssignRenderGroupToPrefix("anim_hair", RENDER_GROUP_HIDDEN);
	}
	else if (theZombieType == ZombieType::ZOMBIE_DOOR)
	{
		SetupDoorArms(aReanim, true);
	}
	else if (theZombieType == ZombieType::ZOMBIE_NEWSPAPER)
	{
		aReanim->AssignRenderGroupToPrefix("Zombie_paper_paper", RENDER_GROUP_HIDDEN);
	}
	else if (theZombieType == ZombieType::ZOMBIE_FLAG)
	{
		aReanim->AssignRenderGroupToPrefix("anim_innerarm", RENDER_GROUP_HIDDEN);
		aReanim->AssignRenderGroupToTrack("Zombie_flaghand", RENDER_GROUP_NORMAL);
		aReanim->AssignRenderGroupToTrack("Zombie_innerarm_screendoor", RENDER_GROUP_NORMAL);
	}
	else if (theZombieType == ZombieType::ZOMBIE_DUCKY_TUBE)
	{
		aReanim->AssignRenderGroupToPrefix("Zombie_duckytube", RENDER_GROUP_NORMAL);
	}
}

void Zombie::ShowDoorArms(bool theShow)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim)
	{
		SetupDoorArms(aBodyReanim, theShow);
		if (!mHasArm)
		{
			ReanimShowPrefix("Zombie_outerarm_lower", RENDER_GROUP_HIDDEN);
			ReanimShowPrefix("Zombie_outerarm_hand", RENDER_GROUP_HIDDEN);
		}
	}
}

void Zombie::ReanimIgnoreClipRect(const char* theTrackName, bool theIgnoreClipRect)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	for (int i = 0; i < aBodyReanim->mDefinition->mTracks.count; i++)
	{
		if (strcasecmp(aBodyReanim->mDefinition->mTracks.tracks[i].mName, theTrackName) == 0)
		{
			aBodyReanim->mTrackInstances[i].mIgnoreClipRect = theIgnoreClipRect;
		}
	}
}

void Zombie::ReanimReenableClipping()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	for (int i = 0; i < aBodyReanim->mDefinition->mTracks.count; i++)
	{
		aBodyReanim->mTrackInstances[i].mIgnoreClipRect = false;
	}
}

void Zombie::LoadPlainZombieReanim()
{
	mZombieAttackRect = Rect(20, 0, 50, 115);
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	SetupReanimLayers(aBodyReanim, mZombieType);
	if (mBoard)
	{
		EnableMustache(mBoard->mMustacheMode);
		EnableFuture(mBoard->mFutureMode);
	}

	if ((mBoard && mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL && mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE) || mZombieType == ZombieType::ZOMBIE_DUCKY_TUBE)
	{
		ReanimShowPrefix("zombie_duckytube", RENDER_GROUP_NORMAL);
		ReanimIgnoreClipRect("Zombie_duckytube", true);
		ReanimIgnoreClipRect("Zombie_outerarm_hand", true);
		ReanimIgnoreClipRect("Zombie_innerarm3", true);
		SetupWaterTrack("Zombie_whitewater");
		SetupWaterTrack("Zombie_whitewater2");
	}
}

Reanimation* Zombie::LoadReanim(ReanimationType theReanimationType)
{
	Reanimation* aBodyReanim = mApp->AddReanimation(0.0f, 0.0f, 0, theReanimationType);
	mBodyReanimID = mApp->ReanimationGetID(aBodyReanim);
	aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aBodyReanim->mIsAttachment = true;

	if (!IsOnBoard())
	{
		if (Rand(4) > 0 && aBodyReanim->TrackExists("anim_idle2"))
		{
			float aRanimRate = RandRangeFloat(12.0f, 24.0f);
			PlayZombieReanim("anim_idle2", ReanimLoopType::REANIM_LOOP, 0, aRanimRate);
		}
		else if (aBodyReanim->TrackExists("anim_idle"))
		{
			float aRanimRate = RandRangeFloat(12.0f, 18.0f);
			PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, aRanimRate);
		}

		aBodyReanim->mAnimTime = RandRangeFloat(0.0f, 0.99f);
	}
	else
	{
		StartWalkAnim(0);
	}

	return aBodyReanim;
}

void Zombie::PlayZombieReanim(const char* theTrackName, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	aBodyReanim->PlayReanim(theTrackName, theLoopType, theBlendTime, theAnimRate);
	if (theAnimRate != 0.0f)
	{
		mOriginalAnimRate = theAnimRate;
	}
	UpdateAnimSpeed();
}

void Zombie::OverrideParticleScale(PvzpParticleSystem* aParticle)
{
	if (aParticle)
	{
		aParticle->OverrideScale(nullptr, mScaleZombie);
	}
}

void Zombie::OverrideParticleColor(PvzpParticleSystem* aParticle)
{
	if (aParticle)
	{
		if (mMindControlled)
		{
			aParticle->OverrideColor(nullptr, ZOMBIE_MINDCONTROLLED_COLOR);
			aParticle->OverrideExtraAdditiveDraw(nullptr, true);
		}
		else if (mChilledCounter > 0 || mIceTrapCounter > 0)
		{
			aParticle->OverrideColor(nullptr, Color(75, 75, 255, 255));
			aParticle->OverrideExtraAdditiveDraw(nullptr, true);
		}
	}
}

void Zombie::DropFlag()
{
	if (mZombieType != ZombieType::ZOMBIE_FLAG || !mHasObject)
		return;

	mApp->RemoveReanimation(mSpecialHeadReanimID);
	ReanimShowPrefix("anim_innerarm", RENDER_GROUP_NORMAL);
	ReanimShowTrack("Zombie_flaghand", RENDER_GROUP_HIDDEN);
	ReanimShowTrack("Zombie_innerarm_screendoor", RENDER_GROUP_HIDDEN);
	mHasObject = false;

	float aFlagPosX, aFlagPosY;
	GetTrackPosition("Zombie_flaghand", aFlagPosX, aFlagPosY);
	PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aFlagPosX + 6.0f, aFlagPosY - 45.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_FLAG);
	OverrideParticleColor(aParticle);
	OverrideParticleScale(aParticle);
}

void Zombie::ApplyZombatarHead(const unsigned char* theRecord)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (!aBodyReanim)
		return;

	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("anim_head1");
	aTrackInstance->mImageOverride = IMAGE_BLANK;
	aBodyReanim->AssignRenderGroupToTrack("anim_head1", RENDER_GROUP_ZOMBATAR_HEAD);
	aBodyReanim->AssignRenderGroupToPrefix("anim_head2", RENDER_GROUP_HIDDEN);
	aBodyReanim->AssignRenderGroupToPrefix("anim_hair", RENDER_GROUP_HIDDEN);
	aBodyReanim->mFrameBasePose = 0;

	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mZombatarHeadReanimID);
	if (!aHeadReanim)
	{
		aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZOMBATAR_HEAD);
		aHeadReanim->PlayReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
		mZombatarHeadReanimID = mApp->ReanimationGetID(aHeadReanim);
		AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
		PvzpScaleRotateTransformMatrix(aAttachEffect->mOffset, -20.0f, -1.0f, 0.2f, 1.0f, 1.0f);
	}

	aHeadReanim->AssignRenderGroupToTrack("anim_hair", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("hats_", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("hair_", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("facialHair_", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("accessories_", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("eyeWear_", RENDER_GROUP_HIDDEN);
	aHeadReanim->AssignRenderGroupToPrefix("tidBits_", RENDER_GROUP_HIDDEN);

	struct RuntimePart
	{
		int mPartSlot;
		int mColorSlot;
		int mMaxCount;
		const char* mPrefix;
		ZombatarPage mPage;
		bool mRemapAccessory;
		bool mCompactTrackRange;
	};

	static constexpr RuntimePart aRuntimeParts[] =
	{
		{ ZOMBATAR_SLOT_HATS, ZOMBATAR_SLOT_HATS_COLOR, 14, "hats_", ZOMBATAR_PAGE_HATS, false, false },
		{ ZOMBATAR_SLOT_HAIR, ZOMBATAR_SLOT_HAIR_COLOR, 16, "hair_", ZOMBATAR_PAGE_HAIR, false, false },
		{ ZOMBATAR_SLOT_TIDBITS, ZOMBATAR_SLOT_TIDBITS_COLOR, 14, "tidBits_", ZOMBATAR_PAGE_TIDBITS, false, false },
		{ ZOMBATAR_SLOT_EYEWEAR, ZOMBATAR_SLOT_EYEWEAR_COLOR, 16, "eyeWear_", ZOMBATAR_PAGE_EYEWEAR, false, false },
		{ ZOMBATAR_SLOT_ACCESSORY, ZOMBATAR_SLOT_ACCESSORY_COLOR, 15, "accessories_", ZOMBATAR_PAGE_ACCESSORY, true, false },
		{ ZOMBATAR_SLOT_FACIAL_HAIR, ZOMBATAR_SLOT_FACIAL_HAIR_COLOR, 25, "facialHair_", ZOMBATAR_PAGE_FACIAL_HAIR, false, true }
	};

	for (const RuntimePart& aPart : aRuntimeParts)
	{
		int aPartIndex = ZombatarReadSignedRecordSlot(theRecord, aPart.mPartSlot);
		if (aPartIndex < 0 || aPartIndex >= aPart.mMaxCount)
			continue;
		int aTrackIndex = aPartIndex;
		if (aPart.mCompactTrackRange && aTrackIndex > 16)
			aTrackIndex -= aTrackIndex / 17;
		if (aPart.mRemapAccessory)
			aTrackIndex = ZombatarRemapAccessoryForRuntime(aTrackIndex);
		std::string aTrackName = ZombatarTrackName(aPart.mPrefix, aTrackIndex);

		const ZombatarPartLayout* aLayout = GetPartLayout(aPart.mPage, aPartIndex);
		const int aDrawOrder = aLayout ? aLayout->mDrawOrder : 0;
		if (aHeadReanim->TrackExists(aTrackName.c_str()))
		{
			aHeadReanim->AssignRenderGroupToTrack(aTrackName.c_str(), aDrawOrder);
			aHeadReanim->GetTrackInstanceByName(aTrackName.c_str())->mTrackColor =
				ZombatarGetColor(ZombatarReadSignedRecordSlot(theRecord, aPart.mColorSlot));
		}

		// some parts exist only as a "_line" detail track without a base track
		std::string aLineTrackName = aTrackName + "_line";
		if (aHeadReanim->TrackExists(aLineTrackName.c_str()))
			aHeadReanim->AssignRenderGroupToTrack(aLineTrackName.c_str(), aDrawOrder + 1);
	}
}

void Zombie::SetupZombatarFlagReanim(int theRecordIndex)
{
	if (theRecordIndex < 0)
		return;

	PlayerInfo* aPlayerInfo = mApp->mPlayerInfo;
	if (!aPlayerInfo || aPlayerInfo->mZombatarData.empty())
		return;

	const unsigned char* aRecord = aPlayerInfo->mZombatarData.data() + static_cast<size_t>(theRecordIndex) * ZOMBATAR_RECORD_SIZE;
	ApplyZombatarHead(aRecord);
}


/*
void Zombie::DrawZombieHead(Graphics* g, const ZombieDrawPosition& theDrawPos, int theFrame)
{

    if (mYuckyFace)
    {
        DrawZombiePart(g, IMAGE_ZOMBIE, mFrame, ZombieParts::PART_HEAD_YUCKY, theDrawPos);
        return;
    }

    if (mIsEating)
    {
        DrawZombiePart(g, IMAGE_ZOMBIE, mFrame, ZombieParts::PART_HEAD_EATING, theDrawPos);

        if (mHelmHealth == 0)
        {
            DrawZombiePart(g, IMAGE_ZOMBIE, 5, ZombieParts::PART_HAIR, theDrawPos);
        }
    }
    else
    {
        DrawZombiePart(g, IMAGE_ZOMBIE, mFrame, ZombieParts::PART_HEAD, theDrawPos);

        if (mVariant)
        {
            DrawZombiePart(g, IMAGE_ZOMBIE, theFrame, ZombieParts::PART_TONGUE, theDrawPos);
        }

        if (mHelmHealth == 0)
        {
            DrawZombiePart(g, IMAGE_ZOMBIE, theFrame, ZombieParts::PART_HAIR, theDrawPos);
        }
    }

}
*/

/*
void Zombie::DrawZombieWithParts(Graphics* g, const ZombieDrawPosition& theDrawPos)
{

    int aFrame = mIsEating ? 0 : mFrame;
    DrawZombiePart(g, IMAGE_ZOMBIE, aFrame, ZombieParts::PARTS_BODY, theDrawPos);

    if (mHasArm && mBodyReanimID == ReanimationID::REANIMATIONID_NULL)
    {
        if (mZombiePhase == ZombiePhase::PHASE_WALKING_DOG && mHasHead)
        {
            ZombieDrawPosition theDrawPosLeash = theDrawPos;
            theDrawPosLeash.mImageOffsetX -= 14.0f;
            theDrawPosLeash.mImageOffsetY += 10.0f;
            DrawZombiePart(g, IMAGE_ZOMBIE, aFrame, ZombieParts::PART_ARM_LEASH, theDrawPosLeash);
        }
        else
        {
            DrawZombiePart(g, IMAGE_ZOMBIE, aFrame, ZombieParts::PART_ARM, theDrawPos);
        }
    }

    if (mHasHead && mBodyReanimID == ReanimationID::REANIMATIONID_NULL)
    {
        DrawZombieHead(g, theDrawPos, aFrame);
    }

}
*/

void Zombie::UpdateReanim()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr || aBodyReanim->mDead)
		return;

	if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		if (GetBodyDamageIndex() == 2 || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
		{
			Reanimation* aReanim = mApp->ReanimationGet(mBodyReanimID);
			if (mSummonCounter != 0)  // Non-zero means the catapult still carries balls.
			{
				aReanim->SetImageOverride("Zombie_catapult_pole", IMAGE_REANIM_ZOMBIE_CATAPULT_POLE_DAMAGE_WITHBALL);
			}
			else
			{
				aReanim->SetImageOverride("Zombie_catapult_pole", IMAGE_REANIM_ZOMBIE_CATAPULT_POLE_DAMAGE);
			}
		}
		else if (mSummonCounter == 0)
		{
			aBodyReanim->SetImageOverride("Zombie_catapult_pole", IMAGE_REANIM_ZOMBIE_CATAPULT_POLE);
		}
	}

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	float anOffsetX = aDrawPos.mImageOffsetX + 15.0f;
	float anOffsetY = aDrawPos.mImageOffsetY + aDrawPos.mBodyY - 28.0f + 20.0f;
	if ((mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT) && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_BURNED)
	{
		if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
		{
			float aShakeRange = PvzpAnimateCurveFloatTime(0.7f, 1.0f, aBodyReanim->mAnimTime, 0.0f, 1.0f, PvzpCurves::CURVE_EASE_OUT) * mApp->GetScreenShakeScale();
			anOffsetX += RandRangeFloat(-aShakeRange, aShakeRange);
			anOffsetY += RandRangeFloat(-aShakeRange, aShakeRange);
		}
		else if (mBodyHealth < 200)
		{
			float aShakeScale = mApp->GetScreenShakeScale();
			anOffsetX += RandRangeFloat(-1.0f, 1.0f) * aShakeScale;
			anOffsetY += RandRangeFloat(-1.0f, 1.0f) * aShakeScale;
		}
	}
	if (mZombieType == ZombieType::ZOMBIE_FOOTBALL && mScaleZombie < 1.0f)
	{
		anOffsetY += 20.0f - mScaleZombie * 20.0f;
	}

	bool anOpposite = false;
	if (IsWalkingBackwards())
	{
		anOpposite = true;
	}
	if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		anOpposite = false;

		if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
		{
			if (!mIsEating)
			{
				anOpposite = true;
			}
		}

		if (mMindControlled)
		{
			anOpposite = !anOpposite;
		}
	}
	if (anOpposite)
	{
		anOffsetX += 90.0f * mScaleZombie;
	}

	aBodyReanim->mOverlayMatrix.m10 = 0.0f;
	aBodyReanim->mOverlayMatrix.m20 = 0.0f;
	aBodyReanim->mOverlayMatrix.m11 = 0.0f;
	aBodyReanim->mOverlayMatrix.m21 = 0.0f;
	aBodyReanim->OverrideScale(mScaleZombie, mScaleZombie);
	aBodyReanim->SetPosition(anOffsetX + 30.0f - mScaleZombie * 30.0f, anOffsetY + 120.0f - mScaleZombie * 120.0f);
	if (anOpposite)
	{
		aBodyReanim->mOverlayMatrix.m00 = -mScaleZombie;
	}

	Reanimation* aMoweredReanim = mApp->ReanimationTryToGet(mMoweredReanimID);
	if (aMoweredReanim)
	{
		aMoweredReanim->Update();

		SexyTransform2D aOverlayMatrix;
		aMoweredReanim->GetAttachmentOverlayMatrix(0, aOverlayMatrix);
		aOverlayMatrix.m00 *= aBodyReanim->mOverlayMatrix.m00;
		aOverlayMatrix.m10 *= aBodyReanim->mOverlayMatrix.m00;
		aOverlayMatrix.m01 *= aBodyReanim->mOverlayMatrix.m11;
		aOverlayMatrix.m11 *= aBodyReanim->mOverlayMatrix.m11;
		aOverlayMatrix.m02 *= aBodyReanim->mOverlayMatrix.m00;
		aOverlayMatrix.m12 *= aBodyReanim->mOverlayMatrix.m11;
		aOverlayMatrix.m02 += aBodyReanim->mOverlayMatrix.m11;
		aOverlayMatrix.m12 += aBodyReanim->mOverlayMatrix.m12;
		aBodyReanim->mOverlayMatrix = aOverlayMatrix;
	}

	aBodyReanim->Update();
	aBodyReanim->PropogateColorToAttachments();
}

void Zombie::SetAnimRate(float theAnimRate)
{
	mOriginalAnimRate = theAnimRate;
	ApplyAnimRate(theAnimRate);
}

void Zombie::ApplyAnimRate(float theAnimRate)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		aBodyReanim->mAnimRate = IsMovingAtChilledSpeed() ? theAnimRate * 0.5f : theAnimRate;
	}
}

void Zombie::UpdateAnimSpeed()
{
	if (!IsOnBoard())
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if (IsImmobilizied() || (mYuckyFace && mYuckyFaceCounter < 170))
	{
		ApplyAnimRate(0.0f);
		return;
	}

	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT || mZombiePhase == ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT || IsDeadOrDying())
	{
		ApplyAnimRate(mOriginalAnimRate);
		return;
	}

	if (mIsEating)
	{
		const float aEatingSpeedMultiplier = mBoard->mZombieTierSunMoney >= HIGH_SUN_EATING_THRESHOLD ?
			HIGH_SUN_EATING_SPEED_PERCENT / 100.0f : 1.0f;
		if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER || mZombieType == ZombieType::ZOMBIE_BALLOON || mZombieType == ZombieType::ZOMBIE_IMP ||
			mZombieType == ZombieType::ZOMBIE_DIGGER || mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX || mZombieType == ZombieType::ZOMBIE_SNORKEL ||
			mZombieType == ZombieType::ZOMBIE_YETI)
		{
			ApplyAnimRate(20.0f * aEatingSpeedMultiplier);
		}
		else
		{
			ApplyAnimRate(36.0f * aEatingSpeedMultiplier);
		}
	}
	else
	{
		if (ZombieNotWalking() || IsBobsledTeamWithSled() || mZombieType == ZombieType::ZOMBIE_CATAPULT ||
			mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING || mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
		{
			ApplyAnimRate(mOriginalAnimRate);
		}
		else if (aBodyReanim->TrackExists("_ground"))
		{
			ReanimatorTrack* aTrack = &aBodyReanim->mDefinition->mTracks.tracks[aBodyReanim->FindTrackIndex("_ground")];
			float aDistance = aTrack->mTransforms.mTransforms[aBodyReanim->mFrameStart + aBodyReanim->mFrameCount - 1].mTransX - aTrack->mTransforms.mTransforms[aBodyReanim->mFrameStart].mTransX;
			if (aDistance >= 1e-6f)
			{
				float aOneOverSpeed = aBodyReanim->mFrameCount / aDistance;
				float aAnimRate = mVelX * aOneOverSpeed * 47.0f / mScaleZombie * ZombieRules::ZombieStrengthSpeedMultiplier(mBoard ? mBoard->mZombieStrengthTier : 0);
				ApplyAnimRate(aAnimRate);
			}
		}
	}
}

void Zombie::EnableMustache(bool theEnableMustache)
{
	if (mFromWave == Zombie::ZOMBIE_WAVE_UI)
		return;

	if (!mHasHead || Zombie::IsZombotany(mZombieType))
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr || !aBodyReanim->TrackExists("Zombie_mustache"))
		return;

	if (theEnableMustache)
	{
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_mustache", RENDER_GROUP_NORMAL);

		switch (RandRangeInt(1, 3))
		{
		case 1:     aBodyReanim->SetImageOverride("Zombie_mustache", nullptr);                          break;
		case 2:     aBodyReanim->SetImageOverride("Zombie_mustache", IMAGE_REANIM_ZOMBIE_MUSTACHE2);    break;
		case 3:     aBodyReanim->SetImageOverride("Zombie_mustache", IMAGE_REANIM_ZOMBIE_MUSTACHE3);    break;
		}
	}
	else
	{
		aBodyReanim->AssignRenderGroupToPrefix("Zombie_mustache", RENDER_GROUP_HIDDEN);
	}
}

void Zombie::EnableFuture(bool theEnableFuture)
{
	if (mFromWave == Zombie::ZOMBIE_WAVE_UI || Zombie::IsZombotany(mZombieType))
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr || aBodyReanim->mReanimationType != ReanimationType::REANIM_ZOMBIE)
		return;

	if (theEnableFuture)
	{
		Image* aImage = nullptr;
		switch (static_cast<unsigned int>(mBoard->ZombieGetID(this)) % 4)
		{
		case 0:     aImage = IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES1;      break;
		case 1:     aImage = IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES2;      break;
		case 2:     aImage = IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES3;      break;
		case 3:     aImage = IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES4;      break;
		default:    PVZP_ASSERT(false);                                       break;
		}
		aBodyReanim->SetImageOverride("anim_head1", aImage);
	}
	else
	{
		aBodyReanim->SetImageOverride("anim_head1", nullptr);
	}
}

void Zombie::EnableDance()
{
	if (!IsOnBoard())
		return;

	if (ZombieNotWalking() || IsDeadOrDying())
		return;

	if (mZombieType == ZombieType::ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE || mZombieType == ZombieType::ZOMBIE_PAIL)
	{
		StartWalkAnim(0);
	}
}

void Zombie::SetupWaterTrack(const char* theTrackName)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(theTrackName);
	aTrackInstance->mIgnoreExtraAdditiveColor = true;
	aTrackInstance->mIgnoreColorOverride = true;
	aTrackInstance->mIgnoreClipRect = true;
}
