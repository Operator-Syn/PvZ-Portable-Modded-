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

static bool PlantHealthRatioLess(Board* theBoard, Plant* thePlantA, Plant* thePlantB)
{
	int64_t aLeft = static_cast<int64_t>(thePlantA->mPlantHealth) * thePlantB->mPlantMaxHealth;
	int64_t aRight = static_cast<int64_t>(thePlantB->mPlantHealth) * thePlantA->mPlantMaxHealth;
	if (aLeft != aRight)
		return aLeft < aRight;
	if (thePlantA->mRow != thePlantB->mRow)
		return thePlantA->mRow < thePlantB->mRow;
	if (thePlantA->mPlantCol != thePlantB->mPlantCol)
		return thePlantA->mPlantCol < thePlantB->mPlantCol;
	return theBoard->mPlants.DataArrayGetID(thePlantA) < theBoard->mPlants.DataArrayGetID(thePlantB);
}

bool PlantHealing::PlantCanRegenerate(Plant* thePlant)
{
	return thePlant != nullptr && !thePlant->mDead && thePlant->IsOnBoard() &&
		thePlant->mPlantHealth > 0 && thePlant->mPlantMaxHealth > 0;
}

static int CountChomperHealTargets(Board* theBoard, PlantID theChomperID)
{
	int aTargetCount = 0;
	for (const Board::ChomperHealAura& anAura : theBoard->mChomperHealAuras)
	{
		if (anAura.mChomperID != theChomperID)
			continue;
		Plant* aPlant = theBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura.mPlantID));
		if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			++aTargetCount;
	}
	return aTargetCount;
}

static float ScaleAreaHealingQuadratically(float theBaseAmount, int theAffectedPlantCount)
{
	return theBaseAmount * std::max(1, theAffectedPlantCount);
}

static float GetPlantHealingAmount(Board* theBoard, Plant* thePlant, float theBaseAmount)
{
	if (!theBoard->mPumpkinOverdriveActive || thePlant == nullptr)
		return theBaseAmount;

	int aNearbyPumpkins = 0;
	for (Plant* aPlant : theBoard->mPlants)
	{
		if (aPlant->mDead || !aPlant->IsOnBoard() || aPlant->mSquished || aPlant->mSeedType != SeedType::SEED_PUMPKINSHELL)
			continue;
		int aColumnDistance = std::abs(aPlant->mPlantCol - thePlant->mPlantCol);
		int aRowDistance = std::abs(aPlant->mRow - thePlant->mRow);
		bool aIsHealingPumpkinItself = aPlant == thePlant;
		if (aIsHealingPumpkinItself || ((aColumnDistance != 0 || aRowDistance != 0) && aColumnDistance <= 1 && aRowDistance <= 1))
			++aNearbyPumpkins;
	}
	int64_t aNearbyPumpkinsSquared = static_cast<int64_t>(aNearbyPumpkins) * aNearbyPumpkins;
	return theBaseAmount * (2.0f + static_cast<float>(aNearbyPumpkinsSquared)) / 2.0f;
}

void PlantHealing::LogHealingChange(Board* theBoard, Plant* thePlant, Plant* theSource,
	std::string_view theCause, int theHealthBefore, float theBaseAmount, float theRequestedAmount,
	float theRemainderBefore, int theSunSpent, unsigned int theSourceID)
{
	const int aHealed = thePlant->mPlantHealth - theHealthBefore;
	if (aHealed == 0 && theSunSpent == 0)
		return;
	const unsigned int aSourceID = theSource != nullptr ? theBoard->mPlants.DataArrayGetID(theSource) : theSourceID;
	const unsigned int aTargetID = theBoard->mPlants.DataArrayGetID(thePlant);
	const float aOverflow = std::max(0.0f, theRequestedAmount - (thePlant->mPlantMaxHealth - theHealthBefore));
	const std::string aDetail = std::format(
		"[healing] tick={} event=plant_regeneration cause={} source_kind={} source=\"{}\" source_id={} source_seed={} "
		"source_row={} source_col={} target=\"{}\" target_id={} target_seed={} target_row={} target_col={} hp_before={} hp_after={} hp_max={} "
		"base={:.3f} requested_with_carry={:.3f} healed={} overflow={:.3f} carry_before={:.3f} carry_after={:.3f} sun_spent={} sun_balance={}",
		theBoard->mMainCounter, theCause, theSource != nullptr ? "plant" : "effect",
		theSource != nullptr ? Plant::GetNameString(theSource->mSeedType, theSource->mImitaterType) : std::string(theCause),
		aSourceID, theSource != nullptr ? static_cast<int>(theSource->mSeedType) : -1,
		theSource != nullptr ? theSource->mRow : -1, theSource != nullptr ? theSource->mPlantCol : -1,
		Plant::GetNameString(thePlant->mSeedType, thePlant->mImitaterType), aTargetID,
		static_cast<int>(thePlant->mSeedType), thePlant->mRow, thePlant->mPlantCol,
		theHealthBefore, thePlant->mPlantHealth, thePlant->mPlantMaxHealth, theBaseAmount,
		theRequestedAmount, aHealed, aOverflow, theRemainderBefore, thePlant->mContinuousHealthRemainder,
		theSunSpent, theBoard->mSunMoney);
	Sexy::LogHealthAudit(std::format("healing:{}:{}:{}", aTargetID, aSourceID, theCause), aDetail,
		theBoard->mMainCounter, theHealthBefore, thePlant->mPlantHealth, 0, aHealed, theSunSpent, aOverflow);
}

void PlantHealing::HealPlant(Board* theBoard, Plant* thePlant, float theBaseAmount,
	std::string_view theCause, Plant* theSource, int theSunSpent, unsigned int theSourceID)
{
	if (!PlantCanRegenerate(thePlant))
		return;
	HealingAudit anAudit;
	HealPlant(theBoard, thePlant, theBaseAmount, std::numeric_limits<int>::max(), &anAudit);
	LogHealingChange(theBoard, thePlant, theSource, theCause, anAudit.mHealthBefore,
		theBaseAmount, anAudit.mRequestedAmount, anAudit.mRemainderBefore, theSunSpent, theSourceID);
}

int PlantHealing::HealPlant(Board* theBoard, Plant* thePlant, float theBaseAmount, int theMaxBaseHealing, HealingAudit* theAudit)
{
	if (!PlantHealing::PlantCanRegenerate(thePlant))
		return 0;
	const int aOldHealth = thePlant->mPlantHealth;
	const int aHealingLimit = thePlant->mPlantMaxHealth - aOldHealth;
	const bool aIsBudgeted = theMaxBaseHealing != std::numeric_limits<int>::max();
	const float aFundedBase = aIsBudgeted ?
		std::min(theBaseAmount, static_cast<float>(std::max(0, std::min(theMaxBaseHealing, aHealingLimit)))) : theBaseAmount;
	const float aHealingMultiplier = GetPlantHealingAmount(theBoard, thePlant, 1.0f);
	const float aModifiedAmount = theBaseAmount * aHealingMultiplier;
	float aHealingAmount = aModifiedAmount + thePlant->mContinuousHealthRemainder;
	const float aFundedAmount = aFundedBase * aHealingMultiplier + thePlant->mContinuousHealthRemainder;
	if (theAudit != nullptr)
	{
		*theAudit = {};
		theAudit->mBaseAmount = theBaseAmount;
		theAudit->mModifiedAmount = aModifiedAmount;
		theAudit->mRequestedAmount = aHealingAmount;
		theAudit->mHealthBefore = aOldHealth;
		theAudit->mHealthAfter = aOldHealth;
		theAudit->mMaxHealth = thePlant->mPlantMaxHealth;
		theAudit->mRemainderBefore = thePlant->mContinuousHealthRemainder;
		theAudit->mRemainderAfter = thePlant->mContinuousHealthRemainder;
		theAudit->mOverflow = std::max(0.0f, aHealingAmount - (thePlant->mPlantMaxHealth - aOldHealth));
		theAudit->mUnaffordable = std::max(0.0f,
			std::min(aHealingAmount, static_cast<float>(aHealingLimit)) -
			std::max(0.0f, std::min(aFundedAmount, static_cast<float>(aHealingLimit))));
	}
	if (theMaxBaseHealing <= 0)
		return 0;
	if (aIsBudgeted)
		aHealingAmount = std::min(aFundedAmount, static_cast<float>(aHealingLimit));
	int aWholeHealing = std::min(static_cast<int>(aHealingAmount), aHealingLimit);
	thePlant->mContinuousHealthRemainder = aHealingAmount - aWholeHealing;
	if (aIsBudgeted && aWholeHealing == aHealingLimit)
		thePlant->mContinuousHealthRemainder = 0.0f;
	thePlant->mPlantHealth = std::min(thePlant->mPlantHealth + aWholeHealing, thePlant->mPlantMaxHealth);
	if (thePlant->mPlantHealth >= thePlant->mPlantMaxHealth && thePlant->mContinuousHealthRemainder > 0.0f)
		thePlant->mContinuousHealthRemainder = 0.0f;
	if (thePlant->mSeedType == SeedType::SEED_SPIKEROCK)
	{
		Reanimation* aBodyReanim = theBoard->mApp->ReanimationTryToGet(thePlant->mBodyReanimID);
		if (aBodyReanim != nullptr)
		{
			aBodyReanim->AssignRenderGroupToTrack("bigspike3",
				thePlant->mPlantHealth > thePlant->mPlantMaxHealth * 2 / 3 ? RENDER_GROUP_NORMAL : RENDER_GROUP_HIDDEN);
			aBodyReanim->AssignRenderGroupToTrack("bigspike2",
				thePlant->mPlantHealth > thePlant->mPlantMaxHealth / 3 ? RENDER_GROUP_NORMAL : RENDER_GROUP_HIDDEN);
		}
	}
	if (theAudit != nullptr)
	{
		theAudit->mHealthAfter = thePlant->mPlantHealth;
		theAudit->mRemainderAfter = thePlant->mContinuousHealthRemainder;
		if (aIsBudgeted)
		{
			theAudit->mPaidBaseHealing = std::min(aWholeHealing, static_cast<int>(std::ceil(aFundedBase)));
			theAudit->mBonusHealing = aWholeHealing - theAudit->mPaidBaseHealing;
		}
	}
	return thePlant->mPlantHealth - aOldHealth;
}

void PlantHealing::ApplyPlantHealthRate(Plant* thePlant, float theHealthPerSecond)
{
	if (!PlantHealing::PlantCanRegenerate(thePlant) || theHealthPerSecond == 0.0f)
		return;
	const int aHealthBefore = thePlant->mPlantHealth;
	const float aRemainderBefore = thePlant->mContinuousHealthRemainder;
	float aChange = thePlant->mContinuousHealthRemainder + theHealthPerSecond / 100.0f;
	int aWholeChange = static_cast<int>(aChange);
	thePlant->mContinuousHealthRemainder = aChange - aWholeChange;
	thePlant->mPlantHealth = std::clamp(thePlant->mPlantHealth + aWholeChange, 0, thePlant->mPlantMaxHealth);
	if (thePlant->mPlantHealth == 0 || (thePlant->mPlantHealth == thePlant->mPlantMaxHealth && theHealthPerSecond > 0.0f))
		thePlant->mContinuousHealthRemainder = 0.0f;
	if (theHealthPerSecond > 0.0f)
		LogHealingChange(thePlant->mBoard, thePlant, thePlant, "positive_health_rate",
			aHealthBefore, theHealthPerSecond / 100.0f, aChange, aRemainderBefore);
}

static bool ApplySunMagnetRegenerationRate(Board* theBoard, Plant* thePlant, int theStackCount,
	int theAffectedPlantCount = 1, Plant* theSource = nullptr, unsigned int theSourceID = 0)
{
	constexpr int aSunReserve = 5000;
	if (!PlantHealing::PlantCanRegenerate(thePlant))
		return false;
	int aSunCost = theBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 50 : 25;
	const int aSunBefore = theBoard->mSunMoney;
	if (thePlant->mPlantHealth >= thePlant->mPlantMaxHealth || theBoard->mSunMoney <= aSunReserve ||
		!theBoard->TakeSunMoneyRate(static_cast<float>(aSunCost * theStackCount)))
		return false;
	PlantHealing::HealPlant(theBoard, thePlant,
		0.25f * theStackCount * std::max(1, theAffectedPlantCount),
		theSource != nullptr || theSourceID != 0 ? "sun_magnet_stacked_regeneration" : "sun_magnet_glow_regeneration",
		theSource, aSunBefore - theBoard->mSunMoney, theSourceID);

	PlantID aPlantID = static_cast<PlantID>(theBoard->mPlants.DataArrayGetID(thePlant));
	auto aVisual = std::find_if(theBoard->mPlantHealVisuals.begin(), theBoard->mPlantHealVisuals.end(),
		[aPlantID](const Board::PlantHealVisual& theVisual){ return theVisual.mPlantID == aPlantID; });
	if (thePlant->mPlantHealth >= thePlant->mPlantMaxHealth)
	{
		if (aVisual != theBoard->mPlantHealVisuals.end())
		{
			PvzpParticleSystem* aParticle = theBoard->mApp->ParticleTryToGet(aVisual->mParticleID);
			if (aParticle != nullptr)
				aParticle->ParticleSystemDie();
			aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
		}
		for (Board::PlantHealGlow& aGlow : theBoard->mPlantHealGlows)
			if (aGlow.mPlantID == aPlantID)
				aGlow.mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	}
	else
	{
		if (aVisual != theBoard->mPlantHealVisuals.end())
			aVisual->mElapsedTicks = 0;
		theBoard->ShowPlantHealGlow(thePlant);
	}
	return true;
}

void Board::StartSunMagnetRegeneration(Plant* theMagnet)
{
	constexpr int aLegacyTicksUntilPulse = 100;
	constexpr int aSunReserve = 5000;
	int aSunCost = mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 50 : 25;
	if (theMagnet == nullptr || theMagnet->mDead || !theMagnet->IsOnBoard() ||
		theMagnet->mSeedType != SeedType::SEED_SUN_MAGNET || mSunMoney < aSunReserve + aSunCost)
		return;

	std::vector<Plant*> aTargets;
	for (Plant* aPlant : mPlants)
	{
		if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			aTargets.push_back(aPlant);
	}
	if (aTargets.empty())
		return;

	std::sort(aTargets.begin(), aTargets.end(), [this](Plant* thePlantA, Plant* thePlantB)
		{ return PlantHealthRatioLess(this, thePlantA, thePlantB); });
	PlantID aMagnetID = static_cast<PlantID>(mPlants.DataArrayGetID(theMagnet));
	int aAssignmentCount = RandRangeInt(10, 20);
	int aAffordableAssignments = (mSunMoney - aSunReserve) / aSunCost;
	aAssignmentCount = std::min(aAssignmentCount, aAffordableAssignments);
	struct HealAssignment
	{
		Plant* mPlant;
		int mStackCount;
	};
	std::vector<HealAssignment> anAssignments;
	anAssignments.reserve(aAssignmentCount);
	for (int i = 0; i < aAssignmentCount; i++)
	{
		Plant* aPlant;
		if (mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			aPlant = aTargets[static_cast<size_t>(i) % aTargets.size()];
		else
			aPlant = aTargets[Rand(static_cast<int>(aTargets.size()))];
		auto anAssignment = std::find_if(anAssignments.begin(), anAssignments.end(),
			[aPlant](const HealAssignment& theAssignment){ return theAssignment.mPlant == aPlant; });
		if (anAssignment == anAssignments.end())
			anAssignments.push_back({ aPlant, 1 });
		else
			++anAssignment->mStackCount;
	}

	for (const HealAssignment& anAssignment : anAssignments)
	{
		Plant* aPlant = anAssignment.mPlant;
		PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(aPlant));
		auto aStack = std::find_if(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
			[aPlantID, aMagnetID](const SunMagnetHealStack& theStack)
			{ return theStack.mPlantID == aPlantID && theStack.mMagnetID == aMagnetID; });
		if (aStack == mSunMagnetHealStacks.end())
		{
			mSunMagnetHealStacks.push_back({ aPlantID, aMagnetID, anAssignment.mStackCount, 0, aLegacyTicksUntilPulse });
		}
		else
		{
			aStack->mStackCount = std::min(aStack->mStackCount + anAssignment.mStackCount, 1000);
			aStack->mElapsedTicks = 0;
			if (aStack->mTicksUntilPulse < 1 || aStack->mTicksUntilPulse > 100)
				aStack->mTicksUntilPulse = aLegacyTicksUntilPulse;
		}

		if (aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			ShowPlantHealGlow(aPlant);
	}
}

void Board::RestorePlantHealGlowsAfterLoad()
{
	for (auto aStack = mSunMagnetHealStacks.begin(); aStack != mSunMagnetHealStacks.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack->mPlantID));
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) || aStack->mStackCount < 1 || aStack->mStackCount > 1000 ||
			aStack->mElapsedTicks < 0 || aStack->mElapsedTicks >= 300 || aStack->mTicksUntilPulse < 1 || aStack->mTicksUntilPulse > 100)
		{
			aStack = mSunMagnetHealStacks.erase(aStack);
			continue;
		}
		++aStack;
	}

	for (auto aEffect = mPlantHealGlows.begin(); aEffect != mPlantHealGlows.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aEffect->mPlantID));
		PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aEffect->mParticleID);
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) || aEffect->mElapsedTicks < 0 || aEffect->mElapsedTicks >= 300 ||
			aEffect->mTicksUntilPulse < 1 || aEffect->mTicksUntilPulse > 100)
		{
			bool aHasChomperSource = std::any_of(mChomperHealAuras.begin(), mChomperHealAuras.end(),
				[&](const ChomperHealAura& theAura){ return theAura.mPlantID == aEffect->mPlantID; });
			bool aHasMagnetSource = std::any_of(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
				[&](const SunMagnetHealStack& theStack){ return theStack.mPlantID == aEffect->mPlantID; });
			if (aParticle != nullptr && !aHasChomperSource && !aHasMagnetSource)
				aParticle->ParticleSystemDie();
			aEffect = mPlantHealGlows.erase(aEffect);
			continue;
		}

		if (aParticle != nullptr && (aParticle->mDead || aParticle->mEffectType != ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW))
		{
			if (!aParticle->mDead)
				aParticle->ParticleSystemDie();
			aParticle = nullptr;
			aEffect->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
		}

		PlantID aPlantID = aEffect->mPlantID;
		aEffect->mParticleID = aParticle != nullptr ? mApp->ParticleGetID(aParticle) : ParticleSystemID::PARTICLESYSTEMID_NULL;
		PlantHealVisual* aVisual = nullptr;
		for (PlantHealVisual& aCandidate : mPlantHealVisuals)
			if (aCandidate.mPlantID == aPlantID) { aVisual = &aCandidate; break; }
		if (aVisual == nullptr)
		{
			mPlantHealVisuals.push_back({ aPlantID, aEffect->mParticleID, aEffect->mElapsedTicks, 0 });
			aVisual = &mPlantHealVisuals.back();
		}
		else if (aEffect->mParticleID != ParticleSystemID::PARTICLESYSTEMID_NULL)
			aVisual->mParticleID = aEffect->mParticleID;
		if (aVisual->mParticleID != ParticleSystemID::PARTICLESYSTEMID_NULL)
		{
			PvzpParticleSystem* aVisualParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
			if (aVisualParticle == nullptr || aVisualParticle->mDead ||
				aVisualParticle->mEffectType != ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW)
			{
				aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
				aVisual->mCreationRetryTicks = 0;
			}
		}
		if (aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			ShowPlantHealGlow(aPlant);
		++aEffect;
	}

	for (auto anAura = mChomperHealAuras.begin(); anAura != mChomperHealAuras.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura->mPlantID));
		Plant* aChomper = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura->mChomperID));
		bool aChomperIsActive = aChomper != nullptr && !aChomper->mDead && aChomper->IsOnBoard() && aChomper->IsChomper();
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) || !aChomperIsActive ||
			anAura->mTicksUntilPulse < 1 || anAura->mTicksUntilPulse > 100)
		{
			anAura = mChomperHealAuras.erase(anAura);
			continue;
		}
		++anAura;
	}

	for (auto aVisual = mPlantHealVisuals.begin(); aVisual != mPlantHealVisuals.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aVisual->mPlantID));
		bool aHasAura = std::any_of(mPlantHealGlows.begin(), mPlantHealGlows.end(),
			[&](const PlantHealGlow& theGlow){ return theGlow.mPlantID == aVisual->mPlantID; }) ||
			std::any_of(mChomperHealAuras.begin(), mChomperHealAuras.end(),
				[&](const ChomperHealAura& theAura){ return theAura.mPlantID == aVisual->mPlantID; }) ||
			std::any_of(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
				[&](const SunMagnetHealStack& theStack){ return theStack.mPlantID == aVisual->mPlantID; });
		PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
		if (!aHasAura || aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) ||
			(aVisual->mParticleID != ParticleSystemID::PARTICLESYSTEMID_NULL &&
			 (aParticle == nullptr || aParticle->mDead || aParticle->mEffectType != ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW)))
		{
			if (aParticle != nullptr)
				aParticle->ParticleSystemDie();
			aVisual = mPlantHealVisuals.erase(aVisual);
			continue;
		}
		if (aPlant->mPlantHealth >= aPlant->mPlantMaxHealth)
		{
			if (aParticle != nullptr)
				aParticle->ParticleSystemDie();
			aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
		}
		else
			ShowPlantHealGlow(aPlant);
		++aVisual;
	}

	for (PvzpParticleSystem* aParticle : mApp->mEffectSystem->mParticleHolder->mParticleSystems)
	{
		if (aParticle->mDead || aParticle->mEffectType != ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW)
			continue;
		ParticleSystemID aParticleID = static_cast<ParticleSystemID>(mApp->ParticleGetID(aParticle));
		bool aPlantOwnsParticle = false;
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mParticleID == aParticleID)
			{
				aPlantOwnsParticle = true;
				break;
			}
		}
		bool aRegenerationOwnsParticle = std::any_of(mPlantHealVisuals.begin(), mPlantHealVisuals.end(),
			[aParticleID](const PlantHealVisual& theVisual){ return theVisual.mParticleID == aParticleID; });
		if (!aPlantOwnsParticle && !aRegenerationOwnsParticle)
			aParticle->ParticleSystemDie();
	}
}

void Board::ShowPlantHealGlow(Plant* thePlant)
{
	if (thePlant == nullptr || !PlantHealing::PlantCanRegenerate(thePlant))
		return;

	PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(thePlant));
	if (thePlant->mPlantHealth >= thePlant->mPlantMaxHealth)
		return;
	auto aVisual = std::find_if(mPlantHealVisuals.begin(), mPlantHealVisuals.end(),
		[aPlantID](const PlantHealVisual& theVisual){ return theVisual.mPlantID == aPlantID; });
	if (aVisual == mPlantHealVisuals.end())
	{
		mPlantHealVisuals.push_back({ aPlantID, ParticleSystemID::PARTICLESYSTEMID_NULL, 0, 0 });
		aVisual = std::prev(mPlantHealVisuals.end());
	}
	if (aVisual->mCreationRetryTicks > 0)
		return;

	PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
	if (aVisual->mParticleID == ParticleSystemID::PARTICLESYSTEMID_NULL)
	{
		aParticle = mApp->AddPvzpParticle(thePlant->mX + 40, thePlant->mY + 60, thePlant->mRenderOrder - 1,
			ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW);
		if (aParticle == nullptr)
		{
			aVisual->mCreationRetryTicks = 100;
			return;
		}
		aVisual->mParticleID = mApp->ParticleGetID(aParticle);
		aVisual->mCreationRetryTicks = 0;
	}
	else if (aParticle == nullptr || aParticle->mDead)
		return;
	for (PlantHealGlow& aGlow : mPlantHealGlows)
		if (aGlow.mPlantID == aPlantID) aGlow.mParticleID = aVisual->mParticleID;
	aParticle->mRenderOrder = thePlant->mRenderOrder - 1;
	for (PvzpListNode<ParticleEmitterID>* aNode = aParticle->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mApp->mEffectSystem->mParticleHolder->mEmitters.DataArrayTryToGet(static_cast<unsigned int>(aNode->mValue));
		if (aEmitter != nullptr)
		{
			aEmitter->SystemMove(thePlant->mX + 40, thePlant->mY + 60);
			aEmitter->mColorOverride.mAlpha = static_cast<int32_t>(255.0f * (1.0f - std::clamp(aVisual->mElapsedTicks, 0, 300) / 300.0f));
		}
	}
}

void Board::UpdatePlantHealGlows()
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING || mApp->mSeedChooserScreen != nullptr)
		return;
	constexpr int aRegenerationDuration = 300;
	if (mMainCounter % 100 == 99)
	{
		for (Plant* aPlant : mPlants)
			if (!aPlant->mDead && aPlant->IsOnBoard() && aPlant->IsChomper())
				StartChomperRegeneration(aPlant);
	}

	for (auto aGlowIt = mPlantHealGlows.begin(); aGlowIt != mPlantHealGlows.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aGlowIt->mPlantID));
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant))
		{
			aGlowIt = mPlantHealGlows.erase(aGlowIt);
			continue;
		}

		++aGlowIt->mElapsedTicks;
		if (aGlowIt->mElapsedTicks >= aRegenerationDuration)
		{
			aGlowIt = mPlantHealGlows.erase(aGlowIt);
			continue;
		}

		if (aPlant->mPlantHealth >= aPlant->mPlantMaxHealth)
		{
			for (PlantHealVisual& aVisual : mPlantHealVisuals)
			{
				if (aVisual.mPlantID == aGlowIt->mPlantID)
				{
					PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual.mParticleID);
					if (aParticle != nullptr) aParticle->ParticleSystemDie();
					aVisual.mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
				}
			}
			aGlowIt->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
		}
		else
			ShowPlantHealGlow(aPlant);

		if (aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			ApplySunMagnetRegenerationRate(this, aPlant, 1);
		++aGlowIt;
	}

	std::vector<SunMagnetHealStack*> anActiveMagnetStacks;
	for (auto aStack = mSunMagnetHealStacks.begin(); aStack != mSunMagnetHealStacks.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack->mPlantID));
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) || aStack->mStackCount < 1 || aStack->mStackCount > 1000)
		{
			aStack = mSunMagnetHealStacks.erase(aStack);
			continue;
		}

		++aStack->mElapsedTicks;
		if (aStack->mElapsedTicks >= aRegenerationDuration)
		{
			aStack = mSunMagnetHealStacks.erase(aStack);
			continue;
		}
		anActiveMagnetStacks.push_back(&*aStack);
		++aStack;
	}
	std::unordered_map<PlantID, int> aSunMagnetTargetCounts;
	if (!anActiveMagnetStacks.empty())
	{
		for (const SunMagnetHealStack& aStack : mSunMagnetHealStacks)
		{
			Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack.mPlantID));
			if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
				++aSunMagnetTargetCounts[aStack.mMagnetID];
		}
	}
	for (SunMagnetHealStack* aStack : anActiveMagnetStacks)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack->mPlantID));
		int aTargetCount = aSunMagnetTargetCounts[aStack->mMagnetID];
		ApplySunMagnetRegenerationRate(this, aPlant, aStack->mStackCount, aTargetCount,
			mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack->mMagnetID)),
			static_cast<unsigned int>(aStack->mMagnetID));
	}

	for (auto anAura = mChomperHealAuras.begin(); anAura != mChomperHealAuras.end();)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura->mPlantID));
		Plant* aChomper = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura->mChomperID));
		bool aChomperIsActive = aChomper != nullptr && !aChomper->mDead && aChomper->IsOnBoard() && aChomper->IsChomper();
		if (aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant) || !aChomperIsActive)
		{
			anAura = mChomperHealAuras.erase(anAura);
			continue;
		}
		int aHealAmount = mChomperOverdriveActive ? 50 : 25;
		int aTargetCount = CountChomperHealTargets(this, anAura->mChomperID);
		if (aTargetCount > 0)
		{
			PlantHealing::HealPlant(this, aPlant, ScaleAreaHealingQuadratically(aHealAmount / 100.0f, aTargetCount),
				"chomper_aura_regeneration", aChomper);
		}
		++anAura;
	}

	for (auto aVisual = mPlantHealVisuals.begin(); aVisual != mPlantHealVisuals.end();)
	{
		bool aHasAura = std::any_of(mPlantHealGlows.begin(), mPlantHealGlows.end(),
			[&](const PlantHealGlow& theGlow){ return theGlow.mPlantID == aVisual->mPlantID; });
		aHasAura = aHasAura || std::any_of(mChomperHealAuras.begin(), mChomperHealAuras.end(),
			[&](const ChomperHealAura& theAura){ return theAura.mPlantID == aVisual->mPlantID; });
		aHasAura = aHasAura || std::any_of(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
			[&](const SunMagnetHealStack& theStack){ return theStack.mPlantID == aVisual->mPlantID; });
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aVisual->mPlantID));
		if (!aHasAura || aPlant == nullptr || !PlantHealing::PlantCanRegenerate(aPlant))
		{
			PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
			if (aParticle != nullptr) aParticle->ParticleSystemDie();
			aVisual = mPlantHealVisuals.erase(aVisual);
			continue;
		}
		if (aPlant->mPlantHealth >= aPlant->mPlantMaxHealth)
		{
			PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
			if (aParticle != nullptr) aParticle->ParticleSystemDie();
			aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
		}
		else
		{
			if (aVisual->mCreationRetryTicks > 0)
				--aVisual->mCreationRetryTicks;
			PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
			if (aVisual->mParticleID != ParticleSystemID::PARTICLESYSTEMID_NULL && (aParticle == nullptr || aParticle->mDead))
			{
				aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
				aVisual->mCreationRetryTicks = 100;
			}
			++aVisual->mElapsedTicks;
			if (aVisual->mElapsedTicks >= aRegenerationDuration)
			{
				PvzpParticleSystem* aParticle = mApp->ParticleTryToGet(aVisual->mParticleID);
				if (aParticle != nullptr) aParticle->ParticleSystemDie();
				aVisual->mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
			}
			else
				ShowPlantHealGlow(aPlant);
		}
		++aVisual;
	}
}

void Board::StartChomperRegeneration(Plant* theChomper)
{
	if (theChomper == nullptr || theChomper->mDead || !theChomper->IsOnBoard() || !theChomper->IsChomper())
		return;

	std::vector<Plant*> aTargets;
	for (Plant* aPlant : mPlants)
	{
		if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mRow == theChomper->mRow && aPlant->mPlantCol > theChomper->mPlantCol &&
			aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
			aTargets.push_back(aPlant);
	}
	std::sort(aTargets.begin(), aTargets.end(), [this, theChomper](Plant* thePlantA, Plant* thePlantB)
	{
		int aDistanceA = thePlantA->mPlantCol - theChomper->mPlantCol;
		int aDistanceB = thePlantB->mPlantCol - theChomper->mPlantCol;
		if (aDistanceA != aDistanceB) return aDistanceA < aDistanceB;
		return PlantHealthRatioLess(this, thePlantA, thePlantB);
	});
	PlantID aChomperID = static_cast<PlantID>(mPlants.DataArrayGetID(theChomper));
	const size_t aTargetCount = std::min<size_t>(2, aTargets.size());
	for (auto anAura = mChomperHealAuras.begin(); anAura != mChomperHealAuras.end();)
	{
		if (anAura->mChomperID != aChomperID)
		{
			++anAura;
			continue;
		}
		bool aStillTargeted = false;
		for (size_t i = 0; i < aTargetCount; i++)
			if (anAura->mPlantID == static_cast<PlantID>(mPlants.DataArrayGetID(aTargets[i])))
				aStillTargeted = true;
		if (!aStillTargeted)
			anAura = mChomperHealAuras.erase(anAura);
		else
			++anAura;
	}
	for (size_t i = 0; i < aTargetCount; i++)
	{
		Plant* aPlant = aTargets[i];
		PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(aPlant));
		auto aAura = std::find_if(mChomperHealAuras.begin(), mChomperHealAuras.end(),
			[aPlantID, aChomperID](const ChomperHealAura& theAura)
			{ return theAura.mPlantID == aPlantID && theAura.mChomperID == aChomperID; });
		if (aAura == mChomperHealAuras.end())
			mChomperHealAuras.push_back({ aPlantID, aChomperID, 100 });
		ShowPlantHealGlow(aPlant);
	}
}

void Board::RefreshChomperRegeneration(Plant* theChomper)
{
	PlantID aChomperID = static_cast<PlantID>(mPlants.DataArrayGetID(theChomper));
	for (ChomperHealAura& anAura : mChomperHealAuras)
	{
		if (anAura.mChomperID == aChomperID)
		{
			Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(anAura.mPlantID));
			if (aPlant != nullptr && aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
				ShowPlantHealGlow(aPlant);
		}
	}
}
