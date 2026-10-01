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

#include "../Entities/Coin.h"
#include "Plant.h"
#include "../Board/Board.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Modes/ZenGarden.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../GameConstants.h"
#include <array>
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "../Rules/TargetingRules.h"

int Plant::CalcRenderOrder()
{
	PLANT_ORDER anOrder = PLANT_ORDER::PLANT_ORDER_NORMAL;
	RenderLayer aLayer = RenderLayer::RENDER_LAYER_PLANT;

	SeedType aSeedType = mSeedType;
	if (mSeedType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
		aSeedType = mImitaterType;

	if (mApp->IsWallnutBowlingLevel())
	{
		aLayer = RenderLayer::RENDER_LAYER_PROJECTILE;
	}
	else if (aSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_PUMPKIN;
	}
	else if (IsFlying(aSeedType))
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_FLYER;
	}
	else if (aSeedType == SeedType::SEED_FLOWERPOT || (aSeedType == SeedType::SEED_LILYPAD && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN))
	{
		anOrder = PLANT_ORDER::PLANT_ORDER_LILYPAD;
	}

	return Board::MakeRenderOrder(aLayer, mRow, anOrder * 5 - mX + 800);
}

void Plant::SetSleeping(bool theIsAsleep)
{
	if (mIsAsleep == theIsAsleep || NotOnGround())
		return;

	mIsAsleep = theIsAsleep;
	if (theIsAsleep)
	{
		float aPosX = mX + 50.0f;
		float aPosY = mY + 40.0f;
		if (mSeedType == SeedType::SEED_FUMESHROOM)
			aPosX += 12.0f;
		else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
			aPosY -= 20.0f;
		else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
			aPosY -= 12.0f;

		Reanimation* aSleepReanim = mApp->AddReanimation(aPosX, aPosY, mRenderOrder + 2, ReanimationType::REANIM_SLEEPING);
		aSleepReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
		aSleepReanim->mAnimRate = RandRangeFloat(6.0f, 8.0f);
		aSleepReanim->mAnimTime = RandRangeFloat(0.0f, 0.9f);
		mSleepingReanimID = mApp->ReanimationGetID(aSleepReanim);
	}
	else
	{
		mApp->RemoveReanimation(mSleepingReanimID);
		mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if (theIsAsleep)
	{
		if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
		{
			aBodyReanim->SetFramesForLayer("anim_bigsleep");
		}
		else if (aBodyReanim->TrackExists("anim_sleep"))
		{
			float aAnimTime = aBodyReanim->mAnimTime;
			aBodyReanim->StartBlend(20);
			aBodyReanim->SetFramesForLayer("anim_sleep");
			aBodyReanim->mAnimTime = aAnimTime;
		}
		else
		{
			aBodyReanim->mAnimRate = 1.0f;
		}

		EndBlink();
	}
	else
	{
		if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
		{
			aBodyReanim->SetFramesForLayer("anim_bigidle");
		}
		else if (aBodyReanim->TrackExists("anim_idle"))
		{
			float aAnimTime = aBodyReanim->mAnimTime;
			aBodyReanim->StartBlend(20);
			aBodyReanim->SetFramesForLayer("anim_idle");
			aBodyReanim->mAnimTime = aAnimTime;
		}

		if (aBodyReanim->mAnimRate < 2.0f && IsInPlay())
			aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
	}
}

void Plant::PlayBodyReanim(const char* theTrackName, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
	ReanimationID aAnimationID = mSeedType == SeedType::SEED_CHOMPERNUT ? mHeadReanimID : mBodyReanimID;
	Reanimation* aBodyReanim = mApp->ReanimationGet(aAnimationID);

	if (theBlendTime > 0)
		aBodyReanim->StartBlend(theBlendTime);
	if (theAnimRate > 0.0f)
		aBodyReanim->mAnimRate = theAnimRate;

	aBodyReanim->mLoopType = theLoopType;
	aBodyReanim->mLoopCount = 0;
	aBodyReanim->SetFramesForLayer(theTrackName);
}

void Plant::UpdateReanimColor()
{
	if (!IsOnBoard())
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	SeedType aSeedType = mBoard->GetSeedTypeInCursor();
	Color aColorOverride;

	bool isOnGlove = false;
	if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
	{
		Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mBoard->mCursorObject->mGlovePlantID));
		if (aPlant && aPlant->mPlantCol == mPlantCol && aPlant->mRow == mRow)
		{
			isOnGlove = true;
		}
	}

	if (isOnGlove)
	{
		aColorOverride = Color(128, 128, 128);
	}
	else if (IsPartOfUpgradableTo(aSeedType) && mBoard->CanPlantAt(mPlantCol, mRow, aSeedType) == PLANTING_OK)
	{
		aColorOverride = GetFlashingColor(mBoard->mMainCounter, 90);
	}
	else if (aSeedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT && mBoard->CanPlantAt(mPlantCol - 1, mRow, aSeedType) == PLANTING_OK)
	{
		aColorOverride = GetFlashingColor(mBoard->mMainCounter, 90);
	}
	else if (mSeedType == SeedType::SEED_GATLINGPEA && mGatlingPeaVisualBlend > 0.0f)
	{
		float aBlend = std::clamp(mGatlingPeaVisualBlend, 0.0f, 1.0f);
		aColorOverride = Color(
			static_cast<int>(255.0f + (108.0f - 255.0f) * aBlend),
			static_cast<int>(255.0f + (78.0f - 255.0f) * aBlend),
			static_cast<int>(255.0f + (88.0f - 255.0f) * aBlend));
	}
	else if (mSeedType == SeedType::SEED_CATTAIL && mBoard->mCatTailOverdriveActive)
	{
		aColorOverride = Color(255, 96, 96);
	}
	else if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
	{
		aColorOverride = Color(255, 64, 64);
	}
	else
	{
		aColorOverride = Color(255, 255, 255);
	}

	aBodyReanim->mColorOverride = aColorOverride;

	if (mHighlighted)
	{
		aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 196);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
		if (mImitaterType == SeedType::SEED_IMITATER)
		{
			aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 92);
		}
	}
	else if (mBeghouledFlashCountdown > 0)
	{
		int anAlpha = PvzpAnimateCurve(50, 0, mBeghouledFlashCountdown % 50, 1, 128, PvzpCurves::CURVE_BOUNCE);
		aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, anAlpha);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
	}
	else if (mEatenFlashCountdown > 0)
	{
		int aGrayness = std::clamp(mEatenFlashCountdown * 3, 0, mImitaterType == SeedType::SEED_IMITATER ? 128 : 255);
		aBodyReanim->mExtraAdditiveColor = Color(aGrayness, aGrayness, aGrayness);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
	}
	else if (mSeedType == SeedType::SEED_CATTAIL && mBoard->mCatTailOverdriveActive)
	{
		int anAlpha = PvzpAnimateCurve(60, 0, mBoard->mMainCounter % 60, 55, 145, PvzpCurves::CURVE_BOUNCE);
		aBodyReanim->mExtraAdditiveColor = Color(255, 24, 16, anAlpha);
		aBodyReanim->mEnableExtraAdditiveDraw = true;
	}
	else
	{
		aBodyReanim->mEnableExtraAdditiveDraw = false;
	}

	if (mBeghouledFlashCountdown > 0)
	{
		int anAlpha = PvzpAnimateCurve(50, 0, mBeghouledFlashCountdown % 50, 1, 128, PvzpCurves::CURVE_BOUNCE);
		aBodyReanim->mExtraOverlayColor = Color(255, 255, 255, anAlpha);
		aBodyReanim->mEnableExtraOverlayDraw = true;
	}
	else
	{
		aBodyReanim->mEnableExtraOverlayDraw = false;
	}

	aBodyReanim->PropogateColorToAttachments();
	if (mSeedType == SeedType::SEED_CHOMPERNUT)
	{
		Reanimation* aChomperReanim = mApp->ReanimationTryToGet(mHeadReanimID);
		if (aChomperReanim != nullptr)
		{
			aChomperReanim->mColorOverride = aBodyReanim->mColorOverride;
			aChomperReanim->mExtraAdditiveColor = aBodyReanim->mExtraAdditiveColor;
			aChomperReanim->mEnableExtraAdditiveDraw = aBodyReanim->mEnableExtraAdditiveDraw;
			aChomperReanim->mExtraOverlayColor = aBodyReanim->mExtraOverlayColor;
			aChomperReanim->mEnableExtraOverlayDraw = aBodyReanim->mEnableExtraOverlayDraw;
		}
	}
}

void Plant::UpdateReanim()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	UpdateReanimColor();

	float aOffsetX = mShakeOffsetX * mApp->GetScreenShakeScale();
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
	float aScaleX = 1.0f, aScaleY = 1.0f;
	if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME) &&
		(mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_MARIGOLD))
	{
		aScaleX = 1.5f;
		aScaleY = 1.5f;
		aOffsetX -= 20.0f;
		aOffsetY -= 40.0f;
	}
	if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		aScaleX = 2.0f;
		aScaleY = 2.0f;
		aOffsetX -= 76.0f;
		aOffsetY -= 64.0f;
	}
	if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aScaleX = 0.8f;
		aScaleY = 0.8f;
		aOffsetX += 12.0f;
		aOffsetY += 10.0f;
	}
	if (mSeedType == SeedType::SEED_POTATOMINE)
	{
		aScaleX = 0.8f;
		aScaleY = 0.8f;
		aOffsetX += 12.0f;
		aOffsetY += 12.0f;
	}
	if (mState == PlantState::STATE_GRAVEBUSTER_EATING)
	{
		aOffsetY += PvzpAnimateCurveFloat(400, 0, mStateCountdown, 0.0f, 30.0f, PvzpCurves::CURVE_LINEAR);
	}
	if (mWakeUpCounter > 0)
	{
		float aScaleFactor = PvzpAnimateCurveFloat(70, 0, mWakeUpCounter, 1.0f, 0.8f, PvzpCurves::CURVE_EASE_SIN_WAVE);
		aScaleY *= aScaleFactor;
		aOffsetY += 80.0f - 80.0f * aScaleFactor;
	}

	aBodyReanim->Update();

	if (mSeedType == SeedType::SEED_LEFTPEATER)
	{
		aOffsetX += 80.0f * aScaleX;
		aScaleX *= -1.0f;
	}

	if (mPottedPlantIndex != -1)
	{
		PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[mPottedPlantIndex];

		if (aPottedPlant->mFacing == PottedPlant::FacingDirection::FACING_LEFT)
		{
			aOffsetX += 80.0f * aScaleX;
			aScaleX *= -1.0f;
		}

		float aOffsetXStart, aOffsetXEnd;
		float aOffsetYStart, aOffsetYEnd;
		float aScaleStart, aScaleEnd;
		if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
		{
			aOffsetXStart = 20.0f;
			aOffsetXEnd = 20.0f;
			aOffsetYStart = 40.0f;
			aOffsetYEnd = 40.0f;
			aScaleStart = 0.5f;
			aScaleEnd = 0.5f;
		}
		else if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
		{
			aOffsetXStart = 20.0f;
			aOffsetXEnd = 10.0f;
			aOffsetYStart = 40.0f;
			aOffsetYEnd = 20.0f;
			aScaleStart = 0.5f;
			aScaleEnd = 0.75f;
		}
		else
		{
			aOffsetXStart = 10.0f;
			aOffsetXEnd = 0.0f;
			aOffsetYStart = 20.0f;
			aOffsetYEnd = 0.0f;
			aScaleStart = 0.75f;
			aScaleEnd = 1.0f;
		}

		float aAnimatedOffsetX = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aOffsetXStart, aOffsetXEnd, PvzpCurves::CURVE_LINEAR);
		float aAnimatedOffsetY = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aOffsetYStart, aOffsetYEnd, PvzpCurves::CURVE_LINEAR);
		float aAnimatedScale = PvzpAnimateCurveFloat(100, 0, mStateCountdown, aScaleStart, aScaleEnd, PvzpCurves::CURVE_LINEAR);

		aOffsetX += aAnimatedOffsetX * aScaleX;
		aOffsetY += aAnimatedOffsetY * aScaleY;
		aScaleX *= aAnimatedScale;
		aScaleY *= aAnimatedScale;
		aOffsetX += mApp->mZenGarden->ZenPlantOffsetX(aPottedPlant);
		aOffsetY += mApp->mZenGarden->PlantPottedDrawHeightOffset(mSeedType, aScaleY);
	}

	aBodyReanim->SetPosition(aOffsetX, aOffsetY);
	aBodyReanim->OverrideScale(aScaleX, aScaleY);
	if (mSeedType == SeedType::SEED_CHOMPERNUT)
	{
		Reanimation* aChomperReanim = mApp->ReanimationTryToGet(mHeadReanimID);
		if (aChomperReanim != nullptr)
		{
			if (!aChomperReanim->mIsAttachment && aBodyReanim->TrackExists("anim_idle"))
				AttachReanim(aBodyReanim->GetTrackInstanceByName("anim_idle")->mAttachmentID, aChomperReanim, 10.0f, -38.0f);
			aChomperReanim->OverrideScale(0.7f * aScaleX, 0.7f * aScaleY);
		}
	}
}

Reanimation* Plant::AttachBlinkAnim(Reanimation* theReanimBody)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(mSeedType);
	LawnApp* aApp = (LawnApp*)gSexyAppBase;
	Reanimation* aAnimToAttach = theReanimBody;
	const char* aTrackToPlay = "anim_blink";
	const char* aTrackToAttach = nullptr;

	if (mSeedType == SeedType::SEED_WALLNUT || IsTallNut() ||
		mSeedType == SeedType::SEED_EXPLODE_O_NUT || mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		int aHit = Rand(10);
		if (aHit < 1 && theReanimBody->TrackExists("anim_blink_twitch"))
		{
			aTrackToPlay = "anim_blink_twitch";
		}
		else
		{
			aTrackToPlay = aHit < 7 ? "anim_blink_twice" : "anim_blink_thrice";
		}
	}
	else if (mSeedType == SeedType::SEED_THREEPEATER)
	{
		int aHit = Rand(3);
		if (aHit == 0)
		{
			aTrackToPlay = "anim_blink1";
			aTrackToAttach = "anim_face1";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head1");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else if (aHit == 1)
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head2");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else
		{
			aTrackToPlay = "anim_blink3";
			aTrackToAttach = "anim_face3";
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_head3");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
	}
	else if (mSeedType == SeedType::SEED_SPLITPEA)
	{
		if (Rand(2) == 0)
		{
			aTrackToPlay = "anim_blink";
			aTrackToAttach = "anim_face";
			aAnimToAttach = mApp->ReanimationTryToGet(mHeadReanimID);
		}
		else
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
			aAnimToAttach = mApp->ReanimationTryToGet(mHeadReanimID2);
		}
	}
	else if (mSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		if (Rand(2) == 0)
		{
			aTrackToPlay = "anim_blink";
			aTrackToAttach = "anim_face";
		}
		else
		{
			aTrackToPlay = "anim_blink2";
			aTrackToAttach = "anim_face2";
		}
	}
	else if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_LEFTPEATER || mSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (theReanimBody->TrackExists("anim_stem"))
		{
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_stem");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
		else if (theReanimBody->TrackExists("anim_idle"))
		{
			ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName("anim_idle");
			aAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
		}
	}

	if (aAnimToAttach == nullptr)
	{
		PvzpLogLn("Missing head anim");
		return nullptr;
	}

	if (!theReanimBody->TrackExists(aTrackToPlay))
		return nullptr;

	Reanimation* aBlinkReanim = aApp->mEffectSystem->mReanimationHolder->AllocReanimation(0.0f, 0.0f, 0, aPlantDef.mReanimationType);
	aBlinkReanim->SetFramesForLayer(aTrackToPlay);
	aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
	aBlinkReanim->mAnimRate = 15.0f;
	aBlinkReanim->mColorOverride = theReanimBody->mColorOverride;

	if (aTrackToAttach && aAnimToAttach->TrackExists(aTrackToAttach))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, aTrackToAttach);
	}
	else if (aAnimToAttach->TrackExists("anim_face"))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, "anim_face");
	}
	else if (aAnimToAttach->TrackExists("anim_idle"))
	{
		aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, "anim_idle");
	}
	else
	{
		PvzpLogLn("Missing anim_idle for blink");
	}

	aBlinkReanim->mFilterEffect = theReanimBody->mFilterEffect;
	return aBlinkReanim;
}

void Plant::DoBlink()
{
	mBlinkCountdown = 400 + Rand(400);

	if (NotOnGround() || mShootingCounter != 0)
		return;

	if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_POTATO_ARMED)
		return;

	if (mState == PlantState::STATE_CACTUS_RISING || mState == PlantState::STATE_CACTUS_HIGH || mState == PlantState::STATE_CACTUS_LOWERING ||
		mState == PlantState::STATE_MAGNETSHROOM_SUCKING || mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
		return;

	EndBlink();
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	if ((IsTallNut() && aBodyReanim->GetImageOverride("anim_idle") == IMAGE_REANIM_TALLNUT_CRACKED2) ||
		(mSeedType == SeedType::SEED_GARLIC && aBodyReanim->GetImageOverride("anim_face") == IMAGE_REANIM_GARLIC_BODY3))
		return;

	if (mSeedType == SeedType::SEED_WALLNUT || IsTallNut() ||
		mSeedType == SeedType::SEED_EXPLODE_O_NUT || mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		mBlinkCountdown = 1000 + Rand(1000);
	}

	Reanimation* aBlinkReanim = AttachBlinkAnim(aBodyReanim);
	if (aBlinkReanim)
	{
		mBlinkReanimID = mApp->ReanimationGetID(aBlinkReanim);
	}
	aBodyReanim->AssignRenderGroupToPrefix("anim_eye", RENDER_GROUP_HIDDEN);
}

void Plant::EndBlink()
{
	if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		mApp->RemoveReanimation(mBlinkReanimID);
		mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;

		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim)
		{
			aBodyReanim->AssignRenderGroupToPrefix("anim_eye", RENDER_GROUP_NORMAL);
		}
	}
}

void Plant::UpdateBlink()
{
	if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		Reanimation* aBlinkReanim = mApp->ReanimationTryToGet(mBlinkReanimID);
		if (aBlinkReanim == nullptr || aBlinkReanim->mLoopCount > 0)
		{
			EndBlink();
		}
	}

	if (mIsAsleep)
		return;

	if (mBlinkCountdown > 0)
	{
		mBlinkCountdown--;
		if (mBlinkCountdown == 0)
		{
			DoBlink();
		}
	}
}

void Plant::AnimateNuts()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	Image* aCracked1;
	Image* aCracked2;
	const char* aTrackToOverride;
	if (mSeedType == SeedType::SEED_WALLNUT)
	{
		aCracked1 = IMAGE_REANIM_WALLNUT_CRACKED1;
		aCracked2 = IMAGE_REANIM_WALLNUT_CRACKED2;
		aTrackToOverride = "anim_face";
	}
	else if (IsTallNut())
	{
		aCracked1 = IMAGE_REANIM_TALLNUT_CRACKED1;
		aCracked2 = IMAGE_REANIM_TALLNUT_CRACKED2;
		aTrackToOverride = "anim_idle";
	}
	else return;

	int aPosX = mX + 40;
	int aPosY = mY + 10;
	if (IsTallNut())
	{
		aPosY -= 32;
	}

	Image* aImageOverride = aBodyReanim->GetImageOverride(aTrackToOverride);
	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != aCracked2)
		{
			aBodyReanim->SetImageOverride(aTrackToOverride, aCracked2);
			mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE);
		}
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != aCracked1)
		{
			aBodyReanim->SetImageOverride(aTrackToOverride, aCracked1);
			mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE);
		}
	}
	else
	{
		aBodyReanim->SetImageOverride(aTrackToOverride, nullptr);
	}

	if (IsInPlay() && !mApp->IsIZombieLevel())
	{
		if (mRecentlyEatenCountdown > 0)
		{
			aBodyReanim->mAnimRate = 0.1f;
			return;
		}

		if (aBodyReanim->mAnimRate < 1.0f && mOnBungeeState != PlantOnBungeeState::RISING_WITH_BUNGEE)
		{
			aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
		}
	}
}

void Plant::AnimateGarlic()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	Image* aImageOverride = aBodyReanim->GetImageOverride("anim_face");

	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != IMAGE_REANIM_GARLIC_BODY3)
		{
			aBodyReanim->SetImageOverride("anim_face", IMAGE_REANIM_GARLIC_BODY3);
			aBodyReanim->AssignRenderGroupToPrefix("Garlic_stem", RENDER_GROUP_HIDDEN);
		}
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != IMAGE_REANIM_GARLIC_BODY2)
		{
			aBodyReanim->SetImageOverride("anim_face", IMAGE_REANIM_GARLIC_BODY2);
		}
	}
	else
	{
		aBodyReanim->SetImageOverride("anim_face", nullptr);
	}
}

void Plant::AnimatePumpkin()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	Image* aImageOverride = aBodyReanim->GetImageOverride("Pumpkin_front");

	if (mPlantHealth < mPlantMaxHealth / 3)
	{
		if (aImageOverride != IMAGE_REANIM_PUMPKIN_DAMAGE3)
			aBodyReanim->SetImageOverride("Pumpkin_front", IMAGE_REANIM_PUMPKIN_DAMAGE3);
	}
	else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
	{
		if (aImageOverride != IMAGE_REANIM_PUMPKIN_DAMAGE1)
			aBodyReanim->SetImageOverride("Pumpkin_front", IMAGE_REANIM_PUMPKIN_DAMAGE1);
	}
	else
	{
		aBodyReanim->SetImageOverride("Pumpkin_front", nullptr);
	}
}

void Plant::Animate()
{
	if ((mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO) && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mShakeOffsetX = RandRangeFloat(-1.0f, 1.0f);
		mShakeOffsetY = RandRangeFloat(-1.0f, 1.0f);
	}

	if (mRecentlyEatenCountdown > 0)
	{
		mRecentlyEatenCountdown--;
	}
	if (mEatenFlashCountdown > 0)
	{
		mEatenFlashCountdown--;
	}
	if (mBeghouledFlashCountdown > 0)
	{
		mBeghouledFlashCountdown--;
	}

	if (mSquished)
	{
		mFrame = 0;
		return;
	}

	if (mSeedType == SeedType::SEED_WALLNUT || IsTallNut())
	{
		AnimateNuts();
	}
	else if (mSeedType == SeedType::SEED_GARLIC)
	{
		AnimateGarlic();
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		AnimatePumpkin();
	}

	UpdateBlink();

	if (mAnimPing)
	{
		if (mAnimCounter < mFrameLength * mNumFrames - 1)
		{
			mAnimCounter++;
		}
		else
		{
			mAnimPing = false;
			mAnimCounter -= mFrameLength;
		}
	}
	else if (mAnimCounter > 0)
	{
		mAnimCounter--;
	}
	else
	{
		mAnimPing = true;
		mAnimCounter += mFrameLength;
	}
	mFrame = mAnimCounter / mFrameLength;
}

void Plant::PlayIdleAnim(float theRate)
{
	ReanimationID aAnimationID = mSeedType == SeedType::SEED_CHOMPERNUT ? mHeadReanimID : mBodyReanimID;
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aAnimationID);
	if (aBodyReanim)
	{
		PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, theRate);
		if (mApp->IsIZombieLevel())
		{
			aBodyReanim->mAnimRate = 0.0f;
		}
	}
}
