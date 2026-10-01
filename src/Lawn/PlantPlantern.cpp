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

#include "Coin.h"
#include "Plant.h"
#include "Board.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "ZenGarden.h"
#include "Challenge.h"
#include "Projectile.h"
#include "SeedPacket.h"
#include "../LawnApp.h"
#include "CursorObject.h"
#include "../GameConstants.h"
#include <array>
#include "System/PlayerInfo.h"
#include "System/ReanimationLawn.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "TargetingRules.h"

bool Plant::HasPlanternTarget()
{
	if (mSeedType != SeedType::SEED_PLANTERN || mTargetZombieID == ZombieID::ZOMBIEID_NULL)
		return false;

	Zombie* aTarget = mBoard->ZombieTryToGet(mTargetZombieID);
	return aTarget && !aTarget->IsDeadOrDying() && aTarget->EffectedByDamage(127U);
}

void Plant::FirePlanternBomb(Zombie* theTarget)
{
	if (!theTarget)
		return;

	mApp->PlayFoley(FoleyType::FOLEY_THROW);
	int aOriginX = mX + 40;
	int aOriginY = mY + 22;
	Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY,
		Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, theTarget->mRow, 0), theTarget->mRow,
		ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB);
	if (aProjectile == nullptr)
		return;
	aProjectile->mMotionType = ProjectileMotion::MOTION_HOMING;
	aProjectile->mWidth = 80;
	aProjectile->mHeight = 80;
	Rect aTargetRect = theTarget->GetZombieRect();
	SexyVector2 aInitialDirection(theTarget->ZombieTargetLeadX(0.0f) - (aOriginX + aProjectile->mWidth / 2.0f),
		aTargetRect.mY + aTargetRect.mHeight / 2.0f - (aOriginY + aProjectile->mHeight / 2.0f));
	float aDirectionLength = std::sqrt(aInitialDirection.x * aInitialDirection.x + aInitialDirection.y * aInitialDirection.y);
	if (aDirectionLength > 0.0f)
	{
		aProjectile->mVelX = aInitialDirection.x * 2.0f / aDirectionLength;
		aProjectile->mVelY = aInitialDirection.y * 2.0f / aDirectionLength;
	}
	else
	{
		aProjectile->mVelX = 2.0f;
		aProjectile->mVelY = 0.0f;
	}
	aProjectile->mTargetZombieID = mBoard->ZombieGetID(theTarget);
	aProjectile->mDamageRangeFlags = 127;

	Reanimation* aCherryBombReanim = mApp->AddReanimation(aOriginX, aOriginY, aProjectile->mRenderOrder, ReanimationType::REANIM_CHERRYBOMB);
	aCherryBombReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aCherryBombReanim->mAnimRate = 10.0f;
	aCherryBombReanim->OverrideScale(0.5f, 0.5f);
	if (aCherryBombReanim->TrackExists("anim_idle"))
		aCherryBombReanim->SetFramesForLayer("anim_idle");
	AttachReanim(aProjectile->mAttachmentID, aCherryBombReanim, 0.0f, 0.0f);
}

bool Plant::FirePlanternCobBomb(Zombie* theTarget, bool theAutoCoffeeBean)
{
	if (!theTarget)
		return false;
	int aOriginX = mX - 44;
	int aOriginY = mY - 184;
	Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY,
		Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, theTarget->mRow, 0), theTarget->mRow,
		ProjectileType::PROJECTILE_COBBIG);
	if (!aProjectile)
		return false;
	aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
	aProjectile->mVelX = 0.001f;
	aProjectile->mVelY = 0.0f;
	aProjectile->mVelZ = -8.0f;
	aProjectile->mAccZ = 0.0f;
	aProjectile->mCobTargetX = theTarget->ZombieTargetLeadX(120.0f) - 30.0f;
	aProjectile->mCobTargetRow = theTarget->mRow;
	aProjectile->mPlanternCob = true;
	aProjectile->mPlanternAutoCoffeeBean = theAutoCoffeeBean;
	aProjectile->mDamageRangeFlags = 127;
	mApp->PlayFoley(FoleyType::FOLEY_THROW);
	return true;
}

void Plant::UpdatePlanternAttack()
{
	if (!IsInPlay() || mSeedType != SeedType::SEED_PLANTERN || mSquished ||
		mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE)
		return;

	Zombie* aTarget = nullptr;
	if (HasPlanternTarget())
	{
		aTarget = mBoard->ZombieTryToGet(mTargetZombieID);
	}
	else
	{
		aTarget = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
		mTargetZombieID = aTarget ? mBoard->ZombieGetID(aTarget) : ZombieID::ZOMBIEID_NULL;
	}

	if (!aTarget)
	{
		mTargetX = -1;
		mTargetY = -1;
		if (mStateCountdown == 0)
			mStateCountdown = 30;
		return;
	}

	Rect aTargetRect = aTarget->GetZombieRect();
	mTargetX = static_cast<int>(aTarget->ZombieTargetLeadX(120.0f));
	mTargetY = aTargetRect.mY + aTargetRect.mHeight / 2;

	if (mStateCountdown == 0)
	{
		bool aMillionSunTier = mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD;
		int aSunCost = aMillionSunTier ? 300 : 150;
		if (mBoard->mSunMoney < 1000 || !mBoard->TakeSunMoney(aSunCost))
		{
			mStateCountdown = 30;
			return;
		}

		FirePlanternBomb(aTarget);
		int aAttackInterval = aMillionSunTier ? 500 : 1000;
		if (mBoard->mSunMoney >= 1000)
			aAttackInterval = std::max(1, aAttackInterval * 2 / 5);
		if (mBoard->mSunMoney >= 50000)
			aAttackInterval = std::max(1, aAttackInterval * 2 / 5);
		if (aMillionSunTier)
			aAttackInterval = std::max(1, aAttackInterval * 2 / 5);
		int aPlanternMaxSpeedInterval = GetPlantDefinition(SeedType::SEED_PEASHOOTER).mLaunchRate * 3 / 2;
		mStateCountdown = std::max(aAttackInterval, aPlanternMaxSpeedInterval);
	}
}

bool Plant::PlanternCoffeeBeanVolley(bool theAutoCoffeeBean)
{
	if (mSeedType != SeedType::SEED_PLANTERN)
		return false;
	std::array<Zombie*, MAX_GRID_SIZE_Y> aTargets{};
	int aLaneCount = 0;
	for (int aRow = 0; aRow < mBoard->GetNumPlayableRows(); aRow++)
	{
		Zombie* aNearest = nullptr;
		for (Zombie* aZombie : mBoard->mZombies)
		{
			if (aZombie->mDead || aZombie->IsDeadOrDying() || aZombie->mRow != aRow || !aZombie->EffectedByDamage(127U))
				continue;
			if (aNearest == nullptr || aZombie->mPosX < aNearest->mPosX)
				aNearest = aZombie;
		}
		if (aNearest)
			aTargets[aLaneCount++] = aNearest;
	}
	int aSunCostPerLane = mBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 1000 : 500;
	if (aLaneCount == 0 || mBoard->mProjectiles.mMaxSize - mBoard->mProjectiles.mSize < static_cast<unsigned int>(aLaneCount) ||
		(!theAutoCoffeeBean && !mBoard->TakeSunMoney(aLaneCount * aSunCostPerLane)))
		return false;
	for (int i = 0; i < aLaneCount; i++)
		FirePlanternCobBomb(aTargets[i], theAutoCoffeeBean);
	if (!theAutoCoffeeBean)
		mStateCountdown = 2500;
	return true;
}
