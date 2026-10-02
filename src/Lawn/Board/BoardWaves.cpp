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
#include "../Rules/WaveRules.h"

#include "BoardConstants.h"

int Board::GetNumWavesPerFlag()
{
	return WaveRules::WavesPerFlag(mApp->IsFirstTimeAdventureMode(), mNumWaves);
}

bool Board::IsFlagWave(int theWaveNumber)
{
	return WaveRules::IsFlagWave(mApp->IsFirstTimeAdventureMode(), mLevel, mNumWaves, theWaveNumber);
}

void ZombiePickerInitForWave(ZombiePicker* theZombiePicker)
{
	theZombiePicker->mZombieCount = 0;
	theZombiePicker->mZombiePoints = 0;
	memset(theZombiePicker->mZombieTypeCount, 0, sizeof(theZombiePicker->mZombieTypeCount));
}

void ZombiePickerInit(ZombiePicker* theZombiePicker)
{
	ZombiePickerInitForWave(theZombiePicker);
	memset(theZombiePicker->mAllWavesZombieTypeCount, 0, sizeof(theZombiePicker->mAllWavesZombieTypeCount));
}

void Board::PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker)
{
	PVZP_ASSERT(theWaveNumber < MAX_ZOMBIE_WAVES && theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE);
	mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount++] = theZombieType;
	if (theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE)
	{
		mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount] = ZombieType::ZOMBIE_INVALID;
	}
	theZombiePicker->mZombiePoints -= GetZombieDefinition(theZombieType).mZombieValue;
	theZombiePicker->mZombieTypeCount[theZombieType]++;
	theZombiePicker->mAllWavesZombieTypeCount[theZombieType]++;
}

void Board::PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker)
{
	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = static_cast<ZombieType>(static_cast<int>(aZombieType) + 1))
	{
		if (theZombiePicker->mZombieTypeCount[aZombieType] <= 0 && aZombieType != ZombieType::ZOMBIE_YETI &&
			aZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR && CanZombieSpawnOnLevel(aZombieType, mLevel))
		{
			PutZombieInWave(aZombieType, theWaveNumber, theZombiePicker);
		}
	}
}

void Board::PickZombieWaves()
{
	// Set the total number of waves
	if (mApp->IsAdventureMode())
	{
		if (mApp->IsWhackAZombieLevel())
		{
			mNumWaves = 8;
		}
		else
		{
			mNumWaves = gZombieWaves[std::clamp(mLevel - 1, 0, NUM_LEVELS - 1)];
			if (!mApp->IsFirstTimeAdventureMode() && !mApp->IsMiniBossLevel())
			{
				mNumWaves = mNumWaves < 10 ? 20 : mNumWaves + 10;
			}
		}
	}
	else
	{
		GameMode aGameMode = mApp->mGameMode;
		if (mApp->IsSurvivalMode() || aGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			mNumWaves = GetNumWavesPerSurvivalStage();
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || aGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM || mApp->IsSquirrelLevel())
			mNumWaves = 0;
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
			mNumWaves = 12;
		else if (aGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || aGameMode == GameMode::GAMEMODE_CHALLENGE_AIR_RAID ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER || aGameMode == GameMode::GAMEMODE_CHALLENGE_HIGH_GRAVITY ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT || aGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL)
			mNumWaves = 20;
		else if (mApp->IsStormyNightLevel() || mApp->IsLittleTroubleLevel() || mApp->IsBungeeBlitzLevel() ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN || mApp->IsShovelLevel() || aGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2 ||
				 aGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2 || aGameMode == GameMode::GAMEMODE_CHALLENGE_POGO_PARTY)
			mNumWaves = 30;
		else
			mNumWaves = 40;
	}

	ZombiePicker aZombiePicker;
	ZombiePickerInit(&aZombiePicker);
	ZombieType aIntroZombieType = GetIntroducedZombieType();
	PVZP_ASSERT(mNumWaves <= MAX_ZOMBIE_WAVES);

	for (int aWave = 0; aWave < mNumWaves; aWave++)
	{
		ZombiePickerInitForWave(&aZombiePicker);
		mZombiesInWave[aWave][0] = ZombieType::ZOMBIE_INVALID;

		bool aIsFlagWave = IsFlagWave(aWave);
		bool aIsFinalWave = aWave == mNumWaves - 1;

		if (mApp->IsBungeeBlitzLevel() && aIsFlagWave)
		{
			for (int _i = 0; _i < 5; _i++)
				PutZombieInWave(ZombieType::ZOMBIE_BUNGEE, aWave, &aZombiePicker);

			if (!aIsFinalWave)
				continue;
		}

		int& aZombiePoints = aZombiePicker.mZombiePoints;
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave + 10) * 2 / 5 + 1;
		}
		else if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave) * 2 / 5 + 1;
		}
		else if (mApp->IsAdventureMode() && mApp->HasFinishedAdventure() && mLevel != 5)
		{
			aZombiePoints = aWave * 2 / 5 + 1;
		}
		else
		{
			aZombiePoints = aWave / 3 + 1;
		}

		if (aIsFlagWave)
		{
			int aPlainZombiesNum = std::min(aZombiePoints, 8);
			aZombiePoints *= 2.5f;

			if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2)
			{
				for (int _i = 0; _i < aPlainZombiesNum; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_NORMAL, aWave, &aZombiePicker);
				}
				PutZombieInWave(ZombieType::ZOMBIE_FLAG, aWave, &aZombiePicker);
			}
		}

		// Certain levels multiply the zombie points
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		{
			aZombiePoints *= 6;
		}
		else if (mApp->IsLittleTroubleLevel() || mApp->IsWallnutBowlingLevel())
		{
			aZombiePoints *= 4;
		}
		else if (mApp->IsMiniBossLevel())
		{
			aZombiePoints *= 3;
		}
		else if (mApp->IsStormyNightLevel() && mApp->IsAdventureMode())
		{
			aZombiePoints *= 3;
		}
		else if (mApp->IsShovelLevel() || mApp->IsBungeeBlitzLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL)
		{
			aZombiePoints *= 2;
		}

		// Newly introduced zombies have fixed spawns in specific waves
		if (aIntroZombieType != ZombieType::ZOMBIE_INVALID && aIntroZombieType != ZombieType::ZOMBIE_DUCKY_TUBE)
		{
			bool aSpawnIntro = false;
			if ((aIntroZombieType == ZombieType::ZOMBIE_DIGGER || aIntroZombieType == ZombieType::ZOMBIE_BALLOON))
			{
				if (aWave + 1== 7 || aIsFinalWave)
				{
					aSpawnIntro = true;
				}
			}
			else if (aIntroZombieType == ZombieType::ZOMBIE_YETI)
			{
				if (aWave == mNumWaves / 2 && !mApp->mSawYeti)
				{
					aSpawnIntro = true;
				}
			}
			else if (aWave == mNumWaves / 2 || aIsFinalWave)
			{
				aSpawnIntro = true;
			}

			if (aSpawnIntro)
			{
				PutZombieInWave(aIntroZombieType, aWave, &aZombiePicker);
			}
		}

		// Level 5-10 adds a Gargantuar to the final wave
		if (mLevel == 50 && aIsFinalWave)
		{
			PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
		}
		// The final wave of an adventure level includes every zombie type of the level
		if (mApp->IsAdventureMode() && aIsFinalWave)
		{
			PutInMissingZombies(aWave, &aZombiePicker);
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		{
			if (aWave % 10 == 5)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_LADDER, aWave, &aZombiePicker);
				}
			}

			if (aWave % 10 == 8)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_JACK_IN_THE_BOX, aWave, &aZombiePicker);
				}
			}

			if (aWave == 19)
			{
				for (int _i = 0; _i < 3; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
			if (aWave == 29)
			{
				for (int _i = 0; _i < 5; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
		}

		// Spend the remaining zombie points on random zombies
		while (aZombiePoints > 0 && aZombiePicker.mZombieCount < MAX_ZOMBIES_IN_WAVE)
		{
			ZombieType aZombieType = PickZombieType(aZombiePoints, aWave, &aZombiePicker);
			PutZombieInWave(aZombieType, aWave, &aZombiePicker);
		}
	}
}

bool Board::CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel)
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(theZombieType);
	if (theZombieType == ZombieType::ZOMBIE_YETI)
	{
		return gLawnApp->CanSpawnYetis();
	}

	if (theLevel < aZombieDef.mStartingLevel || aZombieDef.mPickWeight == 0)
	{
		return false;
	}

	PVZP_ASSERT(gZombieAllowedLevels[theZombieType].mZombieType == theZombieType);
	return gZombieAllowedLevels[theZombieType].mAllowedOnLevel[std::clamp(theLevel - 1, 0, NUM_LEVELS - 1)];
}

ZombieType Board::GetIntroducedZombieType()
{
	if (!mApp->IsAdventureMode() || mLevel == 1)
	{
		return ZombieType::ZOMBIE_INVALID;
	}

	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = static_cast<ZombieType>(static_cast<int>(aZombieType) + 1))
	{
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((aZombieType != ZombieType::ZOMBIE_YETI || mApp->CanSpawnYetis()) && aZombieDef.mStartingLevel == mLevel)
		{
			return aZombieType;
		}
	}
	return ZombieType::ZOMBIE_INVALID;
}

ZombieType Board::PickGraveRisingZombieType()
{
	PvzpWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	int aCount = 2;
	aZombieWeightArray[0].mItem = ZombieType::ZOMBIE_NORMAL;
	aZombieWeightArray[0].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_NORMAL).mPickWeight;
	aZombieWeightArray[1].mItem = ZombieType::ZOMBIE_TRAFFIC_CONE;
	aZombieWeightArray[1].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_TRAFFIC_CONE).mPickWeight;
	if (!StageHasGraveStones())
	{
		aZombieWeightArray[2].mItem = ZombieType::ZOMBIE_PAIL;
		aZombieWeightArray[2].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_PAIL).mPickWeight;
		aCount++;
	}

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = static_cast<ZombieType>(aZombieWeightArray[i].mItem);
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((mApp->IsFirstTimeAdventureMode() && mLevel < aZombieDef.mStartingLevel) || (!mZombieAllowed[aZombieType] && aZombieType != ZombieType::ZOMBIE_NORMAL))
		{
			aZombieWeightArray[i].mWeight = 0;
		}
	}

	return (ZombieType)PvzpPickFromWeightedArray(aZombieWeightArray, aCount);
}

ZombieType Board::PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker)
{
	int aPickCount = 0;
	PvzpWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		if (!mZombieAllowed[aZombieType])
			continue;

		const ZombieDefinition& aZombieDef = GetZombieDefinition((ZombieType)aZombieType);

		// Exclude zombie types that fail the spawn restrictions or exceed the remaining points
		GameMode aGameMode = mApp->mGameMode;
		int aFirstAllowedWave = aZombieDef.mFirstAllowedWave;
		if (mApp->IsSurvivalEndless(aGameMode))
		{
			int aFlags = GetSurvivalFlagsCompleted();
			aFirstAllowedWave = std::max(aFirstAllowedWave - PvzpAnimateCurve(18, 50, aFlags, 0, 15, PvzpCurves::CURVE_LINEAR), 1);
		}
		if (!WaveRules::CanSelectZombie(static_cast<ZombieType>(aZombieType), aGameMode,
			mApp->IsSurvivalEndless(aGameMode), IsFlagWave(theWaveIndex), theWaveIndex,
			theZombiePoints, aFirstAllowedWave, aZombieDef.mZombieValue))
			continue;

		// Survival mode recomputes pick weights from the completed flag count
		int aPickWeight = aZombieDef.mPickWeight;
		if (mApp->IsSurvivalMode())
		{
			int aFlags = GetSurvivalFlagsCompleted();
			// Per-wave spawn cap for Gargantuars and Zomboni.
			if (aZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				int aGargantuarCount = theZombiePicker->mZombieTypeCount[ZombieType::ZOMBIE_GARGANTUAR] +
					theZombiePicker->mZombieTypeCount[ZombieType::ZOMBIE_BULWARK_GARGANTUAR];
				int aCountForCap = aZombieType == ZombieType::ZOMBIE_ZAMBONI ?
					theZombiePicker->mZombieTypeCount[aZombieType] : aGargantuarCount;
				if (aCountForCap >= PvzpAnimateCurve(10, 50, aFlags, 2, 50, PvzpCurves::CURVE_LINEAR))
					continue;
			}
			// Redeye caps: per flag wave, and total across non-flag waves
			else if (aZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
			{
				if (IsFlagWave(theWaveIndex))
				{
					if (theZombiePicker->mZombieTypeCount[aZombieType] >= PvzpAnimateCurve(14, 100, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
				}
				else
				{
					if (theZombiePicker->mAllWavesZombieTypeCount[aZombieType] >= PvzpAnimateCurve(10, 110, aFlags, 1, 50, PvzpCurves::CURVE_LINEAR))
					{
						continue;
					}
					aPickWeight = 1000;
				}
			}
			// Weight decay for normal and conehead zombies
			else if (aZombieType == ZombieType::ZOMBIE_NORMAL)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 10, PvzpCurves::CURVE_LINEAR);
			}
			else if (aZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE)
			{
				aPickWeight = PvzpAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 4, PvzpCurves::CURVE_LINEAR);
			}
		}
		if (mApp->IsSurvivalEndless(aGameMode))
		{
			// Split the existing normal-Garg pick weight 75/25, preserving the combined Garg rate.
			if (aZombieType == ZombieType::ZOMBIE_GARGANTUAR)
				aPickWeight = aPickWeight * 3 / 4;
			else if (aZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
				aPickWeight = GetZombieDefinition(ZombieType::ZOMBIE_GARGANTUAR).mPickWeight / 4;
		}
		else if (aZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
			continue;
		aZombieWeightArray[aPickCount].mItem = aZombieType;
		aZombieWeightArray[aPickCount].mWeight = aPickWeight;
		aPickCount++;
	}

	return (ZombieType)PvzpPickFromWeightedArray(aZombieWeightArray, aPickCount);
}

bool Board::IsZombieTypePoolOnly(ZombieType theZombieType)
{
	return (theZombieType == ZombieType::ZOMBIE_SNORKEL || theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER);
}

bool Board::RowCanHaveZombieType(int theRow, ZombieType theZombieType)
{
	if (!RowCanHaveZombies(theRow))
	{
		return false;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED && mPlantRow[theRow] == PlantRowType::PLANTROW_DIRT && mCurrentWave < 5)
	{
		return false;
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL && !Zombie::ZombieTypeCanGoInPool(theZombieType) && theZombieType != ZombieType::ZOMBIE_BALLOON)
	{
		return false;
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_HIGH_GROUND && !Zombie::ZombieTypeCanGoOnHighGround(theZombieType))
	{
		return false;
	}

	int aCurrentWave = mCurrentWave;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		aCurrentWave += mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage();
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL)
	{
		if (aCurrentWave < 5 && !IsZombieTypePoolOnly(theZombieType))
		{
			return false;
		}
	}
	else if (IsZombieTypePoolOnly(theZombieType))
	{
		return false;
	}
	if (theZombieType == ZOMBIE_BOBSLED && !mIceTimer[theRow])
	{
		if (mSunMoney < TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
			return false;
	}
	if (theRow == 0 && !mApp->IsSurvivalMode())
	{
		if (theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
			theZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR)
		{
			return false;
		}
	}
	if (theZombieType != ZombieType::ZOMBIE_DANCER || StageHasPool())
	{
		return true;
	}
	// Outside pool levels, dancer zombies need rows on both sides to summon backup dancers
	return RowCanHaveZombies(theRow - 1) && RowCanHaveZombies(theRow + 1);
}

int Board::PickRowForNewZombie(ZombieType theZombieType)
{
	// A rake that is attracting a zombie pulls the new zombie into its row
	GridItem* aRake = GetRake();
	if (aRake && aRake->mGridItemState == GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING && RowCanHaveZombieType(aRake->mGridY, theZombieType))
	{
		aRake->mGridItemState = GridItemState::GRIDITEM_STATE_RAKE_WAITING;
		PvzpUpdateSmoothArrayPick(mRowPickingArray, MAX_GRID_SIZE_Y, aRake->mGridY);
		return aRake->mGridY;
	}

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (!RowCanHaveZombieType(aRow, theZombieType))
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
		{
			mRowPickingArray[aRow].mWeight = mChallenge->PortalCombatRowSpawnWeight(aRow);
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_INVISIGHOUL && mCurrentWave <= 3 && aRow == 5)
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		// lawn mower loss protection
		else
		{
			int aWavesMowered = mCurrentWave - mWaveRowGotLawnMowered[aRow];
			if (mApp->IsContinuousChallenge() && mCurrentWave == mNumWaves - 1)
			{
				aWavesMowered = 100;
			}

			if (aWavesMowered <= 1)
			{
				mRowPickingArray[aRow].mWeight = 0.01f;
			}
			else if (aWavesMowered <= 2)
			{
				mRowPickingArray[aRow].mWeight = 0.5f;
			}
			else
			{
				mRowPickingArray[aRow].mWeight = 1.0f;
			}
		}
	}
	return PvzpPickFromSmoothArray(mRowPickingArray, MAX_GRID_SIZE_Y);
}

bool Board::CanAddBobSled()
{
	if (mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		return true;

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (mIceTimer[aRow] > 0 && mIceMinX[aRow] < 700)
		{
			return true;
		}
	}
	return false;
}

Zombie* Board::AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave)
{
	unsigned int aRequiredSlots = theZombieType == ZombieType::ZOMBIE_BOBSLED &&
		theFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE && theFromWave != Zombie::ZOMBIE_WAVE_UI ? 4U : 1U;
	if (mZombies.mSize > mZombies.mMaxSize - aRequiredSlots)
	{
		PvzpLogLn("Too many zombies!!");
		return nullptr;
	}

	if (theZombieType == ZombieType::ZOMBIE_YETI) {
		if (mApp->IsAdventureMode() && mLevel == 40 && theFromWave >= 0)
			ReportAchievement::GiveAchievement(mApp, Zombologist, true);
	}

	bool aVariant = !Rand(5);
	Zombie* aZombie = mZombies.DataArrayAlloc();
	aZombie->ZombieInitialize(theRow, theZombieType, aVariant, nullptr, theFromWave);
	if (theZombieType == ZombieType::ZOMBIE_BOBSLED && aZombie->IsOnBoard())
	{
		for (int _i = 0; _i < 3; _i++)
		{
			mZombies.DataArrayAlloc()->ZombieInitialize(theRow, ZombieType::ZOMBIE_BOBSLED, false, aZombie, theFromWave);
		}
	}
	return aZombie;
}

Zombie* Board::AddZombie(ZombieType theZombieType, int theFromWave)
{
	return AddZombieInRow(theZombieType, PickRowForNewZombie(theZombieType), theFromWave);
}

void Board::RemoveAllZombies()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying())
		{
			aZombie->DieNoLoot();
		}
	}
}

void Board::RemoveZombiesForRepick()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying() && aZombie->mMindControlled && aZombie->mPosX > 720)
		{
			aZombie->DieNoLoot();
		}
	}
}

void Board::RemoveCutsceneZombies()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			aZombie->DieNoLoot();
		}
	}
}

int Board::GetGraveStonesCount()
{
	int aCount = 0;

	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			aCount++;
		}
	}

	return aCount;
}

void Board::PickSpecialGraveStone()
{
	GridItem* aPicks[MAX_GRAVE_STONES];
	int aPickCount = 0;
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			PVZP_ASSERT(aPickCount < MAX_GRAVE_STONES);
			aPicks[aPickCount] = aGridItem;
			aPickCount++;
		}
	}

	if (aPickCount > 0)
	{
		PvzpPickFromArray(aPicks, aPickCount)->mGridItemState = GridItemState::GRIDITEM_STATE_GRAVESTONE_SPECIAL;
	}
}

void Board::SpawnZombiesFromPool()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 21 || mLevel == 22 || mLevel == 31 || mLevel == 32)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 23 || mLevel == 24 || mLevel == 25 || mLevel == 33 || mLevel == 34 || mLevel == 35)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	int aGridArrayCount = 0;
	PvzpWeightedGridArray aGridArray[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	for (int aGridX = 5; aGridX < GetNumPlayableColumns(); aGridX++)
	{
		for (int aGridY = 0; aGridY < GetNumPlayableRows(); aGridY++)
		{
			if (mPlantRow[aGridY] != PlantRowType::PLANTROW_POOL)
				continue;
			aGridArray[aGridArrayCount].mX = aGridX;
			aGridArray[aGridArrayCount].mY = aGridY;
			aGridArray[aGridArrayCount].mWeight = 10000;
			aGridArrayCount++;
			PVZP_ASSERT(aGridArrayCount <= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y);
		}
	}

	aGridArrayCount = std::max(aGridArrayCount, 0);
	for (int i = 0; i < aCount; i++)
	{
		PvzpWeightedGridArray* aGrid = PvzpPickFromWeightedGridArray(aGridArray, aGridArrayCount);
		aGrid->mWeight = 0;

		ZombieType aZombieType = PickGraveRisingZombieType();
		Zombie* aZombie = AddZombieInRow(aZombieType, aGrid->mY, mCurrentWave);
		if (aZombie == nullptr)
		{
			return;
		}

		aZombie->RiseFromGrave(aGrid->mX, aGrid->mY);
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		aZombiePoints = std::max(aZombiePoints, 1);
	}
}

void Board::SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid)
{
	theBungeeDropGrid->mGridArrayCount = 0;
	for (int aGridX = 4; aGridX < GetNumPlayableColumns(); aGridX++)
	{
		for (int aGridY = 0; aGridY <= 4; aGridY++)
		{
			int aCount = theBungeeDropGrid->mGridArrayCount;
			theBungeeDropGrid->mGridArray[aCount].mX = aGridX;
			theBungeeDropGrid->mGridArray[aCount].mY = aGridY;
			theBungeeDropGrid->mGridArray[aCount].mWeight = 10000;
			theBungeeDropGrid->mGridArrayCount++;
			PVZP_ASSERT(static_cast<size_t>(theBungeeDropGrid->mGridArrayCount) <= LENGTH(theBungeeDropGrid->mGridArray));
		}
	}
}

void Board::BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType)
{
	PvzpWeightedGridArray* aGrid = PvzpPickFromWeightedGridArray(theBungeeDropGrid->mGridArray, theBungeeDropGrid->mGridArrayCount);
	aGrid->mWeight = 1;

	Zombie* aBungeeZombie = AddZombie(ZombieType::ZOMBIE_BUNGEE, mCurrentWave);
	Zombie* aZombie = AddZombie(theZombieType, mCurrentWave);
	PVZP_ASSERT(aBungeeZombie && aZombie);

	aBungeeZombie->BungeeDropZombie(aZombie, aGrid->mX, aGrid->mY);
}

void Board::SpawnZombiesFromSky()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 41 || mLevel == 42)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 43 || mLevel == 44 || mLevel == 45)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	BungeeDropGrid aBungeeDropGrid;
	SetupBungeeDrop(&aBungeeDropGrid);
	aCount = std::min(aCount, aBungeeDropGrid.mGridArrayCount);

	if (aBungeeDropGrid.mGridArrayCount == 0 || aCount <= 0)
		return;

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = PickGraveRisingZombieType();
		BungeeDropZombie(&aBungeeDropGrid, aZombieType);
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		aZombiePoints = std::max(aZombiePoints, 1);
	}
}

void Board::SpawnZombiesFromGraves()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_WAR_AND_PEAS_2)
		return;

	if (StageHasRoof())
	{
		SpawnZombiesFromSky();
	}
	else if (StageHasPool())
	{
		SpawnZombiesFromPool();
		return;
	}

//	int aZombiePoints = GetGraveStonesCount();
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType != GridItemType::GRIDITEM_GRAVESTONE || aGridItem->mGridItemCounter < 100)
		{
			continue;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_GRAVE_DANGER && Rand(mNumWaves) > mCurrentWave)
		{
			continue;
		}

		ZombieType aZombieType = PickGraveRisingZombieType();
		Zombie* aZombie = AddZombie(aZombieType, mCurrentWave);
		if (aZombie == nullptr)
		{
			return;
		}

		aZombie->RiseFromGrave(aGridItem->mGridX, aGridItem->mGridY);
		/*
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		if (aZombieType < 1)
		{
			aZombiePoints = 1;
		}
		*/
	}
}

int Board::TotalZombiesHealthInWave(int theWaveIndex)
{
	int aTotalHealth = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == theWaveIndex && !aZombie->mMindControlled && !aZombie->IsDeadOrDying() &&
			aZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && aZombie->mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
		{
			aTotalHealth += aZombie->mBodyHealth + aZombie->mHelmHealth + aZombie->mShieldHealth * 0.2f + aZombie->mFlyingHealth;
		}
	}
	return aTotalHealth;
}

void Board::SpawnZombieWave()
{
	mChallenge->SpawnZombieWave();
	bool aWaveAlreadyHasBungee = false;
	if (mApp->IsBungeeBlitzLevel())
	{
		BungeeDropGrid aBungeeDropGrid;
		SetupBungeeDrop(&aBungeeDropGrid);
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;
			if (aZombieType == ZombieType::ZOMBIE_BUNGEE)
				aWaveAlreadyHasBungee = true;

			if (aZombieType == ZombieType::ZOMBIE_BUNGEE || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				AddZombie(aZombieType, mCurrentWave);
			}
			else
			{
				BungeeDropZombie(&aBungeeDropGrid, aZombieType);
			}
		}
	}
	else
	{
		PVZP_ASSERT(mCurrentWave >= 0 && mCurrentWave < MAX_ZOMBIE_WAVES && mCurrentWave < mNumWaves);
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;
			if (aZombieType == ZombieType::ZOMBIE_BUNGEE)
				aWaveAlreadyHasBungee = true;

			if (aZombieType == ZombieType::ZOMBIE_BOBSLED && !CanAddBobSled())
			{
				for (int i = 0; i < MAX_ZOMBIE_FOLLOWERS; i++)
				{
					AddZombie(ZombieType::ZOMBIE_NORMAL, mCurrentWave);  // spawn 4 normal zombies instead of the bobsled team
				}
			}
			else
			{
				AddZombie(aZombieType, mCurrentWave);
			}
		}
	}
	if (mZombieTierSunMoney >= TWO_MILLION_SUN_THRESHOLD && mZombieAllowed[ZombieType::ZOMBIE_BUNGEE] &&
		!mApp->IsBungeeBlitzLevel() && !aWaveAlreadyHasBungee)
	{
		bool anEndlessMode = mApp->IsSurvivalEndless(mApp->mGameMode);
		bool aBypassesFirstAllowedWave = mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_POGO_PARTY ||
			mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BOBSLED_BONANZA ||
			mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_AIR_RAID;
		bool aWaveAllowed = anEndlessMode ? IsFlagWave(mCurrentWave) :
			(aBypassesFirstAllowedWave || mCurrentWave + 1 >= GetZombieDefinition(ZombieType::ZOMBIE_BUNGEE).mFirstAllowedWave);
		bool aFrequencyWave = anEndlessMode
			? ((mCurrentWave / std::max(1, GetNumWavesPerFlag())) % 2 == 0)
			: (mCurrentWave % 2 == 0);
		if (aWaveAllowed && aFrequencyWave)
			AddZombie(ZombieType::ZOMBIE_BUNGEE, mCurrentWave);
	}

	if (mCurrentWave == mNumWaves - 1 && !mApp->IsContinuousChallenge())
	{
		mRiseFromGraveCounter = 200;
	}
	if (IsFlagWave(mCurrentWave))
	{
		mFlagRaiseCounter = FLAG_RAISE_TIME;
	}
	mCurrentWave++;
	mTotalSpawnedWaves++;
}

static bool IsZombieRainExcludedType(ZombieType theZombieType)
{
	switch (theZombieType)
	{
	case ZombieType::ZOMBIE_GARGANTUAR:
	case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
	case ZombieType::ZOMBIE_ZAMBONI:
	case ZombieType::ZOMBIE_CATAPULT:
	case ZombieType::ZOMBIE_BOBSLED:
	case ZombieType::ZOMBIE_BOSS:
		return true;
	default:
		return false;
	}
}

void Board::UpdateZombieRain()
{
	if (!mZombieRainActive || mApp->mGameScene != GameScenes::SCENE_PLAYING || mCurrentWave <= 0)
		return;

	if (mZombieRainPendingCount <= 0)
	{
		if (mZombieRainCountdown > 0)
			--mZombieRainCountdown;
		if (mZombieRainCountdown > 0)
			return;
		mZombieRainPendingCount = RandRangeInt(5, 50);
	}

	if (mZombieRainPendingCount > static_cast<int>(mZombies.mMaxSize - mZombies.mSize))
	{
		return;
	}

	std::vector<ZombieType> aTypes;
	if (mNumWaves > 0)
	{
		int aWaveIndex = std::clamp(mCurrentWave - 1, 0, std::min(mNumWaves, MAX_ZOMBIE_WAVES) - 1);
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aType = mZombiesInWave[aWaveIndex][i];
			if (aType == ZombieType::ZOMBIE_INVALID)
				break;
			if (!IsZombieRainExcludedType(aType))
				aTypes.push_back(aType);
		}
	}
	if (aTypes.empty())
		aTypes.push_back(ZombieType::ZOMBIE_NORMAL);

	struct ZombieRainSpawn
	{
		ZombieType mType;
		int mRow;
		int mColumn;
	};
	std::vector<ZombieRainSpawn> aSpawns;
	std::vector<std::pair<int, int>> aUsedCells;
	int aRequestedCount = mZombieRainPendingCount;
	int aFromWave = std::max(mCurrentWave - 1, 0);
	for (int i = 0; i < aRequestedCount; i++)
	{
		bool aFoundCell = false;
		for (int aAttempt = 0; aAttempt < static_cast<int>(aTypes.size()) && !aFoundCell; aAttempt++)
		{
			ZombieType aType = aTypes[Rand(static_cast<int>(aTypes.size()))];
			std::vector<std::pair<int, int>> aCells;
			for (int aRow = 0; aRow < GetNumPlayableRows(); aRow++)
			{
				if (!RowCanHaveZombieType(aRow, aType))
					continue;
				for (int aColumn = std::max(0, GetNumPlayableColumns() - 3); aColumn < GetNumPlayableColumns(); aColumn++)
					aCells.emplace_back(aColumn, aRow);
			}
			if (aCells.empty())
				continue;

			std::vector<std::pair<int, int>> aUnusedCells;
			for (const auto& aCell : aCells)
				if (std::find(aUsedCells.begin(), aUsedCells.end(), aCell) == aUsedCells.end())
					aUnusedCells.push_back(aCell);
			if (!aUnusedCells.empty())
				aCells.swap(aUnusedCells);

			const auto& aCell = aCells[Rand(static_cast<int>(aCells.size()))];
			aUsedCells.push_back(aCell);
			aSpawns.push_back({ aType, aCell.second, aCell.first });
			aFoundCell = true;
		}
		if (!aFoundCell)
		{
			std::vector<std::pair<int, int>> aNormalCells;
			for (int aRow = 0; aRow < GetNumPlayableRows(); aRow++)
			{
				if (!RowCanHaveZombieType(aRow, ZombieType::ZOMBIE_NORMAL))
					continue;
				for (int aColumn = std::max(0, GetNumPlayableColumns() - 3); aColumn < GetNumPlayableColumns(); aColumn++)
					aNormalCells.emplace_back(aColumn, aRow);
			}
			if (aNormalCells.empty())
			{
				mZombieRainPendingCount = aRequestedCount - i;
				return;
			}
			std::vector<std::pair<int, int>> aUnusedCells;
			for (const auto& aCell : aNormalCells)
				if (std::find(aUsedCells.begin(), aUsedCells.end(), aCell) == aUsedCells.end())
					aUnusedCells.push_back(aCell);
			if (!aUnusedCells.empty())
				aNormalCells.swap(aUnusedCells);
			const auto& aCell = aNormalCells[Rand(static_cast<int>(aNormalCells.size()))];
			aUsedCells.push_back(aCell);
			aSpawns.push_back({ ZombieType::ZOMBIE_NORMAL, aCell.second, aCell.first });
		}
	}

	for (size_t i = 0; i < aSpawns.size(); i++)
	{
		const ZombieRainSpawn& aSpawn = aSpawns[i];
		Zombie* aZombie = AddZombieInRow(aSpawn.mType, aSpawn.mRow, aFromWave);
		if (aZombie == nullptr)
		{
			mZombieRainPendingCount = static_cast<int32_t>(aSpawns.size() - i);
			return;
		}
		aZombie->mSpawnedByZombieRain = true;
		if (aSpawn.mType == ZombieType::ZOMBIE_BUNGEE)
			continue;

		aZombie->mPosX = static_cast<float>(GridToPixelX(aSpawn.mColumn, aSpawn.mRow));
		aZombie->mPosY = aZombie->GetPosYBasedOnRow(aSpawn.mRow);
		aZombie->mX = static_cast<int>(aZombie->mPosX);
		aZombie->mY = static_cast<int>(aZombie->mPosY);
		aZombie->mAltitude = RandRangeInt(150, 300);
		aZombie->mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	}

	mZombieRainPendingCount = 0;
	mZombieRainCountdown = RandRangeInt(1000, 3000);
}

void Board::UpdateSunSpawning()
{
	if (StageIsNight() ||
		HasLevelAwardDropped() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_UPSELL ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND ||
		mApp->IsIZombieLevel() ||
		mApp->IsScaryPotterLevel() ||
		mApp->IsSquirrelLevel() ||
		HasConveyorBeltSeedBank() ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER || mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		if (mPlants.mSize == 0)
		{
			return;
		}
	}

	mSunCountDown--;
	if (mSunCountDown != 0)
		return;

	mNumSunsFallen++;
	mSunCountDown = std::min(SUN_COUNTDOWN_MAX, SUN_COUNTDOWN + mNumSunsFallen * 10) + Rand(SUN_COUNTDOWN_RANGE);
	CoinType aSunType = mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_SUNNY_DAY ? CoinType::COIN_LARGESUN : CoinType::COIN_SUN;
	AddCoin(RandRangeInt(100, 649), 60, aSunType, CoinMotion::COIN_MOTION_FROM_SKY);
}

void Board::NextWaveComing()
{
	if (mCurrentWave + 1 == mNumWaves)
	{
		if (!IsSurvivalStageWithRepick() && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_LAST_STAND && !mApp->IsContinuousChallenge())
		{
			mApp->AddReanimation(0, 30, MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0), ReanimationType::REANIM_FINAL_WAVE);
			mFinalWaveSoundCounter = 60;
		}
	}

	if (mCurrentWave == 0)
	{
		mApp->PlaySample(Sexy::SOUND_AWOOGA);
	}
	else if (mApp->IsWhackAZombieLevel() ? (mCurrentWave == mNumWaves - 1) : IsFlagWave(mCurrentWave))
	{
		mApp->PlaySample(Sexy::SOUND_SIREN);
	}
}

void Board::UpdateZombieSpawning()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_PLANT_PRACTICE)
		return;
	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;

	if (mFinalWaveSoundCounter > 0)
	{
		mFinalWaveSoundCounter--;
		if (mFinalWaveSoundCounter == 0)
		{
			mApp->PlaySample(Sexy::SOUND_FINALWAVE);
		}
	}

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (HasLevelAwardDropped())
		return;

	if (mRiseFromGraveCounter > 0)
	{
		mRiseFromGraveCounter--;
		if (mRiseFromGraveCounter == 0)
		{
			SpawnZombiesFromGraves();
		}
	}

	if (mHugeWaveCountDown > 0)
	{
		mHugeWaveCountDown--;
		if (mHugeWaveCountDown == 0)
		{
			ClearAdvice(AdviceType::ADVICE_HUGE_WAVE);
			NextWaveComing();
			mZombieCountDown = 1;
		}
		else
		{
			if (mHugeWaveCountDown == 725)
			{
				mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
			}
			else
			{
				if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
				{
					if (mHugeWaveCountDown == 400)
					{
						mApp->mMusic->StartBurst();
					}
				}
				else if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
				{
					if (mHugeWaveCountDown == 700)
					{
						mApp->mMusic->StartBurst();
					}
				}
				return;
			}
		}
	}

	if (mChallenge->UpdateZombieSpawning())
		return;

	if (mCurrentWave == mNumWaves)
	{
		if (IsFinalSurvivalStage())
		{
			return;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			return;
		}
		if (!mApp->IsSurvivalMode() && !mApp->IsContinuousChallenge())
		{
			return;
		}
	}

	mZombieCountDown--;
	if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
	{
		if (mZombieCountDown == 0)
		{
			FadeOutLevel();
		}
		return;
	}

	if (mZombieCountDown > 200 && mZombieCountDownStart - mZombieCountDown > 400 && TotalZombiesHealthInWave(mCurrentWave - 1) <= mZombieHealthToNextWave)
	{
		mZombieCountDown = 200;
	}
	if (mZombieCountDown == 5)
	{
		if (IsFlagWave(mCurrentWave))
		{
			ClearAdviceImmediately();
			DisplayAdviceAgain("[ADVICE_HUGE_WAVE]", MessageStyle::MESSAGE_STYLE_HUGE_WAVE, AdviceType::ADVICE_HUGE_WAVE);
			mHugeWaveCountDown = 750;
			return;
		}
		NextWaveComing();
	}
	if (mZombieCountDown == 0)
	{
		SpawnZombieWave();
		mZombieHealthWaveStart = TotalZombiesHealthInWave(mCurrentWave - 1);

		if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
		{
			mZombieHealthToNextWave = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_REPICK + 1;
		}
		else if (IsFlagWave(mCurrentWave) && !(mApp->IsWallnutBowlingLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND))
		{
			mZombieHealthToNextWave = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_FLAG;
		}
		else
		{
			mZombieHealthToNextWave = RandRangeFloat(0.5f, 0.65f) * mZombieHealthWaveStart;
			if (mApp->IsLittleTroubleLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
			{
				mZombieCountDown = 750;
			}
			else
			{
				mZombieCountDown = ZOMBIE_COUNTDOWN + Rand(ZOMBIE_COUNTDOWN_RANGE);
			}
		}
		mZombieCountDownStart = mZombieCountDown;
	}
}

int Board::GetNumWavesPerSurvivalStage()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND || mApp->IsSurvivalNormal(mApp->mGameMode))
	{
		return 10;
	}
	else if (mApp->IsSurvivalEndless(mApp->mGameMode))
	{
		return 30;
	}
	else if (mApp->IsSurvivalHard(mApp->mGameMode))
	{
		return 20;
	}

	PVZP_ASSERT(false);

	unreachable();
}

int Board::NumberZombiesInWave(int theWaveIndex)
{
	PVZP_ASSERT(theWaveIndex >= 0 && theWaveIndex < MAX_ZOMBIE_WAVES && theWaveIndex < mNumWaves);

	for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
	{
		if (mZombiesInWave[theWaveIndex][i] == ZombieType::ZOMBIE_INVALID)
		{
			return i;
		}
	}

	PVZP_ASSERT(false);
	return 0;
}

bool Board::IsZombieTypeSpawnedOnly(ZombieType theZombieType)
{
	return (theZombieType == ZombieType::ZOMBIE_BACKUP_DANCER || theZombieType == ZombieType::ZOMBIE_BOBSLED || theZombieType == ZombieType::ZOMBIE_IMP);
}
