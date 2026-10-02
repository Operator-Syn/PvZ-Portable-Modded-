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
#include "PlantHealing.h"
#include "../Rules/TargetingRules.h"

MagnetItem* Plant::GetFreeMagnetItem()
{
	if (mSeedType == SeedType::SEED_GOLD_MAGNET || mSeedType == SeedType::SEED_SUN_MAGNET)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			if (mMagnetItems[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
			{
				return &mMagnetItems[i];
			}
		}
		if (mSeedType == SeedType::SEED_SUN_MAGNET && mBoard->mSunMagnetOverdriveActive)
		{
			MagnetItem* anExtraItem = mBoard->GetSunMagnetExtraItems(
				static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(this)), true);
			for (int i = 0; i < GetSunMagnetExtraItemCapacity(mBoard->mSunMoney); i++)
			{
				if (anExtraItem[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
					return &anExtraItem[i];
			}
		}

		return nullptr;
	}

	return &mMagnetItems[0];
}

void Plant::MagnetShroomAttactItem(Zombie* theZombie)
{
	mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
	mStateCountdown = 1500;
	PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
	mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

	MagnetItem* aMagnetItem = GetFreeMagnetItem();
	if (theZombie->mHelmType == HelmType::HELMTYPE_PAIL)
	{
		int aDamageIndex = theZombie->GetHelmDamageIndex();

		theZombie->mHelmHealth = 0;
		theZombie->mHelmType = HelmType::HELMTYPE_NONE;
		theZombie->GetTrackPosition("anim_bucket", aMagnetItem->mPosX, aMagnetItem->mPosY);
		theZombie->ReanimShowPrefix("anim_bucket", RENDER_GROUP_HIDDEN);
		theZombie->ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);

		aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_BUCKET1->GetWidth() / 2;
		aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_BUCKET1->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 25.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
		aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int>(MagnetItemType::MAGNET_ITEM_PAIL_1) + aDamageIndex);
	}
	else if (theZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL)
	{
		int aDamageIndex = theZombie->GetHelmDamageIndex();

		theZombie->mHelmHealth = 0;
		theZombie->mHelmType = HelmType::HELMTYPE_NONE;
		theZombie->GetTrackPosition("zombie_football_helmet", aMagnetItem->mPosX, aMagnetItem->mPosY);
		theZombie->ReanimShowPrefix("zombie_football_helmet", RENDER_GROUP_HIDDEN);
		theZombie->ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);

		aMagnetItem->mPosX += 37.0f;
		aMagnetItem->mPosY -= 60.0f;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
		aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int>(MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_1) + aDamageIndex);
	}
	else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR)
	{
		int aDamageIndex = theZombie->GetShieldDamageIndex();

		theZombie->DetachShield();
		theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
		if (!theZombie->mIsEating)
		{
			PVZP_ASSERT(theZombie->mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL);
			theZombie->StartWalkAnim(0);
		}
		theZombie->GetTrackPosition("anim_screendoor", aMagnetItem->mPosX, aMagnetItem->mPosY);

		aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetWidth() / 2;
		aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
		aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int>(MagnetItemType::MAGNET_ITEM_DOOR_1) + aDamageIndex);
	}
	else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER)
	{
		int aDamageIndex = theZombie->GetShieldDamageIndex();

		theZombie->DetachShield();

		aMagnetItem->mPosX = theZombie->mPosX + 31.0f;
		aMagnetItem->mPosY = theZombie->mPosY + 20.0f;
		aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth() / 2;
		aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_LADDER_5->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
		aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int>(MagnetItemType::MAGNET_ITEM_LADDER_1) + aDamageIndex);
	}
	else if (theZombie->mZombieType == ZombieType::ZOMBIE_POGO)
	{
		theZombie->PogoBreak(16U);
		// ZombieDrawPosition aDrawPos;
		// theZombie->GetDrawPos(aDrawPos);
		theZombie->GetTrackPosition("Zombie_pogo_stick", aMagnetItem->mPosX, aMagnetItem->mPosY);

		aMagnetItem->mPosX += 40.0f - IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth() / 2;
		aMagnetItem->mPosY += 84.0f - IMAGE_REANIM_ZOMBIE_LADDER_5->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
		aMagnetItem->mItemType = theZombie->mHasArm ? MagnetItemType::MAGNET_ITEM_POGO_1 : MagnetItemType::MAGNET_ITEM_POGO_3;
	}
	else if (theZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING)
	{
		theZombie->StopZombieSound();
		theZombie->PickRandomSpeed();
		theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
		theZombie->ReanimShowPrefix("Zombie_jackbox_box", RENDER_GROUP_HIDDEN);
		theZombie->ReanimShowPrefix("Zombie_jackbox_handle", RENDER_GROUP_HIDDEN);
		theZombie->GetTrackPosition("Zombie_jackbox_box", aMagnetItem->mPosX, aMagnetItem->mPosY);

		aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_JACKBOX_BOX->GetWidth() / 2;
		aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_JACKBOX_BOX->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
		aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_JACK_IN_THE_BOX;
	}
	else if (theZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		theZombie->DiggerLoseAxe();
		theZombie->GetTrackPosition("Zombie_digger_pickaxe", aMagnetItem->mPosX, aMagnetItem->mPosY);

		aMagnetItem->mPosX -= IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE->GetWidth() / 2;
		aMagnetItem->mPosY -= IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE->GetHeight() / 2;
		aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 45.0f;
		aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
		aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_PICK_AXE;
	}
}

void Plant::UpdateMagnetShroom()
{
	for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
	{
		MagnetItem* aMagnetItem = &mMagnetItems[i];
		if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
		{
			SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
			if (aVectorToPlant.Magnitude() > 20.0f)
			{
				aMagnetItem->mPosX += aVectorToPlant.x * 0.05f;
				aMagnetItem->mPosY += aVectorToPlant.y * 0.05f;
			}
		}
	}

	if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
	{
		if (mStateCountdown == 0)
		{
			if (mSeedType == SeedType::SEED_SUN_MAGNET && !IsAGoldMagnetAboutToSuck() && FindGoldMagnetTarget())
			{
				mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
				PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
			}
			else
			{
				mState = PlantState::STATE_READY;
			}

			float aAnimRate = RandRangeFloat(10.0f, 15.0f);
			PlayBodyReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 30, aAnimRate);
			if (mApp->IsIZombieLevel())
			{
				Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
				aBodyReanim->mAnimRate = 0.0f;
			}

			mMagnetItems[0].mItemType = MagnetItemType::MAGNET_ITEM_NONE;
		}
	}
	else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
	{
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim->mLoopCount > 0)
		{
			PlayBodyReanim("anim_nonactive_idle2", ReanimLoopType::REANIM_LOOP, 20, 2.0f);
			if (mApp->IsIZombieLevel())
			{
				aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
				aBodyReanim->mAnimRate = 0.0f;
			}

			mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
		}
	}
	else
	{
		float aClosestDistance = 0.0f;
		Zombie* aClosestZombie = nullptr;

		for (Zombie* aZombie : mBoard->mZombies)
		{
			if (aZombie->mDead)
				continue;
			int aDiffY = aZombie->mRow - mRow;
			Rect aZombieRect = aZombie->GetZombieRect();

			if (aZombie->mMindControlled)
				continue;

			if (!aZombie->mHasHead)
				continue;

			if (aZombie->mZombieHeight != ZombieHeight::HEIGHT_ZOMBIE_NORMAL || aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
				continue;

			if (aZombie->IsDeadOrDying())
				continue;

			if (aZombieRect.mX > mApp->mWidth || aDiffY > 2 || aDiffY < -2)
				continue;

			if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
				aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
				aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING ||
				aZombie->mZombieType == ZombieType::ZOMBIE_POGO)
			{
				if (!aZombie->mHasObject)
					continue;
			}
			else if (!(aZombie->mHelmType == HelmType::HELMTYPE_PAIL ||
				aZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL ||
				aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
				aZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
				aZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING))
				continue;

			int aRadius = aZombie->mIsEating ? 320 : 270;
			if (GetCircleRectOverlap(mX, mY + 20, aRadius, aZombieRect))
			{
				float aDistance = Distance2D(mX, mY, aZombieRect.mX, aZombieRect.mY);
				aDistance += abs(aDiffY) * 80.0f;

				if (aClosestZombie == nullptr || aDistance < aClosestDistance)
				{
					aClosestZombie = aZombie;
					aClosestDistance = aDistance;
				}
			}
		}

		if (aClosestZombie)
		{
			MagnetShroomAttactItem(aClosestZombie);
			return;
		}

		float aClosestLadderDist = 0.0f;
		GridItem* aClosestLadder = nullptr;

		for (GridItem* aGridItem : mBoard->mGridItems)
		{
			if (aGridItem->mDead)
				continue;
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
			{
				int aDiffX = abs(aGridItem->mGridX - mPlantCol);
				int aDiffY = abs(aGridItem->mGridY - mRow);
				int aSquareDistance = std::max(aDiffX, aDiffY);
				if (aSquareDistance <= 2)
				{
					float aDistance = aSquareDistance + aDiffY * 0.05f;
					if (aClosestLadder == nullptr || aDistance < aClosestLadderDist)
					{
						aClosestLadder = aGridItem;
						aClosestLadderDist = aDistance;
					}
				}
			}
		}

		if (aClosestLadder)
		{
			mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
			mStateCountdown = 1500;
			PlayBodyReanim("anim_shooting", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
			mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

			aClosestLadder->GridItemDie();

			MagnetItem* aMagnetItem = GetFreeMagnetItem();
			aMagnetItem->mPosX = mBoard->GridToPixelX(aClosestLadder->mGridX, aClosestLadder->mGridY) + 40;
			aMagnetItem->mPosY = mBoard->GridToPixelY(aClosestLadder->mGridX, aClosestLadder->mGridY);
			aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 10.0f;
			aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
			aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_LADDER_PLACED;
		}
	}
}

Coin* Plant::FindGoldMagnetTarget()
{
	Coin* aClosestCoin = nullptr;
	float aClosestDistance = 0.0f;

	for (Coin* aCoin : mBoard->mCoins)
	{
		if (aCoin->mDead)
			continue;
		bool aIsTarget = mSeedType == SeedType::SEED_SUN_MAGNET ? aCoin->IsSun() : aCoin->IsMoney();
		if (aIsTarget && aCoin->mCoinMotion != CoinMotion::COIN_MOTION_FROM_PRESENT && !aCoin->mIsBeingCollected &&
			(mSeedType == SeedType::SEED_SUN_MAGNET || aCoin->mCoinAge >= 50))
		{
			float aDistance = Distance2D(mX + mWidth / 2, mY + mHeight / 2, aCoin->mPosX + aCoin->mWidth / 2, aCoin->mPosY + aCoin->mHeight / 2);
			if (aClosestCoin == nullptr || aDistance < aClosestDistance)
			{
				aClosestCoin = aCoin;
				aClosestDistance = aDistance;
			}
		}
	}

	return aClosestCoin;
}

bool Plant::GoldMagnetFindTargets()
{
	if (GetFreeMagnetItem() == nullptr)
	{
		PVZP_ASSERT(false);
		return false;
	}

	bool aFoundTarget = false;
	for (;;)
	{
		MagnetItem* aMagnetItem = GetFreeMagnetItem();
		if (aMagnetItem == nullptr)
			break;

		Coin* aCoin = FindGoldMagnetTarget();
		if (aCoin == nullptr)
			break;

		aMagnetItem->mPosX = aCoin->mPosX + 15.0f;
		aMagnetItem->mPosY = aCoin->mPosY + 15.0f;
		aMagnetItem->mDestOffsetX = RandRangeFloat(20.0f, 40.0f);
		aMagnetItem->mDestOffsetY = RandRangeFloat(-20.0f, 0.0f) + 20.0f;

		if (mSeedType == SeedType::SEED_SUN_MAGNET)
		{
			int aSunValue = aCoin->GetSunValue();
			if (aCoin->mSunValueOverride <= 0)
				aSunValue = mBoard->ScaleSunValueForCompletedFlags(aSunValue);
			aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int64_t>(MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE) + aSunValue);
		}
		else
		{
			switch (aCoin->mType)
			{
			case CoinType::COIN_SILVER:  aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_SILVER_COIN; break;
			case CoinType::COIN_GOLD:    aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_GOLD_COIN;   break;
			case CoinType::COIN_DIAMOND: aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_DIAMOND;      break;
			default:                     PVZP_ASSERT(false); return aFoundTarget;
			}
		}

		aCoin->Die();
		aFoundTarget = true;
	}

	return aFoundTarget;
}

bool Plant::CollectSunMagnetCoin(Coin* theCoin)
{
	if (mSeedType != SeedType::SEED_SUN_MAGNET || theCoin == nullptr || theCoin->mDead || !theCoin->IsSun() ||
		theCoin->mCoinMotion == CoinMotion::COIN_MOTION_FROM_PRESENT || theCoin->mIsBeingCollected)
		return false;

	MagnetItem* aMagnetItem = GetFreeMagnetItem();
	if (aMagnetItem == nullptr)
		return false;

	aMagnetItem->mPosX = theCoin->mPosX + 15.0f;
	aMagnetItem->mPosY = theCoin->mPosY + 15.0f;
	aMagnetItem->mDestOffsetX = RandRangeFloat(20.0f, 40.0f);
	aMagnetItem->mDestOffsetY = RandRangeFloat(0.0f, 20.0f);
	int aSunValue = theCoin->GetSunValue();
	if (theCoin->mSunValueOverride <= 0)
		aSunValue = mBoard->ScaleSunValueForCompletedFlags(aSunValue);
	if (aSunValue <= 0 || aSunValue > INT32_MAX - static_cast<int>(MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE))
		return false;
	aMagnetItem->mItemType = static_cast<MagnetItemType>(static_cast<int64_t>(MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE) + aSunValue);

	theCoin->Die();
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mState != PlantState::STATE_MAGNETSHROOM_SUCKING || aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
	{
		mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
		PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);
	}
	return true;
}

bool Plant::IsAGoldMagnetAboutToSuck()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (!aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET &&
			aPlant->mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(aPlant->mBodyReanimID);
			if (aBodyReanim->mAnimTime < 0.5f)
			{
				return true;
			}
		}
	}

	return false;
}

void Plant::HealPlantsWithSun()
{
	if (mSeedType == SeedType::SEED_SUN_MAGNET &&
		mBoard->mSunMoney < TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		mBoard->StartSunMagnetRegeneration(this);
}

void Plant::UpdateGoldMagnetShroom()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mSeedType == SeedType::SEED_SUN_MAGNET && mSunMagnetCoffeeTicksRemaining > 0)
	{
		--mSunMagnetCoffeeTicksRemaining;
		PlantHealing::ApplyPlantHealthRate(this, -4.0f);
		if (mPlantHealth <= 0)
		{
			Die();
			return;
		}
		if (mSunMagnetCoffeeTicksRemaining == 0)
			mSunMagnetCoffeeTicksUntilDamage = 0;
	}
	if (mSeedType == SeedType::SEED_SUN_MAGNET)
	{
		if ((mSunMagnetHasPendingPickup || mSunMagnetCoffeeTicksRemaining > 0) &&
			(mState != PlantState::STATE_MAGNETSHROOM_SUCKING || aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP))
		{
			mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
			PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		}
	}

	bool aIsSuckingCoin = false;
	MagnetItem* anExtraItems = mSeedType == SeedType::SEED_SUN_MAGNET
		? mBoard->GetSunMagnetExtraItems(static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(this)), false)
		: nullptr;
	int aMagnetItemCount = MAX_MAGNET_ITEMS + (anExtraItems != nullptr ? SUN_MAGNET_HIGH_OVERDRIVE_EXTRA_ITEMS : 0);
	for (int i = 0; i < aMagnetItemCount; i++)
	{
		MagnetItem* aMagnetItem = i < MAX_MAGNET_ITEMS ? &mMagnetItems[i] : &anExtraItems[i - MAX_MAGNET_ITEMS];
		if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
		{
			SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
			float aDistance = aVectorToPlant.Magnitude();
			if (aDistance < 20.0f)
			{
				if (mSeedType == SeedType::SEED_SUN_MAGNET)
				{
					int aSunValue;
					switch (aMagnetItem->mItemType)
					{
					case MagnetItemType::MAGNET_ITEM_SUN_15:  aSunValue = 15;  break;
					case MagnetItemType::MAGNET_ITEM_SUN_25:  aSunValue = 25;  break;
					case MagnetItemType::MAGNET_ITEM_SUN_50:  aSunValue = 50;  break;
					case MagnetItemType::MAGNET_ITEM_SUN_150: aSunValue = 150; break;
					case MagnetItemType::MAGNET_ITEM_SUN_500: aSunValue = 500; break;
					case MagnetItemType::MAGNET_ITEM_SUN_100: aSunValue = 100; break;
					case MagnetItemType::MAGNET_ITEM_SUN_600: aSunValue = 600; break;
					default:
						if (aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE + 1)
							aSunValue = static_cast<int>(aMagnetItem->mItemType) - static_cast<int>(MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE);
						else if (aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_SUN_RANDOM_MIN &&
							aMagnetItem->mItemType <= MagnetItemType::MAGNET_ITEM_SUN_RANDOM_MAX)
							aSunValue = static_cast<int>(aMagnetItem->mItemType) - static_cast<int>(MagnetItemType::MAGNET_ITEM_SUN_RANDOM_MIN) + 100;
						else
						{
							PVZP_ASSERT(false);
							return;
						}
						break;
					}
					int64_t aSunPayout = static_cast<int64_t>(aSunValue) * (mBoard->mSunMagnetOverdriveActive ? 6 : 2);
					mBoard->AddSunMoney(static_cast<int>(std::min<int64_t>(aSunPayout, INT32_MAX)));
					HealPlantsWithSun();
					mApp->PlayFoley(FoleyType::FOLEY_SUN);
				}
				else
				{
					CoinType aCoinType;
					switch (aMagnetItem->mItemType)
					{
					case MagnetItemType::MAGNET_ITEM_SILVER_COIN: aCoinType = CoinType::COIN_SILVER;  break;
					case MagnetItemType::MAGNET_ITEM_GOLD_COIN:   aCoinType = CoinType::COIN_GOLD;    break;
					case MagnetItemType::MAGNET_ITEM_DIAMOND:     aCoinType = CoinType::COIN_DIAMOND; break;
					default:                                     PVZP_ASSERT(false); return;
					}
					int aValue = Coin::GetCoinValue(aCoinType);
					mApp->mPlayerInfo->AddCoins(aValue);
					mBoard->mCoinsCollected += aValue;
					mApp->PlayFoley(FoleyType::FOLEY_COIN);
				}

				aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_NONE;
			}
			else
			{
				float aMinSpeed = mSeedType == SeedType::SEED_GOLD_MAGNET ? 0.04f : 0.10f;
				float aMaxSpeed = mSeedType == SeedType::SEED_GOLD_MAGNET ? 0.10f : 0.20f;
				float aSpeed = PvzpAnimateCurveFloatTime(30.0f, 0.0f, aDistance, aMinSpeed, aMaxSpeed, PvzpCurves::CURVE_LINEAR);
				aMagnetItem->mPosX += aVectorToPlant.x * aSpeed;
				aMagnetItem->mPosY += aVectorToPlant.y * aSpeed;

				aIsSuckingCoin = true;
			}
		}
	}

	if (mSeedType == SeedType::SEED_SUN_MAGNET)
	{
		if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING && aBodyReanim->ShouldTriggerTimedEvent(0.4f))
		{
			PlantID aPlantID = static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(this));
			for (Coin* aCoin : mBoard->mCoins)
			{
				if (!aCoin->mDead && aCoin->mSunMagnetPickupPending && aCoin->mSunMagnetClaimID == aPlantID)
					CollectSunMagnetCoin(aCoin);
			}
		}
		if (aIsSuckingCoin || mSunMagnetHasPendingPickup || mSunMagnetCoffeeTicksRemaining > 0)
		{
			if (mState != PlantState::STATE_MAGNETSHROOM_SUCKING || aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
			{
				mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
				PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			}
		}
		else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING || mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
		{
			mState = PlantState::STATE_READY;
			PlayIdleAnim(14.0f);
		}
		return;
	}

	if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
	{
		if (mStateCountdown == 0)
		{
			mState = PlantState::STATE_READY;
		}
	}
	else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
	{
		if (mSeedType == SeedType::SEED_GOLD_MAGNET && aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
		{
			PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		}

		if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);
			GoldMagnetFindTargets();
		}

		if (mSeedType == SeedType::SEED_GOLD_MAGNET && aBodyReanim->mLoopCount > 0)
		{
			bool aHasMagnetItems = std::any_of(std::begin(mMagnetItems), std::end(mMagnetItems),
				[](const MagnetItem& theItem){ return theItem.mItemType != MagnetItemType::MAGNET_ITEM_NONE; });
			if (!aHasMagnetItems && FindGoldMagnetTarget())
				GoldMagnetFindTargets();
			aHasMagnetItems = std::any_of(std::begin(mMagnetItems), std::end(mMagnetItems),
				[](const MagnetItem& theItem){ return theItem.mItemType != MagnetItemType::MAGNET_ITEM_NONE; });
			if (!aHasMagnetItems && !FindGoldMagnetTarget())
			{
				mState = PlantState::STATE_READY;
				PlayIdleAnim(14.0f);
			}
		}
	}
	else if (!IsAGoldMagnetAboutToSuck() && Sexy::Rand(25) == 0 && FindGoldMagnetTarget())
	{
		mBoard->ShowCoinBank();
		mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
		PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
	}
}

void Plant::StartSunMagnetCoffeeBoost()
{
	if (mSeedType != SeedType::SEED_SUN_MAGNET || mDead)
		return;

	mSunMagnetCoffeeTicksRemaining = 300;
	mSunMagnetCoffeeTicksUntilDamage = 60;
	if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
	{
		// Preserve the in-flight attraction and its 0.4 timed pickup event.
		mApp->ReanimationGet(mBodyReanimID)->mLoopType = ReanimLoopType::REANIM_LOOP;
	}
	else
	{
		mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
		PlayBodyReanim("anim_attract", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
	}
}
