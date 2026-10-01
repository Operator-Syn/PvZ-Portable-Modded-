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

static ZombieRules::ColdState ColdStateForZombie(Zombie& theZombie)
{
	return {
		.mType = theZombie.mZombieType,
		.mPhase = theZombie.mZombiePhase,
		.mSunAmount = theZombie.mBoard ? theZombie.mBoard->mZombieTierSunMoney : 0,
		.mStrengthTier = theZombie.mBoard ? theZombie.mBoard->mZombieStrengthTier : 0,
		.mHasSled = theZombie.IsBobsledTeamWithSled(),
		.mSpawnedByRain = theZombie.mSpawnedByZombieRain,
		.mSunTierInvulnerable = theZombie.IsSunTierInvulnerable(),
		.mDeadOrDying = theZombie.IsDeadOrDying(),
		.mMindControlled = theZombie.mMindControlled,
		.mFlying = theZombie.IsFlying(),
		.mBouncingPogo = theZombie.IsBouncingPogo()
	};
}

bool Zombie::IsImmobilizied()
{
	return mIceTrapCounter > 0 || mButteredCounter > 0;
}

bool Zombie::IsMovingAtChilledSpeed()
{
	if (mChilledCounter > 0)
		return true;

	if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		Zombie* aLeader;
		if (mZombieType == ZombieType::ZOMBIE_DANCER)
		{
			aLeader = this;
		}
		else
		{
			aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
		}

		if (aLeader)
		{
			if (aLeader->mChilledCounter > 0)
			{
				return true;
			}

			for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
			{
				Zombie* aDancer = mBoard->ZombieTryToGet(aLeader->mFollowerZombieID[i]);
				if (aDancer && aDancer->mChilledCounter > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool Zombie::CanBeChilled()
{
	return ZombieRules::CanBeChilled(ColdStateForZombie(*this));
}

bool Zombie::CanBeFrozen()
{
	return ZombieRules::CanBeFrozen(ColdStateForZombie(*this));
}

bool Zombie::CanBeTargetedByPlants(bool theIgnoreSunTierInvulnerability) const
{
	return (theIgnoreSunTierInvulnerability || !IsSunTierInvulnerable()) && (mZombieType != ZombieType::ZOMBIE_SNORKEL ||
		(!mInPool || mIsEating));
}

bool Zombie::IsSunTierInvulnerable() const
{
	return mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER && mBoard != nullptr &&
		mBoard->mZombieTierSunMoney >= TWO_MILLION_SUN_THRESHOLD && !mDolphinFirstLeapComplete;
}

bool Zombie::EffectedByDamage(unsigned int theDamageRangeFlags, bool theIgnoreSunTierInvulnerability)
{
	if (!TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_DYING)) && IsDeadOrDying())
	{
		return false;
	}
	if (!CanBeTargetedByPlants(theIgnoreSunTierInvulnerability))
		return false;

	if (TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_ONLY_MINDCONTROLLED)))
	{
		if (!mMindControlled)
		{
			return false;
		}
	}
	else if (mMindControlled)
	{
		return false;
	}

	if (mZombieType == ZombieType::ZOMBIE_BUNGEE && mZombiePhase != ZombiePhase::PHASE_BUNGEE_AT_BOTTOM && mZombiePhase != ZombiePhase::PHASE_BUNGEE_GRABBING)
	{
		return false;  // bungee zombies can only be hit while staying at the bottom
	}

	if (mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
	{
		return false;  // cannot be hit while being airdropped
	}

	if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_ENTER && aBodyReanim->mAnimTime < 0.5f)
		{
			return false;
		}
		if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_LEAVE && aBodyReanim->mAnimTime > 0.5f)
		{
			return false;
		}

		if (mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT &&
			mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT &&
			mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_SPIT)
		{
			return false;  // the boss can only be hit while its head is down
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_BOBSLED && GetBobsledPosition() > 0)
	{
		return false;  // while the bobsled exists, only the lead zombie can be hit
	}

	if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
	{
		return TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_OFF_GROUND));
	}

	if (mZombieType != ZombieType::ZOMBIE_BOBSLED && GetZombieRect().mX > mApp->mWidth)
	{
		return false;  // off-board zombies cannot be hit, except the bobsled team
	}

	bool submerged = mZombieType == ZombieType::ZOMBIE_SNORKEL && mInPool && !mIsEating;
	if (TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_SUBMERGED)) && submerged)
	{
		return true;
	}

	bool underground = mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING;
	if (TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_UNDERGROUND)) && underground)
	{
		return true;
	}

	if (TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_FLYING)) && IsFlying())
	{
		return true;
	}

	return TestBit(theDamageRangeFlags, static_cast<int>(DamageRangeFlags::DAMAGES_GROUND)) && !IsFlying() && !submerged && !underground;
}

bool Zombie::IsFlying()
{
	return mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING || mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING;
}

int Zombie::GetBobsledPosition()
{
	if (mZombieType != ZombieType::ZOMBIE_BOBSLED)
	{
		return -1;
	}

	if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL && mFollowerZombieID[0] == ZombieID::ZOMBIEID_NULL)
	{
		return -1;
	}

	if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
	{
		return 0;
	}

	ZombieID anId = mBoard->ZombieGetID(this);
	Zombie* aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
	for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
	{
		if (aLeaderZombie->mFollowerZombieID[i] == anId)
		{
			return i + 1;
		}
	}

	PVZP_ASSERT(false);

	unreachable();
}

bool Zombie::IsBobsledTeamWithSled()
{
	return GetBobsledPosition() != -1;
}

bool Zombie::IsDeadOrDying()
{
	return
		mDead ||
		mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
		mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED ||
		mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED;
}
