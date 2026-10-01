// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Lawn/Plant/PlantRules.h"
#include "Lawn/Rules/PlantingRules.h"
#include "Lawn/Projectile/ProjectileRules.h"
#include "Lawn/Rules/TargetingRules.h"
#include "Lawn/Rules/WaveRules.h"
#include "Lawn/Zombie/ZombieStatusRules.h"
#include "Lawn/Zombie/ZombieStrengthRules.h"
#include "Lawn/Widget/SeedChooserOrder.h"
#include "Lawn/System/SaveGameFormat.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

static void Require(bool theCondition, const char* theBehavior)
{
	if (!theCondition)
	{
		std::fprintf(stderr, "Regression: %s\n", theBehavior);
		std::exit(EXIT_FAILURE);
	}
}

static void StrengthThresholdsAndResistance()
{
	using namespace ZombieStrengthRules;
	constexpr std::array<int, 10> THRESHOLDS = {0, 100000, 200000, 500000, 700000, 800000, 1000000, 2000000, 2500000, 5000000};
	constexpr std::array<int, 10> HEALTH = {1, 5, 10, 8, 12, 12, 18, 41, 62, 62};
	for (int aTier = 0; aTier < static_cast<int>(THRESHOLDS.size()); ++aTier)
	{
		Require(ZombieStrengthTierForSun(THRESHOLDS[aTier]) == aTier, "sun threshold enters its original tier");
		Require(ZombieStrengthSunForTier(aTier) == THRESHOLDS[aTier], "tier restores its original sun threshold");
		Require(ZombieHealthMultiplierForTier(aTier) == HEALTH[aTier], "tier health table remains unchanged");
		if (aTier > 0)
			Require(ZombieStrengthTierForSun(THRESHOLDS[aTier] - 1) == aTier - 1, "sun below a threshold stays in the previous tier");
	}
	Require(ZombieStrengthTierForSun(-1) == 0 && ZombieStrengthTierForSun(std::numeric_limits<int>::max()) == 9, "sun extremes have bounded tiers");
	Require(ZombieHealthMultiplierForTier(-1) == 1 && ZombieHealthMultiplierForTier(100) == 62, "tier lookup clamps outside the table");
	Require(ZombieHealthMultiplierForZombie(ZOMBIE_PAIL, PHASE_ZOMBIE_NORMAL, 7) == 82, "bucket armor doubles at the two-million tier");
	Require(ZombieHealthMultiplierForZombie(ZOMBIE_NEWSPAPER, PHASE_NEWSPAPER_MAD, 7) == 82, "angry newspaper retains its health modifier");
	Require(ZombieHealthMultiplierForZombie(ZOMBIE_NEWSPAPER, PHASE_ZOMBIE_NORMAL, 7) == 41, "normal newspaper has no angry modifier");
	Require(ZombieHealthMultiplierForZombie(ZOMBIE_IMP, PHASE_ZOMBIE_NORMAL, 4) == 18, "imp health scales at tier four");
	Require(QuadraticDamageMultiplier(899999, 5, false, 100) == 100, "resistance begins at 900000 sun");
	Require(QuadraticDamageMultiplier(900000, 5, false, 100) == 4, "initial resistance cap remains four");
	Require(QuadraticDamageMultiplier(2000000, 7, false, 100) == 6, "resistance cap follows health doublings");
	Require(QuadraticDamageMultiplier(5000000, 9, false, 100) == 8, "highest-tier resistance cap remains eight");
	Require(QuadraticDamageMultiplier(5000000, 9, true, 100) == 100, "bungee and absent-target exemptions retain full damage");
	Require(QuadraticDamageMultiplier(900000, 5, false, 2) == 2 && QuadraticDamageMultiplier(900000, 5, false, 0) == 1, "resistance rounds roots upward and keeps minimum damage");
}

static void TargetPriorityAndPlacement()
{
	using namespace TargetingRules;
	Require(HigherCattailPriority({true, 700, 100}, {false, 10, 1}), "balloons precede nearer ground zombies");
	Require(HigherCattailPriority({false, 10, 100}, {false, 20, 1}), "leftmost eligible target precedes the nearest target");
	Require(HigherCattailPriority({false, 10, 1}, {false, 10, 2}), "distance resolves equal left edges");
	Require(!HigherCattailPriority({true, 10, 1}, {true, 10, 1}), "equal target priorities preserve iteration order");
	Require(!HigherCattailPriority({false, 1, 1}, {true, 10, 10}), "ground targets cannot displace balloons");
	Require(PlantingRules::ValidateCell(0, 0, 9, 6, SEED_PEASHOOTER) == PLANTING_OK, "first lawn cell accepts ordinary plants");
	Require(PlantingRules::ValidateCell(8, 5, 9, 6, SEED_PEASHOOTER) == PLANTING_OK, "last serialized lawn row preserves the existing boundary");
	for (auto [aColumn, aRow] : std::array<std::array<int, 2>, 4>{{{-1, 0}, {9, 0}, {0, -1}, {0, 6}}})
		Require(PlantingRules::ValidateCell(aColumn, aRow, 9, 6, SEED_PEASHOOTER) == PLANTING_NOT_HERE, "invalid coordinates fail before grid access");
	Require(PlantingRules::ValidateCell(1, 1, 9, 6, SEED_CHOMPERNUT) == PLANTING_NOT_HERE, "Chomper Nut stays unavailable as a directly planted seed");
	Require(PlantRules::IsMelonPultStackType(SEED_MELONPULT) && PlantRules::IsMelonPultStackType(SEED_WINTERMELON), "melon upgrade stack types share the placement rule");
	Require(PlantRules::IsFumeGloomStackType(SEED_GLOOMSHROOM) && !PlantRules::IsFumeGloomStackType(SEED_PEASHOOTER), "fungus stacks exclude unrelated plants");
	Require(PlantRules::IsMagnetStackType(SEED_SUN_MAGNET) && PlantRules::IsAutomaticPult(SEED_KERNELPULT), "custom magnet and automatic pult classifications are preserved");
	Require(ProjectileRules::IsPultProjectile(PROJECTILE_MELON) && !ProjectileRules::IsPultProjectile(PROJECTILE_PEA), "projectile motion uses the original pult classification");
	for (int anIndex = 0; anIndex < NUM_SEEDS_IN_CHOOSER; ++anIndex)
		Require(SeedChooserOrder::SeedChooserIndexOf(SeedChooserOrder::SeedChooserTypeAtIndex(anIndex)) == anIndex, "chooser rendering and selection share one seed order");
}

static void WavesAndSpawnRestrictions()
{
	using namespace WaveRules;
	Require(!IsFlagWave(true, 1, 5, 4), "first adventure level has no flag waves");
	Require(IsFlagWave(true, 2, 5, 4) && !IsFlagWave(true, 2, 5, 3), "short adventure levels flag their final wave");
	Require(IsFlagWave(false, 10, 20, 9) && IsFlagWave(false, 10, 20, 19), "ordinary levels flag every tenth wave");
	Require(!IsFlagWave(true, 2, 0, 0), "invalid wave count fails without division by zero");
	Require(!CanSelectZombie(ZOMBIE_NORMAL, GAMEMODE_ADVENTURE, false, false, 3, 4, 5, 4), "zombies wait for their first allowed wave");
	Require(CanSelectZombie(ZOMBIE_NORMAL, GAMEMODE_ADVENTURE, false, false, 4, 4, 5, 4), "wave and point boundaries include eligible zombies");
	Require(!CanSelectZombie(ZOMBIE_NORMAL, GAMEMODE_ADVENTURE, false, false, 4, 3, 5, 4), "wave selection respects remaining points");
	Require(!CanSelectZombie(ZOMBIE_BUNGEE, GAMEMODE_ADVENTURE, true, false, 9, 100, 1, 1), "endless bungees only appear on flag waves");
	Require(CanSelectZombie(ZOMBIE_BUNGEE, GAMEMODE_ADVENTURE, true, true, 0, 0, 100, 10), "endless flag bungees preserve their special point and wave exemption");
	for (GameMode aMode : {GAMEMODE_CHALLENGE_POGO_PARTY, GAMEMODE_CHALLENGE_BOBSLED_BONANZA, GAMEMODE_CHALLENGE_AIR_RAID})
		Require(CanSelectZombie(ZOMBIE_NORMAL, aMode, false, false, 0, 0, 100, 10), "special challenge spawn restrictions remain exempt");
}

static void ColdEligibility()
{
	using namespace ZombieRules;
	ColdState aState;
	Require(CanBeChilled(aState) && CanBeFrozen(aState), "ordinary living zombies accept cold effects");
	aState.mType = ZOMBIE_ZAMBONI;
	Require(!CanBeChilled(aState), "Zamboni remains immune to chill");
	aState = {}; aState.mHasSled = true;
	Require(!CanBeChilled(aState), "intact sled teams remain immune to chill");
	aState = {}; aState.mSunAmount = 3500000;
	Require(!CanBeChilled(aState), "ordinary spawns become chill immune at 3.5 million sun");
	aState.mSpawnedByRain = true;
	Require(CanBeChilled(aState), "rain spawns bypass sun-tier cold restrictions");
	aState.mDeadOrDying = true;
	Require(!CanBeChilled(aState), "rain spawn exemptions do not revive dying targets");
	aState = {}; aState.mType = ZOMBIE_GARGANTUAR; aState.mSunAmount = 999999;
	Require(CanBeChilled(aState), "Gargantuar chill eligibility below one million sun");
	aState.mSunAmount = 1000000;
	Require(!CanBeChilled(aState), "Gargantuar chill immunity starts at one million sun");
	aState = {}; aState.mType = ZOMBIE_PAIL; aState.mSunAmount = 2000000;
	Require(!CanBeChilled(aState), "bucket chill immunity starts at two million sun");
	aState = {}; aState.mType = ZOMBIE_IMP; aState.mStrengthTier = 4;
	Require(!CanBeChilled(aState), "armored imp tiers retain chill immunity");
	aState = {}; aState.mMindControlled = true;
	Require(!CanBeChilled(aState), "friendly zombies reject cold effects");
	aState = {}; aState.mPhase = PHASE_DIGGER_TUNNELING;
	Require(!CanBeChilled(aState), "tunneling zombies reject cold effects");
	aState = {}; aState.mFlying = true;
	Require(CanBeChilled(aState) && !CanBeFrozen(aState), "flying targets can chill but cannot freeze");
	aState = {}; aState.mType = ZOMBIE_BUNGEE; aState.mPhase = PHASE_BUNGEE_AT_BOTTOM;
	Require(CanBeFrozen(aState), "landed bungees remain freezable");
	aState.mPhase = PHASE_BUNGEE_GRABBING;
	Require(CanBeChilled(aState) && !CanBeFrozen(aState), "grabbing bungees preserve distinct chill and freeze rules");
	aState = {}; aState.mType = ZOMBIE_BOSS;
	Require(!CanBeChilled(aState), "boss body remains cold immune");
	aState.mPhase = PHASE_BOSS_HEAD_SPIT;
	Require(CanBeFrozen(aState), "exposed boss head preserves its cold eligibility");
}

static void SaveContainerCompatibilityAndFailures()
{
	using namespace SaveGameFormat;
	std::vector<unsigned char> aPayload;
	AppendChunk(aPayload, 1, {1, 0, 0, 0});
	std::vector<unsigned char> aSave;
	Require(static_cast<bool>(EncodePortableSave(aPayload, aSave)), "portable container encodes successfully");
	Require(aSave.size() == HEADER_SIZE_V4 + aPayload.size(), "portable header remains 24 bytes");
	const std::vector<unsigned char> aOriginalFormatFixture = {80, 86, 90, 80, 95, 83, 65, 86, 69, 52, 0, 0, 1, 0, 0, 0, 12, 0, 0, 0, 159, 227, 134, 220, 1, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0};
	Require(aSave == aOriginalFormatFixture, "save container matches the original writer fixture byte for byte");
	Require(aSave[0] == 'P' && aSave[9] == '4' && aSave[12] == 1 && aSave[16] == 12, "portable magic, version and little-endian payload size remain stable");
	auto aDecoded = ValidatePortableSave(aSave);
	Require(static_cast<bool>(aDecoded.mResult) && std::equal(aPayload.begin(), aPayload.end(), aDecoded.mBytes.begin()), "portable payload round trips without changing bytes");
	std::vector<unsigned char> aReencoded;
	Require(static_cast<bool>(EncodePortableSave(aDecoded.mBytes, aReencoded)) && aReencoded == aSave, "save re-encoding preserves its complete container");
	AppendChunk(aPayload, 999, {99, 88, 77});
	EncodePortableSave(aPayload, aSave);
	Require(static_cast<bool>(ValidatePortableSave(aSave).mResult), "unknown future chunks remain accepted");
	aSave.push_back(0);
	Require(static_cast<bool>(ValidatePortableSave(aSave).mResult), "trailing file bytes preserve previous reader compatibility");
	aSave[12] = 2;
	Require(ValidatePortableSave(aSave).mResult.mError == Error::UnsupportedVersion, "unsupported container version fails clearly");
	aSave[12] = 1; aSave[24] ^= 1;
	Require(ValidatePortableSave(aSave).mResult.mError == Error::ChecksumMismatch, "corrupt save bytes fail checksum validation");
	aSave.clear(); EncodePortableSave(aPayload, aSave); aSave.resize(aSave.size() - 1);
	Require(ValidatePortableSave(aSave).mResult.mError == Error::TruncatedPayload, "truncated files fail before state decoding");
	Require(ValidatePortableSave({}).mResult.mError == Error::InvalidHeader, "empty files reject without dereferencing input");
	aPayload.clear(); AppendChunk(aPayload, 999, {}); EncodePortableSave(aPayload, aSave);
	Require(ValidatePortableSave(aSave).mResult.mError == Error::MissingBoard, "unknown chunks cannot replace a required board record");
	aPayload.clear(); AppendChunk(aPayload, 1, {1, 0, 0, 0}); aPayload.push_back(0); EncodePortableSave(aPayload, aSave);
	auto anInvalid = ValidatePortableSave(aSave).mResult;
	Require(anInvalid.mError == Error::InvalidRecord && anInvalid.mOffset == 36, "malformed later records fail before a valid earlier board record is applied");
	const unsigned char aByte = 1; TLVReader aReader(&aByte, 1); const unsigned char* aData = nullptr;
	Require(!aReader.ReadBytes(aData, std::numeric_limits<size_t>::max()) && !aReader.mOk, "oversized TLV lengths cannot overflow bounds checks");
	TLVReader aMissingBuffer(nullptr, 4);
	uint32_t aValue = 0;
	Require(!aMissingBuffer.ReadU32(aValue), "nonempty TLV input requires a buffer");
	TLVReader anEmptyReader(nullptr, 0);
	Require(anEmptyReader.ReadBytes(aData, 0) && aData == nullptr, "empty TLV payload needs no pointer arithmetic");
}

int main(int theArgCount, char** theArgs)
{
	struct TestSuite
	{
		std::string_view mName;
		void (*mRun)();
	};
	constexpr TestSuite SUITES[] = {
		{"strength", StrengthThresholdsAndResistance},
		{"targeting-placement", TargetPriorityAndPlacement},
		{"waves", WavesAndSpawnRestrictions},
		{"cold", ColdEligibility},
		{"save-format", SaveContainerCompatibilityAndFailures}
	};
	Require(theArgCount <= 2, "test runner accepts at most one suite name");
	bool aMatched = false;
	for (const auto& aSuite : SUITES)
		if (theArgCount == 1 || aSuite.mName == theArgs[1])
		{
			aSuite.mRun();
			aMatched = true;
		}
	Require(aMatched, "requested test suite exists");
	return EXIT_SUCCESS;
}
