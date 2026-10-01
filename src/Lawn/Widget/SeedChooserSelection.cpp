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
#include "../Zombie/Zombie.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../Modes/Cutscene.h"
#include "SeedPacket.h"
#include "../../LawnApp.h"
#include "AlmanacDialog.h"
#include "ImitaterDialog.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "../Plant/Plant.h"
#include "ToolTipWidget.h"
#include "SeedChooserScreen.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "misc/Debug.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "widget/WidgetManager.h"
#include <algorithm>






#include "SeedChooserOrder.h"

int SeedChooserScreen::PickFromWeightedArrayUsingSpecialRandSeed(PvzpWeightedArray* theArray, int theCount, MTRand& theLevelRNG)
{
	int aTotalWeight = 0;
	for (int i = 0; i < theCount; i++)
		aTotalWeight += theArray[i].mWeight;
	DBG_ASSERT(aTotalWeight > 0);

	int aRndResult = theLevelRNG.Next((unsigned long)aTotalWeight);
	int aWeight = 0;
	for (int j = 0; j < theCount; j++)
	{
		aWeight += theArray[j].mWeight;
		if (aWeight > aRndResult) return theArray[j].mItem;
	}
	DBG_ASSERT(false);
	unreachable();
}

void SeedChooserScreen::CrazyDavePickSeeds()
{
	PvzpWeightedArray aSeedArray[NUM_SEEDS_IN_CHOOSER];
	for (int aSeedIndex = 0; aSeedIndex < NUM_SEEDS_IN_CHOOSER; aSeedIndex++)
	{
		SeedType aSeedType = SeedChooserOrder::SeedChooserTypeAtIndex(aSeedIndex);
		aSeedArray[aSeedIndex].mItem = aSeedType;
		if (!mApp->HasSeedType(aSeedType) || SeedNotRecommendedToPick(aSeedType) || SeedNotAllowedToPick(aSeedType) || Plant::IsUpgrade(aSeedType) ||
			aSeedType == SEED_IMITATER || aSeedType == SEED_UMBRELLA || aSeedType == SEED_BLOVER)
		{
			aSeedArray[aSeedIndex].mWeight = 0;
		}
		else
		{
			aSeedArray[aSeedIndex].mWeight = 1;
		}
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BUNGEE] || mBoard->mZombieAllowed[ZOMBIE_CATAPULT])
	{
		aSeedArray[SeedChooserOrder::SeedChooserIndexOf(SEED_UMBRELLA)].mWeight = 1;
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BALLOON] || mBoard->StageHasFog())
	{
		aSeedArray[SeedChooserOrder::SeedChooserIndexOf(SEED_BLOVER)].mWeight = 1;
	}
	if (mBoard->StageHasRoof())
	{
		aSeedArray[SeedChooserOrder::SeedChooserIndexOf(SEED_TORCHWOOD)].mWeight = 0;
	}

	MTRand aLevelRNG = MTRand(mBoard->GetLevelRandSeed());
	for (int i = 0; i < 3; i++)
	{
		SeedType aPickedSeed = (SeedType)PickFromWeightedArrayUsingSpecialRandSeed(aSeedArray, NUM_SEEDS_IN_CHOOSER, aLevelRNG);
		aSeedArray[SeedChooserOrder::SeedChooserIndexOf(aPickedSeed)].mWeight = 0;
		ChosenSeed& aChosenSeed = mChosenSeeds[aPickedSeed];

		int aPosX = mBoard->GetSeedPacketPositionX(i);
		aChosenSeed.mX = aPosX;
		aChosenSeed.mY = 8;
		aChosenSeed.mStartX = aPosX;
		aChosenSeed.mStartY = 8;
		aChosenSeed.mEndX = aPosX;
		aChosenSeed.mEndY = 8;
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = i;
		aChosenSeed.mCrazyDavePicked = true;
		mSeedsInBank++;
	}
}

SeedChooserScreen::~SeedChooserScreen() = default;

unsigned int SeedChooserScreen::SeedNotRecommendedToPick(SeedType theSeedType)
{
	uint aRecFlags = mBoard->SeedNotRecommendedForLevel(theSeedType);
	if (TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL) && PickedPlantType(SEED_INSTANT_COFFEE))
		SetBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL, false);
	return aRecFlags;
}

bool SeedChooserScreen::SeedNotAllowedToPick(SeedType theSeedType)
{
	return mApp->mGameMode == GAMEMODE_CHALLENGE_LAST_STAND && (theSeedType == SEED_SUNFLOWER || theSeedType == SEED_SUNSHROOM ||
		theSeedType == SEED_TWINSUNFLOWER || theSeedType == SEED_SEASHROOM || theSeedType == SEED_PUFFSHROOM);
}

bool SeedChooserScreen::SeedNotAllowedDuringTrial(SeedType theSeedType)
{
	return mApp->IsTrialStageLocked() && (theSeedType == SEED_SQUASH || theSeedType == SEED_THREEPEATER);
}

bool SeedChooserScreen::DisplayRepickWarningDialog(const char* theMessage)
{
	return mApp->LawnMessageBox(
		Dialogs::DIALOG_CHOOSER_WARNING,
		"[DIALOG_WARNING]",
		theMessage,
		"[DIALOG_BUTTON_YES]",
		"[REPICK_BUTTON]",
		Dialog::BUTTONS_YES_NO
	) == Dialog::ID_YES;
}

bool SeedChooserScreen::FlyersAreComming()
{
	for (int aWave = 0; aWave < mBoard->mNumWaves; aWave++)
	{
		for (int anIndex = 0; anIndex < MAX_ZOMBIES_IN_WAVE; anIndex++)
		{
			ZombieType aZombieType = mBoard->mZombiesInWave[aWave][anIndex];
			if (aZombieType == ZOMBIE_INVALID)
				break;

			if (aZombieType == ZOMBIE_BALLOON)
				return true;
		}
	}
	return false;
}

bool SeedChooserScreen::FlyProtectionCurrentlyPlanted()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SEED_CATTAIL || aPlant->mSeedType == SEED_CACTUS)
		{
			return true;
		}
	}
	return false;
}

bool SeedChooserScreen::CheckSeedUpgrade(SeedType theSeedTypeTo, SeedType theSeedTypeFrom)
{
	if (mApp->IsSurvivalMode() || !PickedPlantType(theSeedTypeTo) || PickedPlantType(theSeedTypeFrom))
		return true;

	std::string aWarning(PvzpStringTranslate("[SEED_CHOOSER_UPGRADE_WARNING]"));
	aWarning = PvzpReplaceString(aWarning, "{UPGRADE_TO}", Plant::GetNameString(theSeedTypeTo));
	aWarning = PvzpReplaceString(aWarning, "{UPGRADE_FROM}", Plant::GetNameString(theSeedTypeFrom));
	return DisplayRepickWarningDialog(aWarning.c_str());
}

void SeedChooserScreen::OnStartButton()
{
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_SEEING_STARS && !PickedPlantType(SEED_STARFRUIT))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_SEEING_STARS_WARNING]"))
		{
			return;
		}
	}

	if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11 && !PickedPlantType(SEED_PUFFSHROOM))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_PUFFSHROOM_WARNING]"))
		{
			return;
		}
	}
	if (!PickedPlantType(SEED_SUNFLOWER) && !PickedPlantType(SEED_TWINSUNFLOWER) && !PickedPlantType(SEED_SUNSHROOM) &&
		!mBoard->mCutScene->IsSurvivalRepick() && mApp->mGameMode != GAMEMODE_CHALLENGE_LAST_STAND)
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11)
		{
			if (!DisplayRepickWarningDialog("[SEED_CHOOSER_NIGHT_SUN_WARNING]"))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog("[SEED_CHOOSER_SUN_WARNING]"))
		{
			return;
		}
	}
	if (mBoard->StageHasPool() && !PickedPlantType(SEED_LILYPAD) && !PickedPlantType(SEED_SEASHROOM) && !PickedPlantType(SEED_TANGLEKELP) && !mBoard->mCutScene->IsSurvivalRepick())
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 21)
		{
			if (!DisplayRepickWarningDialog("[SEED_CHOOSER_LILY_WARNING]"))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog("[SEED_CHOOSER_POOL_WARNING]"))
		{
			return;
		}
	}
	if (mBoard->StageHasRoof() && !PickedPlantType(SEED_FLOWERPOT) && mApp->HasSeedType(SEED_FLOWERPOT))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_ROOF_WARNING]"))
		{
			return;
		}
	}

	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT && !PickedPlantType(SEED_WALLNUT))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_ART_WALLNUT_WARNING]"))
		{
			return;
		}
	}
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER &&
		(!PickedPlantType(SEED_STARFRUIT) || !PickedPlantType(SEED_UMBRELLA) || !PickedPlantType(SEED_WALLNUT)))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_ART_2_WARNING]"))
		{
			return;
		}
	}

	if (FlyersAreComming() && !FlyProtectionCurrentlyPlanted() && !PickedPlantType(SEED_CATTAIL) && !PickedPlantType(SEED_CACTUS) && !PickedPlantType(SEED_BLOVER))
	{
		if (!DisplayRepickWarningDialog("[SEED_CHOOSER_FLYER_WARNING]"))
		{
			return;
		}
	}

	if (!CheckSeedUpgrade(SEED_GATLINGPEA, SEED_REPEATER) ||
		!CheckSeedUpgrade(SEED_WINTERMELON, SEED_MELONPULT) ||
		!CheckSeedUpgrade(SEED_TWINSUNFLOWER, SEED_SUNFLOWER) ||
		!CheckSeedUpgrade(SEED_SPIKEROCK, SEED_SPIKEWEED) ||
		!CheckSeedUpgrade(SEED_COBCANNON, SEED_KERNELPULT) ||
		!CheckSeedUpgrade(SEED_GOLD_MAGNET, SEED_MAGNETSHROOM) ||
		!CheckSeedUpgrade(SEED_GLOOMSHROOM, SEED_FUMESHROOM) ||
		!CheckSeedUpgrade(SEED_CATTAIL, SEED_LILYPAD))
		return;

	CloseSeedChooser();
}

void SeedChooserScreen::PickRandomSeeds()
{
	for (int anIndex = mSeedsInBank; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType;
		int aRegularSeedCount = mApp->GetSeedsAvailable();
		int aCandidateCount = aRegularSeedCount + (mApp->HasSeedType(SeedType::SEED_SUN_MAGNET) ? 1 : 0);
		do
		{
			int aSeedIndex = Rand(aCandidateCount);
			aSeedType = aSeedIndex == aRegularSeedCount ? SeedType::SEED_SUN_MAGNET : static_cast<SeedType>(aSeedIndex);
		}
		while (!mApp->HasSeedType(aSeedType) || aSeedType == SEED_IMITATER || mChosenSeeds[aSeedType].mSeedState != SEED_IN_CHOOSER);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		aChosenSeed.mTimeStartMotion = 0;
		aChosenSeed.mTimeEndMotion = 0;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		GetSeedPositionInBank(anIndex, aChosenSeed.mEndX, aChosenSeed.mEndY);
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = anIndex;
		mSeedsInBank++;
	}
	for (int aSeedIndex = 0; aSeedIndex < NUM_SEEDS_IN_CHOOSER; aSeedIndex++)
		LandFlyingSeed(mChosenSeeds[SeedChooserOrder::SeedChooserTypeAtIndex(aSeedIndex)]);
	CloseSeedChooser();
}

SeedType SeedChooserScreen::SeedHitTest(int x, int y)
{
	if (mMouseVisible)
	{
		for (int aSeedIndex = 0; aSeedIndex < NUM_SEEDS_IN_CHOOSER; aSeedIndex++)
		{
			SeedType aSeedType = SeedChooserOrder::SeedChooserTypeAtIndex(aSeedIndex);
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (!mApp->HasSeedType(aSeedType) || aChosenSeed.mSeedState == SEED_PACKET_HIDDEN) continue;
			if (Rect(aChosenSeed.mX, aChosenSeed.mY, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT).Contains(x, y)) return aSeedType;
		}
	}
	return SEED_NONE;
}

SeedType SeedChooserScreen::FindSeedInBank(int theIndexInBank)
{
	for (int aSeedIndex = 0; aSeedIndex < NUM_SEEDS_IN_CHOOSER; aSeedIndex++)
	{
		SeedType aSeedType = SeedChooserOrder::SeedChooserTypeAtIndex(aSeedIndex);
		if (mApp->HasSeedType(aSeedType))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (aChosenSeed.mSeedState == SEED_IN_BANK && aChosenSeed.mSeedIndexInBank == theIndexInBank) return aSeedType;
		}
	}
	return SEED_NONE;
}

void SeedChooserScreen::ClickedSeedInBank(ChosenSeed& theChosenSeed)
{
	for (int anIndex = theChosenSeed.mSeedIndexInBank + 1; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		if (aSeedType != SEED_NONE)
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			aChosenSeed.mTimeStartMotion = mSeedChooserAge;
			aChosenSeed.mTimeEndMotion = mSeedChooserAge + 15;
			aChosenSeed.mStartX = aChosenSeed.mX;
			aChosenSeed.mStartY = aChosenSeed.mY;
			GetSeedPositionInBank(anIndex - 1, aChosenSeed.mEndX, aChosenSeed.mEndY);
			aChosenSeed.mSeedState = SEED_FLYING_TO_BANK;
			aChosenSeed.mSeedIndexInBank = anIndex - 1;
			mSeedsInFlight++;
		}
	}
	theChosenSeed.mTimeStartMotion = mSeedChooserAge;
	theChosenSeed.mTimeEndMotion = mSeedChooserAge + 25;
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	GetSeedPositionInChooser(theChosenSeed.mSeedType, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_CHOOSER;
	theChosenSeed.mSeedIndexInBank = 0;
	mSeedsInBank--;
	mSeedsInFlight++;
	RemoveToolTip();
	EnableStartButton(false);
	mApp->PlaySample(Sexy::SOUND_TAP);
}

void SeedChooserScreen::ClickedSeedInChooser(ChosenSeed& theChosenSeed)
{
	if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		return;

	theChosenSeed.mTimeStartMotion = mSeedChooserAge;
	theChosenSeed.mTimeEndMotion = mSeedChooserAge + 25;
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	GetSeedPositionInBank(mSeedsInBank, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_BANK;
	theChosenSeed.mSeedIndexInBank = mSeedsInBank;
	mSeedsInFlight++;
	mSeedsInBank++;

	RemoveToolTip();
	mApp->PlaySample(Sexy::SOUND_TAP);
	if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		EnableStartButton(true);
}

bool SeedChooserScreen::PickedPlantType(SeedType theSeedType)
{
	for (int aSeedIndex = 0; aSeedIndex < NUM_SEEDS_IN_CHOOSER; aSeedIndex++)
	{
		SeedType aSeedType = SeedChooserOrder::SeedChooserTypeAtIndex(aSeedIndex);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		if (aChosenSeed.mSeedState == SEED_IN_BANK)
		{
			if (aChosenSeed.mSeedType == theSeedType || (aChosenSeed.mSeedType == SEED_IMITATER && aChosenSeed.mImitaterType == theSeedType))
			{
				return true;
			}
		}
	}
	return false;
}
