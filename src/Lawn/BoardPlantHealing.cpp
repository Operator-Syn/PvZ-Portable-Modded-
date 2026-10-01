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
#include "ZenGarden.h"
#include "BoardInclude.h"
#include "LawnCommon.h"
#include "System/Music.h"
#include "System/SaveGame.h"
#include "Widget/LawnDialog.h"
#include "System/PlayerInfo.h"
#include "System/PoolEffect.h"
#include "System/TypingCheck.h"
#include "Widget/StoreScreen.h"
#include "Widget/AwardScreen.h"
#include "../PvzpLib/Trail.h"
#include "Widget/ChallengeScreen.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "ZombieStrengthRules.h"
#include "PlantRules.h"
#include "PlantHealing.h"

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

static int ScaleAreaHealingQuadratically(int theBaseAmount, int theAffectedPlantCount)
{
	int64_t aScaledAmount = static_cast<int64_t>(theBaseAmount) * std::max(1, theAffectedPlantCount);
	return static_cast<int>(std::clamp<int64_t>(aScaledAmount, 0, std::numeric_limits<int>::max()));
}

static int GetPlantHealingAmount(Board* theBoard, Plant* thePlant, int theBaseAmount)
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
	return static_cast<int>((static_cast<int64_t>(theBaseAmount) * (2 + aNearbyPumpkinsSquared) + 1) / 2);
}

void PlantHealing::HealPlant(Board* theBoard, Plant* thePlant, int theBaseAmount)
{
	if (!PlantHealing::PlantCanRegenerate(thePlant))
		return;
	int aHealingAmount = GetPlantHealingAmount(theBoard, thePlant, theBaseAmount);
	thePlant->mPlantHealth = static_cast<int32_t>(std::min<int64_t>(
		static_cast<int64_t>(thePlant->mPlantHealth) + aHealingAmount, thePlant->mPlantMaxHealth));
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
}

static int ApplySunMagnetRegenerationPulse(Board* theBoard, Plant* thePlant, int theStackCount, int theAffectedPlantCount = 1)
{
	constexpr int aSunReserve = 5000;
	int aSunCost = theBoard->mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? 50 : 25;
	constexpr int aHealthRestored = 25;
	if (!PlantHealing::PlantCanRegenerate(thePlant))
		return 0;

	int aAppliedStacks = 0;
	for (int i = 0; i < theStackCount; i++)
	{
		if (thePlant->mPlantHealth >= thePlant->mPlantMaxHealth || theBoard->mSunMoney < aSunReserve + aSunCost ||
			!theBoard->TakeSunMoney(aSunCost))
			break;
		PlantHealing::HealPlant(theBoard, thePlant, ScaleAreaHealingQuadratically(aHealthRestored, theAffectedPlantCount));
		++aAppliedStacks;
	}
	if (aAppliedStacks == 0)
		return 0;

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
	return aAppliedStacks;
}

void Board::StartSunMagnetRegeneration(Plant* theMagnet)
{
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

	std::vector<PlantID> anAffectedPlantIDs;
	for (const SunMagnetHealStack& aStack : mSunMagnetHealStacks)
	{
		if (aStack.mMagnetID != aMagnetID)
			continue;
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack.mPlantID));
		if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mPlantHealth < aPlant->mPlantMaxHealth &&
			std::find(anAffectedPlantIDs.begin(), anAffectedPlantIDs.end(), aStack.mPlantID) == anAffectedPlantIDs.end())
			anAffectedPlantIDs.push_back(aStack.mPlantID);
	}
	for (const HealAssignment& anAssignment : anAssignments)
	{
		PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(anAssignment.mPlant));
		if (std::find(anAffectedPlantIDs.begin(), anAffectedPlantIDs.end(), aPlantID) == anAffectedPlantIDs.end())
			anAffectedPlantIDs.push_back(aPlantID);
	}
	int aAffectedPlantCount = std::max(1, static_cast<int>(anAffectedPlantIDs.size()));

	for (const HealAssignment& anAssignment : anAssignments)
	{
		Plant* aPlant = anAssignment.mPlant;
		PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(aPlant));
		auto aStack = std::find_if(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
			[aPlantID, aMagnetID](const SunMagnetHealStack& theStack)
			{ return theStack.mPlantID == aPlantID && theStack.mMagnetID == aMagnetID; });
		if (aStack == mSunMagnetHealStacks.end())
		{
			mSunMagnetHealStacks.push_back({ aPlantID, aMagnetID, anAssignment.mStackCount, 0, 100 });
		}
		else
		{
			aStack->mStackCount = std::min(aStack->mStackCount + anAssignment.mStackCount, 1000);
			aStack->mElapsedTicks = 0;
			if (aStack->mTicksUntilPulse < 1 || aStack->mTicksUntilPulse > 100)
				aStack->mTicksUntilPulse = 100;
		}

		if (aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
		{
			if (ApplySunMagnetRegenerationPulse(this, aPlant, anAssignment.mStackCount, aAffectedPlantCount) == 0)
				break;
		}
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
		--anAura->mTicksUntilPulse;
		if (anAura->mTicksUntilPulse <= 0)
		{
			anAura->mTicksUntilPulse = 100;
			int aHealAmount = mChomperOverdriveActive ? 50 : 25;
			int aTargetCount = CountChomperHealTargets(this, anAura->mChomperID);
			if (aTargetCount > 0)
				PlantHealing::HealPlant(this, aPlant, ScaleAreaHealingQuadratically(aHealAmount, aTargetCount));
			PlantID aPlantID = anAura->mPlantID;
			for (PlantHealVisual& aVisual : mPlantHealVisuals)
				if (aVisual.mPlantID == aPlantID) aVisual.mElapsedTicks = 0;
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
	constexpr int aRegenerationDuration = 300;
	constexpr int aTicksPerPulse = 100;
	if (mMainCounter % aTicksPerPulse == aTicksPerPulse - 1)
	{
		for (Plant* aPlant : mPlants)
			if (!aPlant->mDead && aPlant->IsOnBoard() && aPlant->IsChomper())
				StartChomperRegeneration(aPlant);
	}

	std::vector<Plant*> aDueForPulse;
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

		--aGlowIt->mTicksUntilPulse;
		if (aGlowIt->mTicksUntilPulse <= 0)
		{
			aGlowIt->mTicksUntilPulse = aTicksPerPulse;
			aDueForPulse.push_back(aPlant);
		}
		++aGlowIt;
	}

	std::sort(aDueForPulse.begin(), aDueForPulse.end(), [this](Plant* thePlantA, Plant* thePlantB)
		{ return PlantHealthRatioLess(this, thePlantA, thePlantB); });
	for (Plant* aPlant : aDueForPulse)
		ApplySunMagnetRegenerationPulse(this, aPlant, 1, static_cast<int>(aDueForPulse.size()));

	std::vector<SunMagnetHealStack*> aDueMagnetStacks;
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
		--aStack->mTicksUntilPulse;
		if (aStack->mTicksUntilPulse <= 0)
		{
			aStack->mTicksUntilPulse = aTicksPerPulse;
			aDueMagnetStacks.push_back(&*aStack);
		}
		++aStack;
	}
	std::sort(aDueMagnetStacks.begin(), aDueMagnetStacks.end(), [this](const SunMagnetHealStack* theStackA, const SunMagnetHealStack* theStackB)
	{
		Plant* aPlantA = mPlants.DataArrayTryToGet(static_cast<unsigned int>(theStackA->mPlantID));
		Plant* aPlantB = mPlants.DataArrayTryToGet(static_cast<unsigned int>(theStackB->mPlantID));
		if (aPlantA != aPlantB)
			return PlantHealthRatioLess(this, aPlantA, aPlantB);
		return theStackA->mMagnetID < theStackB->mMagnetID;
	});
	std::unordered_map<PlantID, int> aSunMagnetTargetCounts;
	if (!aDueMagnetStacks.empty())
	{
		for (const SunMagnetHealStack& aStack : mSunMagnetHealStacks)
		{
			Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack.mPlantID));
			if (PlantHealing::PlantCanRegenerate(aPlant) && aPlant->mPlantHealth < aPlant->mPlantMaxHealth)
				++aSunMagnetTargetCounts[aStack.mMagnetID];
		}
	}
	for (SunMagnetHealStack* aStack : aDueMagnetStacks)
	{
		Plant* aPlant = mPlants.DataArrayTryToGet(static_cast<unsigned int>(aStack->mPlantID));
		int aTargetCount = aSunMagnetTargetCounts[aStack->mMagnetID];
		ApplySunMagnetRegenerationPulse(this, aPlant, aStack->mStackCount, aTargetCount);
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
		--anAura->mTicksUntilPulse;
		if (anAura->mTicksUntilPulse <= 0)
		{
			anAura->mTicksUntilPulse = 100;
			int aTargetCount = CountChomperHealTargets(this, anAura->mChomperID);
			if (aTargetCount > 0)
			{
				int aHealAmount = mChomperOverdriveActive ? 50 : 25;
				PlantHealing::HealPlant(this, aPlant, ScaleAreaHealingQuadratically(aHealAmount, aTargetCount));
				PlantID aPlantID = anAura->mPlantID;
				for (PlantHealVisual& aVisual : mPlantHealVisuals)
					if (aVisual.mPlantID == aPlantID) aVisual.mElapsedTicks = 0;
			}
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
