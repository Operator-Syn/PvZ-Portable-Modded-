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
	case SeedType::SEED_EPHRAIM:
		return (1U << DamageRangeFlags::DAMAGES_GROUND) | (1U << DamageRangeFlags::DAMAGES_FLYING);
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

void Plant::LogDamage(int theHealthBefore, std::string_view theCause, Zombie* theSource, Projectile* theProjectile, bool theDestroyed)
{
	if (mBoard == nullptr)
		return;
	const int aEffectiveAfter = theDestroyed ? 0 : std::max(0, mPlantHealth);
	const int aDamage = std::max(0, theHealthBefore - aEffectiveAfter);
	if (aDamage == 0)
		return;
	const ZombieType aSourceType = theSource != nullptr ? theSource->mZombieType :
		(theProjectile != nullptr ? theProjectile->mSourceZombieType : ZombieType::ZOMBIE_INVALID);
	const unsigned int aSourceID = theSource != nullptr ? mBoard->mZombies.DataArrayGetID(theSource) :
		(theProjectile != nullptr ? static_cast<unsigned int>(theProjectile->mSourceZombieID) : 0U);
	const std::string aDetail = std::format("[damage] tick={} cause={} source_kind={} source=\"{}\" source_id={} source_type={} source_row={} "
		"projectile_id={} projectile_type={} target=\"{}\" target_id={} target_seed={} target_row={} target_col={} target_x={} target_y={} "
		"hp_before={} hp_after={} stored_hp_after={} hp_max={} hp_percent_before={:.2f} hp_percent_after={:.2f} damage={} raw_damage={} "
		"overkill={} destroyed={} squished={} sun_balance={}",
		mBoard->mMainCounter, theCause, theProjectile != nullptr ? "zombie_projectile" : (theSource != nullptr ? "zombie" : "effect"),
		aSourceType >= 0 && aSourceType < ZombieType::NUM_ZOMBIE_TYPES ? std::string_view(GetZombieDefinition(aSourceType).mZombieName) :
			(theProjectile != nullptr ? std::string_view("unrecorded_zombie") : theCause), aSourceID,
		static_cast<int>(aSourceType), theSource != nullptr ? theSource->mRow : -1,
		theProjectile != nullptr ? mBoard->mProjectiles.DataArrayGetID(theProjectile) : 0U,
		theProjectile != nullptr ? static_cast<int>(theProjectile->mProjectileType) : -1,
		GetNameString(mSeedType, mImitaterType), mBoard->mPlants.DataArrayGetID(this), static_cast<int>(mSeedType), mRow, mPlantCol, mX, mY,
		theHealthBefore, aEffectiveAfter, mPlantHealth, mPlantMaxHealth,
		mPlantMaxHealth > 0 ? 100.0f * theHealthBefore / mPlantMaxHealth : 0.0f,
		mPlantMaxHealth > 0 ? 100.0f * aEffectiveAfter / mPlantMaxHealth : 0.0f,
		aDamage, theDestroyed ? theHealthBefore : theHealthBefore - mPlantHealth, theDestroyed ? 0 : std::max(0, -mPlantHealth),
		theDestroyed || mPlantHealth <= 0, mSquished, mBoard->mSunMoney);
	Sexy::LogHealthAudit(std::format("damage:{}:{}:{}:{}", mBoard->mPlants.DataArrayGetID(this),
		aSourceID, static_cast<int>(aSourceType), theCause), aDetail, mBoard->mMainCounter, theHealthBefore,
		aEffectiveAfter, aDamage, 0, 0, 0.0f, theDestroyed || mPlantHealth <= 0);
}

void Plant::SpikyTakeDamage(Zombie* theSource)
{
	SpikeweedAttack();

	const int aHealthBefore = mPlantHealth;
	mPlantHealth -= 50;
	LogDamage(aHealthBefore, "spike_contact", theSource);
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

void Plant::GargantuarSmashTakeDamage(Zombie* theSource)
{
	if (!HasTallNutDefense())
		return;

	const int aHealthBefore = mPlantHealth;
	mPlantHealth -= 50;
	LogDamage(aHealthBefore, "gargantuar_smash", theSource);
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

bool Plant::HasTallNutDefense() const
{
	return IsTallNut() || mSeedType == SeedType::SEED_EPHRAIM;
}

int Plant::GetEphraimHitDamage(const Zombie* theZombie, int theBaseDamage, int theMultiplier)
{
	const int64_t aCurrentHealth = std::max(0, theZombie->mBodyHealth);
	const int64_t aBonusDamage = std::max<int64_t>(EPHRAIM_CURRENT_HP_DAMAGE_MINIMUM,
		(aCurrentHealth * EPHRAIM_CURRENT_HP_DAMAGE_PER_MILLE + 999) / 1000);
	int64_t aDamage = (static_cast<int64_t>(theBaseDamage) + aBonusDamage) * theMultiplier;
	if (theZombie->mBodyMaxHealth > 0 && aCurrentHealth * 100 <
		static_cast<int64_t>(theZombie->mBodyMaxHealth) * EPHRAIM_EXECUTE_HEALTH_PERCENT)
		aDamage *= EPHRAIM_EXECUTE_DAMAGE_MULTIPLIER;
	return static_cast<int>(std::clamp<int64_t>(aDamage, 0, std::numeric_limits<int>::max()));
}

void Plant::ApplyEphraimHitEffects(Zombie* theZombie)
{
	if (theZombie == nullptr || theZombie->IsDeadOrDying() || !theZombie->CanBeTargetedByPlants())
		return;
	if (theZombie->mZombieType == ZombieType::ZOMBIE_BALLOON &&
		theZombie->mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING)
	{
		theZombie->mFlyingHealth = 0;
		theZombie->LandFlyer(0U);
	}
	if (!theZombie->IsDeadOrDying() && !theZombie->mFlatTires &&
		(theZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || theZombie->mZombieType == ZombieType::ZOMBIE_CATAPULT) &&
		Rand(100) < EPHRAIM_TIRE_POP_CHANCE_PERCENT)
	{
		theZombie->mFlatTires = true;
		if (theZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI)
			theZombie->ZamboniDeath(1U << DamageFlags::DAMAGE_SPIKE);
		else
			theZombie->CatapultDeath(1U << DamageFlags::DAMAGE_SPIKE);
	}
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

					SpikyTakeDamage(aZombie);
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

void Plant::Squish(Zombie* theSource, std::string_view theCause)
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
	LogDamage(mPlantHealth, theCause, theSource, nullptr, true);
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
