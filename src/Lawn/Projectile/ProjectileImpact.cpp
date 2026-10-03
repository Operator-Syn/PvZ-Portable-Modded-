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

#include "../Board/Board.h"
#include "../Plant/Plant.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../GameConstants.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>




#include "../Rules/TargetingRules.h"
#include "ProjectileRules.h"



void Projectile::PlayImpactSound(Zombie* theZombie)
{
	bool aPlayHelmSound = true;
	bool aPlaySplatSound = true;
	if (mProjectileType == ProjectileType::PROJECTILE_KERNEL)
	{
		mApp->PlayFoley(FoleyType::FOLEY_KERNEL_SPLAT);
		aPlayHelmSound = false;
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_BUTTER)
	{
		mApp->PlayFoley(FoleyType::FOLEY_BUTTER);
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL && IsSplashDamage(theZombie))
	{
		mApp->PlayFoley(FoleyType::FOLEY_IGNITE);
		aPlayHelmSound = false;
		aPlaySplatSound = false;
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON)
	{
		mApp->PlayFoley(FoleyType::FOLEY_MELONIMPACT);
		aPlaySplatSound = false;
	}

	if (aPlayHelmSound && theZombie)
	{
		if (theZombie->mHelmType == HELMTYPE_PAIL)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
			aPlaySplatSound = false;
		}
		else if (theZombie->mHelmType == HELMTYPE_TRAFFIC_CONE || theZombie->mHelmType == HELMTYPE_DIGGER || theZombie->mHelmType == HELMTYPE_FOOTBALL)
		{
			mApp->PlayFoley(FoleyType::FOLEY_PLASTIC_HIT);
		}
	}

	if (aPlaySplatSound)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
	}
}

void Projectile::DoImpact(Zombie* theZombie)
{
	Sexy::FrameProfileScope aProfileScope(Sexy::FrameProfileMetric::PROJECTILE_IMPACT, true);
	if (mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB ||
		mProjectileType == ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB ||
		mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB)
	{
		mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
		mApp->PlayFoley(FoleyType::FOLEY_JUICY);
		int aImpactX = static_cast<int>(mPosX + 40.0f);
		int aImpactY = static_cast<int>(mPosY + mPosZ + 40.0f);
		int aImpactRow = theZombie != nullptr ? theZombie->mRow : mRow;
		mBoard->KillAllZombiesInRadius(aImpactRow, aImpactX, aImpactY, 115, 1, true, mDamageRangeFlags, 0,
			mProjectileType == ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB ||
			mWintermelonCherryShot);
		mApp->AddPvzpParticle(aImpactX, aImpactY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_POWIE);
		mBoard->ShakeBoard(3, -4);
		if (mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB && mBoard->mSunMoney >= 50000)
		{
			mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
			mApp->PlayFoley(FoleyType::FOLEY_JUICY);
			mBoard->DoFwoosh(aImpactRow);
			mBoard->mIceTimer[aImpactRow] = 20;
			for (Zombie* aZombie : mBoard->mZombies)
			{
				if (!aZombie->mDead && (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == aImpactRow) &&
					aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
				{
					aZombie->RemoveColdEffects();
					aZombie->ApplyBurn();
				}
			}
			for (GridItem* aGridItem : mBoard->mGridItems)
			{
				if (!aGridItem->mDead && aGridItem->mGridY == aImpactRow && aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
					aGridItem->DamageLadderByExplosion();
			}
			Zombie* aBossZombie = mBoard->GetBossZombie();
			if (aBossZombie)
				aBossZombie->BossDestroyFireball();
		}
		Die();
		return;
	}

	if (mPiercesZombies && theZombie != nullptr && mProjectileType == ProjectileType::PROJECTILE_PEA &&
		mPiercedZombieCount < MAX_PIERCING_HITS)
	{
		mPiercedZombieIDs[mPiercedZombieCount++] = mBoard->ZombieGetID(theZombie);
	}

	if (mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW && mSniperCriticalArrow && theZombie != nullptr)
		mSniperPiercedZombieIDs.push_back(mBoard->ZombieGetID(theZombie));

	PlayImpactSound(theZombie);

	if (IsSplashDamage(theZombie))
	{
		if (mProjectileType == ProjectileType::PROJECTILE_FIREBALL && theZombie)
		{
			theZombie->RemoveColdEffects();
		}

		DoSplashDamage(theZombie);
	}
	else if (theZombie)
	{
		unsigned int aDamageFlags = GetDamageFlags(theZombie);
		int aDamage = GetProjectileDef().mDamage;
		if (mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW && mSniperCriticalArrow)
		{
			aDamage = aDamage * Plant::SNIPER_CRITICAL_DAMAGE_PERCENT / 100;
			// Include previous contacts so killing a target does not reduce this arrow's scaling.
			int aTargetCount = static_cast<int>(mSniperPiercedZombieIDs.size());
			const Rect anArrowRect = GetProjectileRect();
			for (Zombie* aTarget : mBoard->mZombies)
			{
				if (aTarget->mDead || (aTarget->mRow != mRow && aTarget->mZombieType != ZombieType::ZOMBIE_BOSS) ||
					!aTarget->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)) ||
					std::find(mSniperPiercedZombieIDs.begin(), mSniperPiercedZombieIDs.end(), mBoard->ZombieGetID(aTarget)) != mSniperPiercedZombieIDs.end())
					continue;
				if ((aTarget->mOnHighGround && CantHitHighGround()) ||
					(aTarget->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL && mPosZ <= 45.0f))
					continue;
				const Rect aTargetRect = aTarget->GetZombieRect();
				if (aTargetRect.mY + aTargetRect.mHeight < anArrowRect.mY ||
					aTargetRect.mY > anArrowRect.mY + anArrowRect.mHeight)
					continue;
				if ((mVelX >= 0.0f && aTargetRect.mX + aTargetRect.mWidth < anArrowRect.mX) ||
					(mVelX < 0.0f && aTargetRect.mX > anArrowRect.mX + anArrowRect.mWidth))
					continue;
				++aTargetCount;
			}
			aDamage *= mBoard->GetQuadraticZombieDamageMultiplier(theZombie, aTargetCount);
		}
		if (mProjectileType == ProjectileType::PROJECTILE_SPIKE && mMillionSunDamage)
			aDamage *= 5;
		if (mProjectileType == ProjectileType::PROJECTILE_SPIKE && mTwoMillionSunCatTailDamage)
		{
			int64_t aCurrentBodyHealth = std::max(0, theZombie->mBodyHealth);
			int aCurrentBodyHealthBonus = static_cast<int>((aCurrentBodyHealth * 15 + 999) / 1000);
			aDamage += std::max(50, aCurrentBodyHealthBonus);
		}
		if (mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW)
		{
			// Multiply the fully scaled attack; keep its snapshot through every piercing hit.
			const double aMovementMultiplier = 1.0 + static_cast<double>(mSniperMovementStacks) *
				Plant::SNIPER_MOVEMENT_DAMAGE_PERCENT_PER_STACK / 100.0;
			aDamage = static_cast<int>(std::min<double>(std::numeric_limits<int>::max(),
				std::ceil(aDamage * aMovementMultiplier)));
			aDamageFlags |= 1U << DamageFlags::DAMAGE_SNIPER_ARROW;
		}
		if (mProjectileType == ProjectileType::PROJECTILE_EPHRAIM_JAVELIN)
		{
			aDamage = Plant::GetEphraimHitDamage(theZombie, aDamage);
			SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_BYPASSES_SHIELD), false);
			SetBit(aDamageFlags, static_cast<int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY), false);
		}
		theZombie->TakeDamage(aDamage, aDamageFlags);
		if (mProjectileType == ProjectileType::PROJECTILE_EPHRAIM_JAVELIN)
			Plant::ApplyEphraimHitEffects(theZombie);
		if (mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW)
		{
			if (!theZombie->IsDeadOrDying() && !theZombie->IsSunTierInvulnerable() && theZombie->CanBeTargetedByPlants())
			{
				theZombie->mSniperWoundCounter = Zombie::SNIPER_WOUND_DURATION_TICKS;
				theZombie->ApplyHealingReduction(mSniperSourcePlantID, Zombie::SNIPER_HEALING_REDUCTION_PERCENT,
					Zombie::SNIPER_WOUND_DURATION_TICKS);
			}
			Plant* aSource = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mSniperSourcePlantID));
			if (aSource != nullptr && !aSource->mDead && !aSource->mSquished &&
				aSource->mSeedType == SeedType::SEED_SNIPER_FEMALE && aSource->mShootingCounter > 0)
			{
				const int aSet = aSource->mAnimPing ? 1 : 0;
				const int aElapsed = Plant::SNIPER_ATTACK_DURATION_TICKS[aSet] - aSource->mShootingCounter;
				if (Plant::SniperAttackFrame(aSet, aElapsed) >= Plant::SNIPER_ATTACK_RELEASE_FRAMES[aSet])
					aSource->mSniperHitStopCounter = std::max(aSource->mSniperHitStopCounter, Plant::SNIPER_HITSTOP_TICKS[aSet]);
			}
		}
		if (mProjectileType == ProjectileType::PROJECTILE_EPHRAIM_JAVELIN && mEphraimChargedJavelin &&
			!(theZombie->mZombieType == ZombieType::ZOMBIE_BALLOON &&
				(theZombie->IsFlying() || theZombie->mZombieHeight == ZombieHeight::HEIGHT_FALLING)) &&
			!theZombie->IsDeadOrDying() && theZombie->mZombieType != ZombieType::ZOMBIE_BOSS &&
			theZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE)
		{
			// Rear throws retain stagger without pushing zombies toward the house.
			if (mVelX > 0.0f)
			{
				// Queue displacement for the movement loop instead of teleporting on impact.
				theZombie->mEphraimKnockbackDistanceRemaining = std::clamp(
					theZombie->mEphraimKnockbackDistanceRemaining + Plant::EPHRAIM_JAVELIN_KNOCKBACK_DISTANCE, -BOARD_WIDTH, BOARD_WIDTH);
			}
			theZombie->mEphraimStaggerCounter = Plant::EPHRAIM_JAVELIN_STAGGER_TICKS;
			theZombie->UpdateAnimSpeed();
		}
	}

	float aLastPosX = mPosX - mVelX;
	float aLastPosY = mPosY + mPosZ - mVelY - mVelZ;
	ParticleEffect aEffect = ParticleEffect::PARTICLE_NONE;
	float aSplatPosX = mPosX + 12.0f;
	float aSplatPosY = mPosY + 12.0f;
	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_MELON:
		mApp->AddPvzpParticle(aLastPosX + 30.0f, aLastPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MELONSPLASH);
		break;
	case ProjectileType::PROJECTILE_WINTERMELON:
		mApp->AddPvzpParticle(aLastPosX + 30.0f, aLastPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_WINTERMELON);
		break;
	case ProjectileType::PROJECTILE_COBBIG:
	{
		int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, mCobTargetRow, 2);
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 40.0f, aRenderOrder, ParticleEffect::PARTICLE_BLASTMARK);
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 40.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_POPCORNSPLASH);
		mApp->PlaySample(SOUND_DOOMSHROOM);
		mBoard->ShakeBoard(3, -4);
		break;
	}
	case ProjectileType::PROJECTILE_PEA:
		aSplatPosX -= 15.0f;
		aEffect = ParticleEffect::PARTICLE_PEA_SPLAT;
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
		aSplatPosX -= 15.0f;
		aEffect = ParticleEffect::PARTICLE_SNOWPEA_SPLAT;
		break;
	case ProjectileType::PROJECTILE_FIREBALL:
	{
		if (IsSplashDamage(theZombie))
		{
			Reanimation* aFireReanim = mApp->AddReanimation(mPosX + 38.0f, mPosY - 20.0f, mRenderOrder + 1, ReanimationType::REANIM_JALAPENO_FIRE);
			aFireReanim->mAnimTime = 0.25f;
			aFireReanim->mAnimRate = 24.0f;
			aFireReanim->OverrideScale(0.7f, 0.4f);
		}
		break;
	}
	case ProjectileType::PROJECTILE_STAR:
		aEffect = ParticleEffect::PARTICLE_STAR_SPLAT;
		break;
	case ProjectileType::PROJECTILE_PUFF:
		aSplatPosX -= 20.0f;
		aEffect = ParticleEffect::PARTICLE_PUFF_SPLAT;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
		aSplatPosX = aLastPosX - 38.0f;
		aSplatPosY = aLastPosY + 23.0f;
		aEffect = ParticleEffect::PARTICLE_CABBAGE_SPLAT;
		break;
	case ProjectileType::PROJECTILE_BUTTER:
		aSplatPosX = aLastPosX - 20.0f;
		aSplatPosY = aLastPosY + 63.0f;
		aEffect = ParticleEffect::PARTICLE_BUTTER_SPLAT;

		if (theZombie)
		{
			theZombie->ApplyButter();
		}
		break;
	default:
		break;
	}

	if (aEffect != ParticleEffect::PARTICLE_NONE)
	{
		if (theZombie)
		{
			float aPosX = aSplatPosX + 52.0f - theZombie->mX;
			float aPosY = aSplatPosY - theZombie->mY;
			if (theZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL || theZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL)
			{
				aPosY += 60.0f;
			}
			if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
			{
				aPosX -= 80.0f;
			}
			else if (mPosX > theZombie->mX + 40 && mMotionType != ProjectileMotion::MOTION_LOBBED)
			{
				aPosX -= 60.0f;
			}

			aPosY = std::clamp(aPosY, 20.0f, 100.0f);
			theZombie->AddAttachedParticle(aPosX, aPosY, aEffect);
		}
		else
		{
			mApp->AddPvzpParticle(aSplatPosX, aSplatPosY, mRenderOrder + 1, aEffect);
		}
	}

	if (mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW && mSniperCriticalArrow)
		return;
	if (!mPiercesZombies || mProjectileType != ProjectileType::PROJECTILE_PEA || mPiercedZombieCount >= MAX_PIERCING_HITS)
		Die();
}
