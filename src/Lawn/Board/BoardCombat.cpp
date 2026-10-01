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

ZombieID Board::ZombieGetID(Zombie* theZombie)
{
	return static_cast<ZombieID>(mZombies.DataArrayGetID(theZombie));
}

Zombie* Board::ZombieGet(ZombieID theZombieID)
{
	return mZombies.DataArrayGet(static_cast<unsigned int>(theZombieID));
}

Zombie* Board::ZombieTryToGet(ZombieID theZombieID)
{
	return mZombies.DataArrayTryToGet(static_cast<unsigned int>(theZombieID));
}

int GetRectOverlap(const Rect& rect1, const Rect& rect2)
{
	return std::min(rect1.mX + rect1.mWidth, rect2.mX + rect2.mWidth) -
		std::max(rect1.mX, rect2.mX);
}

bool GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Rect& theRect)
{
	int aNearX = std::clamp(theCircleX, theRect.mX, theRect.mX + theRect.mWidth);
	int aNearY = std::clamp(theCircleY, theRect.mY, theRect.mY + theRect.mHeight);
	int dx = theCircleX - aNearX;
	int dy = theCircleY - aNearY;
	return dx * dx + dy * dy <= theRadius * theRadius;
}

void Board::KillAllPlantsInRadius(int theX, int theY, int theRadius)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (GetCircleRectOverlap(theX, theY, theRadius, aPlant->GetPlantRect()))
		{
			int aDivisor = mZombieStrengthTier >= 3 ? 6 : 3;
			int aDamage = std::max(1, (aPlant->mPlantMaxHealth + aDivisor - 1) / aDivisor);
			aPlant->mPlantHealth -= aDamage;
			aPlant->mEatenFlashCountdown = std::max(aPlant->mEatenFlashCountdown, 25);
			if (aPlant->mPlantHealth <= 0)
			{
				mPlantsEaten++;
				aPlant->Die();
			}
		}
	}
}

unsigned int Board::SeedNotRecommendedForLevel(SeedType theSeedType)
{
	unsigned int aNotRec = 0;
	if (Plant::IsNocturnal(theSeedType) && !StageIsNight())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NOCTURNAL, true);
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE && StageIsNight())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_AT_NIGHT, true);
	}
	if (theSeedType == SeedType::SEED_GRAVEBUSTER && !StageHasGraveStones())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_GRAVES, true);
	}
	if (theSeedType == SeedType::SEED_PLANTERN && !StageHasFog())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_FOG, true);
	}
	if (theSeedType == SeedType::SEED_FLOWERPOT && !StageHasRoof())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_ROOF, true);
	}
	if (StageHasRoof() && (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK))
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_ON_ROOF, true);
	}
	if (!StageHasPool() && Plant::IsAquatic(theSeedType))
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_POOL, true);
	}
	return aNotRec;
}

int Board::CountCoinByType(CoinType theCoinType)
{
	int aCount = 0;

	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mType == theCoinType)
		{
			aCount++;
		}
	}

	return aCount;
}

int Board::GetGraveStoneCount()
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

bool Board::BungeeIsTargetingCell(int theGridX, int theGridY)
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mRow == theGridY && aZombie->mTargetCol == theGridX)
		{
			return true;
		}
	}
	return false;
}

Zombie* Board::GetBossZombie()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
		{
			return aZombie;
		}
	}
	return nullptr;
}

Plant* Board::FindUmbrellaPlant(int theGridX, int theGridY)
{
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_UMBRELLA && !aPlant->NotOnGround() && GridInRange(theGridX, theGridY, aPlant->mPlantCol, aPlant->mRow, 1, 1))
		{
			return aPlant;
		}
	}
	return nullptr;
}

bool Board::PlantingRequirementsMet(SeedType theSeedType)
{
	switch (theSeedType)
	{
	case SeedType::SEED_GATLINGPEA:			return CountPlantByType(SeedType::SEED_REPEATER);
	case SeedType::SEED_TWINSUNFLOWER:		return CountPlantByType(SeedType::SEED_SUNFLOWER);
	case SeedType::SEED_GLOOMSHROOM:		return CountPlantByType(SeedType::SEED_FUMESHROOM);
	case SeedType::SEED_CATTAIL:			return CountEmptyPotsOrLilies(SeedType::SEED_LILYPAD);
	case SeedType::SEED_WINTERMELON:		return CountPlantByType(SeedType::SEED_MELONPULT);
	case SeedType::SEED_GOLD_MAGNET:		return CountPlantByType(SeedType::SEED_MAGNETSHROOM);
	case SeedType::SEED_SPIKEROCK:			return CountPlantByType(SeedType::SEED_SPIKEWEED);
	case SeedType::SEED_COBCANNON:			return HasValidCobCannonSpot();
	default:								return true;
	}
}

int Board::KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags, int theExplosiveDamage, bool theQuadraticDamageByTargetCount)
{
	int aKilledZombies = 0;
	int aTargetCount = 0;
	auto IsAffectedByBlast = [&](Zombie* theZombie)
	{
		if (theZombie->mDead || !theZombie->EffectedByDamage(theDamageRangeFlags))
			return false;
		Rect aZombieRect = theZombie->GetZombieRect();
		int aRowDist = theZombie->mRow - theRow;
		if (theZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			aRowDist = 0;
		return aRowDist <= theRowRange && aRowDist >= -theRowRange &&
			GetCircleRectOverlap(theX, theY, theRadius, aZombieRect);
	};
	if (theQuadraticDamageByTargetCount)
		for (Zombie* aZombie : mZombies)
			if (IsAffectedByBlast(aZombie))
				++aTargetCount;
	for (Zombie* aZombie : mZombies)
	{
		if (IsAffectedByBlast(aZombie))
		{
			if (theBurn)
			{
				aZombie->ApplyBurn();
				if (theQuadraticDamageByTargetCount && aTargetCount > 1 && !aZombie->IsDeadOrDying())
				{
					int aDamageMultiplier = GetQuadraticZombieDamageMultiplier(aZombie, aTargetCount);
					aZombie->TakeDamage(GetZombieExplosiveDamage() * std::max(0, aDamageMultiplier - 1), 18U);
				}
			}
			else
			{
				int aDamage = theExplosiveDamage > 0 ? theExplosiveDamage : GetZombieExplosiveDamage();
				if (theQuadraticDamageByTargetCount)
					aDamage *= GetQuadraticZombieDamageMultiplier(aZombie, aTargetCount);
				aZombie->TakeDamage(aDamage, 18U);
			}
			aKilledZombies++;
		}
	}

	int aGridX = PixelToGridXKeepOnBoard(theX, theY);
	int aGridY = PixelToGridYKeepOnBoard(theX, theY);
	for (GridItem* aGridItem : mGridItems)
	{
		if (aGridItem->mDead)
			continue;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
		{
			if (GridInRange(aGridItem->mGridX, aGridItem->mGridY, aGridX, aGridY, theRowRange, theRowRange))
			{
				aGridItem->GridItemDie();
			}
		}
	}

	return aKilledZombies;
}

LawnMower* Board::FindLawnMowerInRow(int theRow)
{
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mRow == theRow)
		{
			return aLawnMower;
		}
	}
	return nullptr;
}

Zombie* Board::GetWinningZombie()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_WINNER)
		{
			return aZombie;
		}
	}
	return nullptr;
}

int Board::CountZombieByType(ZombieType theZombieType)
{
	int aCount = 0;

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mZombieType == theZombieType)
		{
			aCount++;
		}
	}

	return aCount;
}
