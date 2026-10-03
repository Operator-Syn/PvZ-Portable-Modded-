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




#include "../Rules/TargetingRules.h"
#include "ProjectileRules.h"

void Projectile::UpdateLobMotion()
{
	if (mProjectileType == ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB)
	{
		mPosZ += mVelZ;
		if (mProjectileAge > 20 && mPosZ >= -35.0f)
			DoImpact(nullptr);
		return;
	}

	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG && mPosZ < -700.0f)
	{
		mVelZ = 8.0f;
		mRow = mCobTargetRow;
		mPosX = mCobTargetX;
		int aCobTargetCol = mBoard->PixelToGridXKeepOnBoard(mCobTargetX, 0);
		mPosY = mBoard->GridToPixelY(aCobTargetCol, mCobTargetRow);
		mShadowY = mPosY + 67.0f;
		mRotation = -PI / 2;
	}

	if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL && mCobTargetX > 0.0f)
	{
		mRow = mCobTargetRow;
		int aRemainingFlight = std::max(1, 120 - mProjectileAge);
		float aTargetGroundY = mBoard->GetPosYBasedOnRow(mCobTargetX, mCobTargetRow) + 67.0f;
		float aProjectileGroundOffset = mShadowY - mPosY;
		mVelX = (mCobTargetX - mPosX) / aRemainingFlight;
		mVelY = (aTargetGroundY - aProjectileGroundOffset - mPosY) / aRemainingFlight;
		mShadowY += mVelY;
	}

	if ((ProjectileRules::IsPultProjectile(mProjectileType) || mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB) &&
		mTargetZombieID != ZombieID::ZOMBIEID_NULL)
	{
		Zombie* aTargetZombie = mBoard->ZombieTryToGet(mTargetZombieID);
		if (aTargetZombie && !aTargetZombie->IsDeadOrDying() && aTargetZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
		{
			float aTargetGroundY = mBoard->GetPosYBasedOnRow(mPosX, aTargetZombie->mRow) + 67.0f + mCobTargetRow;
			float aProjectileGroundOffset = mShadowY - mPosY;
			int aFlightDuration = mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY ? 60 : 120;
			int aRemainingFlight = std::max(1, aFlightDuration - mProjectileAge);
			float aTargetX = aTargetZombie->ZombieTargetLeadX(static_cast<float>(aRemainingFlight)) - 30.0f + mCobTargetX;
			mVelX = (aTargetX - mPosX) / aRemainingFlight;
			mVelY = (aTargetGroundY - aProjectileGroundOffset - mPosY) / aRemainingFlight;
		}
	}

	mVelZ += mAccZ;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY)
	{
		mVelZ += mAccZ;
	}
	mPosX += mVelX;
	mPosY += mVelY;
	if ((ProjectileRules::IsPultProjectile(mProjectileType) || mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB) &&
		mTargetZombieID != ZombieID::ZOMBIEID_NULL)
		mShadowY += mVelY;
	if (ProjectileRules::IsPultProjectile(mProjectileType) && mTargetZombieID != ZombieID::ZOMBIEID_NULL)
		mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mShadowY);
	mPosZ += mVelZ;

	bool isRising = mVelZ < 0.0f;
	if (isRising && (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL || mProjectileType == ProjectileType::PROJECTILE_COBBIG))
	{
		return;
	}
	if (mProjectileAge > 20)
	{
		if (isRising)
		{
			return;
		}

		float aMinCollisionZ = 0.0f;
		if (mProjectileType == ProjectileType::PROJECTILE_BUTTER)
		{
			aMinCollisionZ = -32.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL)
		{
			aMinCollisionZ = 60.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_MELON || mProjectileType == ProjectileType::PROJECTILE_WINTERMELON ||
			mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB)
		{
			aMinCollisionZ = -35.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_CABBAGE || mProjectileType == ProjectileType::PROJECTILE_KERNEL)
		{
			aMinCollisionZ = -30.0f;
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
		{
			aMinCollisionZ = -60.0f;
		}
		if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
		{
			aMinCollisionZ += 40.0f;
		}

		if (mPosZ <= aMinCollisionZ)
		{
			return;
		}
	}

	Plant* aPlant = nullptr;
	Zombie* aZombie = nullptr;
	if (mProjectileType == ProjectileType::PROJECTILE_BASKETBALL || mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA)
	{
		aPlant = FindCollisionTargetPlant();
	}
	else
	{
		aZombie = FindCollisionTarget();
	}

	float aGroundZ = 80.0f;
	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		aGroundZ = -40.0f;
	}
	bool hitGround = mPosZ > aGroundZ;
	if (hitGround &&
		(mProjectileType == ProjectileType::PROJECTILE_CABBAGE || mProjectileType == ProjectileType::PROJECTILE_KERNEL ||
			mProjectileType == ProjectileType::PROJECTILE_BUTTER) &&
		mTargetZombieID != ZombieID::ZOMBIEID_NULL)
	{
		Zombie* aTrackedTarget = mBoard->ZombieTryToGet(mTargetZombieID);
		// A cross-lane lob tracks its intended target through the flight. The projectile's
		// sampled row can be off by one at lane boundaries, so row equality must not veto
		// a valid direct hit when the shell reaches that target's horizontal position.
		if (aTrackedTarget != nullptr && !aTrackedTarget->IsDeadOrDying() &&
			aTrackedTarget->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
		{
			Rect aTargetRect = aTrackedTarget->GetZombieRect();
			float aImpactCenterX = mPosX + mWidth * 0.5f;
			if (aImpactCenterX >= aTargetRect.mX - 20.0f &&
				aImpactCenterX <= aTargetRect.mX + aTargetRect.mWidth + 20.0f)
				aZombie = aTrackedTarget;
		}
	}
	if (aZombie == nullptr && aPlant == nullptr && !hitGround)
	{
		return;
	}

	if (aPlant)
	{
		Plant* aUmbrellaPlant = mBoard->FindUmbrellaPlant(aPlant->mPlantCol, aPlant->mRow);
		if (aUmbrellaPlant)
		{
			if (aUmbrellaPlant->mState == PlantState::STATE_UMBRELLA_REFLECTING)
			{
				mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
				int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1);
				mApp->AddPvzpParticle(mPosX + 20.0f, mPosY + 20.0f, aRenderPosition, ParticleEffect::PARTICLE_UMBRELLA_REFLECT);
				Die();
			}
			else if (aUmbrellaPlant->mState != PlantState::STATE_UMBRELLA_TRIGGERED)
			{
				mApp->PlayFoley(FoleyType::FOLEY_UMBRELLA);
				aUmbrellaPlant->DoSpecial();
			}
		}
		else
		{
			const int aHealthBefore = aPlant->mPlantHealth;
			aPlant->mPlantHealth -= GetProjectileDef().mDamage;
			aPlant->LogDamage(aHealthBefore, "catapult_basketball", mBoard->ZombieTryToGet(mSourceZombieID), this);
			aPlant->mEatenFlashCountdown = std::max(aPlant->mEatenFlashCountdown, 25);
			mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
			Die();
		}
	}
	else if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		int aBeforeGargantuarCount = mBoard->GetLiveGargantuarCount();
		if (mPlanternCob)
		{
			int aPlanternCobDamage = std::max(1, mBoard->GetZombieExplosiveDamage() / 2);
			if (mPlanternAutoCoffeeBean)
				aPlanternCobDamage = std::max(1, static_cast<int>((static_cast<int64_t>(aPlanternCobDamage) * 85 + 50) / 100));
			mBoard->KillAllZombiesInRadius(mRow, mPosX + 80, mPosY + 40, 115, 1, false, mDamageRangeFlags,
				aPlanternCobDamage);
		}
		else
			mBoard->KillAllZombiesInRadius(mRow, mPosX + 80, mPosY + 40, 115, 1, true, mDamageRangeFlags);
		int aAfterGargantuarCount = mBoard->GetLiveGargantuarCount();
		mBoard->mGargantuarsKillsByCornCob += aBeforeGargantuarCount - aAfterGargantuarCount;
		if (mBoard->mGargantuarsKillsByCornCob >= 2)
			ReportAchievement::GiveAchievement(mApp, PopcornParty, true);
		if (mPlanternCob)
			mBoard->StartPlanternFlame(mRow, mPlanternAutoCoffeeBean);

		DoImpact(nullptr);
	}
	else
	{
		DoImpact(aZombie);
	}
}

void Projectile::UpdateNormalMotion()
{
	if (mProjectileType == ProjectileType::PROJECTILE_EPHRAIM_JAVELIN || mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW)
	{
		mPosX += mVelX;
	}
	else if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
	{
		mPosX -= 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_HOMING)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
		if (mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB && mTargetZombieID != ZombieID::ZOMBIEID_NULL &&
			(!aZombie || aZombie->IsDeadOrDying() || !aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags))))
		{
			if (!mTargetTrackingEnded)
			{
				Zombie* aClosestTarget = nullptr;
				float aClosestDistance = 0.0f;
				for (Zombie* aCandidate : mBoard->mZombies)
				{
					if (aCandidate->mDead || aCandidate->IsDeadOrDying() ||
						!aCandidate->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
						continue;

					Rect aCandidateRect = aCandidate->GetZombieRect();
					float aDistance = Distance2D(mPosX + mWidth / 2.0f, mPosY + mHeight / 2.0f,
						aCandidateRect.mX + aCandidateRect.mWidth / 2.0f, aCandidateRect.mY + aCandidateRect.mHeight / 2.0f);
					if (aClosestTarget == nullptr || aDistance < aClosestDistance)
					{
						aClosestTarget = aCandidate;
						aClosestDistance = aDistance;
					}
				}

				if (aClosestTarget != nullptr)
				{
					aZombie = aClosestTarget;
					mTargetZombieID = mBoard->ZombieGetID(aClosestTarget);
					mTargetTrackingEnded = true;
				}
			}

			if (mTargetZombieID == ZombieID::ZOMBIEID_NULL || !aZombie || aZombie->IsDeadOrDying() ||
				!aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
			{
				mTargetTrackingEnded = true;
				mTargetZombieID = ZombieID::ZOMBIEID_NULL;

				std::array<int, MAX_GRID_SIZE_Y> aUnoccupiedRows{};
				int aUnoccupiedRowCount = 0;
				for (int aRow = 0; aRow < mBoard->GetNumPlayableRows(); aRow++)
				{
					bool aRowOccupied = mBoard->mPlanternFlameCountdown[aRow] > 0;
					for (Projectile* aOther : mBoard->mProjectiles)
					{
						if (aOther != this && !aOther->mDead &&
							aOther->mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB && aOther->mRow == aRow)
						{
							aRowOccupied = true;
							break;
						}
					}
					if (!aRowOccupied)
						aUnoccupiedRows[aUnoccupiedRowCount++] = aRow;
				}

				int aLaneRow = aUnoccupiedRowCount > 0
					? aUnoccupiedRows[Sexy::Rand(aUnoccupiedRowCount)]
					: Sexy::Rand(mBoard->GetNumPlayableRows());
				mRow = aLaneRow;

				float aDirectionX = mVelX < 0.0f ? -1.0f : 1.0f;
				if (std::abs(mVelX) < 0.001f && Sexy::Rand(2) == 0)
					aDirectionX = -1.0f;
				float aTargetCenterX = aDirectionX > 0.0f ? mApp->mWidth - 20.0f : 20.0f;
				float aTargetCenterY = mBoard->GetPosYBasedOnRow(aTargetCenterX, aLaneRow) + 50.0f;
				float aDeltaX = aTargetCenterX - (mPosX + mWidth / 2.0f);
				float aDeltaY = aTargetCenterY - (mPosY + mHeight / 2.0f);
				float aDistance = std::sqrt(aDeltaX * aDeltaX + aDeltaY * aDeltaY);
				if (aDistance > 0.0f)
				{
					mVelX = aDeltaX * 2.0f / aDistance;
					mVelY = aDeltaY * 2.0f / aDistance;
				}
				else
				{
					mVelX = aDirectionX * 2.0f;
					mVelY = 0.0f;
				}
			}
		}

		if (mProjectileType == ProjectileType::PROJECTILE_SPIKE && mMotionType == ProjectileMotion::MOTION_HOMING &&
			!mTargetTrackingEnded &&
			(!aZombie || aZombie->IsDeadOrDying() || !aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags))))
		{
			Zombie* aClosestTarget = nullptr;
			float aClosestDistance = 0.0f;
			int aClosestLeftEdgeX = 0;
			bool aClosestIsBalloon = false;
			if (mCattailRedirectionCount < 1)
			{
				for (Zombie* aCandidate : mBoard->mZombies)
				{
					if (aCandidate->mDead || aCandidate->IsDeadOrDying() ||
						!aCandidate->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
						continue;

					Rect aCandidateRect = aCandidate->GetZombieRect();
					float aDistance = Distance2D(mPosX + mWidth / 2.0f, mPosY + mHeight / 2.0f,
						aCandidateRect.mX + aCandidateRect.mWidth / 2.0f, aCandidateRect.mY + aCandidateRect.mHeight / 2.0f);
					bool anIsBalloon = aCandidate->mZombieType == ZombieType::ZOMBIE_BALLOON;
					bool aHigherPriority = aClosestTarget == nullptr || TargetingRules::HigherCattailPriority(
						{anIsBalloon, aCandidateRect.mX, aDistance},
						{aClosestIsBalloon, aClosestLeftEdgeX, aClosestDistance});
					if (aHigherPriority)
					{
						aClosestTarget = aCandidate;
						aClosestDistance = aDistance;
						aClosestLeftEdgeX = aCandidateRect.mX;
						aClosestIsBalloon = anIsBalloon;
					}
				}
			}

			if (aClosestTarget != nullptr)
			{
				aZombie = aClosestTarget;
				mTargetZombieID = mBoard->ZombieGetID(aClosestTarget);
				mCattailRedirectionCount = 1;
			}
			else
			{
				// Once its single redirect is spent or no valid target remains, coast without tracking future waves.
				mTargetZombieID = ZombieID::ZOMBIEID_NULL;
				mTargetTrackingEnded = true;
			}
		}

		if (aZombie && !aZombie->IsDeadOrDying() && aZombie->EffectedByDamage(static_cast<unsigned int>(mDamageRangeFlags)))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			SexyVector2 aTargetCenter(aZombie->ZombieTargetLeadX(0.0f), aZombieRect.mY + aZombieRect.mHeight / 2);
			SexyVector2 aProjectileCenter(mPosX + mWidth / 2, mPosY + mHeight / 2);
			SexyVector2 aToTarget = (aTargetCenter - aProjectileCenter).Normalize();
			SexyVector2 aMotion(mVelX, mVelY);

			aMotion += aToTarget * (0.001f * mProjectileAge);
			aMotion = aMotion.Normalize();
			aMotion *= 2.0f;

			mVelX = aMotion.x;
			mVelY = aMotion.y;
			mRotation = -atan2(mVelY, mVelX);
		}
		else if (mProjectileType == ProjectileType::PROJECTILE_SPIKE)
		{
			// Keep its last steering vector so a lost target does not make the shot snap horizontally.
			mTargetZombieID = ZombieID::ZOMBIEID_NULL;
		}

		mPosY += mVelY;
		mPosX += mVelX;
		mShadowY += mVelY;
		if (mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB)
			mPosZ = -12.0f + std::sin(mProjectileAge * 0.12f) * 4.0f;
		mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mPosY);
	}
	else if (mMotionType == ProjectileMotion::MOTION_STAR)
	{
		mPosY += mVelY;
		mPosX += mVelX;
		mShadowY += mVelY;

		if (mVelY != 0.0f)
		{
			mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mPosY);
		}
	}
	else if (mMotionType == ProjectileMotion::MOTION_BEE)
	{
		if (mProjectileAge < 60)
		{
			mPosY -= 0.5f;
		}
		mPosX += 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
	{
		if (mVelZ < 0.0f)
		{
			mVelZ += 0.002f;
			mVelZ = std::min(mVelZ, 0.0f);
			mPosY += mVelZ;
			mRotation = 0.3f - 0.7f * mVelZ * PI * 0.25f;
		}
		mPosX += 0.4f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_BEE_BACKWARDS)
	{
		if (mProjectileAge < 60)
		{
			mPosY -= 0.5f;
		}
		mPosX -= 3.33f;
	}
	else if (mMotionType == ProjectileMotion::MOTION_THREEPEATER)
	{
		mPosX += 3.33f;
		mPosY += mVelY;
		mVelY *= 0.97f;
		mShadowY += mVelY;
	}
	else
	{
		mPosX += 3.33f;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY)
	{
		if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
		{
			mVelZ += 0.004f;
		}
		else
		{
			mVelZ += 0.2f;
		}

		mPosY += mVelZ;
	}

	CheckForCollision();
	CheckForHighGround();
}

void Projectile::UpdateMotion()
{
	if (mAnimTicksPerFrame > 0)
	{
		mAnimCounter = (mAnimCounter + 1) % (mNumFrames * mAnimTicksPerFrame);
		mFrame = mAnimCounter / mAnimTicksPerFrame;
	}

	int aOldRow = mRow;
	float aOldY = mBoard->GetPosYBasedOnRow(mPosX, mRow);
	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		UpdateLobMotion();
	}
	else
	{
		UpdateNormalMotion();
	}

	float aSlopeHeightChange = mBoard->GetPosYBasedOnRow(mPosX, aOldRow) - aOldY;
	if (mProjectileType == ProjectileType::PROJECTILE_COBBIG)
	{
		aSlopeHeightChange = 0.0f;  // Fix The Roof Offset Bug of Corn Cob
	}
	if (mMotionType == ProjectileMotion::MOTION_FLOAT_OVER)
	{
		mPosY += aSlopeHeightChange;
	}
	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		mPosY += aSlopeHeightChange;
		mPosZ -= aSlopeHeightChange;
	}
	mShadowY += aSlopeHeightChange;
	if ((ProjectileRules::IsPultProjectile(mProjectileType) || mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB) &&
		mTargetZombieID != ZombieID::ZOMBIEID_NULL)
		mRow = mBoard->PixelToGridYKeepOnBoard(mPosX, mShadowY);
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY + mPosZ);
}
