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

#include "ZombieConstants.h"

void Zombie::UpdateZombieGargantuar()
{
	if (mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
	{
		if (mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_SMASHING ||
			mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_THROWING || mIsEating)
		{
			StopEating();
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			StartWalkAnim(20);
		}
		return;
	}

	if (mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_SMASHING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.64f))
		{
#ifdef DO_FIX_BUGS
			if (mMindControlled)  // hypnotized gargantuars smash zombies
			{
				Zombie* aZombie = FindZombieTarget();
				if (aZombie)
				{
					int aDamage = mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ? 1000 : 500;
					aZombie->TakeDamage(aDamage, 0U);
				}
			}
			else
#endif
			{
				Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
				if (aPlant)
				{
					if (aPlant->IsSpiky())
					{
						TakeDamage(20, 32U);
						aPlant->SpikyTakeDamage();
						if (aPlant->mPlantHealth <= 0)
						{
							SquishAllInSquare(aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
						}
					}
					else
					{
						PlantsOnLawn aPlantsOnTile;
						mBoard->GetPlantsOnLawn(aPlant->mPlantCol, aPlant->mRow, &aPlantsOnTile);
						Plant* aTallNut = aPlantsOnTile.mNormalPlant;
						if (aTallNut && aTallNut->IsTallNut() &&
							(aPlant == aTallNut || aPlant == aPlantsOnTile.mPumpkinPlant))
						{
							// Keep a Pumpkin shell intact while its layered Tall-nut body absorbs the hit.
							// Chomper Nut shares the Tall-nut damage behavior through IsTallNut().
							aTallNut->GargantuarSmashTakeDamage();
						}
						else
						{
							SquishAllInSquare(aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
						}
					}
				}

				if (mApp->IsScaryPotterLevel())
				{
					int aGridX = mBoard->PixelToGridX(mPosX, mPosY);
					GridItem* aScaryPot = mBoard->GetScaryPotAt(aGridX, mRow);
					if (aScaryPot)
					{
						mBoard->mChallenge->ScaryPotterOpenPot(aScaryPot);
					}
				}

				if (mApp->IsIZombieLevel())
				{
					GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
					if (aBrain)
					{
						mBoard->mChallenge->IZombieSquishBrain(aBrain);
					}
				}
			}

			mApp->PlayFoley(FoleyType::FOLEY_THUMP);
			mBoard->ShakeBoard(0, 3);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			StartWalkAnim(20);
		}

		return;
	}

	float aThrowingDistance = mPosX - 360.0f;
	if (mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_THROWING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.74f))
		{
			mHasObject = false;
			ReanimShowPrefix("Zombie_imp", RENDER_GROUP_HIDDEN);
			ReanimShowTrack("Zombie_gargantuar_whiterope", RENDER_GROUP_HIDDEN);
			mApp->PlayFoley(FoleyType::FOLEY_SWING);

			bool aIsBulwark = mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR;
			int aThrowBatchSize = aIsBulwark ? 8 : 5;
			bool aThrewZombie = false;
			for (int aThrowIndex = 0; aThrowIndex < aThrowBatchSize; aThrowIndex++)
			{
				bool aThrowsPail = aIsBulwark ? aThrowIndex < 5 : aThrowIndex == 0;
				ZombieType aThrownType = aThrowsPail ? ZombieType::ZOMBIE_PAIL : ZombieType::ZOMBIE_IMP;
				Zombie* aThrownZombie = mBoard->AddZombie(aThrownType, mFromWave);
				if (aThrownZombie == nullptr)
					break;

				float aZombieThrowingDistance = aThrowingDistance;
				float aMinThrowDistance = 40.0f;
				if (mBoard->StageHasRoof())
				{
					aZombieThrowingDistance -= 180.0f;
					aMinThrowDistance = -140.0f;
				}
				if (aZombieThrowingDistance < aMinThrowDistance)
				{
					aZombieThrowingDistance = aMinThrowDistance;
				}
				else if (aZombieThrowingDistance > 140.0f)
				{
					aZombieThrowingDistance -= RandRangeFloat(0.0f, 100.0f);
				}
				aZombieThrowingDistance += (aThrowIndex - (aThrowBatchSize - 1) / 2.0f) * 18.0f;

				aThrownZombie->mPosX = mPosX - 133.0f;
				aThrownZombie->mPosY = GetPosYBasedOnRow(mRow);
				aThrownZombie->SetRow(mRow);
				aThrownZombie->mVariant = false;
				aThrownZombie->mAltitude = 88.0f;
				aThrownZombie->mRenderOrder = mRenderOrder + 1;
				aThrownZombie->mZombiePhase = ZombiePhase::PHASE_IMP_GETTING_THROWN;
#ifdef DO_FIX_BUGS
				aThrownZombie->mScaleZombie = mScaleZombie;
				aThrownZombie->mBodyHealth *= mScaleZombie * mScaleZombie;
				aThrownZombie->mBodyMaxHealth *= mScaleZombie * mScaleZombie;
				aThrownZombie->mHelmHealth *= mScaleZombie * mScaleZombie;
				aThrownZombie->mHelmMaxHealth *= mScaleZombie * mScaleZombie;

				if (mMindControlled)
				{
					aThrownZombie->mPosX = mPosX + mWidth;
					aThrownZombie->StartMindControlled();
					aThrownZombie->mVelX = -3.0f;
				}
				else
				{
					aThrownZombie->mVelX = 3.0f;
				}
#else
				aThrownZombie->mVelX = 3.0f;
#endif
				aThrownZombie->mChilledCounter = aThrownZombie->CanBeChilled() ? mChilledCounter : 0;
				aThrownZombie->mVelZ = 0.5f * (aZombieThrowingDistance / aThrownZombie->mVelX) * THOWN_ZOMBIE_GRAVITY;
				if (aThrownType == ZombieType::ZOMBIE_IMP)
					aThrownZombie->PlayZombieReanim("anim_thrown", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 18.0f);
				else
					aThrownZombie->PlayZombieReanim("anim_walk", ReanimLoopType::REANIM_LOOP, 0, 18.0f);
				aThrownZombie->UpdateReanim();
				aThrewZombie = true;
			}
			if (aThrewZombie)
				mApp->PlayFoley(FoleyType::FOLEY_IMP);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			StartWalkAnim(20);
		}

		return;
	}

	if (IsImmobilizied() || !mHasHead)
		return;

	bool aBulwarkCanThrow = mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR && mSummonCounter == 0;
	if ((mHasObject || aBulwarkCanThrow) && mBodyHealth < mBodyMaxHealth / 2 && aThrowingDistance > 40.0f)
	{
		if (aBulwarkCanThrow)
			mSummonCounter = 1;
		mZombiePhase = ZombiePhase::PHASE_GARGANTUAR_THROWING;
		PlayZombieReanim("anim_throw", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		return;
	}

#ifdef DO_FIX_BUGS
	bool doSmash = false;
	if (mMindControlled)
	{
		doSmash = FindZombieTarget() != nullptr;
	}
	else if (FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW))
	{
		doSmash = true;
	}
	else if (mApp->IsScaryPotterLevel())
	{
		int aGridX = mBoard->PixelToGridX(mPosX, mPosY);
		if (mBoard->GetScaryPotAt(aGridX, mRow))
		{
			doSmash = true;
		}
	}
	else if (mApp->IsIZombieLevel())
	{
		if (mBoard->mChallenge->IZombieGetBrainTarget(this))
		{
			doSmash = true;
		}
	}
#else
	bool doSmash = false;
	if (FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW))
	{
		doSmash = true;
	}
	else if (mApp->IsScaryPotterLevel())
	{
		int aGridX = mBoard->PixelToGridX(mPosX, mPosY);
		if (mBoard->GetScaryPotAt(aGridX, mRow))
		{
			doSmash = true;
		}
	}
	else if (mApp->IsIZombieLevel())
	{
		if (mBoard->mChallenge->IZombieGetBrainTarget(this))
		{
			doSmash = true;
		}
	}
#endif

	if (doSmash)
	{
		mZombiePhase = ZombiePhase::PHASE_GARGANTUAR_SMASHING;
		mApp->PlayFoley(FoleyType::FOLEY_LOW_GROAN);
		float aSmashAnimRate = mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 48.0f : 16.0f;
		PlayZombieReanim("anim_smash", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aSmashAnimRate);
	}
}

void Zombie::UpdateZombieThrown()
{
	if (mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN)
	{
		mVelZ -= THOWN_ZOMBIE_GRAVITY;
		mAltitude += mVelZ;
		mPosX -= mVelX;

		float aDiffY = GetPosYBasedOnRow(mRow) - mPosY;
		mPosY += aDiffY;
		mAltitude += aDiffY;
		if (mAltitude <= 0.0f)
		{
			mAltitude = 0.0f;
			if (mZombieType == ZombieType::ZOMBIE_PAIL)
			{
				mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
				StartWalkAnim(0);
			}
			else
			{
				mZombiePhase = ZombiePhase::PHASE_IMP_LANDING;
				PlayZombieReanim("anim_land", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			}
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_IMP_LANDING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			StartWalkAnim(0);
		}
	}
}
