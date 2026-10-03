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

Plant::Plant()
{
	mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
	mGatlingPeaMillionSunVolley = false;
}

void Plant::PlantInitialize(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	mPlantCol = theGridX;
	mRow = theGridY;
	if (mBoard)
	{
		mX = mBoard->GridToPixelX(theGridX, theGridY);
		mY = mBoard->GridToPixelY(theGridX, theGridY);
	}
	mAnimCounter = 0;
	mAnimPing = true;
	mFrame = 0;
	mShootingCounter = 0;
	mEphraimAttackSet = 0;
	mSniperHitStopCounter = 0;
	mSniperCoffeeTicksRemaining = 0;
	mSniperMovementStacks = 0;
	mSniperAttackMovementStacks = -1;
	mSniperHomeRow = theSeedType == SeedType::SEED_SNIPER_FEMALE ? theGridY : -1;
	mSniperDestinationRow = -1;
	mSniperDodgeFromRow = -1;
	mSniperDodgeStartY = 0;
	mSniperDodgeTicksRemaining = 0;
	mSniperDodgeDurationTicks = 0;
	mEphraimHitStopCounter = 0;
	mEphraimAttackPauseFlags = 0;
	mEphraimAfterimageFrame = -1;
	mEphraimAfterimageChancePercent = EPHRAIM_AFTERIMAGE_CHANCE_PERCENT;
	mEphraimAfterimageFailureCount = 0;
	mEphraimAfterimagesRemaining = 0;
	mEphraimAfterimages.clear();
	mContinuousHealthRemainder = 0.0f;
	mSerraHealCooldown = 0;
	mSerraSunPending = false;
	mSerraBlessingTicksRemaining = 0;
	mShakeOffsetX = 0.0f;
	mShakeOffsetY = 0.0f;
	mFrameLength = RandRangeInt(12, 18);
	mTargetX = -1;
	mTargetY = -1;
	mStartRow = mRow;
	mNumFrames = 5;
	if (theSeedType == SeedType::SEED_EPHRAIM)
	{
		mNumFrames = EPHRAIM_IDLE_FRAME_COUNT;
		mFrameLength = 12;  // keep the longer lance idle cycle at the previous breathing pace
	}
	if (theSeedType == SeedType::SEED_SNIPER_FEMALE)
		mNumFrames = SNIPER_IDLE_FRAME_COUNT;
	if (theSeedType == SeedType::SEED_SERRA_BISHOP)
		mNumFrames = 1;
	mState = PlantState::STATE_NOTREADY;
	mDead = false;
	mSquished = false;
	mSeedType = theSeedType;
	mImitaterType = theImitaterType;
	mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
	mGatlingPeaMillionSunVolley = false;
	mGatlingPeaVisualBlend = 0.0f;
	mCattailTargetZombieID = ZombieID::ZOMBIEID_NULL;
	mSunMagnetCoffeeTicksRemaining = 0;
	mSunMagnetCoffeeTicksUntilDamage = 0;
	mSunMagnetHasPendingPickup = false;
	mTwinSunflowerBombCountdown = TWIN_SUNFLOWER_ASSAULT_INTERVAL;
	mPlantHealth = 500;
	mDoSpecialCountdown = 0;
	mDisappearCountdown = 200;
	mStateCountdown = 0;
	mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mBodyReanimID = ReanimationID::REANIMATIONID_NULL;
	mHeadReanimID = ReanimationID::REANIMATIONID_NULL;
	mHeadReanimID2 = ReanimationID::REANIMATIONID_NULL;
	mHeadReanimID3 = ReanimationID::REANIMATIONID_NULL;
	mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mLightReanimID = ReanimationID::REANIMATIONID_NULL;
	mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
	mBlinkCountdown = 0;
	mRecentlyEatenCountdown = 0;
	mEatenFlashCountdown = 0;
	mBeghouledFlashCountdown = 0;
	mWidth = 80;
	mHeight = 80;
	memset(mMagnetItems, 0, sizeof(mMagnetItems));
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	mIsAsleep = false;
	mWakeUpCounter = 0;
	mOnBungeeState = PlantOnBungeeState::NOT_ON_BUNGEE;
	mPottedPlantIndex = -1;
	mLaunchRate = aPlantDef.mLaunchRate;
	mSubclass = aPlantDef.mSubClass;
	mRenderOrder = CalcRenderOrder();

	Reanimation* aBodyReanim = nullptr;
	if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
	{
		float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
		aBodyReanim = mApp->AddReanimation(0.0f, aOffsetY, mRenderOrder + 1, aPlantDef.mReanimationType);
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
		aBodyReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);

		if (aBodyReanim->TrackExists("anim_idle"))
			aBodyReanim->SetFramesForLayer("anim_idle");

		if (mApp->IsWallnutBowlingLevel() && aBodyReanim->TrackExists("_ground"))
		{
			aBodyReanim->SetFramesForLayer("_ground");
			if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_EXPLODE_O_NUT)
				aBodyReanim->mAnimRate = RandRangeFloat(12.0f, 18.0f);
			else if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
				aBodyReanim->mAnimRate = RandRangeFloat(6.0f, 10.0f);
		}

		aBodyReanim->mIsAttachment = true;
		mBodyReanimID = mApp->ReanimationGetID(aBodyReanim);
		mBlinkCountdown = 400 + Sexy::Rand(400);
	}
	if (theSeedType == SeedType::SEED_CHOMPERNUT && aBodyReanim != nullptr)
	{
		Reanimation* aChomperReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, ReanimationType::REANIM_CHOMPER);
		aChomperReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
		aChomperReanim->mAnimRate = aBodyReanim->mAnimRate;
		aChomperReanim->SetFramesForLayer("anim_idle");
		AttachReanim(aBodyReanim->GetTrackInstanceByName("anim_idle")->mAttachmentID, aChomperReanim, 10.0f, -38.0f);
		mHeadReanimID = mApp->ReanimationGetID(aChomperReanim);
	}

	if (IsNocturnal(mSeedType) && mBoard && !mBoard->StageIsNight())
		SetSleeping(true);

	if (mLaunchRate > 0)
	{
		if (mSeedType == SeedType::SEED_SERRA_BISHOP)
			mLaunchCounter = RandRangeInt(mLaunchRate - SERRA_PRODUCTION_JITTER_TICKS, mLaunchRate);
		else if (MakesSun())
			mLaunchCounter = RandRangeInt(300, mLaunchRate / 2);
		else
			mLaunchCounter = RandRangeInt(0, mLaunchRate);
	}
	else
		mLaunchCounter = 0;

	switch (theSeedType)
	{
	case SeedType::SEED_BLOVER:
	{
		mDoSpecialCountdown = 50;

		if (IsInPlay())
		{
			aBodyReanim->SetFramesForLayer("anim_blow");
			aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
			aBodyReanim->mAnimRate = 20.0f;
		}
		else
		{
			aBodyReanim->SetFramesForLayer("anim_idle");
			aBodyReanim->mAnimRate = 10.0f;
		}

		break;
	}
	case SeedType::SEED_PEASHOOTER:
	case SeedType::SEED_SNOWPEA:
	case SeedType::SEED_REPEATER:
	case SeedType::SEED_LEFTPEATER:
	case SeedType::SEED_GATLINGPEA:
		if (aBodyReanim)
		{
			aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
			Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
			aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
			aHeadReanim->SetFramesForLayer("anim_head_idle");
			mHeadReanimID = mApp->ReanimationGetID(aHeadReanim);

			if (aBodyReanim->TrackExists("anim_stem"))
				aHeadReanim->AttachToAnotherReanimation(aBodyReanim, "anim_stem");
			else if (aBodyReanim->TrackExists("anim_idle"))
				aHeadReanim->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
		}
		break;
	case SeedType::SEED_SPLITPEA:
	{
		PVZP_ASSERT(aBodyReanim);

		aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		Reanimation* aHeadReanim1 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
		aHeadReanim1->mLoopType = ReanimLoopType::REANIM_LOOP;
		aHeadReanim1->mAnimRate = aBodyReanim->mAnimRate;
		aHeadReanim1->SetFramesForLayer("anim_head_idle");
		aHeadReanim1->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
		mHeadReanimID = mApp->ReanimationGetID(aHeadReanim1);

		Reanimation* aHeadReanim2 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
		aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
		aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
		aHeadReanim2->SetFramesForLayer("anim_splitpea_idle");
		aHeadReanim2->AttachToAnotherReanimation(aBodyReanim, "anim_idle");
		mHeadReanimID2 = mApp->ReanimationGetID(aHeadReanim2);

		break;
	}
	case SeedType::SEED_THREEPEATER:
	{
		PVZP_ASSERT(aBodyReanim);

		aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		Reanimation* aHeadReanim1 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
		aHeadReanim1->mLoopType = ReanimLoopType::REANIM_LOOP;
		aHeadReanim1->mAnimRate = aBodyReanim->mAnimRate;
		aHeadReanim1->SetFramesForLayer("anim_head_idle1");
		aHeadReanim1->AttachToAnotherReanimation(aBodyReanim, "anim_head1");
		mHeadReanimID = mApp->ReanimationGetID(aHeadReanim1);

		Reanimation* aHeadReanim2 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
		aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
		aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
		aHeadReanim2->SetFramesForLayer("anim_head_idle2");
		aHeadReanim2->AttachToAnotherReanimation(aBodyReanim, "anim_head2");
		mHeadReanimID2 = mApp->ReanimationGetID(aHeadReanim2);

		Reanimation* aHeadReanim3 = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, aPlantDef.mReanimationType);
		aHeadReanim3->mLoopType = ReanimLoopType::REANIM_LOOP;
		aHeadReanim3->mAnimRate = aBodyReanim->mAnimRate;
		aHeadReanim3->SetFramesForLayer("anim_head_idle3");
		aHeadReanim3->AttachToAnotherReanimation(aBodyReanim, "anim_head3");
		mHeadReanimID3 = mApp->ReanimationGetID(aHeadReanim3);

		break;
	}
	case SeedType::SEED_EPHRAIM:
		mPlantHealth = EPHRAIM_MAX_HEALTH;
		break;
	case SeedType::SEED_WALLNUT:
		mPlantHealth = 4000;
		mBlinkCountdown = 1000 + Sexy::Rand(1000);
		break;
	case SeedType::SEED_EXPLODE_O_NUT:
		mPlantHealth = 4000;
		mBlinkCountdown = 1000 + Sexy::Rand(1000);
		aBodyReanim->mColorOverride = Color(255, 64, 64);
		break;
	case SeedType::SEED_GIANT_WALLNUT:
		mPlantHealth = 4000;
		mBlinkCountdown = 1000 + Sexy::Rand(1000);
		break;
	case SeedType::SEED_TALLNUT:
		mPlantHealth = 8000 + (mBoard != nullptr && mBoard->mTallNutOverdriveActive ? 2000 : 0);
		mHeight = 80;
		mBlinkCountdown = 1000 + Sexy::Rand(1000);
		break;
	case SeedType::SEED_CHOMPERNUT:
		mPlantHealth = 8500 + (mBoard != nullptr && mBoard->mTallNutOverdriveActive ? 2000 : 0);
		mHeight = 80;
		mState = PlantState::STATE_READY;
		mBlinkCountdown = 1000 + Sexy::Rand(1000);
		break;
	case SeedType::SEED_GARLIC:
		PVZP_ASSERT(aBodyReanim);
		mPlantHealth = 400;
		aBodyReanim->SetTruncateDisappearingFrames();
		break;
	case SeedType::SEED_GOLD_MAGNET:
	case SeedType::SEED_SUN_MAGNET:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->SetTruncateDisappearingFrames();
		break;
	case SeedType::SEED_IMITATER:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->mAnimRate = RandRangeFloat(25.0f, 30.0f);
		mStateCountdown = 200;
		break;
	case SeedType::SEED_CHERRYBOMB:
	case SeedType::SEED_JALAPENO:
	{
		PVZP_ASSERT(aBodyReanim);

		if (IsInPlay())
		{
			mDoSpecialCountdown = 100;

			aBodyReanim->SetFramesForLayer("anim_explode");
			aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;

			mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
		}

		break;
	}
	case SeedType::SEED_POTATOMINE:
	{
		PVZP_ASSERT(aBodyReanim);

		aBodyReanim->mAnimRate = 12.0f;

		if (IsInPlay())
		{
			aBodyReanim->AssignRenderGroupToTrack("anim_glow", RENDER_GROUP_HIDDEN);
			mStateCountdown = 1500;
		}
		else
		{
			aBodyReanim->SetFramesForLayer("anim_armed");
			mState = PlantState::STATE_POTATO_ARMED;
		}

		break;
	}
	case SeedType::SEED_GRAVEBUSTER:
	{
		PVZP_ASSERT(aBodyReanim);

		if (IsInPlay())
		{
			aBodyReanim->SetFramesForLayer("anim_land");
			aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;

			mState = PlantState::STATE_GRAVEBUSTER_LANDING;
			mApp->PlayFoley(FoleyType::FOLEY_GRAVEBUSTERCHOMP);
		}

		break;
	}
	case SeedType::SEED_SUNSHROOM:
	{
		PVZP_ASSERT(aBodyReanim);

		aBodyReanim->mFrameBasePose = 6;

		if (IsInPlay())
		{
			mX += Sexy::Rand(10) - 5;
			mY += Sexy::Rand(10) - 5;
		}
		else if (mIsAsleep)
			aBodyReanim->SetFramesForLayer("anim_bigsleep");
		else
			aBodyReanim->SetFramesForLayer("anim_bigidle");

		mState = PlantState::STATE_SUNSHROOM_SMALL;
		mStateCountdown = 12000;

		break;
	}
	case SeedType::SEED_PUFFSHROOM:
	case SeedType::SEED_SEASHROOM:
		if (IsInPlay())
		{
			mX += Sexy::Rand(10) - 5;
			mY += Sexy::Rand(6) - 3;
		}
		break;
	case SeedType::SEED_PUMPKINSHELL:
	{
		mPlantHealth = 4000 + (mBoard != nullptr && mBoard->mPumpkinOverdriveActive ? 1000 : 0);
		mWidth = 120;

		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->AssignRenderGroupToTrack("Pumpkin_back", 1);
		break;
	}
	case SeedType::SEED_CHOMPER:
		mState = PlantState::STATE_READY;
		break;
	case SeedType::SEED_PLANTERN:
	{
		mStateCountdown = 2500;

		if (!IsOnBoard() || mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		{
			AddAttachedParticle(mX + 40, mY + 40, static_cast<int>(RenderLayer::RENDER_LAYER_FOG) + 1, ParticleEffect::PARTICLE_LANTERN_SHINE);
		}
		if (IsInPlay())
		{
			mApp->PlaySample(Sexy::SOUND_PLANTERN);
		}

		break;
	}
	case SeedType::SEED_TORCHWOOD:
		break;
	case SeedType::SEED_MARIGOLD:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->mAnimRate = RandRangeFloat(15.0f, 20.0f);
		break;
	case SeedType::SEED_CACTUS:
		mState = PlantState::STATE_CACTUS_LOW;
		break;
	case SeedType::SEED_INSTANT_COFFEE:
		mDoSpecialCountdown = 100;
		break;
	case SeedType::SEED_SCAREDYSHROOM:
		mState = PlantState::STATE_READY;
		break;
	case SeedType::SEED_COBCANNON:
		if (IsInPlay())
		{
			mState = PlantState::STATE_COBCANNON_ARMING;
			mStateCountdown = 500;

			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetFramesForLayer("anim_unarmed_idle");
		}
		break;
	case SeedType::SEED_KERNELPULT:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_HIDDEN);
		break;
	case SeedType::SEED_MAGNETSHROOM:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->SetTruncateDisappearingFrames();
		break;
	case SeedType::SEED_SPIKEROCK:
		mPlantHealth = 1350;
		PVZP_ASSERT(aBodyReanim);
		break;
	case SeedType::SEED_SPIKEWEED:
		mPlantHealth = 900;
		break;
	case SeedType::SEED_SPROUT:
		break;
	case SeedType::SEED_FLOWERPOT:
		if (IsInPlay())
		{
			mState = PlantState::STATE_FLOWERPOT_INVULNERABLE;
			mStateCountdown = 100;
		}
		break;
	case SeedType::SEED_LILYPAD:
		if (IsInPlay())
		{
			mState = PlantState::STATE_LILYPAD_INVULNERABLE;
			mStateCountdown = 100;
		}
		break;
	case SeedType::SEED_TANGLEKELP:
		PVZP_ASSERT(aBodyReanim);
		aBodyReanim->SetTruncateDisappearingFrames();
	default:
		break;
	}

	if ((mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME) &&
		(theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_SUNFLOWER || theSeedType == SeedType::SEED_MARIGOLD))
	{
		mPlantHealth *= 2;
	}
	mPlantMaxHealth = mPlantHealth;

	if (mSeedType != SeedType::SEED_FLOWERPOT && IsOnBoard())
	{
		PVZP_ASSERT(mBoard);
		Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
		if (aFlowerPot)
			mApp->ReanimationGet(aFlowerPot->mBodyReanimID)->mAnimRate = 0.0f;
	}
}

void Plant::UpdateSniperLaneMovement()
{
	if (mSeedType != SeedType::SEED_SNIPER_FEMALE)
		return;
	if (mSniperHomeRow < 0 || mSniperHomeRow >= MAX_GRID_SIZE_Y)
		mSniperHomeRow = mRow;
	// Older lane-movement saves have no adjacent-step route: resume at the
	// saved lane instead of completing a potentially obstructed long jump.
	if (mSniperDestinationRow < 0 || mSniperDodgeFromRow < 0)
	{
		mSniperDestinationRow = mRow;
		mSniperDodgeFromRow = mRow;
		mSniperDodgeTicksRemaining = 0;
		mY = mBoard->GridToPixelY(mPlantCol, mRow);
	}

	auto CanEnterRow = [&](int theRow)
	{
		return mBoard->CanPlantAt(mPlantCol, theRow, SeedType::SEED_SNIPER_FEMALE, true) == PlantingReason::PLANTING_OK;
	};
	auto CanReachRow = [&](int theRow)
	{
		const int aStep = theRow > mRow ? 1 : -1;
		for (int aRow = mRow; aRow != theRow; )
		{
			aRow += aStep;
			if (!CanEnterRow(aRow))
				return false;
		}
		return true;
	};
	auto StartStep = [&](int theRow)
	{
		mSniperDodgeStartY = mY;
		mSniperDodgeFromRow = mRow;
		mRow = theRow;
		const int aDistance = std::abs(mBoard->GridToPixelY(mPlantCol, mRow) - mY);
		mSniperDodgeDurationTicks = std::clamp((aDistance * SNIPER_DODGE_TICKS_PER_100_PIXELS + 99) / 100, 1, SNIPER_MAX_DODGE_TICKS);
		mSniperDodgeTicksRemaining = mSniperDodgeDurationTicks;
		mRenderOrder = CalcRenderOrder();
	};

	if (FindTargetZombie(mSniperHomeRow, PlantWeapon::WEAPON_PRIMARY) != nullptr)
	{
		mSniperDestinationRow = mSniperHomeRow;
		// Reverse an outbound adjacent step immediately when its origin is
		// closer to home, provided that tile is still traversable.
		if (mSniperDodgeTicksRemaining > 0 &&
			std::abs(mSniperDodgeFromRow - mSniperHomeRow) < std::abs(mRow - mSniperHomeRow) &&
			CanEnterRow(mSniperDodgeFromRow))
			StartStep(mSniperDodgeFromRow);
	}
	else if (!IsSniperMoving() && mShootingCounter == 0 &&
		FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY) == nullptr)
	{
		mSniperDestinationRow = mSniperHomeRow;
		int aClosestDistance = MAX_GRID_SIZE_Y;
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			if (aRow == mRow || aRow == mSniperHomeRow || !CanReachRow(aRow) ||
				FindTargetZombie(aRow, PlantWeapon::WEAPON_PRIMARY) == nullptr)
				continue;
			const int aDistance = std::abs(aRow - mRow);
			if (aDistance < aClosestDistance)
			{
				aClosestDistance = aDistance;
				mSniperDestinationRow = aRow;
			}
		}
	}

	if (mSniperDodgeTicksRemaining == 0 && mSniperDestinationRow != mRow)
	{
		const int aNextRow = mRow + (mSniperDestinationRow > mRow ? 1 : -1);
		if (CanEnterRow(aNextRow))
			StartStep(aNextRow);
		else
			mSniperDestinationRow = mRow; // Stop before an obstruction; retry on subsequent updates.
	}
	if (mSniperDodgeTicksRemaining > 0)
	{
		--mSniperDodgeTicksRemaining;
		const float aProgress = 1.0f - static_cast<float>(mSniperDodgeTicksRemaining) / mSniperDodgeDurationTicks;
		const float aBlend = aProgress * aProgress * (3.0f - 2.0f * aProgress);
		const int aDestinationY = mBoard->GridToPixelY(mPlantCol, mRow);
		mY = mSniperDodgeStartY + static_cast<int>(std::lround((aDestinationY - mSniperDodgeStartY) * aBlend));
		if (mSniperDodgeTicksRemaining == 0 && mRow != mSniperDodgeFromRow &&
			mSniperDodgeStartY == mBoard->GridToPixelY(mPlantCol, mSniperDodgeFromRow) &&
			mSniperMovementStacks < std::numeric_limits<int32_t>::max())
			++mSniperMovementStacks;
	}
}

void Plant::UpdateAbilities()
{
	if (!IsInPlay())
		return;

	if (mState == PlantState::STATE_DOINGSPECIAL || mSquished)
	{
		mDisappearCountdown--;
		if (mDisappearCountdown < 0)
		{
			Die();
			return;
		}
	}

	if (mWakeUpCounter > 0)
	{
		mWakeUpCounter--;
		if (mWakeUpCounter == 60)
		{
			mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
		}
		if (mWakeUpCounter == 0)
		{
			SetSleeping(false);
		}
	}
	if (mSeedType == SeedType::SEED_PLANTERN && mIsAsleep)
	{
		mWakeUpCounter = 0;
		SetSleeping(false);
	}

	if (mIsAsleep || mSquished || mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE)
		return;

	UpdateSniperLaneMovement();

	if (mSeedType == SeedType::SEED_GLOOMSHROOM && mLaunchRate == 200)
	{
		mLaunchCounter = std::clamp((mLaunchCounter * 67 + 100) / 200, 0, 67);
		mShootingCounter = std::clamp((mShootingCounter * 67 + 100) / 200, 0, 67);
		mLaunchRate = 67;
	}
	else if (mSeedType == SeedType::SEED_GATLINGPEA && (mLaunchRate == 150 || mLaunchRate == 75))
	{
		int aOldLaunchRate = mLaunchRate;
		int aOldShootingDuration = aOldLaunchRate == 150 ? 100 : 50;
		mLaunchCounter = std::clamp((mLaunchCounter * 40 + aOldLaunchRate / 2) / aOldLaunchRate, 0, 40);
		mShootingCounter = std::clamp((mShootingCounter * 25 + aOldShootingDuration / 2) / aOldShootingDuration, 0, 25);
		mLaunchRate = 40;
	}
	else if (mSeedType == SeedType::SEED_CATTAIL)
	{
		int aDesiredLaunchRate = mBoard->mCatTailOverdriveActive ? 18 : 75;
		if ((mLaunchRate == 150 || mLaunchRate == 75 || mLaunchRate == 37 || mLaunchRate == 18) && mLaunchRate != aDesiredLaunchRate)
		{
			mLaunchCounter = std::clamp((mLaunchCounter * aDesiredLaunchRate + mLaunchRate / 2) / mLaunchRate,
				0, aDesiredLaunchRate);
			mLaunchRate = aDesiredLaunchRate;
		}
	}
	else if (mSeedType == SeedType::SEED_KERNELPULT)
	{
		int aDesiredLaunchRate = mBoard->mKernelPultOverdriveActive ? 75 : 300;
		if ((mLaunchRate == 300 || mLaunchRate == 75) && mLaunchRate != aDesiredLaunchRate)
		{
			mLaunchCounter = std::clamp((mLaunchCounter * aDesiredLaunchRate + mLaunchRate / 2) / mLaunchRate,
				0, aDesiredLaunchRate);
			mLaunchRate = aDesiredLaunchRate;
		}
	}
	else if (mSeedType == SeedType::SEED_WINTERMELON)
	{
		int aDesiredLaunchRate = mBoard->mSunMoney >= WINTER_MELON_QUADRATIC_DAMAGE_SUN_THRESHOLD ? 150 : 300;
		if ((mLaunchRate == 300 || mLaunchRate == 150) && mLaunchRate != aDesiredLaunchRate)
		{
			mLaunchCounter = std::clamp((mLaunchCounter * aDesiredLaunchRate + mLaunchRate / 2) / mLaunchRate,
				0, aDesiredLaunchRate);
			mLaunchRate = aDesiredLaunchRate;
		}
	}
	else if (mSeedType == SeedType::SEED_TWINSUNFLOWER || mSeedType == SeedType::SEED_PLANTERN)
	{
		int aDesiredLaunchRate = mBoard->mTwinSunflowerProductionOverdriveActive ? 312 : 625;
		if (mSeedType == SeedType::SEED_TWINSUNFLOWER &&
			mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			aDesiredLaunchRate *= 7;
		if ((mLaunchRate == 625 || mLaunchRate == 312 || mLaunchRate == TWIN_SUNFLOWER_ASSAULT_INTERVAL ||
			mLaunchRate == 2184 || mLaunchRate == 4375) && mLaunchRate != aDesiredLaunchRate)
		{
			mLaunchCounter = std::clamp((mLaunchCounter * aDesiredLaunchRate + mLaunchRate / 2) / mLaunchRate,
				0, aDesiredLaunchRate);
			mLaunchRate = aDesiredLaunchRate;
		}
	}
	else if (mSeedType == SeedType::SEED_GOLD_MAGNET && mState == PlantState::STATE_MAGNETSHROOM_CHARGING && mStateCountdown > 150)
	{
		mStateCountdown = (mStateCountdown + 1) / 2;
	}

	if (mSeedType == SeedType::SEED_SNIPER_FEMALE)
	{
		const int aSteps = mSniperCoffeeTicksRemaining > 0 ? SNIPER_COFFEE_ATTACK_SPEED_MULTIPLIER : 1;
		for (int aStep = 0; aStep < aSteps; ++aStep)
		{
			// Advance every release tick individually, including critical burst arrows.
			UpdateShooting();
			UpdateShooter();
		}
	}
	else
		UpdateShooting();

	if (mStateCountdown > 0)
	{
		int aCountdownStep = (IsChomper() && mState == PlantState::STATE_CHOMPER_DIGESTING)
			? (mBoard->mChomperOverdriveActive ? 60 : 30) : 1;
		mStateCountdown = std::max(0, mStateCountdown - aCountdownStep);
	}

	if (mApp->IsWallnutBowlingLevel())
	{
		UpdateBowling();
		return;
	}

	if (mSeedType == SeedType::SEED_SQUASH)                                                     UpdateSquash();
	else if (mSeedType == SeedType::SEED_DOOMSHROOM)                                            UpdateDoomShroom();
	else if (mSeedType == SeedType::SEED_ICESHROOM)                                             UpdateIceShroom();
	else if (IsChomper())                                                                       UpdateChomper();
	else if (mSeedType == SeedType::SEED_BLOVER)                                                UpdateBlover();
	else if (mSeedType == SeedType::SEED_FLOWERPOT)                                             UpdateFlowerPot();
	else if (mSeedType == SeedType::SEED_LILYPAD)                                               UpdateLilypad();
	else if (mSeedType == SeedType::SEED_IMITATER)                                              UpdateImitater();
	else if (mSeedType == SeedType::SEED_INSTANT_COFFEE)                                        UpdateCoffeeBean();
	else if (mSeedType == SeedType::SEED_UMBRELLA)                                              UpdateUmbrella();
	else if (mSeedType == SeedType::SEED_COBCANNON)                                             UpdateCobCannon();
	else if (mSeedType == SeedType::SEED_CACTUS)                                                UpdateCactus();
	else if (mSeedType == SeedType::SEED_MAGNETSHROOM)                                          UpdateMagnetShroom();
	else if (mSeedType == SeedType::SEED_GOLD_MAGNET || mSeedType == SeedType::SEED_SUN_MAGNET) UpdateGoldMagnetShroom();
	else if (mSeedType == SeedType::SEED_SUNSHROOM)                                             UpdateSunShroom();
	else if (MakesSun() || mSeedType == SeedType::SEED_MARIGOLD)
	{
		UpdateProductionPlant();
		if (mSeedType == SeedType::SEED_PLANTERN)
			UpdatePlanternAttack();
	}
	else if (mSeedType == SeedType::SEED_GRAVEBUSTER)                                           UpdateGraveBuster();
	else if (mSeedType == SeedType::SEED_TORCHWOOD)                                             UpdateTorchwood();
	else if (mSeedType == SeedType::SEED_POTATOMINE)                                            UpdatePotato();
	else if (mSeedType == SeedType::SEED_SPIKEWEED || mSeedType == SeedType::SEED_SPIKEROCK)    UpdateSpikeweed();
	else if (mSeedType == SeedType::SEED_TANGLEKELP)                                            UpdateTanglekelp();
	else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)                                         UpdateScaredyShroom();

	if (mSubclass == PlantSubClass::SUBCLASS_SHOOTER && mSeedType != SeedType::SEED_SNIPER_FEMALE)
	{
		UpdateShooter();
	}
	if (mDoSpecialCountdown > 0)
	{
		mDoSpecialCountdown--;
		if (mDoSpecialCountdown == 0)
		{
			DoSpecial();
		}
	}
}

void Plant::Update()
{
	if (mSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (mDead)
		{
			mGatlingPeaVisualBlend = 0.0f;
		}
		else
		{
			float aTargetBlend = IsOnBoard() && mBoard->mGatlingPeaOverdriveActive ? 1.0f : 0.0f;
			constexpr float aBlendStep = 1.0f / 60.0f;
			if (mGatlingPeaVisualBlend < aTargetBlend)
				mGatlingPeaVisualBlend = std::min(aTargetBlend, mGatlingPeaVisualBlend + aBlendStep);
			else if (mGatlingPeaVisualBlend > aTargetBlend)
				mGatlingPeaVisualBlend = std::max(aTargetBlend, mGatlingPeaVisualBlend - aBlendStep);

			PvzpParticleHolder* aParticleHolder = mApp->mEffectSystem->mParticleHolder.get();
			int aSmokePhase = (mPlantCol * MAX_GRID_SIZE_Y + mRow) % 48;
			if (mGatlingPeaVisualBlend > 0.0f && !aParticleHolder->IsAtUsageThreshold(75U) &&
				mBoard->mMainCounter % 48 == aSmokePhase)
			{
				PvzpParticleSystem* aSmoke = AddAttachedParticle(mX + 34, mY + 34,
					mRenderOrder - 1, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
				if (aSmoke)
				{
					aSmoke->OverrideScale(nullptr, 0.48f * mGatlingPeaVisualBlend);
					aSmoke->OverrideColor(nullptr, Color(200, 200, 200,
						static_cast<int>(155.0f * mGatlingPeaVisualBlend)));
				}
			}
		}
	}
	bool doUpdate = false;
	if (IsOnBoard() && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->IsWallnutBowlingLevel())
		doUpdate = true;
	else if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		doUpdate = true;
	else if (IsOnBoard() && mBoard->mCutScene->ShouldRunUpsellBoard())
		doUpdate = true;
	else if (!IsOnBoard() || mApp->mGameScene == GameScenes::SCENE_PLAYING)
		doUpdate = true;

	if (doUpdate)
	{
		if (mSerraBlessingTicksRemaining > 0)
			--mSerraBlessingTicksRemaining;
		UpdateAbilities();
		Animate();
		// Duration follows active game time, including updates where abilities return early.
		if (mSeedType == SeedType::SEED_SNIPER_FEMALE && mSniperCoffeeTicksRemaining > 0 &&
			IsOnBoard() && mApp->mGameScene == GameScenes::SCENE_PLAYING && mApp->mSeedChooserScreen == nullptr)
		{
			if (--mSniperCoffeeTicksRemaining == 0)
				PvzpLogLn("[sniper_coffee] tick={} event=expired plant_id={} row={} col={} attack_speed_multiplier=1 projectile_sun_cost=0",
					mBoard->mMainCounter, mBoard->mPlants.DataArrayGetID(this), mRow, mPlantCol);
		}

		if (mPlantHealth < 0)
			Die();

		UpdateReanim();
	}
}

void Plant::Die()
{
	if (IsOnBoard() && mSeedType == SeedType::SEED_TANGLEKELP)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
		if (aZombie)
		{
			aZombie->DieWithLoot();
		}
	}

	mDead = true;
	RemoveEffects();

	if (!Plant::IsFlying(mSeedType) && IsOnBoard())
	{
		GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
		if (aLadder)
		{
			aLadder->GridItemDie();
		}
	}

	if (IsOnBoard())
	{
		Plant* aTopPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
		Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
		if (aFlowerPot && aTopPlant == aFlowerPot)
		{
			Reanimation* aPotReanim = mApp->ReanimationGet(aFlowerPot->mBodyReanimID);
			aPotReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
		}
	}
}
