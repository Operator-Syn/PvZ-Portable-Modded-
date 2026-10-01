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

#include "Plant.h"
#include "Board.h"
#include "../ConstEnums.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "LawnMower.h"
#include "Challenge.h"
#include "Projectile.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "System/PlayerInfo.h"
#include "System/Zombatar.h"
#include "System/Music.h"
#include "Widget/AlmanacDialog.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpCommon.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

#include "ZombieConstants.h"

bool Zombie::HasYuckyFaceImage()
{
	if (mBoard->mFutureMode)
		return false;

	return
		mZombieType == ZombieType::ZOMBIE_NORMAL ||
		mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE ||
		mZombieType == ZombieType::ZOMBIE_PAIL ||
		mZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET ||
		mZombieType == ZombieType::ZOMBIE_FLAG ||
		mZombieType == ZombieType::ZOMBIE_DOOR ||
		mZombieType == ZombieType::ZOMBIE_DUCKY_TUBE ||
		mZombieType == ZombieType::ZOMBIE_DANCER ||
		mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
		mZombieType == ZombieType::ZOMBIE_NEWSPAPER ||
		mZombieType == ZombieType::ZOMBIE_POLEVAULTER;
}

void Zombie::ShowYuckyFace(bool theShow)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if (HasYuckyFaceImage())
	{
		if (theShow)
		{
			aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_HEAD_GROSSOUT);
			aBodyReanim->AssignRenderGroupToTrack("anim_head2", RENDER_GROUP_HIDDEN);
			aBodyReanim->AssignRenderGroupToTrack("anim_head_jaw", RENDER_GROUP_HIDDEN);
			aBodyReanim->AssignRenderGroupToTrack("anim_tongue", RENDER_GROUP_HIDDEN);
		}
		else if (mHasHead)
		{
			aBodyReanim->SetImageOverride("anim_head1", nullptr);
			aBodyReanim->AssignRenderGroupToTrack("anim_head2", RENDER_GROUP_NORMAL);
			aBodyReanim->AssignRenderGroupToTrack("anim_head_jaw", RENDER_GROUP_NORMAL);
			if (mVariant)
			{
				aBodyReanim->AssignRenderGroupToTrack("anim_tongue", RENDER_GROUP_NORMAL);
			}
		}
	}
}

void Zombie::UpdateYuckyFace()
{
	mYuckyFaceCounter++;
	if (mYuckyFaceCounter > 20 && mYuckyFaceCounter < 170 && !HasYuckyFaceImage())
	{
		StopEating();
		mYuckyFaceCounter = 170;
		if (mBoard->CountZombiesOnScreen() <= 5 && mHasHead)
		{
			mApp->PlayFoley(FoleyType::FOLEY_YUCK);
		}
		else if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead && Rand(2) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_YUCK);
		}
	}

	if (mYuckyFaceCounter > 270)
	{
		ShowYuckyFace(false);
		mYuckyFace = false;
		mYuckyFaceCounter = 0;
		return;
	}

	if (mYuckyFaceCounter == 70)
	{
		StopEating();
		ShowYuckyFace(true);
		if (mBoard->CountZombiesOnScreen() <= 5 && mHasHead)
		{
			mApp->PlayFoley(FoleyType::FOLEY_YUCK);
		}
		else if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead && Rand(2) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_YUCK);
		}
	}
	if (mYuckyFaceCounter == 170)
	{
		StartWalkAnim(20);

		bool aCanGoUp = true;
		bool aCanGoDown = true;
		bool aIsPool = mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL;
		if (!mBoard->RowCanHaveZombies(mRow - 1))
		{
			aCanGoUp = false;
		}
		else if (mBoard->mPlantRow[mRow - 1] == PlantRowType::PLANTROW_POOL && !aIsPool)
		{
			aCanGoUp = false;
		}
		else if (mBoard->mPlantRow[mRow - 1] != PlantRowType::PLANTROW_POOL && aIsPool)
		{
			aCanGoUp = false;
		}
		if (!mBoard->RowCanHaveZombies(mRow + 1))
		{
			aCanGoDown = false;
		}
		else if (mBoard->mPlantRow[mRow + 1] == PlantRowType::PLANTROW_POOL && !aIsPool)
		{
			aCanGoDown = false;
		}
		else if (mBoard->mPlantRow[mRow + 1] != PlantRowType::PLANTROW_POOL && aIsPool)
		{
			aCanGoDown = false;
		}

		if (aCanGoDown && !aCanGoUp)
		{
			SetRow(mRow + 1);
		}
		else if (!aCanGoDown && aCanGoUp)
		{
			SetRow(mRow - 1);
		}
		else if (aCanGoDown && aCanGoUp)
		{
			SetRow((Rand(2) == 0) ? (mRow + 1) : (mRow - 1));
		}
		else
		{
			PVZP_ASSERT(false);
		}
	}
}

void Zombie::AnimateChewSound()
{
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT)
		return;

	Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
	if (aPlant)
	{
		if (aPlant->mSeedType == SeedType::SEED_HYPNOSHROOM && !aPlant->mIsAsleep && !IsSunTierInvulnerable())
		{
			mApp->PlayFoley(FoleyType::FOLEY_FLOOP);
			aPlant->Die();

			StartMindControlled();
			mApp->AddPvzpParticle(mPosX + 60.0f, mPosY + 40.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MIND_CONTROL);
			TrySpawnLevelAward();

			mVelX = 0.17f;
			mAnimTicksPerFrame = 18;
			UpdateAnimSpeed();
		}
		else if (aPlant->mSeedType == SeedType::SEED_GARLIC && !IsSunTierInvulnerable())
		{
			if (!mYuckyFace)
			{
				mYuckyFace = true;
				mYuckyFaceCounter = 0;
				UpdateAnimSpeed();
				mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
			}
		}
		else
		{
			if (aPlant->mSeedType == SeedType::SEED_WALLNUT || aPlant->IsTallNut() || aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				mApp->PlayFoley(FoleyType::FOLEY_CHOMP_SOFT);
			}
			else
			{
				mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
			}
		}
	}
	else
	{
		if (mMindControlled)
		{
			mApp->PlayFoley(FoleyType::FOLEY_CHOMP_SOFT);
		}
		else
		{
			mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
		}
	}
}

void Zombie::AnimateChewEffect()
{
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT)
		return;

	if (mApp->IsIZombieLevel())
	{
		GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
		if (aBrain)
		{
			aBrain->mTransparentCounter = std::max(aBrain->mTransparentCounter, 25);
			return;
		}
	}

	Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
	if (aPlant)
	{
		if (aPlant->mSeedType == SeedType::SEED_WALLNUT || aPlant->IsTallNut())
		{
			int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
			ZombieDrawPosition aDrawPos;
			GetDrawPos(aDrawPos);

			float aPosX = mPosX + 37.0f;
			float aPosY = mPosY + 40.0f + aDrawPos.mBodyY;
			if (mZombieType == ZombieType::ZOMBIE_SNORKEL || mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
			{
				aPosX -= 7.0f;
				aPosY += 70.0f;
			}
			else if (IsWalkingBackwards())
			{
				aPosX += 47.0f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
			{
				aPosY += 47.0f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_IMP)
			{
				aPosX += 24.0f;
				aPosY += 40.0f;
			}

			mApp->AddPvzpParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_WALLNUT_EAT_SMALL);
		}

		aPlant->mEatenFlashCountdown = std::max(aPlant->mEatenFlashCountdown, 25);
	}
}

void Zombie::Animate()
{
	mPrevFrame = mFrame;
	if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
		mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
		IsImmobilizied())
		return;

	mAnimCounter++;
	if (mYuckyFace)
	{
		UpdateYuckyFace();
	}

	if (mIsEating && mHasHead)
	{
		int aFrameLength = 6;
		if (mChilledCounter > 0)
		{
			aFrameLength = 12;
		}
		if (mAnimCounter >= mAnimFrames * aFrameLength)
		{
			mAnimCounter = aFrameLength;
		}
		mFrame = mAnimCounter / aFrameLength;

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim)
		{
			float aLeftHandTime = 0.14f;
			float aRightHandTime = 0.68f;
			if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
			{
				aLeftHandTime = 0.38f;
				aRightHandTime = 0.8f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER || mZombieType == ZombieType::ZOMBIE_LADDER)
			{
				aLeftHandTime = 0.42f;
				aRightHandTime = 0.42f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
			{
				aLeftHandTime = 0.53f;
				aRightHandTime = 0.53f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
			{
				aLeftHandTime = 0.33f;
				aRightHandTime = 0.83f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_IMP)
			{
				aLeftHandTime = 0.33f;
				aRightHandTime = 0.79f;
			}

			if (aBodyReanim->ShouldTriggerTimedEvent(aLeftHandTime) || aBodyReanim->ShouldTriggerTimedEvent(aRightHandTime))
			{
				AnimateChewSound();
				AnimateChewEffect();
			}
		}
		else
		{
			if (mAnimCounter == 4 * aFrameLength)
			{
				AnimateChewSound();
			}
			if (mAnimCounter == 7 * aFrameLength && !mMindControlled)
			{
				AnimateChewEffect();
			}
		}
	}
	else
	{
		if (mAnimCounter >= mAnimFrames * mAnimTicksPerFrame)
		{
			mAnimCounter = 0;
		}
		mFrame = mAnimCounter / mAnimTicksPerFrame;
	}
}

/*
void Zombie::DrawZombie(Graphics* g, const ZombieDrawPosition& theDrawPos)
{
    switch (mZombieType)
    {
    case ZombieType::ZOMBIE_NORMAL:
    case ZombieType::ZOMBIE_FLAG:
    case ZombieType::ZOMBIE_TRAFFIC_CONE:
    case ZombieType::ZOMBIE_PAIL:
    case ZombieType::ZOMBIE_NEWSPAPER:
    case ZombieType::ZOMBIE_DOOR:
    case ZombieType::ZOMBIE_FOOTBALL:
    case ZombieType::ZOMBIE_DOLPHIN_RIDER:
    case ZombieType::ZOMBIE_LADDER:
    //case ZombieType::ZOMBIE_DOG_WALKER:
    //  DrawZombieWithParts(g, theDrawPos);
    //  break;

    //case ZombieType::ZOMBIE_DOG:
    //    DrawZombiePart(g, IMAGE_ZOMBIEDOG, mIsEating ? ZombieParts::PART_HEAD : ZombieParts::PARTS_BODY, mFrame, theDrawPos);
    //    break;

    //case ZombieType::ZOMBIE_PROPELLER:
    //    DrawZombiePart(g, IMAGE_PROPELLERZOMBIE, ZombieParts::PARTS_BODY, mFrame, theDrawPos);
    //    break;

    //case ZombieType::ZOMBIE_POLEVAULTER:
    //case ZombieType::ZOMBIE_DANCER:
    //case ZombieType::ZOMBIE_BACKUP_DANCER:
    //case ZombieType::ZOMBIE_DUCKY_TUBE:
    //case ZombieType::ZOMBIE_SNORKEL:
    //case ZombieType::ZOMBIE_ZAMBONI:
    //case ZombieType::ZOMBIE_BOBSLED:
    //case ZombieType::ZOMBIE_JACK_IN_THE_BOX:
    //case ZombieType::ZOMBIE_BALLOON:
    //case ZombieType::ZOMBIE_DIGGER:
    //case ZombieType::ZOMBIE_POGO:
    //case ZombieType::ZOMBIE_YETI:
    //case ZombieType::ZOMBIE_BUNGEE:
    //case ZombieType::ZOMBIE_CATAPULT:
    //case ZombieType::ZOMBIE_GARGANTUAR:
    //case ZombieType::ZOMBIE_IMP:
    //case ZombieType::ZOMBIE_BOSS:
    //    PVZP_ASSERT(false);
    //    break;

    default:
        PVZP_ASSERT(false);
        break;
    }
}
*/

bool Zombie::IsWalkingBackwards()
{
	if (mMindControlled)
		return true;

	if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
	{
		if (mVelZ < 1.5707964f || mVelZ > 4.712389f)
		{
			return true;
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING || mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED || mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
		{
			return true;
		}
		else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
		{
			return mHasObject;
		}

		return false;
	}

	return mZombieType == ZombieType::ZOMBIE_YETI && !mHasObject;
}

void Zombie::DrawZombiePart(Graphics* g, Image* theImage, int theFrame, int theRow, const ZombieDrawPosition& theDrawPos)
{
	// normally never called

	int aCelWidth = theImage->GetCelWidth();
	int aCelHeight = theImage->GetCelHeight();
	float anOffsetX = theDrawPos.mImageOffsetX;
	float anOffsetY = theDrawPos.mImageOffsetY + theDrawPos.mBodyY;
	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT)
	{
		anOffsetX -= 120.0f;
		anOffsetY -= 120.0f;
	}
	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
	{
		anOffsetY += 50.0f;
	}
	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		anOffsetY -= 19.0f;
	}

	float aDrawHeight = aCelHeight;
	if (theDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
	{
		aDrawHeight = std::clamp(aCelHeight - theDrawPos.mClipHeight, 0.0f, static_cast<float>(aCelHeight));
	}

	int anAlpha = 255;
	if (mZombieFade >= 0)
	{
		anAlpha = std::clamp(255 * mZombieFade / 10, 0, 255);
		g->SetColorizeImages(true);
		g->SetColor(Color(255, 255, 255, anAlpha));
	}

	bool aMirror = false;
	if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN || mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_LEFT)
	{
		int aFrame = GetDancerFrame();
		if (!mIsEating && (aFrame == 12 || aFrame == 13 || aFrame == 14 || aFrame == 18 || aFrame == 19 || aFrame == 20))
		{
			aMirror = true;
			anOffsetX -= 30.0f;
		}
	}
	if (aMirror)
	{
		anOffsetX = -anOffsetX;
	}

	Rect aSrcRect(theFrame * aCelWidth, theRow * aCelHeight, aCelWidth, aDrawHeight);
	Rect aDestRect(anOffsetX, anOffsetY, aCelWidth, aDrawHeight);
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
	{
		if (mMindControlled)
		{
			aMirror = true;
		}

		g->SetColorizeImages(true);
		g->SetColor(Color::Black);
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);
	}
	else if (mMindControlled)
	{
		aMirror = true;
		g->SetColorizeImages(true);
		Color aMincontrolledColor = ZOMBIE_MINDCONTROLLED_COLOR;
		aMincontrolledColor.mAlpha = anAlpha;
		g->SetColor(aMincontrolledColor);
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);

		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}
	else if (mChilledCounter > 0 || mIceTrapCounter > 0)
	{
		g->SetColorizeImages(true);
		g->SetColor(Color(75, 75, 255, anAlpha));
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);

		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}
	else
	{
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);
	}

	if (mJustGotShotCounter > 0)
	{
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->SetColorizeImages(true);
		int aGrayness = mJustGotShotCounter * 10;
		g->SetColor(Color(aGrayness, aGrayness, aGrayness, 255));
		g->DrawImageMirror(theImage, aDestRect, aSrcRect, aMirror);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	g->SetColorizeImages(false);
}

void Zombie::DrawBobsledReanim(Graphics* g, const ZombieDrawPosition& theDrawPos, bool theBeforeZombie)
{
	int aPosition = GetBobsledPosition();
	bool aDrawFront = false;
	bool aDrawBack = false;
	Zombie* aZombieLeader;
	if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
	{
		aZombieLeader = this;
	}
	else
	{
		if (aPosition == -1)
		{
			return;
		}
		if (aPosition == 0)
		{
			aZombieLeader = this;
		}
		else
		{
			aZombieLeader = mBoard->ZombieGet(mRelatedZombieID);
		}
	}

	if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
	{
		if (theBeforeZombie)  // seed-picker cutscene: draw sled back, zombie, then sled front
		{
			aDrawBack = true;
		}
		else
		{
			aDrawFront = true;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
	{
		if (aPosition == 0 && !theBeforeZombie)  // crashed: draw sled back and front after the leader zombie
		{
			aDrawFront = true;
			aDrawBack = true;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_SLIDING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
	{
		if (aPosition == 2 && theBeforeZombie)  // sliding or burned: draw sled back and front before the 2nd zombie
		{
			aDrawFront = true;
			aDrawBack = true;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_BOARDING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mAnimTime < 0.5f)  // while airborne: draw sled back and front before the 2nd zombie
		{
			if (aPosition == 2 && theBeforeZombie)
			{
				aDrawFront = true;
				aDrawBack = true;
			}
		}
		else if (aPosition == 0 && !theBeforeZombie)  // once boarded: draw sled front after the leader zombie
		{
			aDrawFront = true;
		}
		else if (aPosition == 3 && theBeforeZombie)  // once boarded: draw sled back before the last zombie
		{
			aDrawBack = true;
		}
	}

	float aOffsetX = aZombieLeader->mPosX + theDrawPos.mImageOffsetX - mPosX - 76.0f;
	float aOffsetY = 15.0f;
	int aBobsledDamageStatus;
	if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
	{
		aBobsledDamageStatus = 3;
		int aAlpha = PvzpAnimateCurve(30, 0, mPhaseCounter, 255, 0, PvzpCurves::CURVE_LINEAR);
		aOffsetX += (BOBSLED_CRASH_TIME - mPhaseCounter) * mVelX / ZOMBIE_LIMP_SPEED_FACTOR;  // rewind to the position where the crash started
		aOffsetX -= PvzpAnimateCurveFloat(BOBSLED_CRASH_TIME, 0, mPhaseCounter, 0.0f, 50.0f, PvzpCurves::CURVE_EASE_OUT);  // horizontal slide from the sled's momentum
		aOffsetY += PvzpAnimateCurveFloat(BOBSLED_CRASH_TIME, 75, mPhaseCounter, 5.0f, 10.0f, PvzpCurves::CURVE_LINEAR);
		if (aAlpha != 255)
		{
			g->SetColorizeImages(true);
			g->SetColor(Color(255, 255, 255, aAlpha));
		}
	}
	else
	{
		aBobsledDamageStatus = aZombieLeader->GetHelmDamageIndex();
	}

	Image* aImage;
	if (aBobsledDamageStatus == 0)
	{
		aImage = IMAGE_ZOMBIE_BOBSLED1;
	}
	else if (aBobsledDamageStatus == 1)
	{
		aImage = IMAGE_ZOMBIE_BOBSLED2;
	}
	else if (aBobsledDamageStatus == 2)
	{
		aImage = IMAGE_ZOMBIE_BOBSLED3;
	}
	else
	{
		aImage = IMAGE_ZOMBIE_BOBSLED4;
	}

	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
	{
		g->SetColorizeImages(true);
		g->SetColor(Color::Black);
	}

	if (aDrawBack && aBobsledDamageStatus != 3)
	{
		g->DrawImageF(IMAGE_ZOMBIE_BOBSLED_INSIDE, aOffsetX, aOffsetY);
	}
	if (aDrawFront)
	{
		g->DrawImageF(aImage, aOffsetX, aOffsetY);
	}

	if (aZombieLeader->mJustGotShotCounter > 0)
	{
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->SetColorizeImages(true);
		int aGrayness = aZombieLeader->mJustGotShotCounter * 10;
		g->SetColor(Color(aGrayness, aGrayness, aGrayness, 255));

		if (aDrawBack && aBobsledDamageStatus != 3)
		{
			g->DrawImageF(IMAGE_ZOMBIE_BOBSLED_INSIDE, aOffsetX, aOffsetY);
		}
		if (aDrawFront)
		{
			g->DrawImageF(aImage, aOffsetX, aOffsetY);
		}

		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	g->SetColorizeImages(false);
}

void Zombie::DrawBungeeReanim(Graphics* g)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	// float anOffsetY = theDrawPos.mBodyY + theDrawPos.mImageOffsetY + 14.0f;
	DrawBungeeCord(g, -22);
	aBodyReanim->Draw(g);

	Zombie* aDroppedZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
	if (aDroppedZombie)
	{
		Graphics aDropGraphics(*g);
		aDropGraphics.mTransY -= mAltitude;
		aDropGraphics.mTransX += aDroppedZombie->mPosX - mPosX;
		//aDropGraphics.Translate(aDroppedZombie->mPosX - mPosX, -mAltitude);

		ZombieDrawPosition aDroppedDrawPos;
		aDroppedZombie->GetDrawPos(aDroppedDrawPos);
		aDroppedZombie->DrawReanim(&aDropGraphics, aDroppedDrawPos, RENDER_GROUP_NORMAL);
	}
	else
	{
		Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mTargetPlantID));
		if (aPlant)
		{
			Graphics aPlantGraphics(*g);
			aPlantGraphics.mTransY += 30.0f - mAltitude;
			if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
			{
				if (aPlant->mSeedType == SeedType::SEED_SPIKEWEED || aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
				{
					aPlantGraphics.mTransY -= 34.0f;
				}
			}
			if (aPlant->mPlantCol <= 4 && mBoard->StageHasRoof())
			{
				aPlantGraphics.mTransY += 10;
			}

			aPlant->Draw(&aPlantGraphics);
		}
	}

	aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_ARMS);
}

void Zombie::DrawBungeeTarget(Graphics* g)
{
	if (!IsOnBoard() || mApp->IsFinalBossLevel())
		return;

	if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_HIT_OUCHY || mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
		return;

	if (mRelatedZombieID != ZombieID::ZOMBIEID_NULL)
		return;

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	float aTargetX = mX + 10.0f;
	float aTargetY = mY + 60.0f + aDrawPos.mBodyY + aDrawPos.mImageOffsetY;
	if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING || mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING)
	{
		aTargetX += PvzpAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT, BUNGEE_ZOMBIE_HEIGHT - 400, static_cast<int>(mAltitude), 30.0f, 0.0f, PvzpCurves::CURVE_LINEAR);
		aTargetY += PvzpAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT, BUNGEE_ZOMBIE_HEIGHT - 400, static_cast<int>(mAltitude), -600.0f, 0.0f, PvzpCurves::CURVE_LINEAR);
	}

	g->DrawImageF(IMAGE_BUNGEETARGET, aTargetX, aTargetY + mAltitude);
}

void Zombie::DrawDancerReanim(Graphics* g)
{
	Color aSpotLightColor;
	bool aDrawSpotLight = false;
	if (mZombiePhase != ZombiePhase::PHASE_DANCER_DANCING_IN && mZombiePhase != ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS &&
		mZombiePhase != ZombiePhase::PHASE_ZOMBIE_NORMAL && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		aDrawSpotLight = true;

		switch (mZombieAge >= 700 ? mZombieAge / 100 * 7 % 5 : 0)
		{
		case 0:
			aSpotLightColor = Color(250, 250, 160);
			break;
		case 1:
			aSpotLightColor = Color(114, 234, 170);
			break;
		case 2:
			aSpotLightColor = Color(216, 126, 202);
			break;
		case 3:
			aSpotLightColor = Color(90, 110, 140);
			break;
		case 4:
			aSpotLightColor = Color(240, 90, 130);
			break;
		}

		g->SetColorizeImages(true);
		g->SetColor(aSpotLightColor);
		PvzpDrawImageScaledF(g, IMAGE_SPOTLIGHT2, -30.0f, 60.0f, 4.0f, 4.0f);
		g->SetColorizeImages(false);
	}

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	aBodyReanim->Draw(g);

	if (aDrawSpotLight)
	{
		g->SetColorizeImages(true);
		g->SetColor(aSpotLightColor);
		PvzpDrawImageScaledF(g, IMAGE_SPOTLIGHT, -30.0f, -480.0f, 4.0f, 4.0f);
		g->SetColorizeImages(false);
	}
}

void Zombie::DrawReanim(Graphics* g, const ZombieDrawPosition& theDrawPos, int theBaseRenderGroup)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
	{
#ifdef PVZ_DEBUG
		PvzpLogLn("Missing zombie reanimation");
#endif
		return;
	}

	if (theDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
	{
		float aDrawHeight = 120.0f - theDrawPos.mClipHeight + 71.0f;
		g->SetClipRect(theDrawPos.mImageOffsetX - 200.0f, theDrawPos.mImageOffsetY + theDrawPos.mBodyY - 78.0f, 520, aDrawHeight);
	}

	int aFadeAlpha = 255;
	if (mZombieFade >= 0)
	{
		aFadeAlpha = std::clamp(255 * mZombieFade / 10, 0, 255);
	}

	Color aColorOverride(255, 255, 255, aFadeAlpha);
	Color aExtraAdditiveColor = Color::Black;
	bool aEnableExtraAdditiveDraw = false;
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
	{
		aColorOverride = Color(0, 0, 0, aFadeAlpha);
		aExtraAdditiveColor = Color::Black;
		aEnableExtraAdditiveDraw = false;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BOSS && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mBodyHealth < mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
	{
		int aGrayness = PvzpAnimateCurve(0, 39, mBoard->mMainCounter % 40, 155, 255, PvzpCurves::CURVE_BOUNCE);
		if (mChilledCounter > 0 || mIceTrapCounter > 0)
		{
			int aColdColor = PvzpAnimateCurve(0, 39, mBoard->mMainCounter % 40, 65, 75, PvzpCurves::CURVE_BOUNCE);
			aColorOverride = Color(aColdColor, aColdColor, aGrayness, aFadeAlpha);
		}
		else
		{
			aColorOverride = Color(aGrayness, aGrayness, aGrayness, aFadeAlpha);
		}

		aExtraAdditiveColor = Color::Black;
		aEnableExtraAdditiveDraw = false;
	}
	else if (mMindControlled)
	{
		aColorOverride = ZOMBIE_MINDCONTROLLED_COLOR;
		aColorOverride.mAlpha = aFadeAlpha;
		aExtraAdditiveColor = aColorOverride;
		aEnableExtraAdditiveDraw = true;
	}
	else if (ZombieRules::IsBulwarkType(mZombieType) && mChilledCounter == 0 && mIceTrapCounter == 0)
	{
		// A cool desaturated tint identifies the Bulwark while preserving the shared Garg rig.
		aColorOverride = Color(145, 190, 220, aFadeAlpha);
		aExtraAdditiveColor = Color::Black;
		aEnableExtraAdditiveDraw = false;
	}
	else if (mChilledCounter > 0 || mIceTrapCounter > 0)
	{
		aColorOverride = Color(75, 75, 255, aFadeAlpha);
		aExtraAdditiveColor = aColorOverride;
		aEnableExtraAdditiveDraw = true;
	}
	else if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM && mBodyHealth < 100)
	{
		aColorOverride = Color(100, 150, 25, aFadeAlpha);
		aExtraAdditiveColor = aColorOverride;
		aEnableExtraAdditiveDraw = true;
	}
	if (mJustGotShotCounter > 0 && !IsBobsledTeamWithSled())
	{
		int aGrayness = mJustGotShotCounter * 10;
		Color aHighlightColor(aGrayness, aGrayness, aGrayness, 255);
		aExtraAdditiveColor = ColorAdd(aHighlightColor, aExtraAdditiveColor);
		aEnableExtraAdditiveDraw = true;
	}
	aBodyReanim->mColorOverride = aColorOverride;
	aBodyReanim->mExtraAdditiveColor = aExtraAdditiveColor;
	aBodyReanim->mEnableExtraAdditiveDraw = aEnableExtraAdditiveDraw;

	if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		DrawBobsledReanim(g, theDrawPos, true);
		aBodyReanim->DrawRenderGroup(g, theBaseRenderGroup);
		DrawBobsledReanim(g, theDrawPos, false);
	}
	else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		DrawBungeeReanim(g);
	}
	else if (mZombieType == ZombieType::ZOMBIE_DANCER)
	{
		DrawDancerReanim(g);
	}
	else
	{
		aBodyReanim->DrawRenderGroup(g, theBaseRenderGroup);
	}

	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
		{
			aBodyReanim->mColorOverride = Color(0, 0, 0, aFadeAlpha);
			aBodyReanim->mExtraAdditiveColor = Color::Black;
			aBodyReanim->mEnableExtraAdditiveDraw = false;
		}
		else if (mShieldJustGotShotCounter > 0)
		{
			int aGrayness = mShieldJustGotShotCounter * 10;
			aBodyReanim->mColorOverride = Color(aGrayness, aGrayness, aGrayness, aFadeAlpha);
			aBodyReanim->mExtraAdditiveColor = Color::White;
			aBodyReanim->mEnableExtraAdditiveDraw = true;
		}
		else
		{
			aBodyReanim->mColorOverride = Color(255, 255, 255, aFadeAlpha);
			aBodyReanim->mExtraAdditiveColor = Color::Black;
			aBodyReanim->mEnableExtraAdditiveDraw = false;
		}

		float aShieldHitOffset = 0.0f;
		if (mShieldRecoilCounter > 0)
		{
			aShieldHitOffset = PvzpAnimateCurveFloat(12, 0, mShieldRecoilCounter, 3.0f, 0.0f, PvzpCurves::CURVE_LINEAR);
		}

		g->mTransX += aShieldHitOffset;
		aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_SHIELD);
		g->mTransX -= aShieldHitOffset;
	}

	if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER || mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_LADDER)
	{
		aBodyReanim->mColorOverride = aColorOverride;
		aBodyReanim->mExtraAdditiveColor = aExtraAdditiveColor;
		aBodyReanim->mEnableExtraAdditiveDraw = aEnableExtraAdditiveDraw;
		aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_OVER_SHIELD);
	}

	if (mZombatarHeadReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_ZOMBATAR_HEAD);
	}

	g->ClearClipRect();
}

int Zombie::GetHelmDamageIndex()
{
	if (mHelmHealth < mHelmMaxHealth / 3)
	{
		return 2;
	}

	if (mHelmHealth < mHelmMaxHealth * 2 / 3)
	{
		return 1;
	}

	return 0;
}

int Zombie::GetBodyDamageIndex()
{
	if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		if (mBodyHealth < mBodyMaxHealth / 2)
		{
			return 2;
		}

		if (mBodyHealth < mBodyMaxHealth * 4 / 5)
		{
			return 1;
		}

		return 0;
	}
	else
	{
		if (mBodyHealth < mBodyMaxHealth / 3)
		{
			return 2;
		}

		if (mBodyHealth < mBodyMaxHealth * 2 / 3)
		{
			return 1;
		}

		return 0;
	}
}

int Zombie::GetShieldDamageIndex()
{
	if (mShieldHealth < mShieldMaxHealth / 3)
	{
		return 2;
	}

	if (mShieldHealth < mShieldMaxHealth * 2 / 3)
	{
		return 1;
	}

	return 0;
}

void Zombie::DrawBungeeCord(Graphics* g, int theOffsetX)
{
	int aCordCelHeight = IMAGE_BUNGEECORD->GetCelHeight() * mScaleZombie;
	float aPosX, aPosY;
	GetTrackPosition("Zombie_bungi_body", aPosX, aPosY);

	bool aSetClip = false;
	if (IsOnBoard() && mApp->IsFinalBossLevel())
	{
		Zombie* aBoss = mBoard->GetBossZombie();

		int aClipAmount = 55;
		if (aBoss->mZombiePhase == ZombiePhase::PHASE_BOSS_BUNGEES_LEAVE)
		{
			Reanimation* aBossReanim = mApp->ReanimationGet(aBoss->mBodyReanimID);
			aClipAmount = PvzpAnimateCurveFloatTime(0.0f, 0.2f, aBossReanim->mAnimTime, 55.0f, 0.0f, PvzpCurves::CURVE_LINEAR);
		}

		if (mTargetCol > aBoss->mTargetCol)
		{
			g->SetClipRect(Rect(-g->mTransX, aClipAmount - g->mTransY, mApp->mWidth, mApp->mHeight));
			aSetClip = true;
		}
	}

	for (float y = aPosY - aCordCelHeight; y > -aCordCelHeight; y -= aCordCelHeight)
	{
		PvzpDrawImageScaledF(g, IMAGE_BUNGEECORD, theOffsetX + 61.0f - 4.0f / mScaleZombie, y - mPosY, mScaleZombie, mScaleZombie);
	}

	if (aSetClip)
	{
		g->ClearClipRect();
	}
}

void Zombie::GetDrawPos(ZombieDrawPosition& theDrawPos)
{
	theDrawPos.mImageOffsetX = mPosX - mX;
	theDrawPos.mImageOffsetY = mPosY - mY;

	if (mIsEating)
	{
		theDrawPos.mHeadX = 47;
		theDrawPos.mHeadY = 4;
	}
	else
	{
		switch (mFrame)
		{
		case 0:
			theDrawPos.mHeadX = 50;
			theDrawPos.mHeadY = 2;
			break;
		case 1:
			theDrawPos.mHeadX = 49;
			theDrawPos.mHeadY = 1;
			break;
		case 2:
			theDrawPos.mHeadX = 49;
			theDrawPos.mHeadY = 2;
			break;
		case 3:
			theDrawPos.mHeadX = 48;
			theDrawPos.mHeadY = 4;
			break;
		case 4:
			theDrawPos.mHeadX = 48;
			theDrawPos.mHeadY = 5;
			break;
		case 5:
			theDrawPos.mHeadX = 48;
			theDrawPos.mHeadY = 4;
			break;
		case 6:
			theDrawPos.mHeadX = 48;
			theDrawPos.mHeadY = 2;
			break;
		case 7:
			theDrawPos.mHeadX = 49;
			theDrawPos.mHeadY = 1;
			break;
		case 8:
			theDrawPos.mHeadX = 49;
			theDrawPos.mHeadY = 2;
			break;
		case 9:
			theDrawPos.mHeadX = 50;
			theDrawPos.mHeadY = 4;
			break;
		case 10:
			theDrawPos.mHeadX = 50;
			theDrawPos.mHeadY = 5;
			break;
		default:
			theDrawPos.mHeadX = 50;
			theDrawPos.mHeadY = 4;
			break;
		}
	}

	theDrawPos.mArmY = theDrawPos.mHeadY / 2;

	switch (mZombieType)
	{
	case ZombieType::ZOMBIE_FOOTBALL:
		theDrawPos.mImageOffsetY -= 16.0f;
		break;
	case ZombieType::ZOMBIE_YETI:
		theDrawPos.mImageOffsetY -= 20.0f;
		break;
	case ZombieType::ZOMBIE_CATAPULT:
		theDrawPos.mImageOffsetX -= 25.0f;
		theDrawPos.mImageOffsetY -= 18.0f;
		break;
	case ZombieType::ZOMBIE_POGO:
		theDrawPos.mImageOffsetY += 16.0f;
		break;
	case ZombieType::ZOMBIE_BALLOON:
		theDrawPos.mImageOffsetY += 17.0f;
		break;
	case ZombieType::ZOMBIE_POLEVAULTER:
		theDrawPos.mImageOffsetX -= 6.0f;
		theDrawPos.mImageOffsetY -= 11.0f;
		break;
	case ZombieType::ZOMBIE_ZAMBONI:
		theDrawPos.mImageOffsetX += 68.0f;
		theDrawPos.mImageOffsetY -= 23.0f;
		break;
	case ZombieType::ZOMBIE_GARGANTUAR:
	case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
	case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
		theDrawPos.mImageOffsetY -= 8.0f;
		break;
	case ZombieType::ZOMBIE_BOBSLED:
		theDrawPos.mImageOffsetY -= 12.0f;
		break;
	default:
		break;
	}

	if (mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
	{
		theDrawPos.mBodyY = -mAltitude;

		if (mInPool)
		{
			theDrawPos.mClipHeight = theDrawPos.mBodyY;
		}
		else
		{
			float aHeightLimit = std::min(mPhaseCounter, 40);
			theDrawPos.mClipHeight = theDrawPos.mBodyY + aHeightLimit;
		}

		if (IsOnHighGround())
		{
			theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
		}

		return;
	}

	if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;

		if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

			if (aBodyReanim->mAnimTime >= 0.56f && aBodyReanim->mAnimTime <= 0.65f)  // jumping onto the dolphin: takeoff
			{
				theDrawPos.mClipHeight = 0.0f;
			}
			else if (aBodyReanim->mAnimTime >= 0.75f)  // jumping onto the dolphin: falling
			{
				theDrawPos.mClipHeight = -mAltitude - 10.0f;
			}
		}
		else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
		{
			theDrawPos.mImageOffsetX += 70.0f;  // compensates the mPosX -= 70.0f applied when mounting the dolphin

			if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
			{
				theDrawPos.mClipHeight = -mAltitude - 15.0f;
			}
			else
			{
				theDrawPos.mClipHeight = -mAltitude - 10.0f;
			}
		}
		else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP)
		{
			theDrawPos.mImageOffsetX += 70.0f + mAltitude;

			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
			if (aBodyReanim->mAnimTime <= 0.06f)  // before jumping out of the water
			{
				theDrawPos.mClipHeight = -mAltitude - 10.0f;
			}
			else if (aBodyReanim->mAnimTime >= 0.5f && aBodyReanim->mAnimTime <= 0.76f) // mid-jump (after leaving the water, before re-entering)
			{
				theDrawPos.mClipHeight = -13.0f;
			}
		}
		else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
		{
			theDrawPos.mImageOffsetY += 50.0f;  // compensates for the distance advanced during the jump

			if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
			{
				theDrawPos.mClipHeight = -mAltitude + 44.0f;
			}
			else if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
			{
				theDrawPos.mClipHeight = -mAltitude + 36.0f;
			}
		}
		else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING && mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
		{
			theDrawPos.mClipHeight = -mAltitude;
		}
		else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN && mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
		{
			theDrawPos.mClipHeight = -mAltitude;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;

		if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
			if (aBodyReanim->mAnimTime >= 0.8f)  // after entering the water
			{
				theDrawPos.mClipHeight = -10.0f;
			}
		}
		else if (mInPool)
		{
			theDrawPos.mClipHeight = -mAltitude - 5.0f;
			theDrawPos.mClipHeight += 20.0f - 20.0f * mScaleZombie;
		}
	}
	else if (mInPool)
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mClipHeight = -mAltitude - 7.0f;
		theDrawPos.mClipHeight += 10.0f - 10.0f * mScaleZombie;

		if (mIsEating)
		{
			theDrawPos.mClipHeight += 7.0f;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mClipHeight = -mAltitude;

		if (IsOnHighGround())
		{
			theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING || mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE)
	{
		theDrawPos.mBodyY = -mAltitude;

		if (mPhaseCounter > 20)
		{
			theDrawPos.mClipHeight = -mAltitude;
		}
		else
		{
			theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mImageOffsetX -= 18.0f;

		if (IsOnHighGround())
		{
			theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
		}

		theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
	}
	else
	{
		theDrawPos.mBodyY = -mAltitude;
		theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
	}
}

int Zombie::GetDancerFrame()
{
	if (mFromWave == Zombie::ZOMBIE_WAVE_UI || IsImmobilizied())
		return 0;

	int aFrameLength = 20;
	int aFramesCount = 23;
	if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
	{
		aFramesCount = 11;
		aFrameLength = 10;
	}

#ifdef DO_FIX_BUGS
	if (mBoard)
	{
		return (mBoard->mMainCounter % (aFrameLength * aFramesCount)) / aFrameLength;  // fixes the "maid" cheat
	}
	else
	{
		return (mApp->mAppCounter % (aFrameLength * aFramesCount)) / aFrameLength;
	}
#else
	return (mApp->mAppCounter % (aFrameLength * aFramesCount)) / aFrameLength;
#endif
}

ZombiePhase Zombie::GetDancerPhase()
{
	int aFrame = GetDancerFrame();

	return
		aFrame <= 11 ? ZombiePhase::PHASE_DANCER_DANCING_LEFT :
		aFrame <= 12 ? ZombiePhase::PHASE_DANCER_WALK_TO_RAISE :
		aFrame <= 18 ? ZombiePhase::PHASE_DANCER_RAISE_LEFT_1 :
					   ZombiePhase::PHASE_DANCER_RAISE_LEFT_2;
}

void Zombie::DrawIceTrap(Graphics* g, const ZombieDrawPosition& theDrawPos, bool theFront)
{
	if (mInPool || mZombieType == ZombieType::ZOMBIE_BOSS)
		return;

	float aOffsetX = 46.0f;
	float aOffsetY = theDrawPos.mBodyY + 92.0f;
	float aScale = 1.0f;
	switch (mZombieType)
	{
	case ZombieType::ZOMBIE_POGO:
		aOffsetX -= 10.0f;
		aOffsetY += 20.0f;
		break;
	case ZombieType::ZOMBIE_GARGANTUAR:
	case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
	case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
		aOffsetX -= 20.0f;
		aOffsetY -= 7.0f;
		aScale = 1.6f;
		break;
	case ZombieType::ZOMBIE_BUNGEE:
		aOffsetX -= 45.0f;
		aOffsetY -= 23.0f;
		aScale = 1.2f;
		break;
	case ZombieType::ZOMBIE_DIGGER:
		aOffsetX -= 27.0f;
		break;
	case ZombieType::ZOMBIE_CATAPULT:
		aOffsetX += 32.0f;
		break;
	case ZombieType::ZOMBIE_BALLOON:
		aOffsetX -= 9.0f;
		aOffsetY += 27.0f;
		break;
	default:
		break;
	}

	PvzpDrawImageScaledF(g, theFront ? IMAGE_ICETRAP : IMAGE_ICETRAP2, aOffsetX, aOffsetY, aScale, aScale);
}

void Zombie::DrawButter(Graphics* g, const ZombieDrawPosition& theDrawPos)
{
	float aOffsetX = mPosX + theDrawPos.mImageOffsetX + theDrawPos.mHeadX + 11.0f;
	float aOffsetY = mPosY + theDrawPos.mImageOffsetY + theDrawPos.mHeadY + theDrawPos.mBodyY + 21.0f;
	float aScale = 1.0f;
	if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING)
	{
		GetTrackPosition("anim_head_look", aOffsetX, aOffsetY);
	}
	else if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		GetTrackPosition("Zombie_catapult_driver_head", aOffsetX, aOffsetY);
	}
	else if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		GetTrackPosition("anim_head1", aOffsetX, aOffsetY);
	}
	aOffsetX -= mPosX + 29.0f;
	aOffsetY -= mPosY + 36.0f;

	switch (mZombieType)
	{
	case ZombieType::ZOMBIE_POGO:
		aOffsetY -= 5.0f;
		break;
	case ZombieType::ZOMBIE_GARGANTUAR:
	case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
	case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
		aOffsetX -= 5.0f;
		aOffsetY -= 15.0f;
		aScale = 1.2f;
		break;
	case ZombieType::ZOMBIE_SQUASH_HEAD:
		aOffsetX += 6.0f;
		aOffsetY -= 9.0f;
		break;
	case ZombieType::ZOMBIE_WALLNUT_HEAD:
		aOffsetX -= 6.0f;
		aOffsetY -= 1.0f;
		break;
	case ZombieType::ZOMBIE_TALLNUT_HEAD:
		aOffsetX -= 24.0f;
		aOffsetY -= 39.0f;
		break;
	default:
		break;
	}

	PvzpDrawImageScaledF(g, IMAGE_REANIM_CORNPULT_BUTTER_SPLAT, aOffsetX, aOffsetY, aScale, aScale);
}

void Zombie::Draw(Graphics* g)
{
	if (mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
		return;

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);

	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON && !SetupDrawZombieWon(g))
		return;

	if (mIceTrapCounter > 0)
	{
		DrawIceTrap(g, aDrawPos, false);
	}
	if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL || mFromWave == Zombie::ZOMBIE_WAVE_UI)
	{
		if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
		{
			DrawReanim(g, aDrawPos, RENDER_GROUP_NORMAL);
			if (mZombieType == ZombieType::ZOMBIE_IMP && mHelmType == HelmType::HELMTYPE_PAIL && mHelmHealth > 0)
			{
				Image* aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET1;
				if (GetHelmDamageIndex() == 1)
					aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET2;
				else if (GetHelmDamageIndex() >= 2)
					aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET3;

				float aHeadX = mPosX + aDrawPos.mImageOffsetX + aDrawPos.mHeadX;
				float aHeadY = mPosY + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY;
				Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
				if (aBodyReanim != nullptr && aBodyReanim->TrackExists("anim_head1"))
					GetTrackPosition("anim_head1", aHeadX, aHeadY);
				aHeadX = aHeadX - mPosX - 29.0f + 20.0f;
				aHeadY = aHeadY - mPosY - 36.0f - 10.0f * mScaleZombie;
				PvzpDrawImageCenterScaledF(g, aBucketImage, aHeadX, aHeadY,
					0.75f * mScaleZombie, 0.75f * mScaleZombie);
			}
			if (mTierBucketArmorHealth > 0)
			{
				Image* aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET1;
				if (mTierBucketArmorHealth < mTierBucketArmorMaxHealth / 3)
					aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET3;
				else if (mTierBucketArmorHealth < mTierBucketArmorMaxHealth * 2 / 3)
					aBucketImage = IMAGE_REANIM_ZOMBIE_BUCKET2;

				float aHeadX = mPosX + aDrawPos.mImageOffsetX + aDrawPos.mHeadX;
				float aHeadY = mPosY + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY;
				Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
				if (aBodyReanim != nullptr && aBodyReanim->TrackExists("anim_head1"))
					GetTrackPosition("anim_head1", aHeadX, aHeadY);
				aHeadX = aHeadX - mPosX - 29.0f + 20.0f;
				aHeadY = aHeadY - mPosY - 36.0f - 10.0f * mScaleZombie;
				PvzpDrawImageCenterScaledF(g, aBucketImage, aHeadX, aHeadY,
					0.75f * mScaleZombie, 0.75f * mScaleZombie);
			}
		}
		else
		{
			//DrawZombie(g, aDrawPos);
		}
	}
	if (mIceTrapCounter > 0)
	{
		DrawIceTrap(g, aDrawPos, true);
	}
	if (mButteredCounter > 0)
	{
		DrawButter(g, aDrawPos);
	}

	if (mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
	{
		Graphics theParticleGraphics(*g);
		MakeParentGraphicsFrame(&theParticleGraphics);
		theParticleGraphics.mTransY += aDrawPos.mBodyY;

		if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
		{
			float aDrawHeight = 120.0f - aDrawPos.mClipHeight + 21.0f;
			theParticleGraphics.ClipRect(mX + aDrawPos.mImageOffsetX - 400.0f, mY + aDrawPos.mImageOffsetY - 28.0f, 920, aDrawHeight);
		}

		AttachmentDraw(mAttachmentID, &theParticleGraphics, false);
	}

	g->ClearClipRect();
}

bool Zombie::HasShadow()
{
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_BOBSLED_BOARDING ||
		mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
		return false;

	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
		mZombieType == ZombieType::ZOMBIE_CATAPULT ||
		mZombieType == ZombieType::ZOMBIE_BOSS)
		return false;

	if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		if (!IsOnBoard() || mHitUmbrella)
		{
			return false;
		}
	}

	if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER ||
		mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY ||
		mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
		return false;

	if (mInPool)
		return false;

	return mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL || mFromWave == Zombie::ZOMBIE_WAVE_UI;
}

bool Zombie::SetupDrawZombieWon(Graphics* g)
{
	if (mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
		return true;

	if (!mBoard->mCutScene->ShowZombieWalking())
		return false;

	switch (mBoard->mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:
	case BackgroundType::BACKGROUND_2_NIGHT:
		g->ClipRect(-123 - mX, -mY, mApp->mWidth, mApp->mHeight);
		break;
	case BackgroundType::BACKGROUND_3_POOL:
	case BackgroundType::BACKGROUND_4_FOG:
		g->ClipRect(-172 - mX, -mY, mApp->mWidth, mApp->mHeight);
		break;
	case BackgroundType::BACKGROUND_5_ROOF:
	case BackgroundType::BACKGROUND_6_BOSS:
		g->ClipRect(-220 - mX, -mY, BOARD_WIDTH, 187);
		break;
	default:
		break;
	}

	return true;
}

void Zombie::DrawShadow(Graphics* g)
{
	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON && !SetupDrawZombieWon(g))
		return;

	// shadows look really dumb in Zombiquarium
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ZOMBIQUARIUM) return;

	int aShadowType = 0;
	float aShadowOffsetX = aDrawPos.mImageOffsetX;
	float aShadowOffsetY = aDrawPos.mImageOffsetY + aDrawPos.mBodyY;
	float aScale = mScaleZombie;
	aShadowOffsetX += mScaleZombie * 20.0f - 20.0f;
	if (IsOnBoard() && mBoard->StageIsNight())
	{
		aShadowType = 1;
	}

	if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX -= 11.0f * mScaleZombie;
		}
		else
		{
			aShadowOffsetX += 20.0f + 21.0f * mScaleZombie;
		}
		aShadowOffsetY += 16.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 5.0f;
		}
		else
		{
			aShadowOffsetX += 29.0f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += -5.0f;
		}
		else
		{
			aShadowOffsetX += 36.0f;
		}
		aShadowOffsetY += 11.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 13.0f;
		}
		else
		{
			aShadowOffsetX += 20.0f;
		}
		aShadowOffsetY += 13.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_IMP)
	{
		aScale *= 0.6f;
		aShadowOffsetY += 7.0f;
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 13.0f;
		}
		else
		{
			aShadowOffsetX += 25.0f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		aShadowOffsetY += 5.0f;
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 14.0f;
		}
		else
		{
			aShadowOffsetX += 17.0f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		aShadowOffsetY += 5.0f;
		if (IsWalkingBackwards())
		{
			aShadowOffsetX -= 2.0f;
		}
		else
		{
			aShadowOffsetX += 35.0f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
	{
		aShadowOffsetY += 11.0f;
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 15.0f;
		}
		else
		{
			aShadowOffsetX += 19.0f;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		aShadowOffsetY += 20.0f;
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 20.0f;
		}
		else
		{
			aShadowOffsetX += 3.0f;
		}
	}
	else if (ZombieRules::IsGargantuarType(mZombieType))
	{
		aScale *= 1.5f;
		aShadowOffsetX += 27.0f;
		aShadowOffsetY += 7.0f;
	}
	else if (mApp->ReanimationTryToGet(mBodyReanimID) != nullptr)
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX += 11.0f;
		}
		else
		{
			aShadowOffsetX += 23.0f;
		}
	}
	else
	{
		if (IsWalkingBackwards())
		{
			aShadowOffsetX -= 2.0f;
		}
		else
		{
			aShadowOffsetX += 35.f;
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
	{
		aShadowOffsetY += 4.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		aShadowOffsetY += 13.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		aShadowOffsetX -= 12.0f;
		aScale = PvzpAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT - 1000, 100, mAltitude, 0.1f, 1.5f, PvzpCurves::CURVE_LINEAR);
	}

	if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER || mZombieHeight == ZombieHeight::HEIGHT_FALLING ||
		mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN || mZombieType == ZombieType::ZOMBIE_BUNGEE || IsBouncingPogo() || IsFlying())
	{
		aShadowOffsetY += mAltitude;
		if (mOnHighGround)
		{
			aShadowOffsetY -= HIGH_GROUND_HEIGHT;
		}
	}

	if (mInPool)
	{
		PvzpDrawImageCenterScaledF(g, IMAGE_WHITEWATER_SHADOW, aShadowOffsetX, aShadowOffsetY + 67.0f, aScale, aScale);
	}
	else
	{
		PvzpDrawImageCenterScaledF(g, aShadowType == 0 ? IMAGE_PLANTSHADOW : IMAGE_PLANTSHADOW2, aShadowOffsetX, aShadowOffsetY + 92.0f, aScale, aScale);
	}

	g->ClearClipRect();
}

void Zombie::GetTrackPosition(const char* theTrackName, float& thePosX, float& thePosY)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
	{
		thePosX = mPosX;
		thePosY = mPosY;
		return;
	}

	int aTrackIndex = aBodyReanim->FindTrackIndex(theTrackName);
	SexyTransform2D aMatrix;
	aBodyReanim->GetTrackMatrix(aTrackIndex, aMatrix);
	thePosX = aMatrix.m02 + mPosX;
	thePosY = aMatrix.m12 + mPosY;
}
