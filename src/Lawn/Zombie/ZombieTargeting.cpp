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

bool Zombie::CanTargetPlant(Plant* thePlant, ZombieAttackType theAttackType)
{
	if (ZombieRules::IsBulwarkType(mZombieType))
		return false;

	if (mApp->IsWallnutBowlingLevel() && theAttackType != ZombieAttackType::ATTACKTYPE_VAULT)
		return false;

	if (thePlant->NotOnGround() || thePlant->mSeedType == SeedType::SEED_TANGLEKELP)
		return false;

	if (!mInPool && mBoard->IsPoolSquare(thePlant->mPlantCol, thePlant->mRow))
		return false;

	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
	{
		return thePlant->mSeedType == SeedType::SEED_POTATOMINE && thePlant->mState == PlantState::STATE_NOTREADY;
	}

	if (thePlant->IsSpiky())
	{
		bool aCanAttackSpikeweed =
			ZombieRules::IsGargantuarType(mZombieType) ||
			mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
			mBoard->IsPoolSquare(thePlant->mPlantCol, thePlant->mRow) ||
			mBoard->GetFlowerPotAt(thePlant->mPlantCol, thePlant->mRow);  // this lets ladder zombies ladder spikeweed/spikerock planted in flower pots
		if (!aCanAttackSpikeweed)
			return false;

		if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && ZombieRules::IsGargantuarType(mZombieType) &&
			mZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR &&
			mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD &&
			mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_EATING_ORDER) == thePlant)
		{
			PlantID aPlantID = static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(thePlant));
			if (mFirstIgnoredSpikyPlantID == aPlantID)
				return false;
		}
		return true;
	}

	if (theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER)
	{
		if (thePlant->mSeedType == SeedType::SEED_CHERRYBOMB || thePlant->mSeedType == SeedType::SEED_JALAPENO ||
			thePlant->mSeedType == SeedType::SEED_BLOVER || thePlant->mSeedType == SeedType::SEED_SQUASH)
		{
			return false;
		}
		if (thePlant->mSeedType == SeedType::SEED_DOOMSHROOM || thePlant->mSeedType == SeedType::SEED_ICESHROOM)
		{
			return thePlant->mIsAsleep;
		}
	}

	if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING || mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING)
	{
		bool aPlaceLadder = false;
		if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->IsTallNut() || thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aPlaceLadder = true;
		}

		if (mBoard->GetLadderAt(thePlant->mPlantCol, thePlant->mRow))
		{
			aPlaceLadder = false;
		}

		if ((theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && aPlaceLadder) || (theAttackType == ZombieAttackType::ATTACKTYPE_LADDER && !aPlaceLadder))
		{
			return false;
		}
	}

	if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW)
	{
		Plant* aTopPlant = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_EATING_ORDER);
		if (aTopPlant != thePlant && aTopPlant && CanTargetPlant(aTopPlant, theAttackType))
		{
			return false;
		}
	}

	if (theAttackType == ZombieAttackType::ATTACKTYPE_VAULT)
	{
		Plant* aTopPlant = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aTopPlant != thePlant && aTopPlant && CanTargetPlant(aTopPlant, theAttackType))
		{
			return false;
		}
	}

	return true;
}

Plant* Zombie::FindPlantTarget(ZombieAttackType theAttackType)
{
	Rect aAttackRect = GetZombieAttackRect();
	if (theAttackType == ZombieAttackType::ATTACKTYPE_VAULT)
	{
		// A support plant or overlapping plant must not hide a tall vault blocker.
		for (Plant* aPlant : mBoard->mPlants)
		{
			if (!aPlant->mDead && aPlant->mRow == mRow && aPlant->HasTallNutDefense() &&
				GetRectOverlap(aAttackRect, aPlant->GetPlantRect()) >= 20 &&
				CanTargetPlant(aPlant, theAttackType))
				return aPlant;
		}
	}
	if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && ZombieRules::IsGargantuarType(mZombieType) &&
		mZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR &&
		mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD &&
		mFirstIgnoredSpikyPlantID == PlantID::PLANTID_NULL)
	{
		Plant* aFirstSpikyPlant = nullptr;
		int aNearestDistance = 100000;
		Rect aZombieRect = GetZombieRect();
		int aZombieCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
		for (Plant* aPlant : mBoard->mPlants)
		{
			if (aPlant->mDead || aPlant->mRow != mRow || !aPlant->IsSpiky() ||
				mBoard->GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_EATING_ORDER) != aPlant ||
				!CanTargetPlant(aPlant, theAttackType))
				continue;

			Rect aPlantRect = aPlant->GetPlantRect();
			if (GetRectOverlap(aAttackRect, aPlantRect) < 20)
				continue;

			int aPlantCenterX = aPlantRect.mX + aPlantRect.mWidth / 2;
			int aDistance = IsWalkingBackwards() ? aPlantCenterX - aZombieCenterX : aZombieCenterX - aPlantCenterX;
			if (aDistance < 0)
				continue;
			if (aDistance < aNearestDistance)
			{
				aNearestDistance = aDistance;
				aFirstSpikyPlant = aPlant;
			}
		}

		if (aFirstSpikyPlant != nullptr)
			mFirstIgnoredSpikyPlantID = static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(aFirstSpikyPlant));
	}
	if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && ZombieRules::IsGargantuarType(mZombieType) &&
		mZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR &&
		mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
	{
		// At the 1m tier, ignore only the first spike plant; choose the next target by distance,
		// not plant allocation order, so a farther Tallnut cannot be selected through another Spikerock.
		Plant* aClosestPlant = nullptr;
		int aClosestDistance = std::numeric_limits<int>::max();
		Rect aZombieRect = GetZombieRect();
		int aZombieCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
		for (Plant* aPlant : mBoard->mPlants)
		{
			if (aPlant->mDead || aPlant->mRow != mRow || !CanTargetPlant(aPlant, theAttackType))
				continue;

			Rect aPlantRect = aPlant->GetPlantRect();
			if (GetRectOverlap(aAttackRect, aPlantRect) < 20)
				continue;

			int aPlantCenterX = aPlantRect.mX + aPlantRect.mWidth / 2;
			int aDistance = IsWalkingBackwards() ? aPlantCenterX - aZombieCenterX : aZombieCenterX - aPlantCenterX;
			if (aDistance >= 0 && aDistance < aClosestDistance)
			{
				aClosestDistance = aDistance;
				aClosestPlant = aPlant;
			}
		}
		if (aClosestPlant != nullptr)
			return aClosestPlant;
	}

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow == mRow)
		{
			Rect aPlantRect = aPlant->GetPlantRect();
			if (GetRectOverlap(aAttackRect, aPlantRect) >= 20 && CanTargetPlant(aPlant, theAttackType))
			{
				return aPlant;
			}
		}
	}

	// A submerged snorkel can pass a pool plant between its narrow bite-rectangle checks.
	// Let it surface to eat an eligible plant as it approaches within one attack reach.
	if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && mZombieType == ZombieType::ZOMBIE_SNORKEL &&
		mInPool && mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
	{
		Plant* aClosestPlant = nullptr;
		int aClosestGap = std::numeric_limits<int>::max();
		for (Plant* aPlant : mBoard->mPlants)
		{
			if (aPlant->mDead || aPlant->mRow != mRow || !mBoard->IsPoolSquare(aPlant->mPlantCol, aPlant->mRow) ||
				!CanTargetPlant(aPlant, theAttackType))
				continue;

			Rect aPlantRect = aPlant->GetPlantRect();
			int aGap = IsWalkingBackwards()
				? aAttackRect.mX - (aPlantRect.mX + aPlantRect.mWidth)
				: aPlantRect.mX - (aAttackRect.mX + aAttackRect.mWidth);
			if (aGap >= 0 && aGap <= 60 && aGap < aClosestGap)
			{
				aClosestPlant = aPlant;
				aClosestGap = aGap;
			}
		}
		if (aClosestPlant != nullptr)
			return aClosestPlant;
	}

	return nullptr;
}

Zombie* Zombie::FindZombieTarget()
{
	if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
		return nullptr;

	Rect aAttackRect = GetZombieAttackRect();

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (mMindControlled != aZombie->mMindControlled &&
			!aZombie->IsFlying() &&
			aZombie->mZombiePhase != ZombiePhase::PHASE_DIGGER_TUNNELING &&
			aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_DIVING &&
			aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING &&
			aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_RISING &&
			aZombie->mZombieHeight != ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED &&
			!aZombie->IsDeadOrDying() &&
			aZombie->mRow == mRow)
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aOverlap = GetRectOverlap(aAttackRect, aZombieRect);
			if (aOverlap >= 20 || (aOverlap >= 0 && aZombie->mIsEating))
			{
				return aZombie;
			}
		}
	}

	return nullptr;
}

void Zombie::SquishAllInSquare(int theX, int theY, ZombieAttackType theAttackType)
{
	PlantsOnLawn aPlantsOnTile;
	mBoard->GetPlantsOnLawn(theX, theY, &aPlantsOnTile);
	Plant* anEphraim = aPlantsOnTile.mNormalPlant;
	if (theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER &&
		(mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT) &&
		!mFlatTires && !IsDeadOrDying() && anEphraim != nullptr &&
		anEphraim->mSeedType == SeedType::SEED_EPHRAIM && !anEphraim->NotOnGround())
	{
		// Take one eighth of maximum HP rather than an instant squash.
		// Resolve before the Pumpkin layer so it cannot hide his tire defense.
		const int aHealthBefore = anEphraim->mPlantHealth;
		anEphraim->mPlantHealth -= std::max(1, anEphraim->mPlantMaxHealth / 8);
		anEphraim->LogDamage(aHealthBefore, "vehicle_contact", this);
		anEphraim->mRecentlyEatenCountdown = 50;
		if (anEphraim->mPlantHealth <= 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
			anEphraim->Die();
		}
		mFlatTires = true;
		if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
			ZamboniDeath(1U << DamageFlags::DAMAGE_SPIKE);
		else
			CatapultDeath(1U << DamageFlags::DAMAGE_SPIKE);
		return;
	}
	Plant* aPlantLayers[] = {
		aPlantsOnTile.mPumpkinPlant,
		aPlantsOnTile.mNormalPlant,
		aPlantsOnTile.mUnderPlant,
		aPlantsOnTile.mFlyingPlant
	};
	for (Plant* aPlant : aPlantLayers)
	{
		if (aPlant == nullptr)
			continue;
		if (theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER && aPlant->IsSpiky())
			continue;
		if (aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
			continue;

		mBoard->mPlantsEaten++;
		const std::string_view aCause = mZombieType == ZombieType::ZOMBIE_BOSS ? "boss_fireball" :
			(mZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD ? "zombotany_jalapeno_explosion" :
				(theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER ? "vehicle_squash" : "gargantuar_squash"));
		aPlant->Squish(this, aCause);
		return;
	}
}

Rect Zombie::GetZombieRect()
{
	Rect aZombieRect = mZombieRect;
	if (IsWalkingBackwards())
	{
		aZombieRect.mX = mWidth - aZombieRect.mX - aZombieRect.mWidth;
	}

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	aZombieRect.Offset(mX, mY + aDrawPos.mBodyY);
	if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
	{
		aZombieRect.mHeight -= aDrawPos.mClipHeight;
		aZombieRect.mHeight = std::max(aZombieRect.mHeight, 0);
	}

	return aZombieRect;
}

Rect Zombie::GetZombieAttackRect()
{
	Rect aAttackRect = mZombieAttackRect;
	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP)
	{
		aAttackRect = Rect(-40, 0, 100, 115);
	}

	if (IsWalkingBackwards())
	{
		aAttackRect.mX = mWidth - aAttackRect.mX - aAttackRect.mWidth;
	}

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	aAttackRect.Offset(mX, mY + aDrawPos.mBodyY);
	if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
	{
		aAttackRect.mHeight -= aDrawPos.mClipHeight;
	}

	return aAttackRect;
}
