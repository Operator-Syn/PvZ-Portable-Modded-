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

void Zombie::ConvertToNormalZombie()
{
	StopZombieSound();
	mPosY = GetPosYBasedOnRow(mRow);
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY);

	mZombieType = ZombieType::ZOMBIE_NORMAL;
	mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
	mZombieAttackRect = Rect(50, 0, 20, 115);

	mAnimFrames = 12;
	mAnimTicksPerFrame = 12;
	mPhaseCounter = 0;

	PickRandomSpeed();
}

void Zombie::StartEating()
{
	if (mIsEating)
		return;

	mIsEating = true;

	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
		return;

	if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING)
	{
		PlayZombieReanim("anim_laddereat", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD)
	{
		PlayZombieReanim("anim_eat_nopaper", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
	}
	else
	{
		if (mZombieType != ZombieType::ZOMBIE_SNORKEL)
		{
			PlayZombieReanim("anim_eat", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
		}

		if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
		{
			ShowDoorArms(false);
		}
	}
}

void Zombie::StartWalkAnim(int theBlendTime)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
		return;

	PickRandomSpeed();
	if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING)
	{
		PlayZombieReanim("anim_ladderwalk", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
	}
	else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD)
	{
		PlayZombieReanim("anim_walk_nopaper", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
	}
	else if (mInPool && mZombieHeight != ZombieHeight::HEIGHT_IN_TO_POOL && mZombieHeight != ZombieHeight::HEIGHT_OUT_OF_POOL && aBodyReanim->TrackExists("anim_swim"))
	{
		PlayZombieReanim("anim_swim", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
	}
	else if ((mZombieType == ZombieType::ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE ||
		mZombieType == ZombieType::ZOMBIE_PAIL || mZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET) && mBoard->mDanceMode)
	{
		PlayZombieReanim("anim_dance", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
	}
	else
	{
		int aWalkAnimVariant = Rand(2);
		if (mZombieType == ZombieType::ZOMBIE_PEA_HEAD)
		{
			aWalkAnimVariant = 0;
		}
		if (mZombieType == ZombieType::ZOMBIE_FLAG)
		{
			aWalkAnimVariant = 0;
		}

		if (aWalkAnimVariant == 0 && aBodyReanim->TrackExists("anim_walk2"))
		{
			PlayZombieReanim("anim_walk2", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
		}
		else if (aBodyReanim->TrackExists("anim_walk"))
		{
			PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
		}
	}
}

void Zombie::StopEating()
{
	if (!mIsEating)
		return;

	mIsEating = false;
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
		return;

	if (aBodyReanim && mZombieType != ZombieType::ZOMBIE_SNORKEL)
	{
		StartWalkAnim(20);
	}

	if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
	{
		ShowDoorArms(true);
	}

	UpdateAnimSpeed();
}

void Zombie::CheckIfPreyCaught()
{
	if (mZombieType == ZombieType::ZOMBIE_BUNGEE ||
		mZombieType == ZombieType::ZOMBIE_GARGANTUAR ||
		mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
		ZombieRules::IsBulwarkType(mZombieType) ||
		mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
		mZombieType == ZombieType::ZOMBIE_CATAPULT ||
		mZombieType == ZombieType::ZOMBIE_BOSS ||
		IsBouncingPogo() ||
		IsBobsledTeamWithSled() ||
		mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
		mZombiePhase == ZombiePhase::PHASE_IMP_LANDING ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING ||
		mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING ||
		mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED ||
		mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER ||
		mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL ||
		mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL ||
		IsTangleKelpTarget() ||
		mZombieHeight == ZombieHeight::HEIGHT_FALLING ||
		!mHasHead ||
		IsFlying())
		return;

	int aBiteSpeedNumerator = mBoard->mZombieStrengthTier >= 4 ? 33 :
		mBoard->mZombieStrengthTier > 0 ? 30 : 20;
	if (mBoard->mZombieStrengthTier >= 4 && mZombieType == ZombieType::ZOMBIE_IMP)
		aBiteSpeedNumerator = 40;
	aBiteSpeedNumerator *= mBoard->mZombieTierSunMoney >= HIGH_SUN_EATING_THRESHOLD ? HIGH_SUN_EATING_SPEED_PERCENT : 100;
	constexpr int aBiteSpeedDenominator = 20 * 100 * TICKS_BETWEEN_EATS;
	int aBiteClock = mChilledCounter > 0 ? mZombieAge / 2 : mZombieAge;
	if (aBiteClock <= 0 ||
		(static_cast<int64_t>(aBiteClock) * aBiteSpeedNumerator) / aBiteSpeedDenominator ==
		(static_cast<int64_t>(aBiteClock - 1) * aBiteSpeedNumerator) / aBiteSpeedDenominator)
	{
		return;
	}

	Zombie* aZombie = FindZombieTarget();
	if (aZombie)
	{
		EatZombie(aZombie);
		return;
	}

	if (!mMindControlled)
	{
		Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
		if (aPlant)
		{
			EatPlant(aPlant);
			return;
		}
	}

	if (mApp->IsIZombieLevel() && mBoard->mChallenge->IZombieEatBrain(this))
	{
		return;
	}

	if (mIsEating)
	{
		StopEating();
	}
}

void Zombie::PoolSplash(bool theInToPoolSound)
{
	float aOffsetX = 23.0f;
	float aOffsetY = 78.0f;
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
	{
		aOffsetX -= 37.0f;
		aOffsetY -= -8.0f;
	}

	mApp->AddReanimation(mX + aOffsetX, mY + aOffsetY, mRenderOrder + 1, ReanimationType::REANIM_SPLASH)->OverrideScale(0.8f, 0.8f);
	mApp->AddPvzpParticle(mX + aOffsetX + 37.0f, mY + aOffsetY + 42.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL);

	if (theInToPoolSound)
	{
		mApp->PlayFoley(FoleyType::FOLEY_ZOMBIESPLASH);
	}
	else
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
	}
}

void Zombie::CheckForPool()
{
	if (!Zombie::ZombieTypeCanGoInPool(mZombieType) || IsFlying())
	{
		return;
	}
	// Wait until falling zombies land before starting their pool transition.
	if (mZombieHeight == ZombieHeight::HEIGHT_FALLING)
		return;
	if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER || mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		return;
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL || mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
	{
		return;
	}

	const int aPoolEntryRightEdge = LAWN_XMIN + mBoard->GetNumPlayableColumns() * 80 - 80;
	bool aIsPoolSquare =
		mBoard->IsPoolSquare(mBoard->PixelToGridX(mX + 75, mY), mRow) &&
		mBoard->IsPoolSquare(mBoard->PixelToGridX(mX + 45, mY), mRow) &&
		mX < aPoolEntryRightEdge;

	if (!mInPool && aIsPoolSquare)
	{
		if (mBoard->mIceTrapCounter > 0 && CanBeFrozen())
		{
			mIceTrapCounter = mBoard->mIceTrapCounter;
			ApplyChill(true);
		}
		else
		{
			mZombieHeight = ZombieHeight::HEIGHT_IN_TO_POOL;
			mInPool = true;
			PoolSplash(true);
		}
	}
	else if (mInPool && !aIsPoolSquare)
	{
		mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
		StartWalkAnim(0);
		PoolSplash(false);
	}
}

bool Zombie::IsOnHighGround()
{
	return IsOnBoard() && mBoard->mGridSquareType[mBoard->PixelToGridXKeepOnBoard(mX + 75, mY)][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
}

void Zombie::CheckForHighGround()
{
	if (mZombieHeight != ZombieHeight::HEIGHT_ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_BUNGEE)
		return;

	bool aIsHighGround = IsOnHighGround();
	if (!mOnHighGround && aIsHighGround)
	{
		mZombieHeight = ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND;
		mOnHighGround = true;
	}
	else if (mOnHighGround && !aIsHighGround)
	{
		mZombieHeight = ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND;
	}
}

void Zombie::StartMindControlled()
{
	mApp->PlaySample(SOUND_MINDCONTROLLED);
	mMindControlled = true;
	mLastPortalX = -1;

	if (mZombieType == ZombieType::ZOMBIE_DANCER)
	{
		for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
		{
			mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		Zombie* aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
		if (aLeader)
		{
			ZombieID aId = mBoard->ZombieGetID(this);
			for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
			{
				if (aLeader->mFollowerZombieID[i] == aId)
				{
					aLeader->mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
					break;
				}
			}
		}

		mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
	}
	else
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
		if (aZombie)
		{
			aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
			mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
		}
	}
}

void Zombie::EatPlant(Plant* thePlant)
{
	if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
	{
		mPhaseCounter = 1;
		return;
	}

	if (mYuckyFace)
		return;

	if (mBoard->GetLadderAt(thePlant->mPlantCol, thePlant->mRow) && mZombieType != ZombieType::ZOMBIE_DIGGER)  // digger zombies ignore ladders
	{
		StopEating();

		if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL && mUseLadderCol != thePlant->mPlantCol)
		{
			mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
			mUseLadderCol = thePlant->mPlantCol;
		}

		return;
	}

	StartEating();
	if (thePlant->mSeedType == SeedType::SEED_JALAPENO ||
		thePlant->mSeedType == SeedType::SEED_CHERRYBOMB ||
		thePlant->mSeedType == SeedType::SEED_DOOMSHROOM ||
		thePlant->mSeedType == SeedType::SEED_ICESHROOM ||
		thePlant->mSeedType == SeedType::SEED_HYPNOSHROOM ||
		thePlant->mState == PlantState::STATE_FLOWERPOT_INVULNERABLE ||
		thePlant->mState == PlantState::STATE_LILYPAD_INVULNERABLE ||
		thePlant->mState == PlantState::STATE_SQUASH_LOOK ||
		thePlant->mState == PlantState::STATE_SQUASH_PRE_LAUNCH)
	{
		if (!thePlant->mIsAsleep)
		{
			return;
		}
	}
	if (thePlant->mSeedType == SeedType::SEED_POTATOMINE && thePlant->mState != PlantState::STATE_NOTREADY)
	{
		return;
	}

	bool triggered = false;
	if (thePlant->mSeedType == SeedType::SEED_BLOVER)
	{
		triggered = true;
	}
	if (thePlant->mSeedType == SeedType::SEED_ICESHROOM  && !thePlant->mIsAsleep)
	{
		triggered = true;
	}
	if (triggered)
	{
		thePlant->DoSpecial();
		return;
	}

	if (mChilledCounter > 0 && mZombieAge % 2 == 1)
		return;

	if (mApp->IsIZombieLevel() && thePlant->mSeedType == SeedType::SEED_SUNFLOWER)
	{
		int aStageBeforeChew = thePlant->mPlantHealth / 40;
		int aStageAfterChew = (thePlant->mPlantHealth - DAMAGE_PER_EAT) / 40;
		if (aStageAfterChew < aStageBeforeChew || thePlant->mPlantHealth - DAMAGE_PER_EAT <= 0)  // if this chew lowers the plant's health by at least one stage
		{
			mBoard->AddCoin(thePlant->mX, thePlant->mY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
		}
	}

	const int aHealthBefore = thePlant->mPlantHealth;
	thePlant->mPlantHealth -= DAMAGE_PER_EAT;
	thePlant->mRecentlyEatenCountdown = 50;
	if (mApp->IsIZombieLevel() && mJustGotShotCounter < -500)
	{
		if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->IsTallNut() || thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			thePlant->mPlantHealth -= DAMAGE_PER_EAT;
		}
	}

	thePlant->LogDamage(aHealthBefore, "zombie_bite", this);
	if (thePlant->mPlantHealth <= 0)
	{
		mApp->PlaySample(SOUND_GULP);

		mBoard->mPlantsEaten++;
		thePlant->Die();
		mBoard->mChallenge->ZombieAtePlant(thePlant);

		if (mBoard->mLevel >= 2 && mBoard->mLevel <= 4 && mApp->IsFirstTimeAdventureMode())
		{
			if (thePlant->mPlantCol > 4 && mBoard->mPlants.mSize < 15 && thePlant->mSeedType == SeedType::SEED_PEASHOOTER)
			{
				mBoard->DisplayAdvice("[ADVICE_PEASHOOTER_DIED]", MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_PEASHOOTER_DIED);
			}
		}
	}
}

void Zombie::EatZombie(Zombie* theZombie)
{
	theZombie->TakeDamage(DAMAGE_PER_EAT, 9U);
	StartEating();
	if (theZombie->mBodyHealth <= 0)
	{
		mApp->PlaySample(SOUND_GULP);
	}
}
