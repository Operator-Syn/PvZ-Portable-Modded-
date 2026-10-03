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

#include "../Plant/Plant.h"
#include "../Board/Board.h"
#include "../../ConstEnums.h"
#include "Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Entities/LawnMower.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/PlayerInfo.h"
#include "../System/Zombatar.h"
#include "../System/Music.h"
#include "../Widget/AlmanacDialog.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

void Zombie::UpdateZombiePeaHead()
{
	if (!mHasHead)
		return;

	if (mPhaseCounter == 35)
	{
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);
	}
	else if (mPhaseCounter == 0)
	{
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_head_idle", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 15.0f);
		mApp->PlayFoley(FoleyType::FOLEY_THROW);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		int aTrackIndex = aBodyReanim->FindTrackIndex("anim_head1");
		ReanimatorTransform aTransform;
		aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);

		float aOriginX = mPosX + aTransform.mTransX - 9.0f;
		float aOriginY = mPosY + aTransform.mTransY + 6.0f - mAltitude;
#ifdef DO_FIX_BUGS
		if (mMindControlled)  // hypnotized: fire a friendly pea instead
		{
			aOriginX += 90.0f * mScaleZombie;
			Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_PEA);
			if (aProjectile != nullptr)
			{
				aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
				aProjectile->mSourceZombieType = mZombieType;
			}
			if (aProjectile)
				aProjectile->mDamageRangeFlags = 1;
		}
		else
		{
			Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
			if (aProjectile != nullptr)
			{
				aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
				aProjectile->mSourceZombieType = mZombieType;
			}
			if (aProjectile)
				aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
		}
#else
		Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
		if (aProjectile != nullptr)
		{
			aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
			aProjectile->mSourceZombieType = mZombieType;
		}
		if (aProjectile)
			aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
#endif

		mPhaseCounter = 150;
	}
}

void Zombie::BurnRow(int theRow)  // only used by the DO_FIX_BUGS jalapeno zombie fix
{
	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if ((aZombie->mZombieType == ZombieType::ZOMBIE_BOSS || aZombie->mRow == theRow) && aZombie->EffectedByDamage(127))
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

void Zombie::UpdateZombieJalapenoHead()
{
	if (!mHasHead)
		return;

	if (mPhaseCounter == 0)
	{
		mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
		mApp->PlayFoley(FoleyType::FOLEY_JUICY);
		mBoard->DoFwoosh(mRow);
		mBoard->ShakeBoard(3, -4);

#ifdef DO_FIX_BUGS
		if (mMindControlled)
		{
			BurnRow(mRow);
		}
		else
		{
			for (Plant* aPlant : mBoard->mPlants)
			{
				if (aPlant->mDead)
					continue;
				//Rect aPlantRect = aPlant->GetPlantRect();
				if (aPlant->mRow == mRow && !aPlant->NotOnGround())
				{
					mBoard->mPlantsEaten++;
					aPlant->Die();
				}
			}
		}
#else
		for (Plant* aPlant : mBoard->mPlants)
		{
			if (aPlant->mDead)
				continue;
			//Rect aPlantRect = aPlant->GetPlantRect();
			if (aPlant->mRow == mRow && !aPlant->NotOnGround())
			{
				mBoard->mPlantsEaten++;
				aPlant->Die();
			}
		}
#endif
		DieNoLoot();
	}
}

void Zombie::UpdateZombieGatlingHead()
{
	if (!mHasHead)
		return;

	if (mPhaseCounter == 100)
	{
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 38.0f);
	}
	else if (mPhaseCounter == 18 || mPhaseCounter == 35 || mPhaseCounter == 51 || mPhaseCounter == 68)
	{
		mApp->PlayFoley(FoleyType::FOLEY_THROW);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		int aTrackIndex = aBodyReanim->FindTrackIndex("anim_head1");
		ReanimatorTransform aTransform;
		aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);

		float aOriginX = mPosX + aTransform.mTransX - 9.0f;
		float aOriginY = mPosY + aTransform.mTransY + 6.0f;
#ifdef DO_FIX_BUGS
		if (mMindControlled)  // hypnotized: fire a friendly pea instead
		{
			aOriginX += 90.0f * mScaleZombie;
			Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_PEA);
			if (aProjectile != nullptr)
			{
				aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
				aProjectile->mSourceZombieType = mZombieType;
			}
			if (aProjectile)
				aProjectile->mDamageRangeFlags = 1;
		}
		else
		{
			Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
			if (aProjectile != nullptr)
			{
				aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
				aProjectile->mSourceZombieType = mZombieType;
			}
			if (aProjectile)
				aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
		}
#else
		Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
		if (aProjectile != nullptr)
		{
			aProjectile->mSourceZombieID = mBoard->ZombieGetID(this);
			aProjectile->mSourceZombieType = mZombieType;
		}
		if (aProjectile)
			aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
#endif
	}
	else if (mPhaseCounter == 0)
	{
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_head_idle", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 15.0f);
		mPhaseCounter = 150;
	}
}

void Zombie::UpdateZombieSquashHead()
{
	if (mHasHead && mIsEating && mZombiePhase == ZombiePhase::PHASE_SQUASH_PRE_LAUNCH)
	{
		StopEating();
		PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mHasHead = false;

		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_jumpup", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		aHeadReanim->mRenderOrder = mRenderOrder + 1;
		aHeadReanim->SetPosition(mPosX + 6.0f, mPosY - 21.0f);

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("anim_head1");
		AttachmentDetach(aTrackInstance->mAttachmentID);
		aHeadReanim->OverrideScale(0.75f, 0.75f);
		aHeadReanim->mOverlayMatrix.m10 = 0.0f;

		mZombiePhase = ZombiePhase::PHASE_SQUASH_RISING;
		mPhaseCounter = 95;
	}

	if (mZombiePhase == ZombiePhase::PHASE_SQUASH_RISING)
	{
		int aDestX = mBoard->GridToPixelX(mBoard->PixelToGridXKeepOnBoard(mX, mY), mRow);
#ifdef DO_FIX_BUGS
		if (mMindControlled)
		{
			Zombie* aZombie = FindZombieTarget();
			if (aZombie)
			{
				aDestX = aZombie->ZombieTargetLeadX(0.0f);
			}
			else
			{
				aDestX += 90.0f * mScaleZombie;
			}
		}
#endif
		int aPosX = PvzpAnimateCurve(50, 20, mPhaseCounter, 0, aDestX - mPosX, PvzpCurves::CURVE_EASE_IN_OUT);
		int aPosY = PvzpAnimateCurve(50, 20, mPhaseCounter, 0, -20, PvzpCurves::CURVE_EASE_IN_OUT);

		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->SetPosition(mPosX + aPosX + 6.0f, mPosY + aPosY - 21.0f);

		if (mPhaseCounter == 0)
		{
			aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
			aHeadReanim->PlayReanim("anim_jumpdown", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 60.0f);
			mZombiePhase = ZombiePhase::PHASE_SQUASH_FALLING;
			mPhaseCounter = 10;
		}
	}

	if (mZombiePhase == ZombiePhase::PHASE_SQUASH_FALLING)
	{
		int aPosY = PvzpAnimateCurve(10, 0, mPhaseCounter, -20, 74, PvzpCurves::CURVE_LINEAR);
		int aDestX = mBoard->GridToPixelX(mBoard->PixelToGridXKeepOnBoard(mX, mY), mRow);
#ifdef DO_FIX_BUGS
		if (mMindControlled)
		{
			Zombie* aZombie = FindZombieTarget();
			if (aZombie)
			{
				aDestX = aZombie->ZombieTargetLeadX(0.0f);
			}
			else
			{
				aDestX += 90.0f * mScaleZombie;
			}
		}
#endif

		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->SetPosition(mPosX + 6.0f + aDestX - mPosX, mPosY - 21.0f + aPosY);

		if (mPhaseCounter == 2)
		{
#ifdef DO_FIX_BUGS
			if (mMindControlled)  // hypnotized: squash zombies instead
			{
				Rect aAttackRect(aDestX - 73, mPosY + 4, 65, 90);  // rect values not verified, TBD

				for (Zombie* aZombie : mBoard->mZombies)
				{
					if (aZombie->mDead)
						continue;
					if ((aZombie->mRow == mRow || aZombie->mZombieType == ZombieType::ZOMBIE_BOSS) && aZombie->EffectedByDamage(13U))
					{
						Rect aZombieRect = aZombie->GetZombieRect();
						if (GetRectOverlap(aAttackRect, aZombieRect) > (aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ? -20 : 0))
						{
							aZombie->TakeDamage(1800, 18U);
						}
					}
				}
			}
			else
			{
				SquishAllInSquare(mBoard->PixelToGridXKeepOnBoard(mX, mY), mRow, ZombieAttackType::ATTACKTYPE_CHEW);
			}
#else
			SquishAllInSquare(mBoard->PixelToGridXKeepOnBoard(mX, mY), mRow, ZombieAttackType::ATTACKTYPE_CHEW);
#endif
		}

		if (mPhaseCounter == 0)
		{
			mZombiePhase = ZombiePhase::PHASE_SQUASH_DONE_FALLING;
			mPhaseCounter = 100;

			mBoard->ShakeBoard(1, 4);
			mApp->PlayFoley(FoleyType::FOLEY_THUMP);
		}
	}

	if (mZombiePhase == ZombiePhase::PHASE_SQUASH_DONE_FALLING && mPhaseCounter == 0)
	{
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->ReanimationDie();
		mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;

		TakeDamage(1800, 9U);
	}
}
