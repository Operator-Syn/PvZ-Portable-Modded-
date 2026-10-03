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

Zombie* Plant::FindTargetZombie(int theRow, PlantWeapon thePlantWeapon, const std::vector<Zombie*>* theExcludedZombies, const int* theAttackTargetX, bool theMeleeOnly, const int* theAttackOriginX)
{
	Sexy::FrameProfileScope aProfileScope(Sexy::FrameProfileMetric::PLANT_TARGETING, true);
	int aDamageRangeFlags = GetDamageRangeFlags(thePlantWeapon);
	Rect aAttackRect = GetPlantAttackRect(thePlantWeapon);
	int aHighestWeight = 0;
	Zombie* aBestZombie = nullptr;
	Zombie* aLockedTarget = nullptr;
	bool aBestIsBalloon = false;
	int aBestLeftEdgeX = 0;
	float aBestCattailDistance = 0.0f;
	if (mSeedType == SeedType::SEED_CATTAIL)
	{
		aLockedTarget = mBoard->ZombieTryToGet(mCattailTargetZombieID);
		if (aLockedTarget != nullptr && (aLockedTarget->IsDeadOrDying() || !aLockedTarget->CanBeTargetedByPlants()))
			aLockedTarget = nullptr;
		if (aLockedTarget == nullptr)
			mCattailTargetZombieID = ZombieID::ZOMBIEID_NULL;
	}

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (theExcludedZombies != nullptr && std::find(theExcludedZombies->begin(), theExcludedZombies->end(), aZombie) != theExcludedZombies->end())
			continue;
		int aRowDeviation = aZombie->mRow - theRow;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
		{
			aRowDeviation = 0;
		}

		if (!aZombie->mHasHead || aZombie->IsTangleKelpTarget())
		{
			if (mSeedType == SeedType::SEED_POTATOMINE || IsChomper() || mSeedType == SeedType::SEED_TANGLEKELP)
			{
				continue;
			}
		}

		bool needPortalCheck = false;
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
		{
			if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_CACTUS || mSeedType == SeedType::SEED_REPEATER)
			{
				needPortalCheck = true;
			}
		}

		bool aCanTargetAcrossLanes = mSeedType == SeedType::SEED_CATTAIL || mSeedType == SeedType::SEED_PLANTERN ||
			PlantRules::IsAutomaticPult(mSeedType);
		if (!aCanTargetAcrossLanes)
		{
		if (mSeedType == SeedType::SEED_GLOOMSHROOM)
		{
			if (aRowDeviation < -4 || aRowDeviation > 4)
				{
					continue;
				}
			}
			else if (IsChomper())
			{
				if (aRowDeviation < -2 || aRowDeviation > 2)
					continue;
			}
			else if (needPortalCheck)
			{
				if (!mBoard->mChallenge->CanTargetZombieWithPortals(this, aZombie))
				{
					continue;
				}
			}
			else if (aRowDeviation)
			{
				continue;
			}
		}

		bool aTangleKelpCountersDolphinImmunity = mSeedType == SeedType::SEED_TANGLEKELP && aZombie->IsSunTierInvulnerable();
		if (aZombie->EffectedByDamage(aDamageRangeFlags, aTangleKelpCountersDolphinImmunity))
		{
			int aExtraRange = 0;

			if (IsChomper())
			{
				if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
				{
					aAttackRect.mX += 20;
					aAttackRect.mWidth -= 20;
				}

				if (aZombie->mZombiePhase == ZombiePhase::PHASE_POGO_BOUNCING || (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol == mPlantCol))
				{
					continue;
				}

				if (aZombie->mIsEating || mState == PlantState::STATE_CHOMPER_BITING)
				{
					aExtraRange = 60;
				}
			}

			if (mSeedType == SeedType::SEED_POTATOMINE)
			{
				if ((aZombie->mZombieType == ZombieType::ZOMBIE_POGO && aZombie->mHasObject) ||
					aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
				{
					continue;
				}

				if (aZombie->mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
				{
					aAttackRect.mX += 40;
					aAttackRect.mWidth -= 40;  // classic potato mine quirk; lets pole-vaulters set off the mine ("four pole-vaulters" trick)
				}

				if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol != mPlantCol)
				{
					continue;
				}

				if (aZombie->mIsEating)
				{
					aExtraRange = 30;
				}
			}

			if ((mSeedType == SeedType::SEED_EXPLODE_O_NUT && aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT) ||
				(mSeedType == SeedType::SEED_TANGLEKELP && !aZombie->mInPool))
			{
				continue;
			}

			Rect aZombieRect = aZombie->GetZombieRect();
			if (mSeedType == SeedType::SEED_EPHRAIM && theMeleeOnly &&
				std::abs(aZombieRect.mX + aZombieRect.mWidth / 2 - (theAttackOriginX ? *theAttackOriginX : mX + mWidth / 2)) > EPHRAIM_ATTACK_RANGE_FRONT)
				continue;
			if (mSeedType == SeedType::SEED_EPHRAIM && ((mShootingCounter > 0 && theAttackOriginX == nullptr) || theAttackTargetX != nullptr))
			{
				const int aPlantCenterX = theAttackOriginX ? *theAttackOriginX : mX + mWidth / 2;
				const int aZombieCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
				const bool aAttackingLeft = (theAttackTargetX ? *theAttackTargetX : mTargetX) < aPlantCenterX;
				if ((aZombieCenterX < aPlantCenterX) != aAttackingLeft)
					continue;
			}
			if (!needPortalCheck && GetRectOverlap(aAttackRect, aZombieRect) < -aExtraRange)
			{
				continue;
			}

			int aWeight = -aZombieRect.mX;
			if (mSeedType == SeedType::SEED_EPHRAIM)
			{
				const int aPlantCenterX = theAttackOriginX ? *theAttackOriginX : mX + mWidth / 2;
				const int aZombieCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
				aWeight = -std::abs(aZombieCenterX - aPlantCenterX);
			}
			if (aCanTargetAcrossLanes)
			{
				aWeight = -Distance2D(mX + 40.0f, mY + 40.0f, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2);
				if (mSeedType == SeedType::SEED_KERNELPULT && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
				{
					aWeight += 10000;
				}
			}

			if (mSeedType == SeedType::SEED_CATTAIL)
			{
				const bool anIsBalloon = aZombie->mZombieType == ZombieType::ZOMBIE_BALLOON;
				const float aCattailDistance = Distance2D(mX + 40.0f, mY + 40.0f,
					aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2);
				bool aIsHigherPriority = aBestZombie == nullptr || TargetingRules::HigherCattailPriority(
					{anIsBalloon, aZombieRect.mX, aCattailDistance},
					{aBestIsBalloon, aBestLeftEdgeX, aBestCattailDistance});
				if (aIsHigherPriority)
				{
					aBestZombie = aZombie;
					aBestIsBalloon = anIsBalloon;
					aBestLeftEdgeX = aZombieRect.mX;
					aBestCattailDistance = aCattailDistance;
				}
			}
			else if (aBestZombie == nullptr || aWeight > aHighestWeight)
			{
				aHighestWeight = aWeight;
				aBestZombie = aZombie;
			}
		}
	}

	if (mSeedType == SeedType::SEED_CATTAIL && aBestZombie == nullptr)
		aBestZombie = aLockedTarget;
	if (mSeedType == SeedType::SEED_CATTAIL && aBestZombie != nullptr)
		mCattailTargetZombieID = mBoard->ZombieGetID(aBestZombie);
	return aBestZombie;
}

int Plant::DistanceToClosestZombie()
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
	int aClosestDistance = 1000;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mRow == mRow && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aDistance = -GetRectOverlap(aAttackRect, aZombieRect);
			if (aDistance < aClosestDistance)
			{
				aClosestDistance = std::max(aDistance, 0);
			}
		}
	}

	return aClosestDistance;
}

Rect Plant::GetPlantRect()
{
	Rect aRect;
	if (IsTallNut())
	{
		aRect = Rect(mX + 10, mY, mWidth, mHeight);
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aRect = Rect(mX, mY, mWidth - 20, mHeight);
	}
	else if (mSeedType == SeedType::SEED_COBCANNON)
	{
		aRect = Rect(mX, mY, 140, 80);
	}
	else
	{
		aRect = Rect(mX + 10, mY, mWidth - 20, mHeight);
	}

	return aRect;
}

Rect Plant::GetPlantAttackRect(PlantWeapon thePlantWeapon)
{
	Rect aRect;
	if (mApp->IsWallnutBowlingLevel())
	{
		aRect = Rect(mX, mY, mWidth - 20, mHeight);
	}
	else if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY && mSeedType == SeedType::SEED_SPLITPEA)
	{
		aRect = Rect(0, mY, mX + 16, mHeight);
	}
	else switch (mSeedType)
	{
	case SeedType::SEED_LEFTPEATER:     aRect = Rect(0,             mY,             mX,                 mHeight);               break;
	case SeedType::SEED_SQUASH:         aRect = Rect(mX + 20,       mY,             mWidth - 35,        mHeight);               break;
	case SeedType::SEED_CHOMPER:
	case SeedType::SEED_CHOMPERNUT:     aRect = Rect(mX + 80,       mY - 160,        280,                mHeight + 320);        break;
	// Endless Pool extends the logical playfield beyond the stock board width.
	case SeedType::SEED_EPHRAIM:        aRect = Rect(0, mY, mApp->mWidth, mHeight); break;
	case SeedType::SEED_SNIPER_FEMALE:  aRect = Rect(mX, mY, mApp->mWidth - mX, mHeight); break;
	case SeedType::SEED_SPIKEWEED:
	case SeedType::SEED_SPIKEROCK:      aRect = Rect(mX + 20,       mY,             mWidth - 50,        mHeight);               break;
	case SeedType::SEED_POTATOMINE:     aRect = Rect(mX,            mY,             mWidth - 25,        mHeight);               break;
	case SeedType::SEED_TORCHWOOD:      aRect = Rect(mX + 50,       mY,             30,                 mHeight);               break;
	case SeedType::SEED_PUFFSHROOM:
	case SeedType::SEED_SEASHROOM:      aRect = Rect(mX + 60,       mY,             230,                mHeight);               break;
	case SeedType::SEED_FUMESHROOM:     aRect = Rect(mX + 60,       mY,             340,                mHeight);               break;
	case SeedType::SEED_GLOOMSHROOM:    aRect = Rect(mX - 320,      mY - 320,        720,                720);                   break;
	case SeedType::SEED_TANGLEKELP:     aRect = Rect(mX,            mY,             mWidth,             mHeight);               break;
	case SeedType::SEED_WINTERMELON:    aRect = Rect(-mApp->mWidth, -mApp->mHeight, mApp->mWidth * 2, mApp->mHeight * 2); break;
	case SeedType::SEED_CABBAGEPULT:
	case SeedType::SEED_KERNELPULT:
	case SeedType::SEED_MELONPULT:      aRect = Rect(mX + 60, -mApp->mHeight, mApp->mWidth, mApp->mHeight * 2); break;
	case SeedType::SEED_CATTAIL:
	case SeedType::SEED_PLANTERN:       aRect = Rect(-mApp->mWidth, -mApp->mHeight, mApp->mWidth * 2, mApp->mHeight * 2); break;
	default:                            aRect = Rect(mX + 60,       mY,             mApp->mWidth,        mHeight);               break;
	}

	return aRect;
}
