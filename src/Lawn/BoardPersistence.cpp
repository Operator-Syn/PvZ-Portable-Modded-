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

void Board::TryToSaveGame()
{
	std::string aFileName = GetSavedGameName(mApp->mGameMode, mApp->mPlayerInfo->mId);

	if (NeedSaveGame())
	{
		if (mBoardFadeOutCounter > 0)
		{
			CompleteEndLevelSequenceForSaving();
			return;
		}

		MkDir(GetAppDataPath("userdata"));
		mApp->mMusic->GameMusicPause(true);
		SaveGame(aFileName);
		mApp->ClearUpdateBacklog();
		SurvivalSaveScore();
	}
}

bool Board::NeedSaveGame()
{
	return
		mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ICE &&
		mApp->mGameMode != GameMode::GAMEMODE_UPSELL &&
		mApp->mGameMode != GameMode::GAMEMODE_INTRO &&
		mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN &&
		mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING;
}

void Board::SaveGame(const std::string& theFileName)
{
	const auto aResult = LawnSaveGameDetailed(this, theFileName);
	if (!aResult)
		PvzpLogLn("Save failed: {} (offset {}, record {})", SaveGameFormat::ErrorMessage(aResult.mError), aResult.mOffset, aResult.mRecordType);
}

bool Board::LoadGame(const std::string& theFileName)
{
	mPlantHealGlows.clear();
	mPlantHealVisuals.clear();
	mChomperHealAuras.clear();
	mSunMagnetHealStacks.clear();
	mSunMagnetExtraItems.clear();
	mChomperOverdriveActive = false;
	mKernelPultOverdriveActive = false;
	mSunMagnetOverdriveActive = false;
	mGoldMagnetOverdriveActive = false;
	mCatTailOverdriveActive = false;
	mSpikeweedOverdriveActive = false;
	mGatlingPeaOverdriveActive = false;
	mTwinSunflowerProductionOverdriveActive = false;
	mTwinSunflowerBombardmentOverdriveActive = false;
	mTwinSunflowerHighOverdriveActive = false;
	mPumpkinOverdriveActive = false;
	mTallNutOverdriveActive = false;
	mZombieStrengthTier = 0;
	mZombieTierSunMoney = 0;
	mAutoReuseEndlessSeeds = false;
	mZombieRainActive = false;
	mZombieRainCountdown = 0;
	mZombieRainPendingCount = 0;
	mPlanternFlameCountdown.fill(0);
	mPlanternFlameTick.fill(0);
	mPlanternFlameDamagePercent.fill(100);
	const auto aResult = LawnLoadGameDetailed(this, theFileName);
	if (!aResult)
	{
		PvzpLogLn("Load failed: {} (offset {}, record {})", SaveGameFormat::ErrorMessage(aResult.mError), aResult.mOffset, aResult.mRecordType);
		return false;
	}
	PvzpLogLn("Loaded save game");
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		const std::string aDetailsJson = std::format(
			R"({{"mower_id":{},"mower_type":{},"row_index":{},"lane":{},"state":{},"dead":{},"visible":{},"x":{:.2f},"y":{:.2f}}})",
			mLawnMowers.DataArrayGetID(aLawnMower), static_cast<int>(aLawnMower->mMowerType), aLawnMower->mRow,
			aLawnMower->mRow + 1, static_cast<int>(aLawnMower->mMowerState),
			aLawnMower->mDead ? "true" : "false", aLawnMower->mVisible ? "true" : "false",
			aLawnMower->mPosX, aLawnMower->mPosY);
		RecordGameplayEvent("lawn_mower_restored", aDetailsJson);
	}
	for (Plant* aPlant : mPlants)
	{
		if (!aPlant->mDead && aPlant->mPlantMaxHealth == 300 && aPlant->mPlantHealth > 0)
		{
			// Older saves keep the previous default plant health. Preserve lost HP while applying the +200 HP increase.
			aPlant->mPlantMaxHealth = 500;
			aPlant->mPlantHealth = std::min(500, aPlant->mPlantHealth + 200);
		}
	}

	RestorePlantHealGlowsAfterLoad();
	LoadBackgroundImages();
	mApp->ClearUpdateBacklog();
	ResetFPSStats();
	UpdateLayers();
	return true;
}
