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

void Plant::UpdateGraveBuster()
{
	if (mState == PlantState::STATE_GRAVEBUSTER_LANDING)
	{
		if (mApp->ReanimationGet(mBodyReanimID)->mLoopCount > 0)
		{
			PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 10, 12.0f);
			mStateCountdown = 400;
			mState = PlantState::STATE_GRAVEBUSTER_EATING;
			AddAttachedParticle(mX + 40, mY + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER);
		}
	}
	else if (mState == PlantState::STATE_GRAVEBUSTER_EATING && mStateCountdown == 0)
	{
		GridItem* aGraveStone = mBoard->GetGraveStoneAt(mPlantCol, mRow);
		if (aGraveStone)
		{
			aGraveStone->GridItemDie();
			mBoard->mGravesCleared++;
		}

		mApp->AddPvzpParticle(mX + 40, mY + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE);
		Die();
		mBoard->DropLootPiece(mX + 40, mY, 12);
	}
}

void Plant::UpdatePotato()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

	if (mState == PlantState::STATE_NOTREADY)
	{
		if (mStateCountdown == 0)
		{
			mApp->AddPvzpParticle(mX + mWidth / 2, mY + mHeight / 2, mRenderOrder, ParticleEffect::PARTICLE_POTATO_MINE_RISE);
			PlayBodyReanim("anim_rise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
			mState = PlantState::STATE_POTATO_RISING;
			mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
		}
	}
	else if (mState == PlantState::STATE_POTATO_RISING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			float aRate = RandRangeFloat(12.0f, 15.0f);
			PlayBodyReanim("anim_armed", ReanimLoopType::REANIM_LOOP, 0, aRate);

			Reanimation* aLightReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, GetPlantDefinition(mSeedType).mReanimationType);
			aLightReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aLightReanim->mAnimRate = aRate - 2.0f;
			aLightReanim->SetFramesForLayer("anim_glow");
			aLightReanim->mFrameCount = 10;
			aLightReanim->ShowOnlyTrack("anim_glow");
			aLightReanim->SetTruncateDisappearingFrames("anim_glow", false);
			mLightReanimID = mApp->ReanimationGetID(aLightReanim);
			aLightReanim->AttachToAnotherReanimation(aBodyReanim, "anim_light");

			mState = PlantState::STATE_POTATO_ARMED;
			mBlinkCountdown = 400 + Sexy::Rand(4000);
		}
	}
	else if (mState == PlantState::STATE_POTATO_ARMED)
	{
		if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
		{
			DoSpecial();
		}
		else
		{
			Reanimation* aLightReanim = mApp->ReanimationTryToGet(mLightReanimID);
			if (aLightReanim)
			{
				aLightReanim->mFrameCount = PvzpAnimateCurve(200, 50, DistanceToClosestZombie(), 10, 3, PvzpCurves::CURVE_LINEAR);
			}
		}
	}
}

void Plant::UpdateTanglekelp()
{
	if (mState != PlantState::STATE_TANGLEKELP_GRABBING)
	{
		Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
		if (aZombie)
		{
			mApp->PlayFoley(FoleyType::FOLEY_FLOOP);
			mState = PlantState::STATE_TANGLEKELP_GRABBING;
			mStateCountdown = 100;
			aZombie->PoolSplash(false);

			float aVinesPosX = -13.0f;
			float aVinesPosY = 15.0f;
			if (aZombie->mZombieType == ZombieType::ZOMBIE_SNORKEL)
			{
				aVinesPosX = -43.0f;
				aVinesPosY = 55.0f;
			}
			if (aZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
			{
				aVinesPosX = -20.0f;
				aVinesPosY = 37.0f;
			}
			Reanimation* aGrabReanim = aZombie->AddAttachedReanim(aVinesPosX, aVinesPosY, ReanimationType::REANIM_TANGLEKELP);
			if (aGrabReanim)
			{
				aGrabReanim->SetFramesForLayer("anim_grab");
				aGrabReanim->mAnimRate = 24.0f;
				aGrabReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
			}

			mTargetZombieID = mBoard->ZombieGetID(aZombie);
		}
	}
	else
	{
		if (mStateCountdown == 50)
		{
			Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
			if (aZombie)
			{
				aZombie->DragUnder();
				aZombie->PoolSplash(false);
			}
		}

		if (mStateCountdown == 20)
		{
			int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
			Reanimation* aSplashReanim = mApp->AddReanimation(mX - 23, mY + 7, aRenderPosition, ReanimationType::REANIM_SPLASH);
			aSplashReanim->OverrideScale(1.3f, 1.3f);

			mApp->AddPvzpParticle(mX + 31, mY + 64, aRenderPosition, ParticleEffect::PARTICLE_PLANTING_POOL);
			mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
		}

		if (mStateCountdown == 0)
		{
			Die();

			Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
			if (aZombie)
			{
				aZombie->DieWithLoot();
			}
		}
	}
}

void Plant::SpikeweedAttack()
{
	PVZP_ASSERT(IsSpiky());

	if (mState != PlantState::STATE_SPIKEWEED_ATTACKING)
	{
		bool aOverdrive = mBoard->mSpikeweedOverdriveActive;
		PlayBodyReanim("anim_attack", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aOverdrive ? 126.0f : 18.0f);
		mApp->PlaySample(SOUND_THROW);

		mState = PlantState::STATE_SPIKEWEED_ATTACKING;
		mStateCountdown = aOverdrive ? 13 : 100;
	}
}

void Plant::UpdateSpikeweed()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_SPIKEWEED_ATTACKING)
	{
		if (mStateCountdown == 0)
		{
			if (mBoard->mSpikeweedOverdriveActive)
			{
				const int aHealthBefore = mPlantHealth;
				mPlantHealth -= mSeedType == SeedType::SEED_SPIKEROCK ? 2 : 1;
				LogDamage(aHealthBefore, "spikeweed_overdrive_attack_cost");
				if (mPlantHealth <= 0)
				{
					Die();
					return;
				}
			}
			mState = PlantState::STATE_NOTREADY;
		}
		else if (mSeedType == SeedType::SEED_SPIKEROCK)
		{
			if (mBoard->mSpikeweedOverdriveActive
				? (mStateCountdown == 10 || mStateCountdown == 5)
				: (mStateCountdown == 70 || mStateCountdown == 32))
			{
				DoRowAreaDamage(20, 33U);
			}
		}
		else if (mStateCountdown == (mBoard->mSpikeweedOverdriveActive ? 10 : 75))
		{
			DoRowAreaDamage(20, 33U);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			PlayIdleAnim(RandRangeFloat(12.0f, 15.0f));
		}
	}
	else if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
	{
		SpikeweedAttack();
	}
}

void Plant::UpdateScaredyShroom()
{
	if (mShootingCounter > 0)
		return;

	bool aHasZombieNearby = false;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		Rect aZombieRect = aZombie->GetZombieRect();
		int aDiffY = (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) ? 0 : (aZombie->mRow - mRow);
		if (!aZombie->mMindControlled && !aZombie->IsDeadOrDying() && aDiffY <= 1 && aDiffY >= -1 && GetCircleRectOverlap(mX, mY + 20.0f, 120, aZombieRect))
		{
			aHasZombieNearby = true;
			break;
		}
	}

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_READY)
	{
		if (aHasZombieNearby)
		{
			mState = PlantState::STATE_SCAREDYSHROOM_LOWERING;
			PlayBodyReanim("anim_scared", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 10.0f);
		}
	}
	else if (mState == PlantState::STATE_SCAREDYSHROOM_LOWERING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_SCAREDYSHROOM_SCARED;
			PlayBodyReanim("anim_scaredidle", ReanimLoopType::REANIM_LOOP, 10, 0.0f);
		}
	}
	else if (mState == PlantState::STATE_SCAREDYSHROOM_SCARED)
	{
		if (!aHasZombieNearby)
		{
			mState = PlantState::STATE_SCAREDYSHROOM_RAISING;

			float aAnimRate = RandRangeFloat(7.0f, 12.0f);
			PlayBodyReanim("anim_grow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, aAnimRate);
		}
	}
	else if (mState == PlantState::STATE_SCAREDYSHROOM_RAISING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_READY;

			float aAnimRate = RandRangeFloat(10.0f, 15.0f);
			PlayIdleAnim(aAnimRate);
		}
	}

	if (mState != PlantState::STATE_READY)
	{
		mLaunchCounter = mLaunchRate;
	}
}

void Plant::UpdateTorchwood()
{
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

	for (Projectile* aProjectile : mBoard->mProjectiles)
	{
		if (aProjectile->mDead)
			continue;
		if ((aProjectile->mRow == mRow) &&
			(aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA || aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA))
		{
			Rect aProjectileRect = aProjectile->GetProjectileRect();
			if (GetRectOverlap(aAttackRect, aProjectileRect) >= 10)
			{
				if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA)
				{
					aProjectile->ConvertToFireball(mPlantCol);
				}
				else if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
				{
					aProjectile->ConvertToPea(mPlantCol);
				}
			}
		}
	}
}

void Plant::DoSquashDamage()
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mRow == mRow || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			if (GetRectOverlap(aAttackRect, aZombieRect) > (aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ? -20 : 0))
			{
				aZombie->TakeDamage(1800, 18U);
			}
		}
	}
}

Zombie* Plant::FindSquashTarget()
{
	int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
	Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

	int aClosestRange = 0;
	Zombie* aClosestZombie = nullptr;

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mRow == mRow || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) &&
			aZombie->mHasHead && !aZombie->IsTangleKelpTarget() && aZombie->EffectedByDamage(aDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();

			if ((
					aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT && aZombieRect.mX < mX + 20
				) || (
					aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT &&
					aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_IN_VAULT &&
					aZombie->mZombiePhase != ZombiePhase::PHASE_SNORKEL_INTO_POOL &&
					aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_INTO_POOL &&
					aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_RIDING &&
					aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_IN_JUMP &&
					!aZombie->IsBobsledTeamWithSled()
				))
			{
				int aRange = -GetRectOverlap(aAttackRect, aZombieRect);
				if (aRange <= (aZombie->mIsEating ? 110 : 70))
				{
					int aPlantX = aAttackRect.mX;
					if (aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_POST_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
						aZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL || aZombie->mZombieType == ZombieType::ZOMBIE_IMP ||
						aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL || mApp->IsScaryPotterLevel())
					{
						aPlantX = aAttackRect.mX - 60;
					}

					if (aZombie->IsWalkingBackwards() || aZombieRect.mX + aZombieRect.mWidth >= aPlantX)
					{
						if (mBoard->ZombieGetID(aZombie) == mTargetZombieID)
							return aZombie;

						if (aClosestZombie == nullptr || aRange < aClosestRange)
						{
							aClosestZombie = aZombie;
							aClosestRange = aRange;
						}
					}
				}
			}
		}
	}

	return aClosestZombie;
}

void Plant::UpdateSquash()
{
	[[maybe_unused]] Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);  // unused in Release mode
	PVZP_ASSERT(aBodyReanim);

	if (mState == PlantState::STATE_NOTREADY)
	{
		Zombie* aZombie = FindSquashTarget();
		if (aZombie)
		{
			mTargetZombieID = mBoard->ZombieGetID(aZombie);
			mTargetX = aZombie->ZombieTargetLeadX(0.0f) - mWidth / 2;
			mState = PlantState::STATE_SQUASH_LOOK;
			mStateCountdown = 80;
			PlayBodyReanim(mTargetX < mX ? "anim_lookleft" : "anim_lookright", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
			mApp->PlayFoley(FoleyType::FOLEY_SQUASH_HMM);
		}
	}
	else if (mState == PlantState::STATE_SQUASH_LOOK)
	{
		if (mStateCountdown <= 0)
		{
			PlayBodyReanim("anim_jumpup", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
			mState = PlantState::STATE_SQUASH_PRE_LAUNCH;
			mStateCountdown = 45;
		}
	}
	else if (mState == PlantState::STATE_SQUASH_PRE_LAUNCH)
	{
		if (mStateCountdown <= 0)
		{
			Zombie* aZombie = FindSquashTarget();
			if (aZombie)
			{
				mTargetX = aZombie->ZombieTargetLeadX(30.0f) - mWidth / 2;
			}

			mState = PlantState::STATE_SQUASH_RISING;
			mStateCountdown = 50;
			mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
		}
	}
	else
	{
		int aTargetCol = mBoard->PixelToGridXKeepOnBoard(mTargetX, mY);
		int aDestY = mBoard->GridToPixelY(aTargetCol, mRow) + 8;

		if (mState == PlantState::STATE_SQUASH_RISING)
		{
			mX = PvzpAnimateCurve(50, 20, mStateCountdown, mBoard->GridToPixelX(mPlantCol, mStartRow), mTargetX, PvzpCurves::CURVE_EASE_IN_OUT);
			mY = PvzpAnimateCurve(50, 20, mStateCountdown, mBoard->GridToPixelY(mPlantCol, mStartRow), aDestY - 120, PvzpCurves::CURVE_EASE_IN_OUT);

			if (mStateCountdown == 0)
			{
				PlayBodyReanim("anim_jumpdown", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 60.0f);
				mState = PlantState::STATE_SQUASH_FALLING;
				mStateCountdown = 10;
			}
		}
		else if (mState == PlantState::STATE_SQUASH_FALLING)
		{
			mY = PvzpAnimateCurve(10, 0, mStateCountdown, aDestY - 120, aDestY, PvzpCurves::CURVE_LINEAR);

			if (mStateCountdown == 5)
			{
				DoSquashDamage();
			}

			if (mStateCountdown == 0)
			{
				if (mBoard->IsPoolSquare(aTargetCol, mRow))
				{
					mApp->AddReanimation(mX - 11, mY + 20, mRenderOrder + 1, ReanimationType::REANIM_SPLASH);
					mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
					mApp->PlaySample(SOUND_ZOMBIESPLASH);

					Die();
				}
				else
				{
					mState = PlantState::STATE_SQUASH_DONE_FALLING;
					mStateCountdown = 100;

					mBoard->ShakeBoard(1, 4);
					mApp->PlayFoley(FoleyType::FOLEY_THUMP);
					float aOffsetY = mBoard->StageHasRoof() ? 69.0f : 80.0f;
					mApp->AddPvzpParticle(mX + 40, mY + aOffsetY, mRenderOrder + 4, ParticleEffect::PARTICLE_DUST_SQUASH);
				}
			}
		}
		else if (mState == PlantState::STATE_SQUASH_DONE_FALLING)
		{
			if (mStateCountdown == 0)
			{
				Die();
			}
		}
	}
}

void Plant::UpdateDoomShroom()
{
	if (mIsAsleep || mState == PlantState::STATE_DOINGSPECIAL)
		return;

	mState = PlantState::STATE_DOINGSPECIAL;
	mDoSpecialCountdown = 100;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	PVZP_ASSERT(aBodyReanim);

	aBodyReanim->SetFramesForLayer("anim_explode");
	aBodyReanim->mAnimRate = 23.0f;
	aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	aBodyReanim->SetShakeOverride("DoomShroom_head1", 1.0f);
	aBodyReanim->SetShakeOverride("DoomShroom_head2", 2.0f);
	aBodyReanim->SetShakeOverride("DoomShroom_head3", 2.0f);
	mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
}

void Plant::UpdateIceShroom()
{
	if (!mIsAsleep && mState != PlantState::STATE_DOINGSPECIAL)
	{
		mState = PlantState::STATE_DOINGSPECIAL;
		mDoSpecialCountdown = 100;
	}
}

void Plant::UpdateBlover()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (aBodyReanim->mLoopCount > 0 && aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
	{
		aBodyReanim->SetFramesForLayer("anim_loop");
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	}
}

void Plant::UpdateFlowerPot()
{
	if (mState == PlantState::STATE_FLOWERPOT_INVULNERABLE && mStateCountdown == 0)
		mState = PlantState::STATE_NOTREADY;
}

void Plant::UpdateLilypad()
{
	if (mState == PlantState::STATE_LILYPAD_INVULNERABLE && mStateCountdown == 0)
		mState = PlantState::STATE_NOTREADY;
}

void Plant::UpdateCoffeeBean()
{
	if (mState == PlantState::STATE_DOINGSPECIAL)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			Die();
		}
	}
}

void Plant::UpdateUmbrella()
{
	if (mState == PlantState::STATE_UMBRELLA_TRIGGERED)
	{
		if (mStateCountdown == 0)
		{
			mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow + 1, 0);
			mState = PlantState::STATE_UMBRELLA_REFLECTING;
		}
	}
	else if (mState == PlantState::STATE_UMBRELLA_REFLECTING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayIdleAnim(0.0f);
			mState = PlantState::STATE_NOTREADY;
			mRenderOrder = CalcRenderOrder();
		}
	}
}

void Plant::UpdateCobCannon()
{
	if (mState == PlantState::STATE_COBCANNON_ARMING)
	{
		if (mStateCountdown == 0)
		{
			mState = PlantState::STATE_COBCANNON_LOADING;
			PlayBodyReanim("anim_charge", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
		}
	}
	else if (mState == PlantState::STATE_COBCANNON_LOADING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.5f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHOOP);
		}
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_COBCANNON_READY;
			PlayIdleAnim(12.0f);
		}
	}
	else if (mState == PlantState::STATE_COBCANNON_READY)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aCobTrack = aBodyReanim->GetTrackInstanceByName("CobCannon_cob");
		aCobTrack->mTrackColor = GetFlashingColor(mBoard->mMainCounter, 75);
	}
	else if (mState == PlantState::STATE_COBCANNON_FIRING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.48f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_COB_LAUNCH);
		}
	}
}

void Plant::UpdateCactus()
{
	if (mShootingCounter > 0)
		return;

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState == PlantState::STATE_CACTUS_RISING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_CACTUS_HIGH;
			PlayBodyReanim("anim_idlehigh", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
			if (mApp->IsIZombieLevel())
			{
				aBodyReanim->mAnimRate = 0;
			}

			mLaunchCounter = 1;
		}
	}
	else if (mState == PlantState::STATE_CACTUS_HIGH)
	{
		if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY) == nullptr)
		{
			mState = PlantState::STATE_CACTUS_LOWERING;
			PlayBodyReanim("anim_lower", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
		}
	}
	else if (mState == PlantState::STATE_CACTUS_LOWERING)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			mState = PlantState::STATE_CACTUS_LOW;
			PlayIdleAnim(0.0f);
		}
	}
	else if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
	{
		mState = PlantState::STATE_CACTUS_RISING;
		PlayBodyReanim("anim_rise", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
		mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
	}
}

void Plant::UpdateChomper()
{
	ReanimationID aAnimationID = mSeedType == SeedType::SEED_CHOMPERNUT ? mHeadReanimID : mBodyReanimID;
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aAnimationID);
	if (aBodyReanim == nullptr)
		return;
	if (mState == PlantState::STATE_CHOMPER_BITING_GOT_ONE || mState == PlantState::STATE_CHOMPER_DIGESTING ||
		mState == PlantState::STATE_CHOMPER_SWALLOWING)
	{
		mBoard->RefreshChomperRegeneration(this);
	}
	if (mState == PlantState::STATE_READY)
	{
		if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
		{
			PlayBodyReanim("anim_bite", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
			mState = PlantState::STATE_CHOMPER_BITING;
			mStateCountdown = 70;
		}
	}
	else if (mState == PlantState::STATE_CHOMPER_BITING)
	{
		if (mStateCountdown == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_BIGCHOMP);

			Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
			bool doBite = false;
			if (aZombie)
			{
				if (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
					aZombie->mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR ||
					aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
				{
					doBite = true;
				}
			}
			bool doMiss = false;
			if (aZombie == nullptr)
			{
				doMiss = true;
			}
			else if (!aZombie->IsImmobilizied())
			{
				if (aZombie->IsBouncingPogo() ||
					aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
				{
					doMiss = true;
				}
			}

			if (doBite)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
					aZombie->TakeDamage(gProjectileDefinition[ProjectileType::PROJECTILE_MELON].mDamage, 0U);
				mState = PlantState::STATE_CHOMPER_BITING_MISSED;
			}
			else if (doMiss)
			{
				mState = PlantState::STATE_CHOMPER_BITING_MISSED;
			}
			else
			{
				aZombie->DieWithLoot();
				mState = PlantState::STATE_CHOMPER_BITING_GOT_ONE;
			}
		}
	}
	else if (mState == PlantState::STATE_CHOMPER_BITING_GOT_ONE)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayBodyReanim("anim_chew", ReanimLoopType::REANIM_LOOP, 0, 15.0f);
			if (mApp->IsIZombieLevel())
			{
				aBodyReanim->mAnimRate = 0;
			}

			mState = PlantState::STATE_CHOMPER_DIGESTING;
			mStateCountdown = 4000;
		}
	}
	else if (mState == PlantState::STATE_CHOMPER_DIGESTING)
	{
		if (mStateCountdown == 0)
		{
			PlayBodyReanim("anim_swallow", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
			mState = PlantState::STATE_CHOMPER_SWALLOWING;
		}
	}
	else if ((mState == PlantState::STATE_CHOMPER_SWALLOWING || mState == PlantState::STATE_CHOMPER_BITING_MISSED) && aBodyReanim->mLoopCount > 0)
	{
		PlayIdleAnim(aBodyReanim->mDefinition->mFPS);
		mState = PlantState::STATE_READY;
	}
}

void Plant::UpdateBowling()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim && aBodyReanim->TrackExists("_ground"))
	{
		float aSpeed = aBodyReanim->GetTrackVelocity("_ground");
		if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aSpeed *= 2;
		}

		mX -= aSpeed;
		if (mX > 800)
			Die();
	}

	if (mState == PlantState::STATE_BOWLING_UP)
	{
		mY -= 2;
	}
	else if (mState == PlantState::STATE_BOWLING_DOWN)
	{
		mY += 2;
	}
	int aDistToGrid = mBoard->GridToPixelY(0, mRow) - mY;
	if (aDistToGrid < -2 || aDistToGrid > 2)
		return;

	PlantState aNewState = mState;
	if (mState == PlantState::STATE_BOWLING_UP && mRow <= 0)
	{
		aNewState = PlantState::STATE_BOWLING_DOWN;
	}
	else if (mState == PlantState::STATE_BOWLING_DOWN && mRow >= 4)
	{
		aNewState = PlantState::STATE_BOWLING_UP;
	}

	Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
	if (aZombie)
	{
		int aPosX = mX + mWidth / 2;
		int aPosY = mY + mHeight / 2;

		if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
		{
			mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
			mApp->PlaySample(SOUND_BOWLINGIMPACT2);

			int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY) | 32U;
			mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 90, 1, true, aDamageRangeFlags);
			mApp->AddPvzpParticle(aPosX, aPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_POWIE);
			mBoard->ShakeBoard(3, -4);

			Die();

			return;
		}

		mApp->PlayFoley(FoleyType::FOLEY_BOWLINGIMPACT);
		mBoard->ShakeBoard(1, -2);

		if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aZombie->TakeDamage(1800, 0U);
		}
		else if (aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR && mState != PlantState::STATE_NOTREADY)
		{
			aZombie->TakeDamage(1800, 0U);
		}
		else if (aZombie->mShieldType != ShieldType::SHIELDTYPE_NONE)
		{
			aZombie->TakeShieldDamage(400, 0U);
		}
		else if (aZombie->mHelmType != HelmType::HELMTYPE_NONE)
		{
			if (aZombie->mHelmType == HelmType::HELMTYPE_PAIL)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
			}
			else if (aZombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE)
			{
				mApp->PlayFoley(FoleyType::FOLEY_PLASTIC_HIT);
			}

			aZombie->TakeHelmDamage(900, 0U);
		}
		else
		{
			aZombie->TakeDamage(1800, 0U);
		}

		if ((!mApp->IsFirstTimeAdventureMode() || mApp->mPlayerInfo->GetLevel() > 10) && mSeedType == SeedType::SEED_WALLNUT)
		{
			mLaunchCounter++;
			if (mLaunchCounter == 2)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter == 3)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX - 5.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX + 5.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter == 4)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX - 10.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
				mBoard->AddCoin(aPosX + 10.0f, aPosY, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
			}
			else if (mLaunchCounter >= 5)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
				mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
				ReportAchievement::GiveAchievement(mApp, RollSomeHeads, true);
			}
		}

		if (mSeedType != SeedType::SEED_GIANT_WALLNUT)
		{
			if (mRow == 4 || mState == PlantState::STATE_BOWLING_DOWN)
			{
				aNewState = PlantState::STATE_BOWLING_UP;
			}
			else if (mRow == 0 || mState == PlantState::STATE_BOWLING_UP)
			{
				aNewState = PlantState::STATE_BOWLING_DOWN;
			}
			else
			{
				aNewState = Sexy::Rand(2) ? PlantState::STATE_BOWLING_UP : PlantState::STATE_BOWLING_DOWN;
			}
		}
	}

	if (aNewState == PlantState::STATE_BOWLING_UP)
	{
		mRow--;
		mState = PlantState::STATE_BOWLING_UP;
		mRenderOrder = CalcRenderOrder();
	}
	else if (aNewState == PlantState::STATE_BOWLING_DOWN)
	{
		mState = PlantState::STATE_BOWLING_DOWN;
		mRenderOrder = CalcRenderOrder();
		mRow++;
	}
}

void Plant::ImitaterMorph()
{
	Die();
	Plant* aPlant = mBoard->AddPlant(mPlantCol, mRow, mImitaterType, SeedType::SEED_IMITATER);

	FilterEffect aFilter = FilterEffect::FILTER_EFFECT_WASHED_OUT;
	if (mImitaterType == SeedType::SEED_HYPNOSHROOM || mImitaterType == SeedType::SEED_SQUASH || mImitaterType == SeedType::SEED_POTATOMINE ||
		mImitaterType == SeedType::SEED_GARLIC || mImitaterType == SeedType::SEED_LILYPAD)
		aFilter = FilterEffect::FILTER_EFFECT_LESS_WASHED_OUT;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
	if (aBodyReanim)
	{
		aBodyReanim->mFilterEffect = aFilter;
	}
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(aPlant->mHeadReanimID);
	if (aHeadReanim)
	{
		aHeadReanim->mFilterEffect = aFilter;
	}Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID2);
	if (aHeadReanim2)
	{
		aHeadReanim2->mFilterEffect = aFilter;
	}Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID3);
	if (aHeadReanim3)
	{
		aHeadReanim3->mFilterEffect = aFilter;
	}
}

void Plant::UpdateImitater()
{
	if (mState != PlantState::STATE_IMITATER_MORPHING)
	{
		if (mStateCountdown == 0)
		{
			mState = PlantState::STATE_IMITATER_MORPHING;
			PlayBodyReanim("anim_explode", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 26.0f);
		}
	}
	else
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.8f))
		{
			mApp->AddPvzpParticle(mX + 40, mY + 40, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_IMITATER_MORPH);
		}
		if (aBodyReanim->mLoopCount > 0)
		{
			ImitaterMorph();
		}
	}
}
