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

#include "Board.h"
#include "Cutscene.h"
#include "LawnMower.h"
#include "../LawnApp.h"
#include "System/ReanimationLawn.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/PvzpStringFile.h"
#include <format>

static std::string MowerStateEventDetails(Board* theBoard, LawnMower* theMower)
{
	return std::format(
		R"({{"mower_id":{},"mower_type":{},"row_index":{},"lane":{},"state":{},"dead":{},"visible":{},"x":{:.2f},"y":{:.2f}}})",
		theBoard->mLawnMowers.DataArrayGetID(theMower), static_cast<int>(theMower->mMowerType),
		theMower->mRow, theMower->mRow + 1, static_cast<int>(theMower->mMowerState),
		theMower->mDead ? "true" : "false", theMower->mVisible ? "true" : "false", theMower->mPosX, theMower->mPosY);
}

void LawnMower::LawnMowerInitialize(int theRow)
{
	mApp = (LawnApp*)gSexyAppBase;
	mRow = theRow;
	mPosX = -160.0f;
	mBoard = mApp->mBoard;
	mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, theRow, 0);
	mPosY = mBoard->GetPosYBasedOnRow(mPosX + 40.0f, theRow) + 23.0f;
	mDead = false;
	mMowerState = LawnMowerState::MOWER_READY;
	mVisible = true;
	mChompCounter = 0;
	mRollingInCounter = 0;
	mSquishedCounter = 0;
	mLastPortalX = -1;

	ReanimationType aReanimType;
	if (mBoard->StageHasRoof())
	{
		mMowerType = LawnMowerType::LAWNMOWER_ROOF;
		aReanimType = ReanimationType::REANIM_ROOF_CLEANER;
	}
	else if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL && mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER])
	{
		mMowerType = LawnMowerType::LAWNMOWER_POOL;
		aReanimType = ReanimationType::REANIM_POOL_CLEANER;
	}
	else
	{
		mMowerType = LawnMowerType::LAWNMOWER_LAWN;
		aReanimType = ReanimationType::REANIM_LAWNMOWER;
	}

	Reanimation* aMowerReanim = mApp->AddReanimation(0.0f, 18.0f, mRenderOrder, aReanimType);
	aMowerReanim->mAnimRate = 0.0f;
	aMowerReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aMowerReanim->mIsAttachment = true;
	aMowerReanim->OverrideScale(0.85f, 0.85f);
	mReanimID = mApp->ReanimationGetID(aMowerReanim);

	if (mMowerType == LawnMowerType::LAWNMOWER_LAWN)
	{
		aMowerReanim->SetFramesForLayer("anim_normal");
	}
	else if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		aMowerReanim->OverrideScale(0.8f, 0.8f);
		aMowerReanim->SetFramesForLayer("anim_land");
		aMowerReanim->SetTruncateDisappearingFrames(nullptr, false);
	}

	if (mBoard->mSuperMowerMode && mMowerType == LawnMowerType::LAWNMOWER_LAWN)
	{
		EnableSuperMower(true);
	}
}

void LawnMower::ConvertToPoolCleaner()
{
	if (mMowerType == LawnMowerType::LAWNMOWER_POOL ||
		!mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER])
		return;

	Reanimation* aOldReanim = mApp->ReanimationTryToGet(mReanimID);
	if (aOldReanim)
		aOldReanim->ReanimationDie();

	mMowerType = LawnMowerType::LAWNMOWER_POOL;
	mMowerHeight = MowerHeight::MOWER_HEIGHT_LAND;
	mAltitude = 0.0f;
	Reanimation* aMowerReanim = mApp->AddReanimation(0.0f, 18.0f, mRenderOrder, ReanimationType::REANIM_POOL_CLEANER);
	aMowerReanim->mAnimRate = 0.0f;
	aMowerReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aMowerReanim->mIsAttachment = true;
	aMowerReanim->OverrideScale(0.8f, 0.8f);
	aMowerReanim->SetFramesForLayer("anim_land");
	aMowerReanim->SetTruncateDisappearingFrames(nullptr, false);
	mReanimID = mApp->ReanimationGetID(aMowerReanim);
}

void LawnMower::UpdatePool()
{
	bool isPoolRange = false;
	if (mPosX > 26.0f && mPosX < 660.0f)
	{
		isPoolRange = true;
	}

	Reanimation* aMowerReanim = mApp->ReanimationGet(mReanimID);
	if (isPoolRange && mMowerHeight == MowerHeight::MOWER_HEIGHT_LAND)
	{
		Reanimation* aSplashReanim = mApp->AddReanimation(mPosX + 0.0f, mPosY + 25.0f, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
		aSplashReanim->OverrideScale(1.2f, 0.8f);
		mApp->AddPvzpParticle(mPosX + 0.0f + 50.0f, mPosY + 0.0f + 42.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
		mApp->PlayFoley(FoleyType::FOLEY_ZOMBIESPLASH);
		mMowerHeight = MowerHeight::MOWER_HEIGHT_DOWN_TO_POOL;
	}
	else if (mMowerHeight == MowerHeight::MOWER_HEIGHT_DOWN_TO_POOL)
	{
		mAltitude -= 2.0f;
		if (mAltitude <= -28.0f)
		{
			mAltitude = 0.0f;
			mMowerHeight = MowerHeight::MOWER_HEIGHT_IN_POOL;
			aMowerReanim->PlayReanim("anim_water", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
		}
	}
	else if (mMowerHeight == MowerHeight::MOWER_HEIGHT_IN_POOL)
	{
		if (!isPoolRange)
		{
			mAltitude = -28.0f;
			mMowerHeight = MowerHeight::MOWER_HEIGHT_UP_TO_LAND;
			Reanimation* aSplashReanim = mApp->AddReanimation(mPosX + 0.0f, mPosY + 25.0f, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
			aSplashReanim->OverrideScale(1.2f, 0.8f);
			mApp->AddPvzpParticle(mPosX + 0.0f + 50.0f, mPosY + 0.0f + 42.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
			mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
			aMowerReanim->PlayReanim("anim_land", ReanimLoopType::REANIM_LOOP, 0, 0.0f);
		}
	}
	else if (mMowerHeight == MowerHeight::MOWER_HEIGHT_UP_TO_LAND)
	{
		mAltitude += 2.0f;
		if (mAltitude >= 0.0f)
		{
			mAltitude = 0.0f;
			mMowerHeight = MowerHeight::MOWER_HEIGHT_LAND;
		}
	}

	if (mMowerHeight == MowerHeight::MOWER_HEIGHT_IN_POOL && aMowerReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aMowerReanim->mLoopCount > 0)
	{
		aMowerReanim->PlayReanim("anim_water", ReanimLoopType::REANIM_LOOP, 10, 35.0f);
	}
}

void LawnMower::MowZombie(Zombie* theZombie)
{
	if (mMowerState == LawnMowerState::MOWER_READY)
	{
		StartMower(theZombie);
		mChompCounter = 25;
	}
	else if (mMowerState == LawnMowerState::MOWER_TRIGGERED)
	{
		mChompCounter = 50;
	}

	if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SHOOP);
		if (mMowerHeight == MowerHeight::MOWER_HEIGHT_IN_POOL)
		{
			Reanimation* aMowerReanim = mApp->ReanimationGet(mReanimID);
			aMowerReanim->PlayReanim("anim_suck", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 35.0f);
		}
		else
		{
			Reanimation* aMowerReanim = mApp->ReanimationGet(mReanimID);
			aMowerReanim->PlayReanim("anim_landsuck", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 35.0f);
		}

		theZombie->DieWithLoot();
	}
	else
	{
		mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
		theZombie->MowDown();
	}
}

void LawnMower::Update()
{
	if (mMowerState == LawnMowerState::MOWER_SQUISHED)
	{
		mSquishedCounter--;
		if (mSquishedCounter <= 0)
		{
			Die();
		}
		return;
	}

	if (mMowerState == LawnMowerState::MOWER_ROLLING_IN)
	{
		mRollingInCounter++;
		mPosX = PvzpAnimateCurveFloat(0, 100, mRollingInCounter, -160.0f, -21.0f, PvzpCurves::CURVE_EASE_IN_OUT);
		if (mRollingInCounter == 100)
		{
			mMowerState = LawnMowerState::MOWER_READY;
			mBoard->RecordGameplayEvent("lawn_mower_ready", MowerStateEventDetails(mBoard, this));
		}
		return;
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mBoard->mCutScene->ShouldRunUpsellBoard())
	{
		return;
	}
	if (!mVisible && mMowerState == LawnMowerState::MOWER_READY && mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		mVisible = true;
		if (mPosX < -21.0f)
			mPosX = -21.0f;
		mPosY = mBoard->GetPosYBasedOnRow(mPosX + 40.0f, mRow) + 23.0f;
		mBoard->RecordGameplayEvent("lawn_mower_visibility_recovered", MowerStateEventDetails(mBoard, this));
	}
	if (mMowerState == LawnMowerState::MOWER_READY && !mVisible)
		return;

	Rect aAttackRect = GetLawnMowerAttackRect();
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			continue;

		if (aZombie->mRow - mRow)
			continue;

		if (aZombie->mZombieType != ZombieType::ZOMBIE_BOSS &&
			aZombie->mRow - mRow == 0 &&
			aZombie->mZombiePhase != ZombiePhase::PHASE_ZOMBIE_MOWERED &&
			!aZombie->IsTangleKelpTarget() &&
			aZombie->EffectedByDamage(127U, aZombie->IsSunTierInvulnerable()))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aOverlap = GetRectOverlap(aAttackRect, aZombieRect);
			if (aOverlap > (aZombie->mZombieType == ZombieType::ZOMBIE_BALLOON ? 20 : 0))
			{
				// bungee zombies and dead zombies cannot trigger the mower themselves
				if (mMowerState != LawnMowerState::MOWER_READY || (aZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && aZombie->mHasHead))
				{
					MowZombie(aZombie);
				}
			}
		}
	}

	if (mMowerState != LawnMowerState::MOWER_TRIGGERED && mMowerState != LawnMowerState::MOWER_SQUISHED)
	{
		return;
	}

	float aSpeed = 3.33f;
	if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		aSpeed = 2.5f;
	}
	if (mChompCounter > 0)
	{
		mChompCounter--;
		aSpeed = PvzpAnimateCurveFloat(50, 0, mChompCounter, aSpeed, 1.0f, PvzpCurves::CURVE_BOUNCE_SLOW_MIDDLE);
	}
	mPosX += aSpeed;
	mPosY = mBoard->GetPosYBasedOnRow(mPosX + 40.0f, mRow) + 23.0f;

	if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		UpdatePool();
	}
	if (mMowerType == LawnMowerType::LAWNMOWER_LAWN && mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL && mPosX > 50.0f)
	{
		Reanimation* aSplashReanim = mApp->AddReanimation(mPosX, mPosY + 25.0f, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
		aSplashReanim->OverrideScale(1.2f, 0.8f);
		mApp->AddPvzpParticle(mPosX + 50.0f, mPosY + 67.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);
		mApp->PlaySample(SOUND_ZOMBIE_ENTERING_WATER);
		mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_LAWNMOWER);
		Die();
	}
	if (mPosX > mApp->mWidth)
	{
		Die();
	}

	mApp->ReanimationGet(mReanimID)->Update();
}

void LawnMower::Draw(Graphics* g)
{
	if (!mVisible)
		return;

	if (mMowerHeight != MowerHeight::MOWER_HEIGHT_UP_TO_LAND &&
		mMowerHeight != MowerHeight::MOWER_HEIGHT_DOWN_TO_POOL &&
		mMowerHeight != MowerHeight::MOWER_HEIGHT_IN_POOL &&
		mMowerState != LawnMowerState::MOWER_SQUISHED)
	{
		int aShadowType = 0;
		float aScaleX = 1.0f;
		float aScaleY = 1.0f;
		if (mBoard->StageIsNight())
		{
			aShadowType = 1;
		}

		float aShadowX = mPosX - 7.0f;
		float aShadowY = mPosY - mAltitude + 47.0f;
		if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
		{
			aShadowX -= 17.0f;
			aShadowY -= 8.0f;
		}
		if (mMowerType == LawnMowerType::LAWNMOWER_ROOF)
		{
			aShadowX -= 9.0f;
			aShadowY -= 36.0f;
			aScaleY = 1.2f;
			if (mMowerState == LawnMowerState::MOWER_TRIGGERED)
			{
				aShadowY += 36.0f;
			}
		}

		if (aShadowType == 0)
		{
			PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW, aShadowX, aShadowY, 0, aScaleX, aScaleY);
		}
		else
		{
			PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW2, aShadowX, aShadowY, 0, aScaleX, aScaleY);
		}
	}

	Graphics aMowerGraphics(*g);
	aMowerGraphics.mTransX += mPosX + 6.0f;
	aMowerGraphics.mTransY += mPosY - mAltitude;
	if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		if (mMowerState == LawnMowerState::MOWER_TRIGGERED)
		{
			aMowerGraphics.mTransY -= 7.0f;
			aMowerGraphics.mTransX -= 10.0f;
		}
		else
		{
			aMowerGraphics.mTransY -= 33.0f;
		}

		if (mMowerHeight == MowerHeight::MOWER_HEIGHT_UP_TO_LAND || mMowerHeight == MowerHeight::MOWER_HEIGHT_DOWN_TO_POOL)
		{
			aMowerGraphics.SetClipRect(-50, -50, 150, 132 + mAltitude);
		}
	}
	else if (mMowerType == LawnMowerType::LAWNMOWER_ROOF)
	{
		if (mMowerState == LawnMowerState::MOWER_TRIGGERED)
		{
			aMowerGraphics.mTransY -= 4.0f;
			aMowerGraphics.mTransX -= 10.0f;
		}
		else
		{
			aMowerGraphics.mTransY -= 40.0f;
		}
	}

	if (mMowerState == LawnMowerState::MOWER_TRIGGERED || mMowerState == LawnMowerState::MOWER_SQUISHED)
	{
		mApp->ReanimationGet(mReanimID)->Draw(&aMowerGraphics);
	}
	else
	{
		LawnMowerType aMowerType = mMowerType;
		if (mMowerType == LawnMowerType::LAWNMOWER_LAWN && mBoard->mSuperMowerMode)
		{
			aMowerType = LawnMowerType::LAWNMOWER_SUPER_MOWER;
		}
		mApp->mReanimatorCache->DrawCachedMower(&aMowerGraphics, 0.0f, 19.0f, aMowerType);
	}
}

void LawnMower::Die()
{
	mDead = true;
	mBoard->RecordGameplayEvent("lawn_mower_removed", MowerStateEventDetails(mBoard, this));
	mApp->RemoveReanimation(mReanimID);
	if (mBoard->mBonusLawnMowersRemaining > 0 && !mBoard->HasLevelAwardDropped())
	{
		LawnMower* aLawnMower = mBoard->mLawnMowers.DataArrayAlloc();
		aLawnMower->LawnMowerInitialize(mRow);
		aLawnMower->mMowerState = LawnMowerState::MOWER_ROLLING_IN;
		mBoard->RecordGameplayEvent("lawn_mower_rolling_in", MowerStateEventDetails(mBoard, aLawnMower));
		mBoard->mBonusLawnMowersRemaining--;
	}
}

void LawnMower::StartMower(Zombie* theTriggeringZombie)
{
	if (mMowerState == LawnMowerState::MOWER_TRIGGERED)
	{
		return;
	}
	const int aPreviousState = static_cast<int>(mMowerState);
	const Rect aMowerAttackRect = GetLawnMowerAttackRect();
	const Rect aZombieRect = theTriggeringZombie->GetZombieRect();
	const auto& aZombieDefinition = GetZombieDefinition(theTriggeringZombie->mZombieType);

	Reanimation* aMowerReanim = mApp->ReanimationGet(mReanimID);
	if (mMowerType == LawnMowerType::LAWNMOWER_POOL)
	{
		aMowerReanim->mAnimRate = 35.0f;
		mApp->PlayFoley(FoleyType::FOLEY_POOL_CLEANER);
	}
	else
	{
		aMowerReanim->mAnimRate = 70.0f;
		mApp->PlayFoley(FoleyType::FOLEY_LAWNMOWER);
	}

	mBoard->mWaveRowGotLawnMowered[mRow] = mBoard->mCurrentWave;
	mBoard->mTriggeredLawnMowers++;
	mMowerState = LawnMowerState::MOWER_TRIGGERED;
	const std::string aDetailsJson = std::format(
		R"({{"mower_id":{},"mower_type":{},"row_index":{},"lane":{},"state_before":{},"state_after":{},"visible":{},"mower_x":{:.2f},"mower_y":{:.2f},"mower_attack_rect":{{"x":{},"y":{},"width":{},"height":{}}},"zombie_id":{},"zombie_type_id":{},"zombie_type":"{}","zombie_row_index":{},"zombie_lane":{},"zombie_phase":{},"zombie_has_head":{},"zombie_sun_tier_invulnerable":{},"zombie_x":{:.2f},"zombie_y":{:.2f},"zombie_rect":{{"x":{},"y":{},"width":{},"height":{}}},"body_hp":{},"helmet_hp":{},"shield_hp":{},"overlap_pixels":{}}})",
		mBoard->mLawnMowers.DataArrayGetID(this), static_cast<int>(mMowerType), mRow, mRow + 1,
		aPreviousState, static_cast<int>(mMowerState), mVisible ? "true" : "false", mPosX, mPosY,
		aMowerAttackRect.mX, aMowerAttackRect.mY, aMowerAttackRect.mWidth, aMowerAttackRect.mHeight,
		static_cast<int>(mBoard->ZombieGetID(theTriggeringZombie)), static_cast<int>(theTriggeringZombie->mZombieType),
		aZombieDefinition.mZombieName, theTriggeringZombie->mRow, theTriggeringZombie->mRow + 1,
		static_cast<int>(theTriggeringZombie->mZombiePhase), theTriggeringZombie->mHasHead ? "true" : "false",
		theTriggeringZombie->IsSunTierInvulnerable() ? "true" : "false",
		theTriggeringZombie->mPosX, theTriggeringZombie->mPosY,
		aZombieRect.mX, aZombieRect.mY, aZombieRect.mWidth, aZombieRect.mHeight,
		theTriggeringZombie->mBodyHealth, theTriggeringZombie->mHelmHealth, theTriggeringZombie->mShieldHealth,
		GetRectOverlap(aMowerAttackRect, aZombieRect));
	mBoard->RecordGameplayEvent("lawn_mower_triggered", aDetailsJson);

	const std::string aZombieName(PvzpStringTranslate(std::format("[{}]", aZombieDefinition.mZombieName)));
	mBoard->DisplayAdvice(std::format("Mower triggered by {} in lane {}", aZombieName, mRow + 1),
		MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
}

void LawnMower::SquishMower()
{
	Reanimation* aMowerReanim = mApp->ReanimationGet(mReanimID);
	aMowerReanim->OverrideScale(0.85f, 0.22f);
	aMowerReanim->SetPosition(-11.0f, 65.0f);

	mMowerState = LawnMowerState::MOWER_SQUISHED;
	mSquishedCounter = 500;
	mBoard->RecordGameplayEvent("lawn_mower_squished", MowerStateEventDetails(mBoard, this));
	mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
}

Rect LawnMower::GetLawnMowerAttackRect()
{
	return Rect(mPosX, mPosY, 50, 80);
}

void LawnMower::EnableSuperMower([[maybe_unused]] bool theEnable) // Is theEnable being unused a bug?
{
	if (mMowerType == LawnMowerType::LAWNMOWER_LAWN)
	{
		mApp->ReanimationGet(mReanimID)->SetFramesForLayer("anim_tricked");
	}
}
