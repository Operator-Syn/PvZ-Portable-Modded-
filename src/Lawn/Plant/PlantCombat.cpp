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

int Plant::GetDamageRangeFlags(PlantWeapon thePlantWeapon)
{
	switch (mSeedType)
	{
	case SeedType::SEED_CACTUS:
		return thePlantWeapon == PlantWeapon::WEAPON_SECONDARY ? 1 : 2;
	case SeedType::SEED_CHERRYBOMB:
	case SeedType::SEED_JALAPENO:
	case SeedType::SEED_COBCANNON:
	case SeedType::SEED_DOOMSHROOM:
		return 127;
	case SeedType::SEED_MELONPULT:
	case SeedType::SEED_CABBAGEPULT:
	case SeedType::SEED_KERNELPULT:
	case SeedType::SEED_WINTERMELON:
		return 13;
	case SeedType::SEED_POTATOMINE:
		return 77;
	case SeedType::SEED_SQUASH:
		return 13;
	case SeedType::SEED_PUFFSHROOM:
	case SeedType::SEED_SEASHROOM:
	case SeedType::SEED_FUMESHROOM:
	case SeedType::SEED_GLOOMSHROOM:
	case SeedType::SEED_CHOMPER:
	case SeedType::SEED_CHOMPERNUT:
		return 9;
	case SeedType::SEED_CATTAIL:
		return 11;
	case SeedType::SEED_TANGLEKELP:
		return 5;
	case SeedType::SEED_GIANT_WALLNUT:
		return 17;
	default:
		return 1;
	}
}

bool Plant::IsOnHighGround()
{
	return mBoard && mBoard->mGridSquareType[mPlantCol][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
}

void Plant::SpikyTakeDamage()
{
	SpikeweedAttack();

	mPlantHealth -= 50;
	if (mSeedType == SeedType::SEED_SPIKEROCK)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (mPlantHealth <= mPlantMaxHealth * 2 / 3)
			aBodyReanim->AssignRenderGroupToTrack("bigspike3", RENDER_GROUP_HIDDEN);
		if (mPlantHealth <= mPlantMaxHealth / 3)
			aBodyReanim->AssignRenderGroupToTrack("bigspike2", RENDER_GROUP_HIDDEN);
	}
	if (mPlantHealth <= 0)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
		Die();
	}
}

void Plant::GargantuarSmashTakeDamage()
{
	if (!IsTallNut())
		return;

	mPlantHealth -= 50;
	mRecentlyEatenCountdown = 50;
	if (mPlantHealth <= 0)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
		Die();
	}
}

bool Plant::IsSpiky()
{
	return mSeedType == SeedType::SEED_SPIKEWEED || mSeedType == SeedType::SEED_SPIKEROCK;
}

bool Plant::IsChomper() const
{
	return mSeedType == SeedType::SEED_CHOMPER || mSeedType == SeedType::SEED_CHOMPERNUT;
}

bool Plant::IsTallNut() const
{
	return mSeedType == SeedType::SEED_TALLNUT || mSeedType == SeedType::SEED_CHOMPERNUT;
}

void Plant::DoRowAreaDamage(int theDamage, unsigned int theDamageFlags)
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		int aDiffY = (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) ? 0 : (aZombie->mRow - mRow);
		if (mSeedType == SeedType::SEED_GLOOMSHROOM)
		{
			if (aDiffY < -4 || aDiffY > 4)
				continue;
		}
		else if (aDiffY)
			continue;

		if (aZombie->mOnHighGround == IsOnHighGround() && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			if (GetRectOverlap(aAttackRect, aZombieRect) > 0)
			{
				int aDamage = theDamage;
				if ((aZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || aZombie->mZombieType == ZombieType::ZOMBIE_CATAPULT) &&
					(TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE)))
				{
					aDamage = 1800;

					SpikyTakeDamage();
				}

				aZombie->TakeDamage(aDamage, theDamageFlags);
				mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
			}
		}
	}
}

PvzpParticleSystem* Plant::AddAttachedParticle(int thePosX, int thePosY, int theRenderPosition, ParticleEffect theEffect)
{
	PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(mParticleID);
	if (aParticle)
		aParticle->ParticleSystemDie();

	PvzpParticleSystem* aNewParticle = mApp->AddPvzpParticle(thePosX, thePosY, theRenderPosition, theEffect);
	if (aNewParticle)
		mParticleID = mApp->ParticleGetID(aNewParticle);

	return aNewParticle;
}

void Plant::RemoveEffects()
{
	mApp->RemoveParticle(mParticleID);
	mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mApp->RemoveReanimation(mBodyReanimID);
	mApp->RemoveReanimation(mHeadReanimID);
	mApp->RemoveReanimation(mHeadReanimID2);
	mApp->RemoveReanimation(mHeadReanimID3);
	mApp->RemoveReanimation(mLightReanimID);
	mApp->RemoveReanimation(mBlinkReanimID);
	mApp->RemoveReanimation(mSleepingReanimID);
}

void Plant::Squish()
{
	if (NotOnGround())
		return;

	if (!mIsAsleep)
	{
		if (mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO ||
			mSeedType == SeedType::SEED_DOOMSHROOM || mSeedType == SeedType::SEED_ICESHROOM)
		{
			DoSpecial();
			return;
		}
		else if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_NOTREADY)
		{
			DoSpecial();
			return;
		}
	}

	if (mSeedType == SeedType::SEED_SQUASH && mState != PlantState::STATE_NOTREADY)
		return;

	mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mRow, 8);
	mSquished = true;
	mDisappearCountdown = 500;
	mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
	RemoveEffects();

	GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
	if (aLadder)
	{
		aLadder->GridItemDie();
	}

	if (mApp->IsIZombieLevel())
	{
		mBoard->mChallenge->IZombiePlantDropRemainingSun(this);
	}
}
