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

namespace
{
	struct EphraimAttackTiming
	{
		int mAttackSet;
		int mTotalTicks;
		int mImpactFrame;
	};

	EphraimAttackTiming GetEphraimAttackTiming(int theAttackSet)
	{
		const int anIndex = std::clamp(theAttackSet, 0, Plant::EPHRAIM_ATTACK_VARIANT_COUNT - 1);
		return { anIndex, Plant::EPHRAIM_ATTACK_DURATION_TICKS[anIndex],
			Plant::EPHRAIM_ATTACK_IMPACT_FRAMES[anIndex] };
	}

}

bool Plant::FindTargetAndFire(int theRow, PlantWeapon thePlantWeapon)
{
	if (mSeedType == SeedType::SEED_EPHRAIM && mShootingCounter > 0)
		return false;

	Zombie* aZombie = FindTargetZombie(theRow, thePlantWeapon);
	if (aZombie == nullptr)
		return false;

	EndBlink();
	if (mSeedType == SeedType::SEED_EPHRAIM)
	{
		Rect aTargetRect = aZombie->GetZombieRect();
		mTargetX = aTargetRect.mX + aTargetRect.mWidth / 2;
		const bool aRanged = std::abs(mTargetX - (mX + mWidth / 2)) > EPHRAIM_ATTACK_RANGE_FRONT;
		mEphraimAttackSet = RandRangeInt(aRanged ? 2 : 0, EPHRAIM_ATTACK_VARIANT_COUNT - 1);
		mEphraimAfterimagesRemaining = RollEphraimAfterimageCount();
		const bool anAfterimage = mEphraimAfterimagesRemaining > 0;
		// Pick a different complete animation, with its own pose timing and contact frame.
		const int anAfterimageSet = anAfterimage
			? (aRanged ? 5 - mEphraimAttackSet :
				(mEphraimAttackSet + RandRangeInt(1, EPHRAIM_ATTACK_VARIANT_COUNT - 1)) % EPHRAIM_ATTACK_VARIANT_COUNT) : 0;
		mEphraimAttackPauseFlags = anAfterimage
			? EPHRAIM_ATTACK_FLAG_AFTERIMAGE | (anAfterimageSet << EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_SHIFT) : 0;
		if (aRanged)
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_RANGED;
		mEphraimAfterimageFrame = -1;
		mShootingCounter = GetEphraimAttackTiming(mEphraimAttackSet).mTotalTicks;
		mFrame = 0;
		mLaunchCounter = aRanged ? EPHRAIM_RANGED_REST_TICKS[mEphraimAttackSet] : EPHRAIM_ATTACK_INTERVAL_TICKS[mEphraimAttackSet];
		return true;
	}

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);

	if (mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
	{
		Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
		aHeadReanim2->StartBlend(20);
		aHeadReanim2->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
		aHeadReanim2->mAnimRate = 35.0f;
		aHeadReanim2->SetFramesForLayer("anim_splitpea_shooting");
		mShootingCounter = 26;
	}
	else if (aHeadReanim && aHeadReanim->TrackExists("anim_shooting"))
	{
		aHeadReanim->StartBlend(20);
		aHeadReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
		aHeadReanim->mAnimRate = 35.0f;
		aHeadReanim->SetFramesForLayer("anim_shooting");

		mShootingCounter = 35;
		if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_SPLITPEA || mSeedType == SeedType::SEED_LEFTPEATER)
		{
			aHeadReanim->mAnimRate = 45.0f;
			mShootingCounter = 26;
		}
		else if (mSeedType == SeedType::SEED_GATLINGPEA)
		{
			aHeadReanim->mAnimRate = 152.0f;
			mShootingCounter = 25;
		}
	}
	else if (mState == PlantState::STATE_CACTUS_HIGH)
	{
		PlayBodyReanim("anim_shootinghigh", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);
		mShootingCounter = 23;
	}
	else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 42.0f);
		mShootingCounter = 67;
	}
	else if (mSeedType == SeedType::SEED_CATTAIL)
	{
		bool aCatTailOverdrive = mBoard->mCatTailOverdriveActive;
		PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aCatTailOverdrive ? 80.0f : 30.0f);
		mShootingCounter = aCatTailOverdrive ? 20 : 50;
	}
	else if (aBodyReanim && aBodyReanim->TrackExists("anim_shooting"))
	{
		PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);

		switch (mSeedType)
		{
		case SeedType::SEED_FUMESHROOM:     mShootingCounter = 50;  break;
		case SeedType::SEED_PUFFSHROOM:     mShootingCounter = 29;  break;
		case SeedType::SEED_SCAREDYSHROOM:  mShootingCounter = 25;  break;
		case SeedType::SEED_CABBAGEPULT:    mShootingCounter = 32;  break;
		case SeedType::SEED_MELONPULT:
		case SeedType::SEED_WINTERMELON:    mShootingCounter = 36;  break;
		case SeedType::SEED_KERNELPULT:
		{
			bool aShouldButter = mBoard->mSunMoney >= KERNEL_PULT_BUTTER_BARRAGE_SUN_THRESHOLD
				? Sexy::Rand(100) < 90
				: (mBoard->mKernelPultOverdriveActive ? Sexy::Rand(2) == 0 : Sexy::Rand(4) == 0);
			if (aShouldButter)
			{
				aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
				aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_NORMAL);
				aBodyReanim->AssignRenderGroupToPrefix("Cornpult_kernal", RENDER_GROUP_HIDDEN);
				mState = PlantState::STATE_KERNELPULT_BUTTER;
			}

			mShootingCounter = 30;
			break;
		}
		case SeedType::SEED_CACTUS:         mShootingCounter = 35;  break;
		default:                            mShootingCounter = 29;  break;
		}
	}
	else
		Fire(aZombie, theRow, thePlantWeapon);

	return true;
}

void Plant::LaunchThreepeater()
{
	int rowAbove = mRow - 1;
	int rowBelow = mRow + 1;

	if ((FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY)) ||
		(mBoard->RowCanHaveZombies(rowAbove) && FindTargetZombie(rowAbove, PlantWeapon::WEAPON_PRIMARY)) ||
		(mBoard->RowCanHaveZombies(rowBelow) && FindTargetZombie(rowBelow, PlantWeapon::WEAPON_PRIMARY)))
	{
		Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);
		Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
		Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);

		if (mBoard->RowCanHaveZombies(rowBelow))
		{
			aHeadReanim1->StartBlend(10);
			aHeadReanim1->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
			aHeadReanim1->mAnimRate = 20.0f;
			aHeadReanim1->SetFramesForLayer("anim_shooting1");
		}

		aHeadReanim2->StartBlend(10);
		aHeadReanim2->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
		aHeadReanim2->mAnimRate = 20.0f;
		aHeadReanim2->SetFramesForLayer("anim_shooting2");

		if (mBoard->RowCanHaveZombies(rowAbove))
		{
			aHeadReanim3->StartBlend(10);
			aHeadReanim3->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
			aHeadReanim3->mAnimRate = 20.0f;
			aHeadReanim3->SetFramesForLayer("anim_shooting3");
		}

		mShootingCounter = 35;
	}
}

bool Plant::FindStarFruitTarget()
{
	if (mRecentlyEatenCountdown > 0)
		return true;

	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	int aCenterStarX = mX + 40;
	int aCenterStarY = mY + 40;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		Rect aZombieRect = aZombie->GetZombieRect();
		if (aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS && mPlantCol >= 5)
				return true;

			if (aZombie->mRow == mRow)
			{
				if (aZombieRect.mX + aZombieRect.mWidth < aCenterStarX)
					return true;
			}
			else
			{
				if (aZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
					aZombieRect.mWidth += 10;

				float aProjectileTime = Distance2D(aCenterStarX, aCenterStarY, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2) / 3.33f;
				int aZombieHitX = aZombie->ZombieTargetLeadX(aProjectileTime) - aZombieRect.mWidth / 2;
				if ((aZombieHitX + aZombieRect.mWidth > aCenterStarX) && (aZombieHitX < aCenterStarX))
					return true;

				int aCenterZombieX = aZombieHitX + aZombieRect.mWidth / 2;
				int aCenterZombieY = aZombieRect.mY + aZombieRect.mHeight / 2;
				float angle = RAD_TO_DEG(atan2(aCenterZombieY - aCenterStarY, aCenterZombieX - aCenterStarX));
				if (abs(aZombie->mRow - mRow) < 2)
				{
					if ((angle > 20.0f && angle < 40.0f) || (angle < -25.0f && angle > -45.0f))
						return true;
				}
				else
				{
					if ((angle > 25.0f && angle < 35.0f) || (angle < -28.0f && angle > -38.0f))
						return true;
				}
			}
		}
	}

	return false;
}

void Plant::LaunchStarFruit()
{
	if (FindStarFruitTarget())
	{
		PlayBodyReanim("anim_shoot", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 28.0f);
		mShootingCounter = 40;
	}
}

void Plant::StarFruitFire()
{
	mApp->PlayFoley(FoleyType::FOLEY_THROW);

	float aShootAngleX = cos(DEG_TO_RAD(30.0f)) * 3.33f;
	float aShootAngleY = sin(DEG_TO_RAD(30.0f)) * 3.33f;
	for (int i = 0; i < 5; i++)
	{
		Projectile* aProjectile = mBoard->AddProjectile(mX + 25, mY + 25, mRenderOrder - 1, mRow, ProjectileType::PROJECTILE_STAR);
		if (aProjectile == nullptr)
			return;
		aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
		aProjectile->mMotionType = ProjectileMotion::MOTION_STAR;

		switch (i)
		{
		case 0:     aProjectile->mVelX = -3.33f;         aProjectile->mVelY = 0.0f;             break;
		case 1:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = 3.33f;             break;
		case 2:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = -3.33f;            break;
		case 3:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = aShootAngleY;      break;
		case 4:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = -aShootAngleY;     break;
		default:    PVZP_ASSERT(false);                                                               break;
		}
	}
}

void Plant::UpdateShooter()
{
	if (mSeedType == SeedType::SEED_EPHRAIM)
	{
		// Recovery starts after the complete primary animation and all its holds.
		// Independent echoes never participate in this countdown.
		if (mShootingCounter > 0)
			return;
		if (--mLaunchCounter <= 0)
		{
			mEphraimAttackPauseFlags &= ~EPHRAIM_ATTACK_FLAG_RECOVERY;
			mLaunchCounter = 10;
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
		}
		return;
	}
	mLaunchCounter--;
	if (mLaunchCounter <= 0)
	{
		int aLaunchJitter = mSeedType == SeedType::SEED_CATTAIL && mBoard->mCatTailOverdriveActive ? Sexy::Rand(3) :
			(mSeedType == SeedType::SEED_GLOOMSHROOM ? Sexy::Rand(5) :
			(mSeedType == SeedType::SEED_KERNELPULT && mBoard->mKernelPultOverdriveActive ? Sexy::Rand(4) : Sexy::Rand(15)));
		mLaunchCounter = mLaunchRate - aLaunchJitter;

		if (mSeedType == SeedType::SEED_THREEPEATER)
		{
			LaunchThreepeater();
		}
		else if (mSeedType == SeedType::SEED_STARFRUIT)
		{
			LaunchStarFruit();
		}
		else if (mSeedType == SeedType::SEED_SPLITPEA)
		{
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
		}
		else if (mSeedType == SeedType::SEED_CACTUS)
		{
			if (mState == PlantState::STATE_CACTUS_HIGH)
			{
				FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
			}
			else if (mState == PlantState::STATE_CACTUS_LOW)
			{
				FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
			}
		}
		else
		{
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
		}
	}

	if (mLaunchCounter == 25 && mSeedType == SeedType::SEED_CATTAIL && !mBoard->mCatTailOverdriveActive)
	{
		FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
	}
	if (mLaunchCounter == 25)
	{
		if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_LEFTPEATER)
		{
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_PRIMARY);
		}
		else if (mSeedType == SeedType::SEED_SPLITPEA)
		{
			FindTargetAndFire(mRow, PlantWeapon::WEAPON_SECONDARY);
		}
	}
}

void Plant::LaunchEphraimJavelin(int theTargetX, int theAttackSet, int theTrailOffset, const int* theAttackOriginX)
{
	const bool aTrailLeft = theTargetX < mX + mWidth / 2;
	const int anOriginX = theAttackOriginX ? *theAttackOriginX : mX + mWidth / 2 + (aTrailLeft ? theTrailOffset : -theTrailOffset);
	const bool aLeft = theTargetX < anOriginX;
	Projectile* aProjectile = mBoard->AddProjectile(anOriginX - EPHRAIM_JAVELIN_WIDTH / 2,
		mY - 45, mRenderOrder + 1, mRow, ProjectileType::PROJECTILE_EPHRAIM_JAVELIN);
	if (aProjectile != nullptr)
	{
		aProjectile->mEphraimChargedJavelin = theAttackSet == 3;
		const float aSpeed = aProjectile->mEphraimChargedJavelin ? EPHRAIM_WINDUP_JAVELIN_SPEED : EPHRAIM_JAVELIN_SPEED;
		aProjectile->mVelX = aLeft ? -aSpeed : aSpeed;
		aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
		mApp->PlayFoley(FoleyType::FOLEY_THROW);
	}
}

void Plant::HealEphraimOnAttack()
{
	if (mDead || mSquished || mPlantHealth <= 0 || mPlantMaxHealth <= 0)
		return;
	const float aHealing = static_cast<float>(mPlantMaxHealth) * EPHRAIM_ATTACK_HEAL_PER_MILLE / 1000.0f + mContinuousHealthRemainder;
	const int aWholeHealing = static_cast<int>(aHealing);
	mContinuousHealthRemainder = aHealing - aWholeHealing;
	mPlantHealth = std::min(mPlantHealth + aWholeHealing, mPlantMaxHealth);
	if (mPlantHealth >= mPlantMaxHealth)
		mContinuousHealthRemainder = 0.0f;
}

int Plant::RollEphraimAfterimageCount()
{
	if (RandRangeInt(0, 99) < mEphraimAfterimageChancePercent)
	{
		const int aCount = std::clamp(mEphraimAfterimageFailureCount, 1, EPHRAIM_MAX_AFTERIMAGES);
		mEphraimAfterimageFailureCount = 0;
		mEphraimAfterimageChancePercent = EPHRAIM_AFTERIMAGE_CHANCE_PERCENT;
		return aCount;
	}
	mEphraimAfterimageFailureCount = std::min(EPHRAIM_MAX_AFTERIMAGES, mEphraimAfterimageFailureCount + 1);
	mEphraimAfterimageChancePercent = std::min(100, mEphraimAfterimageChancePercent + EPHRAIM_AFTERIMAGE_CHANCE_INCREMENT_PERCENT);
	return 0;
}

int Plant::GetEphraimAfterimageOriginX(const EphraimAfterimage& theEcho) const
{
	if (theEcho.mOriginX != -10000)
		return theEcho.mOriginX;
	return mX + mWidth / 2 + (theEcho.mTargetX < mX + mWidth / 2 ? theEcho.mTrailOffset : -theEcho.mTrailOffset);
}

void Plant::ConfigureEphraimAfterimage(EphraimAfterimage& theEcho, int theParentSet, int thePreviousSet)
{
	// Choose once before windup, so a moving target cannot splice attack rows.
	if (Zombie* aTarget = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY, nullptr, nullptr, false, &theEcho.mOriginX))
	{
		const Rect aRect = aTarget->GetZombieRect();
		theEcho.mTargetX = aRect.mX + aRect.mWidth / 2;
	}
	const bool aRanged = std::abs(theEcho.mTargetX - theEcho.mOriginX) > EPHRAIM_ATTACK_RANGE_FRONT;
	theEcho.mPauseFlags = aRanged ? EPHRAIM_ATTACK_FLAG_RANGED : 0;
	std::array<int, EPHRAIM_ATTACK_VARIANT_COUNT> aChoices{};
	int aCount = 0;
	for (int aSet = aRanged ? 2 : 0; aSet < EPHRAIM_ATTACK_VARIANT_COUNT; aSet++)
		if (aSet != theParentSet && aSet != thePreviousSet)
			aChoices[aCount++] = aSet;
	// With only two ranged styles, keep sibling variety when both are excluded.
	if (aCount == 0)
		for (int aSet = aRanged ? 2 : 0; aSet < EPHRAIM_ATTACK_VARIANT_COUNT; aSet++)
			if (aSet != thePreviousSet)
				aChoices[aCount++] = aSet;
	theEcho.mAttackSet = aChoices[RandRangeInt(0, aCount - 1)];
}

void Plant::SpawnEphraimAfterimages()
{
	const int aCount = std::clamp(mEphraimAfterimagesRemaining, 1, EPHRAIM_MAX_AFTERIMAGES);
	int aPreviousSet = mEphraimAttackSet;
	for (int i = 0; i < aCount; i++)
	{
		EphraimAfterimage anEcho;
		// All clones in this trigger appear together; each owns one attack.
		anEcho.mTargetX = mTargetX;
		anEcho.mTrailOffset = EPHRAIM_AFTERIMAGE_TRAIL_OFFSET + i * EPHRAIM_AFTERIMAGE_SPACING;
		anEcho.mOriginX = GetEphraimAfterimageOriginX(anEcho);
		ConfigureEphraimAfterimage(anEcho, mEphraimAttackSet, aPreviousSet);
		aPreviousSet = anEcho.mAttackSet;
		mEphraimAfterimages.push_back(anEcho);
	}
	mEphraimAfterimagesRemaining = 0;
	mEphraimAfterimageFrame = -1;
}

void Plant::UpdateEphraimAfterimages()
{
	// Queue children so spawning cannot invalidate a live echo reference.
	std::vector<EphraimAfterimage> aChildren;
	for (EphraimAfterimage& anEcho : mEphraimAfterimages)
	{
		if (anEcho.mDelayTicks > 0)
		{
			anEcho.mDelayTicks--;
			continue;
		}
		if (anEcho.mHitStopTicks > 0)
		{
			anEcho.mHitStopTicks--;
			continue;
		}
		const int anOriginX = GetEphraimAfterimageOriginX(anEcho);
		const int aSet = anEcho.mAttackSet;
		const int aFrame = EphraimAttackAtlasFrame(aSet, anEcho.mElapsedTicks);
		const bool aRanged = (anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0;
		if ((anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0 &&
			aFrame == EPHRAIM_ATTACK_RELEASE_FRAMES[aSet] && (anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_RELEASE) == 0)
		{
			anEcho.mPauseFlags |= EPHRAIM_ATTACK_FLAG_RELEASE;
			anEcho.mHitStopTicks = EPHRAIM_ATTACK_RELEASE_TICKS[aSet];
			continue;
		}
		if (aFrame == EPHRAIM_ATTACK_RECOIL_FRAMES[aSet] && (anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_RECOIL) == 0)
		{
			anEcho.mPauseFlags |= EPHRAIM_ATTACK_FLAG_RECOIL;
			anEcho.mHitStopTicks = aRanged ? EPHRAIM_RANGED_RECOIL_TICKS[aSet] : EPHRAIM_ATTACK_RECOIL_TICKS[aSet];
			continue;
		}
		if (aFrame == EPHRAIM_ATTACK_ANTICIPATION_FRAMES[aSet] &&
			(anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_AFTERIMAGE_ANTICIPATION) == 0)
		{
			anEcho.mPauseFlags |= EPHRAIM_ATTACK_FLAG_AFTERIMAGE_ANTICIPATION;
			anEcho.mHitStopTicks = aRanged ? EPHRAIM_RANGED_WINDUP_TICKS[aSet] : EPHRAIM_ATTACK_ANTICIPATION_TICKS[aSet];
			continue;
		}
		if (aFrame == EPHRAIM_ATTACK_IMPACT_FRAMES[aSet] &&
			(anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_AFTERIMAGE_IMPACT) == 0)
		{
			anEcho.mPauseFlags |= EPHRAIM_ATTACK_FLAG_AFTERIMAGE_IMPACT;
			HealEphraimOnAttack();
			if ((anEcho.mPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0)
			{
				LaunchEphraimJavelin(anEcho.mTargetX, aSet, anEcho.mTrailOffset, &anOriginX);
			}
			else
			{
				std::vector<Zombie*> aHitZombies;
				while (Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY, &aHitZombies, &anEcho.mTargetX, true, &anOriginX))
				{
					aZombie->TakeDamage(EPHRAIM_ATTACK_DAMAGE * EPHRAIM_ATTACK_DAMAGE_PERCENT[aSet] / 100, 0U);
					aHitZombies.push_back(aZombie);
				}
				if (!aHitZombies.empty() && anEcho.mNestingDepth < EPHRAIM_MAX_AFTERIMAGE_NESTING)
				{
					const int aCount = RollEphraimAfterimageCount();
					int aPreviousSet = aSet;
					for (int i = 0; i < aCount; i++)
					{
						EphraimAfterimage aChild;
						aChild.mTargetX = anEcho.mTargetX;
						aChild.mTrailOffset = anEcho.mTrailOffset + (i + 1) * EPHRAIM_AFTERIMAGE_SPACING;
						aChild.mOriginX = anOriginX + (anEcho.mTargetX < anOriginX ? 1 : -1) * (i + 1) * EPHRAIM_AFTERIMAGE_SPACING;
						aChild.mNestingDepth = anEcho.mNestingDepth + 1;
						ConfigureEphraimAfterimage(aChild, aSet, aPreviousSet);
						aPreviousSet = aChild.mAttackSet;
						aChildren.push_back(aChild);
					}
				}
				anEcho.mHitStopTicks = aHitZombies.empty()
					? EPHRAIM_ATTACK_MISS_STOP_TICKS[aSet] : EPHRAIM_ATTACK_HITSTOP_TICKS[aSet];
				if (anEcho.mHitStopTicks > 0)
					continue;
			}
		}
		anEcho.mElapsedTicks++;
	}
	mEphraimAfterimages.insert(mEphraimAfterimages.end(), aChildren.begin(), aChildren.end());
	std::erase_if(mEphraimAfterimages, [](const EphraimAfterimage& theEcho)
	{
		return theEcho.mElapsedTicks >= EPHRAIM_ATTACK_DURATION_TICKS[theEcho.mAttackSet];
	});
}

void Plant::UpdateShooting()
{
	if (NotOnGround() || (mShootingCounter == 0 && mSeedType != SeedType::SEED_EPHRAIM))
		return;

	if (mSeedType == SeedType::SEED_EPHRAIM)
	{
		// Migrate an older save's single echo before advancing independent clocks.
		if (mEphraimAfterimageFrame >= 0)
		{
			const int aLegacyFrame = mEphraimAfterimageFrame;
			if ((mEphraimAttackPauseFlags & (EPHRAIM_ATTACK_FLAG_AFTERIMAGE | EPHRAIM_ATTACK_FLAG_PRIMARY_IMPACT)) ==
				(EPHRAIM_ATTACK_FLAG_AFTERIMAGE | EPHRAIM_ATTACK_FLAG_PRIMARY_IMPACT))
			{
				const size_t aFirstEcho = mEphraimAfterimages.size();
				SpawnEphraimAfterimages();
				// Keep the row already underway in a legacy save.
				mEphraimAfterimages[aFirstEcho].mAttackSet = std::clamp(
					(mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_MASK) >> EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_SHIFT,
					(mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0 ? 2 : 0, EPHRAIM_ATTACK_VARIANT_COUNT - 1);
				mEphraimAfterimages[aFirstEcho].mElapsedTicks = std::min(aLegacyFrame,
					EPHRAIM_ATTACK_DURATION_TICKS[mEphraimAfterimages[aFirstEcho].mAttackSet] - 1);
				mEphraimAfterimages[aFirstEcho].mPauseFlags = mEphraimAttackPauseFlags &
					(EPHRAIM_ATTACK_FLAG_AFTERIMAGE_IMPACT | EPHRAIM_ATTACK_FLAG_AFTERIMAGE_ANTICIPATION | EPHRAIM_ATTACK_FLAG_RANGED);
			}
			mEphraimAfterimageFrame = -1;
		}
		UpdateEphraimAfterimages();
		if (mEphraimHitStopCounter > 0)
		{
			mEphraimHitStopCounter--;
			return;
		}
		if (mShootingCounter == 0)
			return;

		const EphraimAttackTiming aPrimaryTiming = GetEphraimAttackTiming(mEphraimAttackSet);
		const int aImpactFrame = aPrimaryTiming.mImpactFrame;
		const int aPrimarySet = std::clamp(mEphraimAttackSet, 0, EPHRAIM_ATTACK_VARIANT_COUNT - 1);
		const bool aRanged = (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0;
		if ((mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0 &&
			mFrame == EPHRAIM_ATTACK_RELEASE_FRAMES[aPrimarySet] && (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RELEASE) == 0)
		{
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_RELEASE;
			mEphraimHitStopCounter = EPHRAIM_ATTACK_RELEASE_TICKS[aPrimarySet];
			return;
		}
		if (mFrame == EPHRAIM_ATTACK_RECOIL_FRAMES[aPrimarySet] && (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RECOIL) == 0)
		{
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_RECOIL;
			mEphraimHitStopCounter = aRanged ? EPHRAIM_RANGED_RECOIL_TICKS[aPrimarySet] : EPHRAIM_ATTACK_RECOIL_TICKS[aPrimarySet];
			return;
		}

		// Drive charge and damage from the pose currently on screen. This keeps
		// impact on the weapon contact frame, rather than a separate timer guess.
		if (mShootingCounter > 0 && mFrame == EPHRAIM_ATTACK_ANTICIPATION_FRAMES[aPrimarySet] &&
			(mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_ANTICIPATION_PAUSE) == 0)
		{
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_ANTICIPATION_PAUSE;
			mEphraimHitStopCounter = aRanged ? EPHRAIM_RANGED_WINDUP_TICKS[aPrimarySet] : EPHRAIM_ATTACK_ANTICIPATION_TICKS[aPrimarySet];
			return;
		}
		if (mShootingCounter > 0 && mFrame == aImpactFrame && (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_PRIMARY_IMPACT) == 0)
		{
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_PRIMARY_IMPACT;
			HealEphraimOnAttack();
			if ((mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_AFTERIMAGE) != 0)
				SpawnEphraimAfterimages();
			if ((mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RANGED) != 0)
			{
				LaunchEphraimJavelin(mTargetX, aPrimarySet);
			}
			else
			{
				std::vector<Zombie*> aHitZombies;
				while (Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY, &aHitZombies, nullptr, true))
				{
					aZombie->TakeDamage(EPHRAIM_ATTACK_DAMAGE * EPHRAIM_ATTACK_DAMAGE_PERCENT[aPrimarySet] / 100, 0U);
					aHitZombies.push_back(aZombie);
				}
				mEphraimHitStopCounter = aHitZombies.empty()
					? EPHRAIM_ATTACK_MISS_STOP_TICKS[aPrimarySet] : EPHRAIM_ATTACK_HITSTOP_TICKS[aPrimarySet];
				if (mEphraimHitStopCounter > 0)
					return;
			}
		}

		if (mShootingCounter > 0 && --mShootingCounter == 0)
		{
			mEphraimAttackPauseFlags |= EPHRAIM_ATTACK_FLAG_RECOVERY;
			mLaunchCounter = (aRanged ? EPHRAIM_RANGED_REST_TICKS[aPrimarySet] : EPHRAIM_ATTACK_INTERVAL_TICKS[aPrimarySet]) + 1;
		}
		return;
	}

	mShootingCounter--;

	if (mSeedType == SeedType::SEED_FUMESHROOM && mShootingCounter == 15)
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
		AddAttachedParticle(mX + 85, mY + 31, aRenderPosition, ParticleEffect::PARTICLE_FUMECLOUD);
	}

	if (mSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		if (mShootingCounter == 46 || mShootingCounter == 36 || mShootingCounter == 27 || mShootingCounter == 17)
		{
			int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
			AddAttachedParticle(mX + 40, mY + 40, aRenderPosition, ParticleEffect::PARTICLE_GLOOMCLOUD);
		}
		if (mShootingCounter == 42 || mShootingCounter == 33 || mShootingCounter == 23 || mShootingCounter == 14)
		{
			Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
		}
	}
	else if (mSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (mShootingCounter == 24)
		{
			mGatlingPeaMillionSunVolley = mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD;
			mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
			if (!mGatlingPeaMillionSunVolley && mBoard->mGatlingPeaOverdriveActive && Sexy::Rand(100) < 15)
			{
				switch (Sexy::Rand(3))
				{
				case 0: mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_BUTTER; break;
				case 1: mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_CHERRYBOMB; break;
				case 2: mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_MELON; break;
				default: PVZP_ASSERT(false); break;
				}
			}
		}
		if (mShootingCounter == 1 || mShootingCounter == 8 || mShootingCounter == 16 || mShootingCounter == 24)
		{
			if (mGatlingPeaMillionSunVolley)
			{
				mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
				if (Sexy::Rand(100) < 50 && mBoard->TakeSunMoney(150))
					mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_CHERRYBOMB;
			}
			Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
			if (mShootingCounter == 1 || mGatlingPeaMillionSunVolley)
				mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
			if (mShootingCounter == 1)
				mGatlingPeaMillionSunVolley = false;
		}
	}
	else if (mSeedType == SeedType::SEED_CATTAIL)
	{
		if (mShootingCounter == 19)
		{
			Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
			if (aZombie)
			{
				Fire(aZombie, mRow, PlantWeapon::WEAPON_PRIMARY);
			}
		}
	}
	else if (mShootingCounter == 1)
	{
		if (mSeedType == SeedType::SEED_THREEPEATER)
		{
			int rowAbove = mRow - 1;
			int rowBelow = mRow + 1;
			Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
			Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);
			Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);

			if (aHeadReanim1->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				Fire(nullptr, rowBelow, PlantWeapon::WEAPON_PRIMARY);
			}
			if (aHeadReanim2->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
			}
			if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				Fire(nullptr, rowAbove, PlantWeapon::WEAPON_PRIMARY);
			}
		}
		else if (mSeedType == SeedType::SEED_SPLITPEA)
		{
			Reanimation* aHeadBackReanim = mApp->ReanimationTryToGet(mHeadReanimID2);
			Reanimation* aHeadFrontReanim = mApp->ReanimationTryToGet(mHeadReanimID);
			if (aHeadFrontReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && mLaunchCounter > 25)
			{
				Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
			}
			if (aHeadBackReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				Fire(nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
			}
		}
		else if (mState == PlantState::STATE_CACTUS_LOW)
		{
			Fire(nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
		}
		else if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_KERNELPULT || mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
		{
			PlantWeapon aPlantWeapon = PlantWeapon::WEAPON_PRIMARY;
			if (mState == PlantState::STATE_KERNELPULT_BUTTER)
			{
				Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
				aBodyReanim->AssignRenderGroupToPrefix("Cornpult_butter", RENDER_GROUP_HIDDEN);
				aBodyReanim->AssignRenderGroupToPrefix("Cornpult_kernal", RENDER_GROUP_NORMAL);
				mState = PlantState::STATE_NOTREADY;
				aPlantWeapon = PlantWeapon::WEAPON_SECONDARY;
			}

			Zombie* aZombie = FindTargetZombie(mRow, aPlantWeapon);
			int aKernelPultVolleySize = mSeedType == SeedType::SEED_KERNELPULT &&
				aPlantWeapon == PlantWeapon::WEAPON_SECONDARY &&
				mBoard->mSunMoney >= KERNEL_PULT_BUTTER_BARRAGE_SUN_THRESHOLD
				? RandRangeInt(2, 5) : 1;
			if (aKernelPultVolleySize > 1 && aZombie != nullptr)
			{
				std::vector<Zombie*> aButterVolleyTargets;
				aButterVolleyTargets.reserve(aKernelPultVolleySize);
				aButterVolleyTargets.push_back(aZombie);
				while (static_cast<int>(aButterVolleyTargets.size()) < aKernelPultVolleySize)
				{
					Zombie* aNextTarget = FindTargetZombie(mRow, aPlantWeapon, &aButterVolleyTargets);
					if (aNextTarget == nullptr)
						break;
					aButterVolleyTargets.push_back(aNextTarget);
				}

				int aActualVolleySize = static_cast<int>(aButterVolleyTargets.size());
				for (int i = 0; i < aActualVolleySize; i++)
				{
					Fire(aButterVolleyTargets[i], mRow, aPlantWeapon, -1, false, false,
						aActualVolleySize > 1 ? i : -1, aActualVolleySize);
				}
			}
			else
			{
				Fire(aZombie, mRow, aPlantWeapon);
			}
		}
		else
		{
			Fire(nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
		}

		return;
	}

	if (mShootingCounter != 0)
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);
	if (mSeedType == SeedType::SEED_THREEPEATER)
	{
		Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);
		Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);

		if (aHeadReanim2->mLoopCount > 0)
		{
			if (aHeadReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				aHeadReanim->StartBlend(20);
				aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
				aHeadReanim->SetFramesForLayer("anim_head_idle1");
				aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
				aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
			}

			aHeadReanim2->StartBlend(20);
			aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
			aHeadReanim2->SetFramesForLayer("anim_head_idle2");
			aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
			aHeadReanim2->mAnimTime = aBodyReanim->mAnimTime;

			if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
			{
				aHeadReanim3->StartBlend(20);
				aHeadReanim3->mLoopType = ReanimLoopType::REANIM_LOOP;
				aHeadReanim3->SetFramesForLayer("anim_head_idle3");
				aHeadReanim3->mAnimRate = aBodyReanim->mAnimRate;
				aHeadReanim3->mAnimTime = aBodyReanim->mAnimTime;
			}

			return;
		}
	}
	else if (mSeedType == SeedType::SEED_SPLITPEA)
	{
		Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

		if (aHeadReanim->mLoopCount > 0)
		{
			aHeadReanim->StartBlend(20);
			aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aHeadReanim->SetFramesForLayer("anim_head_idle");
			aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
			aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
		}

		if (aHeadReanim2->mLoopCount > 0)
		{
			aHeadReanim2->StartBlend(20);
			aHeadReanim2->mLoopType = ReanimLoopType::REANIM_LOOP;
			aHeadReanim2->SetFramesForLayer("anim_splitpea_idle");
			aHeadReanim2->mAnimRate = aBodyReanim->mAnimRate;
			aHeadReanim2->mAnimTime = aBodyReanim->mAnimTime;
		}

		return;
	}
	else if (mState == PlantState::STATE_CACTUS_HIGH)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayBodyReanim("anim_idlehigh", ReanimLoopType::REANIM_LOOP, 20, 0.0f);

			aBodyReanim->mAnimRate = aBodyReanim->mDefinition->mFPS;
			if (mApp->IsIZombieLevel())
			{
				aBodyReanim->mAnimRate = 0.0f;
			}

			return;
		}
	}
	else if (aHeadReanim)
	{
		if (aHeadReanim->mLoopCount > 0)
		{
			aHeadReanim->StartBlend(20);
			aHeadReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aHeadReanim->SetFramesForLayer("anim_head_idle");
			aHeadReanim->mAnimRate = aBodyReanim->mAnimRate;
			aHeadReanim->mAnimTime = aBodyReanim->mAnimTime;
			return;
		}
	}
	else if (mSeedType == SeedType::SEED_COBCANNON)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_COBCANNON_ARMING;
			mStateCountdown = 3000;
			PlayBodyReanim("anim_unarmed_idle", ReanimLoopType::REANIM_LOOP, 20, aBodyReanim->mDefinition->mFPS);
			return;
		}
	}
	else if (aBodyReanim && aBodyReanim->mLoopCount > 0)
	{
		PlayIdleAnim(aBodyReanim->mDefinition->mFPS);
		return;
	}

	mShootingCounter = 1;
}
