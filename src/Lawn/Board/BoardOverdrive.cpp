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

#include <time.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <SDL.h>
#include <format>
#include "../Modes/ZenGarden.h"
#include "BoardInclude.h"
#include "../Entities/LawnCommon.h"
#include "../System/Music.h"
#include "../System/SaveGame.h"
#include "../Widget/LawnDialog.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/TypingCheck.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/AwardScreen.h"
#include "../../PvzpLib/Trail.h"
#include "../Widget/ChallengeScreen.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "../Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "../Zombie/ZombieStrengthRules.h"
#include "../Plant/PlantRules.h"
#include "../Plant/PlantHealing.h"

MagnetItem* Board::GetSunMagnetExtraItems(PlantID thePlantID, bool theCreate)
{
	auto anEntry = std::find_if(mSunMagnetExtraItems.begin(), mSunMagnetExtraItems.end(),
		[thePlantID](const SunMagnetExtraItems& theItems){ return theItems.mPlantID == thePlantID; });
	if (anEntry != mSunMagnetExtraItems.end())
		return anEntry->mItems;
	if (!theCreate)
		return nullptr;

	SunMagnetExtraItems aNewEntry{};
	aNewEntry.mPlantID = thePlantID;
	mSunMagnetExtraItems.push_back(aNewEntry);
	return mSunMagnetExtraItems.back().mItems;
}

void Board::UpdateSunMagnetCollection()
{
	Sexy::FrameProfileScope aProfileScope(Sexy::FrameProfileMetric::SUN_MAGNET_ASSIGNMENT, true);
	struct SunMagnetCandidate
	{
		Plant* mPlant;
		PlantID mPlantID;
		int mPendingClaims;
		int mAvailableClaims;
	};
	std::array<SunMagnetCandidate, 1024> aMagnets{};
	std::array<size_t, 1024> aMagnetIndicesByPlantSlot{};
	aMagnetIndicesByPlantSlot.fill(aMagnets.size());
	size_t aMagnetCount = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mSeedType == SeedType::SEED_SUN_MAGNET)
			aPlant->mSunMagnetHasPendingPickup = false;
		if (!aPlant->mDead && aPlant->mSeedType == SeedType::SEED_SUN_MAGNET && aPlant->IsOnBoard() && !aPlant->NotOnGround())
		{
			unsigned int aPlantID = mPlants.DataArrayGetID(aPlant);
			int aAvailableClaims = 0;
			for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
				if (aPlant->mMagnetItems[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
					++aAvailableClaims;
			PlantID aPlantIDValue = static_cast<PlantID>(aPlantID);
			if (mSunMagnetOverdriveActive)
			{
				MagnetItem* anExtraItems = GetSunMagnetExtraItems(aPlantIDValue, true);
				for (int i = 0; i < GetSunMagnetExtraItemCapacity(mSunMoney); i++)
					if (anExtraItems[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
						++aAvailableClaims;
			}
			size_t aPlantSlot = aPlantID & DATA_ARRAY_INDEX_MASK;
			aMagnetIndicesByPlantSlot[aPlantSlot] = aMagnetCount;
			aMagnets[aMagnetCount++] = { aPlant, aPlantIDValue, 0, aAvailableClaims };
		}
	}
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mSunMagnetClaimID == PlantID::PLANTID_NULL)
			continue;
		unsigned int anOwnerSlot = static_cast<unsigned int>(aCoin->mSunMagnetClaimID) & DATA_ARRAY_INDEX_MASK;
		size_t anOwnerIndex = anOwnerSlot < aMagnetIndicesByPlantSlot.size()
			? aMagnetIndicesByPlantSlot[anOwnerSlot] : aMagnets.size();
		if (anOwnerIndex >= aMagnetCount || aMagnets[anOwnerIndex].mPlantID != aCoin->mSunMagnetClaimID)
		{
			aCoin->mSunMagnetClaimID = PlantID::PLANTID_NULL;
			aCoin->mSunMagnetPickupPending = false;
		}
		else if (!aCoin->mDead && aCoin->mSunMagnetPickupPending)
		{
			SunMagnetCandidate& anOwner = aMagnets[anOwnerIndex];
			++anOwner.mPendingClaims;
			anOwner.mPlant->mSunMagnetHasPendingPickup = true;
		}
	}
	if (aMagnetCount == 0)
		return;

	int aCoinOrdinal = 0;
	const size_t aFirstMagnet = static_cast<size_t>(mMainCounter) % aMagnetCount;
	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead || !aCoin->IsSun() || aCoin->mCoinMotion == CoinMotion::COIN_MOTION_FROM_PRESENT ||
			aCoin->mIsBeingCollected || aCoin->mSunMagnetClaimID != PlantID::PLANTID_NULL)
			continue;

		Plant* aBestMagnet = nullptr;
		size_t aBestMagnetIndex = 0;
		float aBestDistanceSquared = 0.0f;
		for (size_t aMagnetOffset = 0; aMagnetOffset < aMagnetCount; aMagnetOffset++)
		{
			size_t anIndex = (aFirstMagnet + static_cast<size_t>(aCoinOrdinal) + aMagnetOffset) % aMagnetCount;
			SunMagnetCandidate& aCandidate = aMagnets[anIndex];
			if (aCandidate.mAvailableClaims <= aCandidate.mPendingClaims)
				continue;

			Plant* aMagnet = aCandidate.mPlant;
			float aDeltaX = aMagnet->mX + aMagnet->mWidth / 2 - aCoin->mPosX - aCoin->mWidth / 2;
			float aDeltaY = aMagnet->mY + aMagnet->mHeight / 2 - aCoin->mPosY - aCoin->mHeight / 2;
			float aDistanceSquared = aDeltaX * aDeltaX + aDeltaY * aDeltaY;
			if (aBestMagnet == nullptr || aDistanceSquared < aBestDistanceSquared - 0.01f)
			{
				aBestMagnet = aMagnet;
				aBestMagnetIndex = anIndex;
				aBestDistanceSquared = aDistanceSquared;
			}
		}

		if (aBestMagnet)
		{
			aCoin->mSunMagnetClaimID = aMagnets[aBestMagnetIndex].mPlantID;
			aCoin->mSunMagnetPickupPending = true;
			++aMagnets[aBestMagnetIndex].mPendingClaims;
			++aCoinOrdinal;
		}
	}
}

void Board::StartPlanternFlame(int theRow, bool theAutoCoffeeBean)
{
	if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
		return;
	mPlanternFlameCountdown[theRow] = 300;
	mPlanternFlameTick[theRow] = 60;
	mPlanternFlameDamagePercent[theRow] = theAutoCoffeeBean ? 85 : 100;
}

void Board::UpdatePlanternFlames()
{
	for (int aRow = 0; aRow < GetNumPlayableRows(); aRow++)
	{
		if (mPlanternFlameCountdown[aRow] <= 0)
		{
			mPlanternFlameDamagePercent[aRow] = 100;
			continue;
		}
		--mPlanternFlameCountdown[aRow];
		--mPlanternFlameTick[aRow];
		if (mPlanternFlameCountdown[aRow] % 15 == 0)
		{
			float aFirstFireX = static_cast<float>(GridToPixelX(0, aRow) + 40);
			float aLastFireX = static_cast<float>(std::max(
				GridToPixelX(GetNumPlayableColumns() - 1, aRow) + 40, mApp->mWidth - LAWN_XMIN));
			float aFireX = RandRangeFloat(aFirstFireX, aLastFireX);
			float aFireY = static_cast<float>(GetPosYBasedOnRow(aFireX, aRow));
			Reanimation* aFire = mApp->AddReanimation(aFireX, aFireY, MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, aRow, 3),
				ReanimationType::REANIM_JALAPENO_FIRE);
			aFire->SetFramesForLayer("anim_flame");
			aFire->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE;
			aFire->mAnimRate = 18.0f;
			aFire->OverrideScale(0.52f, 0.36f);
		}
		if (mPlanternFlameTick[aRow] > 0)
			continue;
		mPlanternFlameTick[aRow] = 60;
		if (!TakeSunMoney(300))
		{
			mPlanternFlameCountdown[aRow] = 0;
			continue;
		}
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead || aZombie->IsDeadOrDying() || aZombie->mRow != aRow ||
				!aZombie->EffectedByDamage(127U))
				continue;
			int aHealth = std::max(1, aZombie->mBodyHealth + aZombie->mHelmHealth + aZombie->mShieldHealth + aZombie->mFlyingHealth);
			int aBaseDamage = std::max(1, static_cast<int>((static_cast<int64_t>(aHealth) * 2 + 99) / 100));
			int aDamagePercent = std::clamp(mPlanternFlameDamagePercent[aRow], 1, 100);
			int aDamage = std::max(1, static_cast<int>((static_cast<int64_t>(aBaseDamage) * aDamagePercent + 50) / 100));
			aZombie->TakeDamage(aDamage, 127U);
		}
	}
}

bool Board::TryLaunchTwinSunflowerSunBomb()
{
	std::vector<Zombie*> aTargets;
	for (Zombie* aZombie : mZombies)
	{
		if (!aZombie->mDead && !aZombie->IsDeadOrDying() && aZombie->EffectedByDamage(127U))
			aTargets.push_back(aZombie);
	}
	int aSunCost = mTwinSunflowerHighOverdriveActive ? 600 : 300;
	if (mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		aSunCost *= 2;
	if (aTargets.empty() || mSunMoney < aSunCost)
		return false;

	Zombie* aTarget = aTargets[RandRangeInt(0, static_cast<int>(aTargets.size()) - 1)];
	int aTargetRow = aTarget->mRow;
	float aTargetX = aTarget->ZombieTargetLeadX(50.0f) - 40.0f;
	Rect aTargetRect = aTarget->GetZombieRect();
	float aTargetY = aTargetRect.mY + aTargetRect.mHeight / 2.0f - 5.0f;
	Projectile* aProjectile = AddProjectile(static_cast<int>(aTargetX), static_cast<int>(aTargetY),
		MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, aTargetRow, 0), aTargetRow,
		ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB);
	if (aProjectile == nullptr)
		return false;
	aProjectile->mMillionSunDamage = mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD;
	if (!TakeSunMoney(aSunCost))
	{
		aProjectile->mDead = true;
		return false;
	}
	aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
	aProjectile->mPosX = aTargetX;
	aProjectile->mPosY = aTargetY;
	aProjectile->mPosZ = -240.0f;
	aProjectile->mVelX = 0.0f;
	aProjectile->mVelY = 0.0f;
	aProjectile->mVelZ = 4.0f;
	aProjectile->mAccZ = 0.0f;
	aProjectile->mShadowY = aTargetY + 67.0f;
	aProjectile->mDamageRangeFlags = 127;
	aProjectile->mWidth = 80;
	aProjectile->mHeight = 80;

	Reanimation* aSunReanim = mApp->AddReanimation(0.0f, 0.0f, aProjectile->mRenderOrder, ReanimationType::REANIM_SUN);
	aSunReanim->SetPosition(aProjectile->mPosX + 40.0f, aProjectile->mPosY + 40.0f);
	aSunReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aSunReanim->mAnimRate = 6.0f;
	AttachReanim(aProjectile->mAttachmentID, aSunReanim, 40.0f, 40.0f);
	return true;
}

void Board::UpdatePlantOverdrive()
{
	float aContinuousSunCost = 0.0f;
	int aPaidGloomShrooms = 0;
	int aGloomShroomCount = 0;
	for (Plant* aPlant : mPlants)
		if (PlantHealing::PlantCanRegenerate(aPlant) && !aPlant->mSquished && aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM)
			++aGloomShroomCount;
	aPaidGloomShrooms = std::min(aGloomShroomCount, mContinuousPaidGloomShroomCount);
	bool aGloomShroomUpkeepPaid = aPaidGloomShrooms == 0 || TakeSunMoneyRate(aPaidGloomShrooms * 150.0f);
	int aHealedGloomShrooms = 0;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead || !aPlant->IsOnBoard() || aPlant->mSquished)
			continue;
		if (aGloomShroomUpkeepPaid && aHealedGloomShrooms < aPaidGloomShrooms && aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM)
		{
			PlantHealing::HealPlant(this, aPlant, 0.25f);
			++aHealedGloomShrooms;
		}
		if (mGoldMagnetOverdriveActive && aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET)
		{
			aContinuousSunCost += 250.0f;
			if (aPlant->mPlantHealth > 1)
				PlantHealing::ApplyPlantHealthRate(aPlant, -25.0f);
			if (aPlant->mPlantHealth <= 1) { aPlant->mPlantHealth = 1; aPlant->mContinuousHealthRemainder = 0.0f; }
		}
		if (mPumpkinOverdriveActive && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
			PlantHealing::HealPlant(this, aPlant, 0.5f);
		if (mTallNutOverdriveActive && aPlant->IsTallNut())
		{
			PlantHealing::HealPlant(this, aPlant, 0.5f);
			aContinuousSunCost += 200.0f;
		}
		if (mChomperOverdriveActive && aPlant->IsChomper())
			aContinuousSunCost += 100.0f;
		if (mKernelPultOverdriveActive && aPlant->mSeedType == SeedType::SEED_KERNELPULT)
			aContinuousSunCost += mSunMoney >= KERNEL_PULT_BUTTER_BARRAGE_SUN_THRESHOLD ? 175.0f : 150.0f;
		if (mSunMoney >= WINTER_MELON_QUADRATIC_DAMAGE_SUN_THRESHOLD && aPlant->mSeedType == SeedType::SEED_WINTERMELON)
		{
			aContinuousSunCost += 500.0f;
			if (aPlant->mPlantHealth > 1)
				PlantHealing::ApplyPlantHealthRate(aPlant, -10.0f);
			if (aPlant->mPlantHealth <= 1) { aPlant->mPlantHealth = 1; aPlant->mContinuousHealthRemainder = 0.0f; }
		}
		if (mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD && aPlant->mSeedType == SeedType::SEED_GATLINGPEA)
			aContinuousSunCost += 300.0f;
		if (mSunMoney >= TWO_MILLION_SUN_THRESHOLD && aPlant->mSeedType == SeedType::SEED_CATTAIL)
		{
			PlantHealing::ApplyPlantHealthRate(aPlant, -std::max(1.0f, aPlant->mPlantMaxHealth / 100.0f));
			if (aPlant->mPlantHealth <= 0) { aPlant->Die(); continue; }
		}
		if (mSunMagnetOverdriveActive && aPlant->mSeedType == SeedType::SEED_SUN_MAGNET)
		{
			if (aPlant->mPlantHealth > 1)
				PlantHealing::ApplyPlantHealthRate(aPlant, -1.0f);
			if (aPlant->mPlantHealth <= 1) { aPlant->mPlantHealth = 1; aPlant->mContinuousHealthRemainder = 0.0f; }
		}
	}
	if (aContinuousSunCost > 0.0f)
		TakeSunMoneyRate(aContinuousSunCost);
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead || aZombie->IsDeadOrDying() || aZombie->mBodyHealth <= 0 || aZombie->mBodyHealth >= aZombie->mBodyMaxHealth)
			continue;
		float aHealRate = 0.0f;
		if (mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			aHealRate += aZombie->mBodyMaxHealth * 0.02575f;
		if (mZombieTierSunMoney >= TWO_MILLION_SUN_THRESHOLD && aZombie->mIsEating &&
			(aZombie->mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE || aZombie->mZombieType == ZombieType::ZOMBIE_PAIL ||
			 aZombie->mZombieType == ZombieType::ZOMBIE_BULWARK_BUCKET))
			aHealRate += 25.0f;
		float aChange = aZombie->mContinuousHealthRemainder + aHealRate / 100.0f;
		int aWholeChange = static_cast<int>(aChange);
		aZombie->mContinuousHealthRemainder = aChange - aWholeChange;
		aZombie->mBodyHealth = std::min(aZombie->mBodyHealth + aWholeChange, aZombie->mBodyMaxHealth);
		if (aZombie->mBodyHealth >= aZombie->mBodyMaxHealth)
			aZombie->mContinuousHealthRemainder = 0.0f;
	}
	if (mMainCounter % 100 != 99)
		return;
	int64_t aGloomAvailableSunAtSecondStart = static_cast<int64_t>(mSunMoney) + CountSunBeingCollected();
	int aGloomShroomCountAtSecondStart = 0;
	for (Plant* aPlant : mPlants)
		if (PlantHealing::PlantCanRegenerate(aPlant) && !aPlant->mSquished && aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM)
			++aGloomShroomCountAtSecondStart;
	mContinuousPaidGloomShroomCount = static_cast<int>(std::min<int64_t>(aGloomShroomCountAtSecondStart,
		std::max<int64_t>(0, aGloomAvailableSunAtSecondStart / 150)));

	int aSunAtSecondStart = mSunMoney;
	mZombieTierSunMoney = std::max({mZombieTierSunMoney, aSunAtSecondStart, ZombieStrengthRules::ZombieStrengthSunForTier(mZombieStrengthTier)});
	int aZombieSunTier = mZombieTierSunMoney;
	bool aHasGoldMagnet = false;
	for (Plant* aPlant : mPlants)
	{
		if (!aPlant->mDead && aPlant->IsOnBoard() && !aPlant->mSquished && aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET)
		{
			aHasGoldMagnet = true;
			break;
		}
	}
	mChomperOverdriveActive = aSunAtSecondStart > 15000;
	mKernelPultOverdriveActive = aSunAtSecondStart > 20000;
	mSunMagnetOverdriveActive = aSunAtSecondStart >= 5000;
	mGoldMagnetOverdriveActive = aSunAtSecondStart >= 50000 && aHasGoldMagnet;
	mCatTailOverdriveActive = aSunAtSecondStart >= 25000;
	mSpikeweedOverdriveActive = aSunAtSecondStart >= 1000;
	if (mGatlingPeaOverdriveActive)
		mGatlingPeaOverdriveActive = aSunAtSecondStart >= 190000;
	else
		mGatlingPeaOverdriveActive = aSunAtSecondStart >= 200000;
	mTwinSunflowerProductionOverdriveActive = aSunAtSecondStart > 1000;
	mTwinSunflowerBombardmentOverdriveActive = aSunAtSecondStart >= 10000;
	mTwinSunflowerHighOverdriveActive = aSunAtSecondStart >= 100000;
	mPumpkinOverdriveActive = aSunAtSecondStart >= 5000;
	mTallNutOverdriveActive = aSunAtSecondStart >= 10000;
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead || !aPlant->IsOnBoard())
			continue;
		int aDesiredMaxHealth = 0;
		bool aOverdriveActive = false;
		if (aPlant->IsTallNut())
		{
			aDesiredMaxHealth = (mTallNutOverdriveActive ? 10000 : 8000) +
				(aPlant->mSeedType == SeedType::SEED_CHOMPERNUT ? 500 : 0);
			aOverdriveActive = mTallNutOverdriveActive;
		}
		else if (aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aDesiredMaxHealth = mPumpkinOverdriveActive ? 5000 : 4000;
			aOverdriveActive = mPumpkinOverdriveActive;
		}
		else
			continue;
		if (aPlant->mPlantMaxHealth != aDesiredMaxHealth)
		{
			aPlant->mPlantMaxHealth = aDesiredMaxHealth;
			if (!aOverdriveActive)
				aPlant->mPlantHealth = std::min(aPlant->mPlantHealth, aDesiredMaxHealth);
		}
	}
	int aNewZombieStrengthTier = std::max(mZombieStrengthTier, ZombieStrengthRules::ZombieStrengthTierForSun(aZombieSunTier));
	if (aNewZombieStrengthTier != mZombieStrengthTier)
	{
		int aPreviousZombieStrengthTier = mZombieStrengthTier;
		mZombieStrengthTier = aNewZombieStrengthTier;
		for (Zombie* aZombie : mZombies)
		{
			if (!aZombie->mDead && !aZombie->IsDeadOrDying())
			{
				ApplyZombieStrengthTierToZombie(aZombie, aPreviousZombieStrengthTier, aNewZombieStrengthTier);
				aZombie->UpdateAnimSpeed();
			}
		}
	}
	bool aThreeMillionSunDurabilityActive = aZombieSunTier >= THREE_MILLION_SUN_THRESHOLD;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead || aZombie->IsDeadOrDying() ||
			aZombie->mThreeMillionSunDurabilityApplied == aThreeMillionSunDurabilityActive)
			continue;
		ApplyThreeMillionSunDurabilityToZombie(aZombie, aThreeMillionSunDurabilityActive);
	}
	if (!mZombieRainActive && aSunAtSecondStart >= 50000 &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING && mCurrentWave > 0)
	{
		mZombieRainActive = true;
		mZombieRainCountdown = RandRangeInt(1000, 3000);
		mZombieRainPendingCount = 0;
	}
	if (aSunAtSecondStart >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		for (Plant* aPlant : mPlants)
			if (!aPlant->mDead && aPlant->IsOnBoard() && aPlant->mSeedType == SeedType::SEED_SUN_MAGNET)
				StartSunMagnetRegeneration(aPlant);
	for (auto anItems = mSunMagnetExtraItems.begin(); anItems != mSunMagnetExtraItems.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anItems->mPlantID));
		if (aPlant == nullptr || aPlant->mDead || aPlant->mSeedType != SeedType::SEED_SUN_MAGNET)
			anItems = mSunMagnetExtraItems.erase(anItems);
		else
			++anItems;
	}
	if (aSunAtSecondStart >= TWO_MILLION_SUN_THRESHOLD && mMainCounter % 300 == 299 &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING && BoardPlanting::CoffeeBeanIsInChosenSeedBank(this))
		BoardPlanting::AutomaticallyCoffeeBeanPlanterns(this);
}
