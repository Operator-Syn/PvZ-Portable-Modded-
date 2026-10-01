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

#include "PlantConstants.h"

void Plant::CobCannonFire(int theTargetX, int theTargetY)
{
	PVZP_ASSERT(mState == PlantState::STATE_COBCANNON_READY);

	mState = PlantState::STATE_COBCANNON_FIRING;
	mShootingCounter = 206;
	PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

	mTargetX = theTargetX - 47.0f;
	mTargetY = theTargetY;

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("CobCannon_Cob");
	aTrackInstance->mTrackColor = Color::White;
}

void Plant::Fire(Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon,
	int theWintermelonVolleyIndex, bool theFireWintermelonCherryBomb, bool theSkipCatTailOverdriveVolley,
	int theKernelPultVolleyIndex, int theKernelPultVolleySize, bool theMillionSunCatTailVolley,
	bool theTwoMillionSunCatTailVolley)
{
	if (mSeedType == SeedType::SEED_CATTAIL && mBoard->mCatTailOverdriveActive && !theSkipCatTailOverdriveVolley)
	{
		constexpr int aVolleySize = 6;
		bool aMillionSunTier = mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD;
		bool aTwoMillionSunTier = mBoard->mSunMoney >= TWO_MILLION_SUN_THRESHOLD;
		int aProjectileCost = aTwoMillionSunTier ? 750 : aMillionSunTier ? 375 : 125;
		if (theTargetZombie == nullptr || mBoard->mProjectiles.mMaxSize - mBoard->mProjectiles.mSize < aVolleySize ||
			mBoard->mSunMoney < aVolleySize * aProjectileCost)
			return;
		for (int i = 0; i < aVolleySize; i++)
		{
			if (!mBoard->TakeSunMoney(aProjectileCost))
				return;
			Fire(theTargetZombie, theRow, thePlantWeapon, -1, false, true, -1, 0, aMillionSunTier, aTwoMillionSunTier);
		}
		return;
	}

	if (mSeedType == SeedType::SEED_WINTERMELON && theWintermelonVolleyIndex == -1 && !theFireWintermelonCherryBomb)
	{
		if (mBoard->mSunMoney >= WINTER_MELON_QUADRATIC_DAMAGE_SUN_THRESHOLD)
		{
			if (theTargetZombie == nullptr)
				return;
			int aCherryBombChance = mPlantHealth > 1
				? WINTER_MELON_OVERDRIVE_SACRIFICE_CHERRY_CHANCE_PERCENT
				: WINTER_MELON_OVERDRIVE_BASE_CHERRY_CHANCE_PERCENT;
			if (Rand(100) < aCherryBombChance)
			{
				Fire(theTargetZombie, theRow, thePlantWeapon, -2, true);
				return;
			}

			if (mBoard->mProjectiles.mMaxSize - mBoard->mProjectiles.mSize < WINTER_MELON_OVERDRIVE_VOLLEY_SIZE ||
				!mBoard->TakeSunMoney(WINTER_MELON_OVERDRIVE_VOLLEY_SIZE * WINTER_MELON_OVERDRIVE_PROJECTILE_COST))
				return;

			std::vector<Zombie*> aVolleyTargets;
			aVolleyTargets.reserve(WINTER_MELON_OVERDRIVE_VOLLEY_SIZE);
			aVolleyTargets.push_back(theTargetZombie);
			while (static_cast<int>(aVolleyTargets.size()) < WINTER_MELON_OVERDRIVE_VOLLEY_SIZE)
			{
				Zombie* aNextTarget = FindTargetZombie(theRow, thePlantWeapon, &aVolleyTargets);
				if (aNextTarget == nullptr)
					break;
				aVolleyTargets.push_back(aNextTarget);
			}

			for (int i = 0; i < WINTER_MELON_OVERDRIVE_VOLLEY_SIZE; i++)
			{
				Fire(aVolleyTargets[static_cast<size_t>(i) % aVolleyTargets.size()], theRow, thePlantWeapon, i);
			}
			return;
		}

		int aSpecialShotRoll = Rand(10000);
		if (aSpecialShotRoll < 200)
		{
			Fire(theTargetZombie, theRow, thePlantWeapon, -2, true);
			return;
		}

		if (aSpecialShotRoll < 700)
		{
			std::vector<Zombie*> aVolleyTargets;
			if (theTargetZombie != nullptr)
				aVolleyTargets.push_back(theTargetZombie);
			while (aVolleyTargets.size() < 5)
			{
				Zombie* aNextTarget = FindTargetZombie(theRow, thePlantWeapon, &aVolleyTargets);
				if (aNextTarget == nullptr)
					break;
				aVolleyTargets.push_back(aNextTarget);
			}

			if (!aVolleyTargets.empty())
			{
				for (int i = 0; i < 5; i++)
				{
					Fire(aVolleyTargets[i % aVolleyTargets.size()], theRow, thePlantWeapon, i, false);
				}
				return;
			}
		}
	}

	if (mSeedType == SeedType::SEED_FUMESHROOM)
	{
		DoRowAreaDamage(20, 2U);
		mApp->PlayFoley(FoleyType::FOLEY_FUME);
		return;
	}
	if (mSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		int aMelonDamage = gProjectileDefinition[ProjectileType::PROJECTILE_MELON].mDamage;
		DoRowAreaDamage(std::max(aMelonDamage + 1, aMelonDamage * 3 / 2), 2U);
		return;
	}
	if (mSeedType == SeedType::SEED_STARFRUIT)
	{
		StarFruitFire();
		return;
	}

	ProjectileType aProjectileType;
	switch (mSeedType)
	{
	case SeedType::SEED_PEASHOOTER:
	case SeedType::SEED_REPEATER:
	case SeedType::SEED_THREEPEATER:
	case SeedType::SEED_SPLITPEA:
	case SeedType::SEED_GATLINGPEA:
	case SeedType::SEED_LEFTPEATER:
		aProjectileType = ProjectileType::PROJECTILE_PEA;
		break;
	case SeedType::SEED_SNOWPEA:
		aProjectileType = ProjectileType::PROJECTILE_SNOWPEA;
		break;
	case SeedType::SEED_PUFFSHROOM:
	case SeedType::SEED_SCAREDYSHROOM:
	case SeedType::SEED_SEASHROOM:
		aProjectileType = ProjectileType::PROJECTILE_PUFF;
		break;
	case SeedType::SEED_CACTUS:
	case SeedType::SEED_CATTAIL:
		aProjectileType = ProjectileType::PROJECTILE_SPIKE;
		break;
	case SeedType::SEED_CABBAGEPULT:
		aProjectileType = ProjectileType::PROJECTILE_CABBAGE;
		break;
	case SeedType::SEED_KERNELPULT:
		aProjectileType = ProjectileType::PROJECTILE_KERNEL;
		break;
	case SeedType::SEED_MELONPULT:
		aProjectileType = ProjectileType::PROJECTILE_MELON;
		break;
	case SeedType::SEED_WINTERMELON:
		aProjectileType = ProjectileType::PROJECTILE_WINTERMELON;
		break;
	case SeedType::SEED_COBCANNON:
		aProjectileType = ProjectileType::PROJECTILE_COBBIG;
		break;
	default:
		PVZP_ASSERT(false);
		break;
	}
	if (theFireWintermelonCherryBomb)
	{
		aProjectileType = ProjectileType::PROJECTILE_CHERRYBOMB;
	}
	if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
	{
		aProjectileType = ProjectileType::PROJECTILE_BUTTER;
	}
	if (mSeedType == SeedType::SEED_GATLINGPEA && mGatlingPeaVolleyProjectileType != ProjectileType::PROJECTILE_PEA)
	{
		aProjectileType = mGatlingPeaVolleyProjectileType;
	}

	if (mSeedType != SeedType::SEED_KERNELPULT || thePlantWeapon != PlantWeapon::WEAPON_SECONDARY || theKernelPultVolleyIndex <= 0)
		mApp->PlayFoley(FoleyType::FOLEY_THROW);
	if (mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_WINTERMELON)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SNOW_PEA_SPARKLES);
	}
	else if (mSeedType == SeedType::SEED_PUFFSHROOM || mSeedType == SeedType::SEED_SCAREDYSHROOM || mSeedType == SeedType::SEED_SEASHROOM)
	{
		mApp->PlayFoley(FoleyType::FOLEY_PUFF);
	}

	int aOriginX, aOriginY;
	if (mSeedType == SeedType::SEED_PUFFSHROOM)
	{
		aOriginX = mX + 40;
		aOriginY = mY + 40;
	}
	else if (mSeedType == SeedType::SEED_SEASHROOM)
	{
		aOriginX = mX + 45;
		aOriginY = mY + 63;
	}
	else if (mSeedType == SeedType::SEED_CABBAGEPULT)
	{
		aOriginX = mX + 5;
		aOriginY = mY - 12;
	}
	else if (mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
	{
		aOriginX = mX + 25;
		aOriginY = mY - 46;
	}
	else if (mSeedType == SeedType::SEED_CATTAIL)
	{
		aOriginX = mX + 20;
		aOriginY = mY - 3;
	}
	else if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_PRIMARY)
	{
		aOriginX = mX + 19;
		aOriginY = mY - 37;
	}
	else if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
	{
		aOriginX = mX + 12;
		aOriginY = mY - 56;
	}
	else if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_REPEATER)
	{
		int aOffsetX, aOffsetY;
		GetPeaHeadOffset(aOffsetX, aOffsetY);
		aOriginX = mX + aOffsetX + 24;
		aOriginY = mY + aOffsetY - 33;
	}
	else if (mSeedType == SeedType::SEED_LEFTPEATER)
	{
		int aOffsetX, aOffsetY;
		GetPeaHeadOffset(aOffsetX, aOffsetY);
		aOriginX = mX - aOffsetX + 27;
		aOriginY = mY + aOffsetY - 33;
	}
	else if (mSeedType == SeedType::SEED_GATLINGPEA)
	{
		int aOffsetX, aOffsetY;
		GetPeaHeadOffset(aOffsetX, aOffsetY);
		aOriginX = mX + aOffsetX + 34;
		aOriginY = mY + aOffsetY - 33;
	}
	else if (mSeedType == SeedType::SEED_SPLITPEA)
	{
		int aOffsetX, aOffsetY;
		GetPeaHeadOffset(aOffsetX, aOffsetY);
		aOriginY = mY + aOffsetY - 33;

		if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
		{
			aOriginX = mX + aOffsetX - 64;
		}
		else
		{
			aOriginX = mX + aOffsetX + 24;
		}
	}
	else if (mSeedType == SeedType::SEED_THREEPEATER)
	{
		aOriginX = mX + 45;
		aOriginY = mY + 10;
	}
	else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		aOriginX = mX + 29;
		aOriginY = mY + 21;
	}
	else if (mSeedType == SeedType::SEED_CACTUS)
	{
		if (thePlantWeapon == PlantWeapon::WEAPON_PRIMARY)
		{
			aOriginX = mX + 93;
			aOriginY = mY - 50;
		}
		else
		{
			aOriginX = mX + 70;
			aOriginY = mY + 23;
		}
	}
	else if (mSeedType == SeedType::SEED_COBCANNON)
	{
		aOriginX = mX - 44;
		aOriginY = mY - 184;
	}
	else
	{
		aOriginX = mX + 10;
		aOriginY = mY + 5;
	}
	if (mBoard->GetFlowerPotAt(mPlantCol, mRow))
	{
		aOriginY -= 5;
	}

	if (mSeedType == SeedType::SEED_SNOWPEA)
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
		mApp->AddPvzpParticle(aOriginX + 8, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_SNOWPEA_PUFF);
	}
	else if (mSeedType == SeedType::SEED_PUFFSHROOM)
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
		mApp->AddPvzpParticle(aOriginX + 18, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE);
	}
	else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
		mApp->AddPvzpParticle(aOriginX + 27, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE);
	}

	Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder - 1, theRow, aProjectileType);
	if (aProjectile == nullptr)
		return;
	if (mSeedType == SeedType::SEED_WINTERMELON && theFireWintermelonCherryBomb)
		aProjectile->mWintermelonCherryShot = true;
	if (mSeedType == SeedType::SEED_CATTAIL && theMillionSunCatTailVolley)
		aProjectile->mMillionSunDamage = true;
	if (mSeedType == SeedType::SEED_CATTAIL && theTwoMillionSunCatTailVolley)
		aProjectile->mTwoMillionSunCatTailDamage = true;
	if (mSeedType == SeedType::SEED_GATLINGPEA && aProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB)
	{
		aProjectile->mGatlingCherryShot = true;
		PvzpParticleSystem* aSmoke = mApp->AddPvzpParticle(aOriginX + 14, aOriginY + 14,
			aProjectile->mRenderOrder - 1, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
		if (aSmoke != nullptr)
		{
			aSmoke->OverrideScale(nullptr, 0.22f);
			aSmoke->OverrideColor(nullptr, Color(95, 86, 110, 150));
			AttachParticle(aProjectile->mAttachmentID, aSmoke, 14.0f, 14.0f);
		}
	}
	aProjectile->mDamageRangeFlags = theFireWintermelonCherryBomb ||
		(mSeedType == SeedType::SEED_GATLINGPEA && aProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB)
		? 127 : GetDamageRangeFlags(thePlantWeapon);
	if (mSeedType == SeedType::SEED_WINTERMELON && theWintermelonVolleyIndex >= 0)
	{
		constexpr int aVolleyXOffsets[] = { -24, -12, 0, 12, 24 };
		constexpr int aVolleyYOffsets[] = { -12, 12, 0, -12, 12 };
		aProjectile->mCobTargetX = static_cast<float>(aVolleyXOffsets[theWintermelonVolleyIndex]);
		aProjectile->mCobTargetRow = aVolleyYOffsets[theWintermelonVolleyIndex];
	}

	if (PlantRules::IsAutomaticPult(mSeedType))
	{
		float aRangeX, aRangeY;
		float aLaneOffsetY = 0.0f;
		if (theTargetZombie)
		{
			Rect aZombieRect = theTargetZombie->GetZombieRect();
			aRangeX = theTargetZombie->ZombieTargetLeadX(50.0f) - aOriginX - 30.0f;
			aRangeY = aZombieRect.mY - aOriginY;
			if (theTargetZombie->mZombieType != ZombieType::ZOMBIE_BOSS)
			{
				aLaneOffsetY = mBoard->GetPosYBasedOnRow(aOriginX, theTargetZombie->mRow) - mBoard->GetPosYBasedOnRow(aOriginX, mRow);
				aRangeY -= aLaneOffsetY;
				if (mSeedType == SeedType::SEED_WINTERMELON || theTargetZombie->mRow != mRow)
					aProjectile->mTargetZombieID = mBoard->ZombieGetID(theTargetZombie);
			}

			if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
			{
				aRangeX -= 60.0f;
			}
			if (theTargetZombie->mZombieType == ZombieType::ZOMBIE_POGO && theTargetZombie->mHasObject)
			{
				aRangeX -= 60.0f;
			}
			if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
			{
				aRangeX -= 40.0f;
			}
			if (theTargetZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			{
				aRangeY = mBoard->GridToPixelY(8, mRow) - aOriginY;
			}
		}
		else
		{
			aRangeX = 700.0f - aOriginX;
			aRangeY = 0.0f;
		}
		if (aRangeX < 40.0f && !(mSeedType == SeedType::SEED_WINTERMELON && theTargetZombie && aRangeX < 0.0f))
		{
			aRangeX = 40.0f;
		}
		if (mSeedType == SeedType::SEED_WINTERMELON && theWintermelonVolleyIndex >= 0)
		{
			aRangeX += aProjectile->mCobTargetX;
			aLaneOffsetY += static_cast<float>(aProjectile->mCobTargetRow);
		}
		if (mSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY &&
			theKernelPultVolleyIndex >= 0 && theKernelPultVolleySize > 1)
		{
			aRangeX += (theKernelPultVolleyIndex - (theKernelPultVolleySize - 1) / 2.0f) * 12.0f;
		}

		aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
		aProjectile->mVelX = aRangeX / 120.0f;
		aProjectile->mVelY = aLaneOffsetY / 120.0f;
		aProjectile->mVelZ = aRangeY / 120.0f - 7.0f;
		aProjectile->mAccZ = 0.115f;
	}
	else if (mSeedType == SeedType::SEED_THREEPEATER)
	{
		if (theRow < mRow)
		{
			aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
			aProjectile->mVelY = -3.0f;
			aProjectile->mShadowY += 80.0f;
		}
		else if (theRow > mRow)
		{
			aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
			aProjectile->mVelY = 3.0f;
			aProjectile->mShadowY -= 80.0f;
		}
	}
	else if (mSeedType == SeedType::SEED_PUFFSHROOM || mSeedType == SeedType::SEED_SEASHROOM)
	{
		aProjectile->mMotionType = ProjectileMotion::MOTION_PUFF;
	}
	else if (mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
	{
		aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
	}
	else if (mSeedType == SeedType::SEED_LEFTPEATER)
	{
		aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
	}
	else if (mSeedType == SeedType::SEED_CATTAIL)
	{
		aProjectile->mVelX = 2.0f;
		aProjectile->mMotionType = ProjectileMotion::MOTION_HOMING;
		aProjectile->mTargetZombieID = mBoard->ZombieGetID(theTargetZombie);
	}
	else if (mSeedType == SeedType::SEED_COBCANNON)
	{
		aProjectile->mVelX = 0.001f;
		aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
		aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
		aProjectile->mVelY = 0.0f;
		aProjectile->mAccZ = 0.0f;
		aProjectile->mVelZ = -8.0f;
		aProjectile->mCobTargetX = mTargetX - 40;
		aProjectile->mCobTargetRow = mBoard->PixelToGridYKeepOnBoard(mTargetX, mTargetY);
	}

	if (theFireWintermelonCherryBomb)
	{
		Reanimation* aCherryBombReanim = mApp->AddReanimation(aOriginX, aOriginY, aProjectile->mRenderOrder, ReanimationType::REANIM_CHERRYBOMB);
		aCherryBombReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
		aCherryBombReanim->mAnimRate = RandRangeFloat(10.0f, 15.0f);
		if (aCherryBombReanim->TrackExists("anim_idle"))
			aCherryBombReanim->SetFramesForLayer("anim_idle");
		AttachReanim(aProjectile->mAttachmentID, aCherryBombReanim, 0.0f, 0.0f);
	}
}
