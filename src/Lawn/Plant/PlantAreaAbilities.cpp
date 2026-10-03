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

void Plant::IceZombies()
{
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->HitIceTrap();
	}

	mBoard->mIceTrapCounter = 300;
	PvzpParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mBoard->mPoolSparklyParticleID);
	if (aPoolSparklyParticle)
	{
		aPoolSparklyParticle->mDontUpdate = false;
	}

	Zombie* aBossZombie = mBoard->GetBossZombie();
	if (aBossZombie)
	{
		aBossZombie->BossDestroyFireball();
	}
}

void Plant::BurnRow(int theRow)
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == theRow) && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			aZombie->RemoveColdEffects();
			aZombie->ApplyBurn();
		}
	}

	for (GridItem* aGridItem : mBoard->mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridY == theRow && aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
		{
			aGridItem->DamageLadderByExplosion();
		}
	}

	Zombie* aBossZombie = mBoard->GetBossZombie();
	if (aBossZombie && aBossZombie->mFireballRow == theRow)
	{
		aBossZombie->BossDestroyIceballInRow();
	}
}

void Plant::BlowAwayFliers()
{
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead || aZombie->IsDeadOrDying() || !aZombie->IsOnBoard())
			continue;
		if (!mBoard->TakeSunMoney(BLOVER_ZOMBIE_KNOCKBACK_COST))
			break;

		// Preserve Blover's full balloon removal; push every other zombie 3 tiles back.
		if (aZombie->mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING)
			aZombie->mBlowingAway = true;
		else
			aZombie->mBloverKnockbackDistanceRemaining += BLOVER_ZOMBIE_KNOCKBACK_DISTANCE;
	}

	mApp->PlaySample(SOUND_BLOVER);
	mBoard->mFogBlownCountDown = 4000;
}

void Plant::KillAllPlantsNearDoom()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow == mRow && aPlant->mPlantCol == mPlantCol)
		{
			aPlant->Die();
		}
	}
}

void Plant::DoSpecial()
{
	int aPosX = mX + mWidth / 2;
	int aPosY = mY + mHeight / 2;
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

	switch (mSeedType)
	{
	case SeedType::SEED_BLOVER:
	{
		if (mState != PlantState::STATE_DOINGSPECIAL)
		{
			mState = PlantState::STATE_DOINGSPECIAL;
			BlowAwayFliers();
		}
		break;
	}
	case SeedType::SEED_CHERRYBOMB:
	{
		mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
		mApp->PlayFoley(FoleyType::FOLEY_JUICY);

		if (mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 115, 1, true, aDamageRangeFlags) >= 10)
			ReportAchievement::GiveAchievement(mApp, Explodonator, true);

		mApp->AddPvzpParticle(aPosX, aPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_POWIE);
		mBoard->ShakeBoard(3, -4);

		Die();
		break;
	}
	case SeedType::SEED_DOOMSHROOM:
	{
		mApp->PlaySample(SOUND_DOOMSHROOM);

		mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 250, 3, true, aDamageRangeFlags);
		KillAllPlantsNearDoom();

		mApp->AddPvzpParticle(aPosX, aPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_DOOM);
		mBoard->AddACrater(mPlantCol, mRow)->mGridItemCounter = 18000;
		mBoard->ShakeBoard(3, -4);

		Die();
		break;
	}
	case SeedType::SEED_JALAPENO:
	{
		mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
		mApp->PlayFoley(FoleyType::FOLEY_JUICY);

		mBoard->DoFwoosh(mRow);
		mBoard->ShakeBoard(3, -4);

		BurnRow(mRow);
		mBoard->mIceTimer[mRow] = 20;

		Die();
		break;
	}
	case SeedType::SEED_UMBRELLA:
	{
		if (mState != PlantState::STATE_UMBRELLA_TRIGGERED && mState != PlantState::STATE_UMBRELLA_REFLECTING)
		{
			mState = PlantState::STATE_UMBRELLA_TRIGGERED;
			mStateCountdown = 5;

			PlayBodyReanim("anim_block", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 22.0f);
		}

		break;
	}
	case SeedType::SEED_ICESHROOM:
	{
		mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
		IceZombies();
		mApp->AddPvzpParticle(aPosX, aPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_ICE_TRAP);

		Die();
		break;
	}
	case SeedType::SEED_POTATOMINE:
	{
		aPosX = mX + mWidth / 2 - 20;
		aPosY = mY + mHeight / 2;

		mApp->PlaySample(SOUND_POTATO_MINE);
		if (mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 60, 0, false, aDamageRangeFlags) >= 1)
			ReportAchievement::GiveAchievement(mApp, Spudow, true);

		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
		mApp->AddPvzpParticle(aPosX + 20.0f, aPosY, aRenderPosition, ParticleEffect::PARTICLE_POTATO_MINE);
		mBoard->ShakeBoard(3, -4);

		Die();
		break;
	}
	case SeedType::SEED_INSTANT_COFFEE:
	{
		Plant* aPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && aPlant->mIsAsleep)
		{
			aPlant->mWakeUpCounter = 100;
		}

		mState = PlantState::STATE_DOINGSPECIAL;
		PlayBodyReanim("anim_crumble", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 22.0f);
		mApp->PlayFoley(FoleyType::FOLEY_COFFEE);

		break;
	}
	default:
		break;
	}
}
