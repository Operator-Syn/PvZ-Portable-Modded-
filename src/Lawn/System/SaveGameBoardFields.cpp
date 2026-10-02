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

#include "Music.h"
#include "SaveGame.h"
#include "../Board/Board.h"
#include "../Modes/Challenge.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../Widget/MessageWidget.h"
#include "../../PvzpLib/Trail.h"
#include "zlib.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/DataArray.h"
#include "../../PvzpLib/PvzpList.h"
#include "DataSync.h"
#include "../../GameConstants.h"
#include "misc/Buffer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>


#include "SaveGameInternal.h"

namespace SaveGameInternal
{
// Field IDs for Board base: These IDs are part of the on-disk format and must NOT be renumbered.
enum BoardBaseFieldId : uint32_t
{
	BOARD_FIELD_PAUSED = 1,
	BOARD_FIELD_GRID_SQUARE_TYPE,
	BOARD_FIELD_GRID_CEL_LOOK,
	BOARD_FIELD_GRID_CEL_OFFSET,
	BOARD_FIELD_GRID_CEL_FOG,
	BOARD_FIELD_ENABLE_GRAVESTONES,
	BOARD_FIELD_SPECIAL_GRAVESTONE_X,
	BOARD_FIELD_SPECIAL_GRAVESTONE_Y,
	BOARD_FIELD_FOG_OFFSET,
	BOARD_FIELD_FOG_BLOWN_COUNTDOWN,
	BOARD_FIELD_PLANT_ROW,
	BOARD_FIELD_WAVE_ROW_GOT_LAWN_MOWERED,
	BOARD_FIELD_BONUS_LAWN_MOWERS_REMAINING,
	BOARD_FIELD_ICE_MIN_X,
	BOARD_FIELD_ICE_TIMER,
	BOARD_FIELD_ICE_PARTICLE_ID,
	BOARD_FIELD_ROW_PICKING_ARRAY,
	BOARD_FIELD_ZOMBIES_IN_WAVE,
	BOARD_FIELD_ZOMBIE_ALLOWED,
	BOARD_FIELD_SUN_COUNTDOWN,
	BOARD_FIELD_NUM_SUNS_FALLEN,
	BOARD_FIELD_SHAKE_COUNTER,
	BOARD_FIELD_SHAKE_AMOUNT_X,
	BOARD_FIELD_SHAKE_AMOUNT_Y,
	BOARD_FIELD_BACKGROUND,
	BOARD_FIELD_LEVEL,
	BOARD_FIELD_SOD_POSITION,
	BOARD_FIELD_PREV_MOUSE_X,
	BOARD_FIELD_PREV_MOUSE_Y,
	BOARD_FIELD_SUN_MONEY,
	BOARD_FIELD_NUM_WAVES,
	BOARD_FIELD_MAIN_COUNTER,
	BOARD_FIELD_EFFECT_COUNTER,
	BOARD_FIELD_DRAW_COUNT,
	BOARD_FIELD_RISE_FROM_GRAVE_COUNTER,
	BOARD_FIELD_OUT_OF_MONEY_COUNTER,
	BOARD_FIELD_CURRENT_WAVE,
	BOARD_FIELD_TOTAL_SPAWNED_WAVES,
	BOARD_FIELD_TUTORIAL_STATE,
	BOARD_FIELD_TUTORIAL_PARTICLE_ID,
	BOARD_FIELD_TUTORIAL_TIMER,
	BOARD_FIELD_LAST_BUNGEE_WAVE,
	BOARD_FIELD_ZOMBIE_HEALTH_TO_NEXT_WAVE,
	BOARD_FIELD_ZOMBIE_HEALTH_WAVE_START,
	BOARD_FIELD_ZOMBIE_COUNTDOWN,
	BOARD_FIELD_ZOMBIE_COUNTDOWN_START,
	BOARD_FIELD_HUGE_WAVE_COUNTDOWN,
	BOARD_FIELD_HELP_DISPLAYED,
	BOARD_FIELD_HELP_INDEX,
	BOARD_FIELD_FINAL_BOSS_KILLED,
	BOARD_FIELD_SHOW_SHOVEL,
	BOARD_FIELD_COIN_BANK_FADE_COUNT,
	BOARD_FIELD_DEBUG_TEXT_MODE,
	BOARD_FIELD_LEVEL_COMPLETE,
	BOARD_FIELD_BOARD_FADE_OUT_COUNTER,
	BOARD_FIELD_NEXT_SURVIVAL_STAGE_COUNTER,
	BOARD_FIELD_SCORE_NEXT_MOWER_COUNTER,
	BOARD_FIELD_LEVEL_AWARD_SPAWNED,
	BOARD_FIELD_PROGRESS_METER_WIDTH,
	BOARD_FIELD_FLAG_RAISE_COUNTER,
	BOARD_FIELD_ICE_TRAP_COUNTER,
	BOARD_FIELD_BOARD_RAND_SEED,
	BOARD_FIELD_POOL_SPARKLY_PARTICLE_ID,
	BOARD_FIELD_FWOOSH_ID,
	BOARD_FIELD_FWOOSH_COUNTDOWN,
	BOARD_FIELD_TIME_STOP_COUNTER,
	BOARD_FIELD_DROPPED_FIRST_COIN,
	BOARD_FIELD_FINAL_WAVE_SOUND_COUNTER,
	BOARD_FIELD_COB_CANNON_CURSOR_DELAY_COUNTER,
	BOARD_FIELD_COB_CANNON_MOUSE_X,
	BOARD_FIELD_COB_CANNON_MOUSE_Y,
	BOARD_FIELD_KILLED_YETI,
	BOARD_FIELD_MUSTACHE_MODE,
	BOARD_FIELD_SUPER_MOWER_MODE,
	BOARD_FIELD_FUTURE_MODE,
	BOARD_FIELD_PINATA_MODE,
	BOARD_FIELD_DANCE_MODE,
	BOARD_FIELD_DAISY_MODE,
	BOARD_FIELD_SUKHBIR_MODE,
	BOARD_FIELD_PREV_BOARD_RESULT,
	BOARD_FIELD_TRIGGERED_LAWN_MOWERS,
	BOARD_FIELD_PLAY_TIME_ACTIVE_LEVEL,
	BOARD_FIELD_PLAY_TIME_INACTIVE_LEVEL,
	BOARD_FIELD_MAX_SUN_PLANTS,
	BOARD_FIELD_START_DRAW_TIME,
	BOARD_FIELD_INTERVAL_DRAW_TIME,
	BOARD_FIELD_INTERVAL_DRAW_COUNT_START,
	BOARD_FIELD_MIN_FPS,
	BOARD_FIELD_PRELOAD_TIME,
	BOARD_FIELD_GAME_ID,
	BOARD_FIELD_GRAVES_CLEARED,
	BOARD_FIELD_PLANTS_EATEN,
	BOARD_FIELD_PLANTS_SHOVELED,
	BOARD_FIELD_PEA_SHOOTER_USED,
	BOARD_FIELD_CATAPULT_PLANTS_USED,
	BOARD_FIELD_MUSHROOM_AND_COFFEE_BEANS_ONLY,
	BOARD_FIELD_MUSHROOMS_USED,
	BOARD_FIELD_LEVEL_COINS_COLLECTED,
	BOARD_FIELD_GARGANTUARS_KILLS_BY_CORN_COB,
	BOARD_FIELD_COINS_COLLECTED,
	BOARD_FIELD_DIAMONDS_COLLECTED,
	BOARD_FIELD_POTTED_PLANTS_COLLECTED,
	BOARD_FIELD_CHOCOLATE_COLLECTED,
	BOARD_FIELD_EXPANDED_BOARD_ROWS,
	BOARD_FIELD_FINAL_EXPANDED_BOARD_ROWS,
	BOARD_FIELD_PLANT_HEAL_GLOWS,
	BOARD_FIELD_EXPANDED_BOARD_COLUMNS,
	BOARD_FIELD_PLANT_HEAL_VISUALS,
	BOARD_FIELD_CHOMPER_HEAL_AURAS,
	BOARD_FIELD_PLANT_OVERDRIVE_STATE,
	BOARD_FIELD_TWIN_SUNFLOWER_OVERDRIVE_STATE,
	BOARD_FIELD_SUN_MAGNET_OVERDRIVE_STATE,
	BOARD_FIELD_SUN_MAGNET_EXTRA_ITEMS,
	BOARD_FIELD_ZOMBIE_STRENGTH_AND_RAIN_STATE,
	BOARD_FIELD_PLANT_OVERDRIVE_EXTENSIONS,
	BOARD_FIELD_PUMPKIN_TALLNUT_OVERDRIVE_STATE,
	BOARD_FIELD_SUN_MAGNET_HEAL_STACKS,
	BOARD_FIELD_AUTO_REUSE_ENDLESS_SEEDS,
	BOARD_FIELD_SPIKEWEED_OVERDRIVE_STATE,
	BOARD_FIELD_PLANTERN_FLAMES,
	BOARD_FIELD_SUN_MAGNET_HIGH_EXTRA_ITEMS,
	BOARD_FIELD_PARTICLE_SHAKE_SCALE,
	BOARD_FIELD_PLANTERN_FLAME_DAMAGE_PERCENT,
	BOARD_FIELD_ZOMBIE_TIER_SUN_MONEY,
	BOARD_FIELD_CONTINUOUS_SUN_COST_REMAINDER,
	BOARD_FIELD_CONTINUOUS_PAID_GLOOM_COUNT,
	BOARD_FIELD_COUNT
};

struct BoardBaseFieldEntry
{
	uint32_t	mFieldId;
	void		(*mSync)(PortableSaveContext&, Board*);
};

template <typename TSync>
static void SyncBoardGridRows(PortableSaveContext& theContext, int theFirstRow, int theRowCount, TSync theSync)
{
	for (int x = 0; x < LEGACY_BOARD_GRID_SIZE_X; x++)
	{
		for (int y = theFirstRow; y < theFirstRow + theRowCount; y++)
		{
			theSync(x, y);
		}
	}
}

static void SyncPlantHealGlows(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mPlantHealGlows.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE))
	{
		theContext.mFailed = true;
		if (theContext.mReading)
			theBoard->mPlantHealGlows.clear();
		return;
	}

	if (theContext.mReading)
		theBoard->mPlantHealGlows.resize(static_cast<size_t>(aCount));
	for (Board::PlantHealGlow& aGlow : theBoard->mPlantHealGlows)
	{
		SyncEnumU32(theContext, aGlow.mPlantID);
		SyncEnumU32(theContext, aGlow.mParticleID);
		theContext.SyncInt32(aGlow.mElapsedTicks);
		theContext.SyncInt32(aGlow.mTicksUntilPulse);
	}
}

static void SyncPlantHealVisuals(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mPlantHealVisuals.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE))
	{
		theContext.mFailed = true;
		if (theContext.mReading)
			theBoard->mPlantHealVisuals.clear();
		return;
	}
	if (theContext.mReading)
		theBoard->mPlantHealVisuals.resize(static_cast<size_t>(aCount));
	for (Board::PlantHealVisual& aVisual : theBoard->mPlantHealVisuals)
	{
		SyncEnumU32(theContext, aVisual.mPlantID);
		SyncEnumU32(theContext, aVisual.mParticleID);
		theContext.SyncInt32(aVisual.mElapsedTicks);
		theContext.SyncInt32(aVisual.mCreationRetryTicks);
	}
}

static void SyncChomperHealAuras(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mChomperHealAuras.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE))
	{
		theContext.mFailed = true;
		if (theContext.mReading)
			theBoard->mChomperHealAuras.clear();
		return;
	}
	if (theContext.mReading)
		theBoard->mChomperHealAuras.resize(static_cast<size_t>(aCount));
	for (Board::ChomperHealAura& anAura : theBoard->mChomperHealAuras)
	{
		SyncEnumU32(theContext, anAura.mPlantID);
		SyncEnumU32(theContext, anAura.mChomperID);
		theContext.SyncInt32(anAura.mTicksUntilPulse);
	}
}

static void SyncSunMagnetExtraItems(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mSunMagnetExtraItems.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE))
	{
		theContext.mFailed = true;
		if (theContext.mReading)
			theBoard->mSunMagnetExtraItems.clear();
		return;
	}
	if (theContext.mReading)
		theBoard->mSunMagnetExtraItems.resize(static_cast<size_t>(aCount));
	for (Board::SunMagnetExtraItems& anItems : theBoard->mSunMagnetExtraItems)
	{
		SyncEnumU32(theContext, anItems.mPlantID);
		for (int i = 0; i < SUN_MAGNET_OVERDRIVE_EXTRA_ITEMS; i++)
			SyncMagnetItemPortable(theContext, anItems.mItems[i]);
	}
}

static void SyncSunMagnetHighExtraItems(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mSunMagnetExtraItems.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE) ||
		(theContext.mReading && static_cast<size_t>(aCount) != theBoard->mSunMagnetExtraItems.size()))
	{
		theContext.mFailed = true;
		return;
	}

	for (Board::SunMagnetExtraItems& anItems : theBoard->mSunMagnetExtraItems)
	{
		PlantID aPlantID = anItems.mPlantID;
		SyncEnumU32(theContext, aPlantID);
		if (theContext.mReading && aPlantID != anItems.mPlantID)
		{
			theContext.mFailed = true;
			return;
		}
		for (int i = SUN_MAGNET_OVERDRIVE_EXTRA_ITEMS; i < SUN_MAGNET_HIGH_OVERDRIVE_EXTRA_ITEMS; i++)
			SyncMagnetItemPortable(theContext, anItems.mItems[i]);
	}
}

static void SyncZombieStrengthAndRainState(PortableSaveContext& theContext, Board* theBoard)
{
	theContext.SyncInt32(theBoard->mZombieStrengthTier);
	theContext.SyncBool(theBoard->mZombieRainActive);
	theContext.SyncInt32(theBoard->mZombieRainCountdown);
	theContext.SyncInt32(theBoard->mZombieRainPendingCount);
	if (theContext.mReading && (theBoard->mZombieStrengthTier < 0 || theBoard->mZombieStrengthTier > 9 ||
		theBoard->mZombieRainCountdown < 0 || theBoard->mZombieRainCountdown > 3000 ||
		theBoard->mZombieRainPendingCount < 0 || theBoard->mZombieRainPendingCount > 50))
	{
		theContext.mFailed = true;
		theBoard->mZombieStrengthTier = 0;
		theBoard->mZombieRainActive = false;
		theBoard->mZombieRainCountdown = 0;
		theBoard->mZombieRainPendingCount = 0;
	}
}

static void SyncPlantOverdriveExtensions(PortableSaveContext& theContext, Board* theBoard)
{
	theContext.SyncBool(theBoard->mGoldMagnetOverdriveActive);
	theContext.SyncBool(theBoard->mCatTailOverdriveActive);
	theContext.SyncBool(theBoard->mGatlingPeaOverdriveActive);
	theContext.SyncBool(theBoard->mTwinSunflowerHighOverdriveActive);
}

static void SyncPumpkinTallNutOverdriveState(PortableSaveContext& theContext, Board* theBoard)
{
	theContext.SyncBool(theBoard->mPumpkinOverdriveActive);
	theContext.SyncBool(theBoard->mTallNutOverdriveActive);
}

static void SyncSunMagnetHealStacks(PortableSaveContext& theContext, Board* theBoard)
{
	int32_t aCount = theContext.mReading ? 0 : static_cast<int32_t>(theBoard->mSunMagnetHealStacks.size());
	theContext.SyncInt32(aCount);
	if (aCount < 0 || aCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE))
	{
		theContext.mFailed = true;
		if (theContext.mReading)
			theBoard->mSunMagnetHealStacks.clear();
		return;
	}
	if (theContext.mReading)
		theBoard->mSunMagnetHealStacks.resize(static_cast<size_t>(aCount));
	for (Board::SunMagnetHealStack& aStack : theBoard->mSunMagnetHealStacks)
	{
		SyncEnumU32(theContext, aStack.mPlantID);
		SyncEnumU32(theContext, aStack.mMagnetID);
		theContext.SyncInt32(aStack.mStackCount);
		theContext.SyncInt32(aStack.mElapsedTicks);
		theContext.SyncInt32(aStack.mTicksUntilPulse);
		if (theContext.mReading && (aStack.mStackCount < 1 || aStack.mStackCount > 1000 ||
			aStack.mElapsedTicks < 0 || aStack.mElapsedTicks >= 300 ||
			aStack.mTicksUntilPulse < 1 || aStack.mTicksUntilPulse > 100))
		{
			theContext.mFailed = true;
			aStack.mStackCount = 0;
		}
	}
}

// Single source of truth for Board base fields: field order is the write order.
static constexpr BoardBaseFieldEntry gBoardBaseFields[] = {
	{ BOARD_FIELD_PAUSED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mPaused); } },
	{ BOARD_FIELD_GRID_SQUARE_TYPE, [](PortableSaveContext& c, Board* theBoard){ SyncBoardGridRows(c, 0, LEGACY_BOARD_GRID_SIZE_Y, [&](int x, int y){ SyncEnum32(c, theBoard->mGridSquareType[x][y]); }); } },
	{ BOARD_FIELD_GRID_CEL_LOOK, [](PortableSaveContext& c, Board* theBoard){ SyncBoardGridRows(c, 0, LEGACY_BOARD_GRID_SIZE_Y, [&](int x, int y){ c.SyncInt32(theBoard->mGridCelLook[x][y]); }); } },
	{ BOARD_FIELD_GRID_CEL_OFFSET, [](PortableSaveContext& c, Board* theBoard){ SyncBoardGridRows(c, 0, LEGACY_BOARD_GRID_SIZE_Y, [&](int x, int y){ c.SyncInt32(theBoard->mGridCelOffset[x][y][0]); c.SyncInt32(theBoard->mGridCelOffset[x][y][1]); }); } },
	{ BOARD_FIELD_GRID_CEL_FOG, [](PortableSaveContext& c, Board* theBoard){ SyncBoardGridRows(c, 0, LEGACY_BOARD_GRID_SIZE_Y + 1, [&](int x, int y){ c.SyncInt32(theBoard->mGridCelFog[x][y]); }); } },
	{ BOARD_FIELD_ENABLE_GRAVESTONES, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mEnableGraveStones); } },
	{ BOARD_FIELD_SPECIAL_GRAVESTONE_X, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mSpecialGraveStoneX); } },
	{ BOARD_FIELD_SPECIAL_GRAVESTONE_Y, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mSpecialGraveStoneY); } },
	{ BOARD_FIELD_FOG_OFFSET, [](PortableSaveContext& c, Board* theBoard){ c.SyncFloat(theBoard->mFogOffset); } },
	{ BOARD_FIELD_FOG_BLOWN_COUNTDOWN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mFogBlownCountDown); } },
	{ BOARD_FIELD_PLANT_ROW, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32Array(c, &theBoard->mPlantRow[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_WAVE_ROW_GOT_LAWN_MOWERED, [](PortableSaveContext& c, Board* theBoard){ SyncInt32Array(c, &theBoard->mWaveRowGotLawnMowered[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_BONUS_LAWN_MOWERS_REMAINING, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mBonusLawnMowersRemaining); } },
	{ BOARD_FIELD_ICE_MIN_X, [](PortableSaveContext& c, Board* theBoard){ SyncInt32Array(c, &theBoard->mIceMinX[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_ICE_TIMER, [](PortableSaveContext& c, Board* theBoard){ SyncInt32Array(c, &theBoard->mIceTimer[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_ICE_PARTICLE_ID, [](PortableSaveContext& c, Board* theBoard){ SyncEnumU32Array(c, &theBoard->mIceParticleID[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_ROW_PICKING_ARRAY, [](PortableSaveContext& c, Board* theBoard){ SyncPvzpSmoothArrayList(c, &theBoard->mRowPickingArray[0], LEGACY_BOARD_GRID_SIZE_Y); } },
	{ BOARD_FIELD_ZOMBIES_IN_WAVE, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32Array(c, &theBoard->mZombiesInWave[0][0], MAX_ZOMBIE_WAVES * MAX_ZOMBIES_IN_WAVE); } },
	{ BOARD_FIELD_ZOMBIE_ALLOWED, [](PortableSaveContext& c, Board* theBoard){ SyncBoolArray(c, &theBoard->mZombieAllowed[0], 100); } },
	{ BOARD_FIELD_SUN_COUNTDOWN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mSunCountDown); } },
	{ BOARD_FIELD_NUM_SUNS_FALLEN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mNumSunsFallen); } },
	{ BOARD_FIELD_SHAKE_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mShakeCounter); } },
	{ BOARD_FIELD_SHAKE_AMOUNT_X, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mShakeAmountX); } },
	{ BOARD_FIELD_SHAKE_AMOUNT_Y, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mShakeAmountY); } },
	{ BOARD_FIELD_BACKGROUND, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32(c, theBoard->mBackground); } },
	{ BOARD_FIELD_LEVEL, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mLevel); } },
	{ BOARD_FIELD_SOD_POSITION, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mSodPosition); } },
	{ BOARD_FIELD_PREV_MOUSE_X, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mPrevMouseX); } },
	{ BOARD_FIELD_PREV_MOUSE_Y, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mPrevMouseY); } },
	{ BOARD_FIELD_SUN_MONEY, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mSunMoney); } },
	{ BOARD_FIELD_NUM_WAVES, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mNumWaves); } },
	{ BOARD_FIELD_MAIN_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mMainCounter); } },
	{ BOARD_FIELD_EFFECT_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mEffectCounter); } },
	{ BOARD_FIELD_DRAW_COUNT, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mDrawCount); } },
	{ BOARD_FIELD_RISE_FROM_GRAVE_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mRiseFromGraveCounter); } },
	{ BOARD_FIELD_OUT_OF_MONEY_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mOutOfMoneyCounter); } },
	{ BOARD_FIELD_CURRENT_WAVE, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mCurrentWave); } },
	{ BOARD_FIELD_TOTAL_SPAWNED_WAVES, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mTotalSpawnedWaves); } },
	{ BOARD_FIELD_TUTORIAL_STATE, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32(c, theBoard->mTutorialState); } },
	{ BOARD_FIELD_TUTORIAL_PARTICLE_ID, [](PortableSaveContext& c, Board* theBoard){ SyncEnumU32(c, theBoard->mTutorialParticleID); } },
	{ BOARD_FIELD_TUTORIAL_TIMER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mTutorialTimer); } },
	{ BOARD_FIELD_LAST_BUNGEE_WAVE, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mLastBungeeWave); } },
	{ BOARD_FIELD_ZOMBIE_HEALTH_TO_NEXT_WAVE, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mZombieHealthToNextWave); } },
	{ BOARD_FIELD_ZOMBIE_HEALTH_WAVE_START, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mZombieHealthWaveStart); } },
	{ BOARD_FIELD_ZOMBIE_COUNTDOWN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mZombieCountDown); } },
	{ BOARD_FIELD_ZOMBIE_COUNTDOWN_START, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mZombieCountDownStart); } },
	{ BOARD_FIELD_HUGE_WAVE_COUNTDOWN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mHugeWaveCountDown); } },
	{ BOARD_FIELD_HELP_DISPLAYED, [](PortableSaveContext& c, Board* theBoard){ SyncBoolArray(c, &theBoard->mHelpDisplayed[0], NUM_ADVICE_TYPES); } },
	{ BOARD_FIELD_HELP_INDEX, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32(c, theBoard->mHelpIndex); } },
	{ BOARD_FIELD_FINAL_BOSS_KILLED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mFinalBossKilled); } },
	{ BOARD_FIELD_SHOW_SHOVEL, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mShowShovel); } },
	{ BOARD_FIELD_COIN_BANK_FADE_COUNT, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mCoinBankFadeCount); } },
	{ BOARD_FIELD_DEBUG_TEXT_MODE, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32(c, theBoard->mDebugTextMode); } },
	{ BOARD_FIELD_LEVEL_COMPLETE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mLevelComplete); } },
	{ BOARD_FIELD_BOARD_FADE_OUT_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mBoardFadeOutCounter); } },
	{ BOARD_FIELD_NEXT_SURVIVAL_STAGE_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mNextSurvivalStageCounter); } },
	{ BOARD_FIELD_SCORE_NEXT_MOWER_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mScoreNextMowerCounter); } },
	{ BOARD_FIELD_LEVEL_AWARD_SPAWNED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mLevelAwardSpawned); } },
	{ BOARD_FIELD_PROGRESS_METER_WIDTH, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mProgressMeterWidth); } },
	{ BOARD_FIELD_FLAG_RAISE_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mFlagRaiseCounter); } },
	{ BOARD_FIELD_ICE_TRAP_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mIceTrapCounter); } },
	{ BOARD_FIELD_BOARD_RAND_SEED, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mBoardRandSeed); } },
	{ BOARD_FIELD_POOL_SPARKLY_PARTICLE_ID, [](PortableSaveContext& c, Board* theBoard){ SyncEnumU32(c, theBoard->mPoolSparklyParticleID); } },
	{ BOARD_FIELD_FWOOSH_ID, [](PortableSaveContext& c, Board* theBoard){ SyncEnumU32Array(c, &theBoard->mFwooshID[0][0], LEGACY_BOARD_GRID_SIZE_Y * 12); } },
	{ BOARD_FIELD_FWOOSH_COUNTDOWN, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mFwooshCountDown); } },
	{ BOARD_FIELD_TIME_STOP_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mTimeStopCounter); } },
	{ BOARD_FIELD_DROPPED_FIRST_COIN, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mDroppedFirstCoin); } },
	{ BOARD_FIELD_FINAL_WAVE_SOUND_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mFinalWaveSoundCounter); } },
	{ BOARD_FIELD_COB_CANNON_CURSOR_DELAY_COUNTER, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mCobCannonCursorDelayCounter); } },
	{ BOARD_FIELD_COB_CANNON_MOUSE_X, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mCobCannonMouseX); } },
	{ BOARD_FIELD_COB_CANNON_MOUSE_Y, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mCobCannonMouseY); } },
	{ BOARD_FIELD_KILLED_YETI, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mKilledYeti); } },
	{ BOARD_FIELD_MUSTACHE_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mMustacheMode); } },
	{ BOARD_FIELD_SUPER_MOWER_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mSuperMowerMode); } },
	{ BOARD_FIELD_FUTURE_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mFutureMode); } },
	{ BOARD_FIELD_PINATA_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mPinataMode); } },
	{ BOARD_FIELD_DANCE_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mDanceMode); } },
	{ BOARD_FIELD_DAISY_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mDaisyMode); } },
	{ BOARD_FIELD_SUKHBIR_MODE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mSukhbirMode); } },
	{ BOARD_FIELD_PREV_BOARD_RESULT, [](PortableSaveContext& c, Board* theBoard){ SyncEnum32(c, theBoard->mPrevBoardResult); } },
	{ BOARD_FIELD_TRIGGERED_LAWN_MOWERS, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mTriggeredLawnMowers); } },
	{ BOARD_FIELD_PLAY_TIME_ACTIVE_LEVEL, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mPlayTimeActiveLevel); } },
	{ BOARD_FIELD_PLAY_TIME_INACTIVE_LEVEL, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mPlayTimeInactiveLevel); } },
	{ BOARD_FIELD_MAX_SUN_PLANTS, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mMaxSunPlants); } },
	{ BOARD_FIELD_START_DRAW_TIME, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt64(theBoard->mStartDrawTime); } },
	{ BOARD_FIELD_INTERVAL_DRAW_TIME, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt64(theBoard->mIntervalDrawTime); } },
	{ BOARD_FIELD_INTERVAL_DRAW_COUNT_START, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mIntervalDrawCountStart); } },
	{ BOARD_FIELD_MIN_FPS, [](PortableSaveContext& c, Board* theBoard){ c.SyncFloat(theBoard->mMinFPS); } },
	{ BOARD_FIELD_PRELOAD_TIME, [](PortableSaveContext& c, Board* theBoard){ c.SyncInt32(theBoard->mPreloadTime); } },
	{ BOARD_FIELD_GAME_ID, [](PortableSaveContext& c, Board* theBoard){ int64_t aGameId = static_cast<int64_t>(theBoard->mGameID); c.SyncInt64(aGameId); if (c.mReading) theBoard->mGameID = static_cast<intptr_t>(aGameId); } },
	{ BOARD_FIELD_GRAVES_CLEARED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mGravesCleared); } },
	{ BOARD_FIELD_PLANTS_EATEN, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mPlantsEaten); } },
	{ BOARD_FIELD_PLANTS_SHOVELED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mPlantsShoveled); } },
	{ BOARD_FIELD_PEA_SHOOTER_USED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mPeaShooterUsed); } },
	{ BOARD_FIELD_CATAPULT_PLANTS_USED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mCatapultPlantsUsed); } },
	{ BOARD_FIELD_MUSHROOM_AND_COFFEE_BEANS_ONLY, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mMushroomAndCoffeeBeansOnly); } },
	{ BOARD_FIELD_MUSHROOMS_USED, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mMushroomsUsed); } },
	{ BOARD_FIELD_LEVEL_COINS_COLLECTED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mLevelCoinsCollected); } },
	{ BOARD_FIELD_GARGANTUARS_KILLS_BY_CORN_COB, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mGargantuarsKillsByCornCob); } },
	{ BOARD_FIELD_COINS_COLLECTED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mCoinsCollected); } },
	{ BOARD_FIELD_DIAMONDS_COLLECTED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mDiamondsCollected); } },
	{ BOARD_FIELD_POTTED_PLANTS_COLLECTED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mPottedPlantsCollected); } },
	{ BOARD_FIELD_CHOCOLATE_COLLECTED, [](PortableSaveContext& c, Board* theBoard){ c.SyncUInt32(theBoard->mChocolateCollected); } },
	{ BOARD_FIELD_EXPANDED_BOARD_ROWS, [](PortableSaveContext&, Board*){} },
	{ BOARD_FIELD_FINAL_EXPANDED_BOARD_ROWS, [](PortableSaveContext&, Board*){} },
	{ BOARD_FIELD_PLANT_HEAL_GLOWS, SyncPlantHealGlows },
	{ BOARD_FIELD_EXPANDED_BOARD_COLUMNS, [](PortableSaveContext&, Board*){} },
	{ BOARD_FIELD_PLANT_HEAL_VISUALS, SyncPlantHealVisuals },
	{ BOARD_FIELD_CHOMPER_HEAL_AURAS, SyncChomperHealAuras },
	{ BOARD_FIELD_PLANT_OVERDRIVE_STATE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mChomperOverdriveActive); c.SyncBool(theBoard->mKernelPultOverdriveActive); } },
	{ BOARD_FIELD_TWIN_SUNFLOWER_OVERDRIVE_STATE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mTwinSunflowerProductionOverdriveActive); c.SyncBool(theBoard->mTwinSunflowerBombardmentOverdriveActive); } },
	{ BOARD_FIELD_SUN_MAGNET_OVERDRIVE_STATE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mSunMagnetOverdriveActive); } },
	{ BOARD_FIELD_SUN_MAGNET_EXTRA_ITEMS, SyncSunMagnetExtraItems },
	{ BOARD_FIELD_ZOMBIE_STRENGTH_AND_RAIN_STATE, SyncZombieStrengthAndRainState },
	{ BOARD_FIELD_PLANT_OVERDRIVE_EXTENSIONS, SyncPlantOverdriveExtensions },
	{ BOARD_FIELD_PUMPKIN_TALLNUT_OVERDRIVE_STATE, SyncPumpkinTallNutOverdriveState },
	{ BOARD_FIELD_SUN_MAGNET_HEAL_STACKS, SyncSunMagnetHealStacks },
	{ BOARD_FIELD_AUTO_REUSE_ENDLESS_SEEDS, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mAutoReuseEndlessSeeds); } },
	{ BOARD_FIELD_SPIKEWEED_OVERDRIVE_STATE, [](PortableSaveContext& c, Board* theBoard){ c.SyncBool(theBoard->mSpikeweedOverdriveActive); } },
	{ BOARD_FIELD_PLANTERN_FLAMES, [](PortableSaveContext& c, Board* theBoard){ SyncInt32Array(c, theBoard->mPlanternFlameCountdown.data(), MAX_GRID_SIZE_Y); SyncInt32Array(c, theBoard->mPlanternFlameTick.data(), MAX_GRID_SIZE_Y); } },
	{ BOARD_FIELD_SUN_MAGNET_HIGH_EXTRA_ITEMS, SyncSunMagnetHighExtraItems },
	{ BOARD_FIELD_PARTICLE_SHAKE_SCALE, [](PortableSaveContext& c, Board* theBoard){
		auto& aScale = theBoard->mApp->mEffectSystem->mParticleHolder->mPreviousScreenShakeScale;
		c.SyncFloat(aScale);
		if (c.mReading && !(aScale >= 0.0f && aScale <= 1.0f))
		{
			c.mFailed = true;
			aScale = 1.0f;
		}
	} },
	{ BOARD_FIELD_PLANTERN_FLAME_DAMAGE_PERCENT, [](PortableSaveContext& c, Board* theBoard){
		SyncInt32Array(c, theBoard->mPlanternFlameDamagePercent.data(), MAX_GRID_SIZE_Y);
		if (c.mReading)
		{
			for (int32_t& aDamagePercent : theBoard->mPlanternFlameDamagePercent)
			{
				if (aDamagePercent != 85 && aDamagePercent != 100)
				{
					c.mFailed = true;
					aDamagePercent = 100;
				}
			}
		}
	} },
	{ BOARD_FIELD_ZOMBIE_TIER_SUN_MONEY, [](PortableSaveContext& c, Board* theBoard){
		c.SyncInt32(theBoard->mZombieTierSunMoney);
		if (c.mReading && theBoard->mZombieTierSunMoney < 0)
		{
			c.mFailed = true;
			theBoard->mZombieTierSunMoney = std::max(0, theBoard->mSunMoney);
		}
	} },
	{ BOARD_FIELD_CONTINUOUS_SUN_COST_REMAINDER, [](PortableSaveContext& c, Board* theBoard){
		c.SyncFloat(theBoard->mContinuousSunCostRemainder);
		if (c.mReading && !(theBoard->mContinuousSunCostRemainder >= 0.0f && theBoard->mContinuousSunCostRemainder < 1.0f))
		{
			c.mFailed = true;
			theBoard->mContinuousSunCostRemainder = 0.0f;
		}
	} },
	{ BOARD_FIELD_CONTINUOUS_PAID_GLOOM_COUNT, [](PortableSaveContext& c, Board* theBoard){
		c.SyncInt32(theBoard->mContinuousPaidGloomShroomCount);
		if (c.mReading && (theBoard->mContinuousPaidGloomShroomCount < 0 ||
			theBoard->mContinuousPaidGloomShroomCount > static_cast<int32_t>(DATA_ARRAY_MAX_SIZE)))
		{
			c.mFailed = true;
			theBoard->mContinuousPaidGloomShroomCount = 0;
		}
	} },
};

// The enum is contiguous starting at 1: the table must cover every id, in id order, so readers can index it directly.
static_assert([]{
	if (sizeof(gBoardBaseFields) / sizeof(gBoardBaseFields[0]) != BOARD_FIELD_COUNT - 1)
		return false;
	for (uint32_t i = 0; i < sizeof(gBoardBaseFields) / sizeof(gBoardBaseFields[0]); i++)
		if (gBoardBaseFields[i].mFieldId != i + 1)
			return false;
	return true;
}(), "gBoardBaseFields must cover every BoardBaseFieldId in id order");

void SyncBoardBasePortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			if (aFieldId >= 1 && aFieldId <= sizeof(gBoardBaseFields) / sizeof(gBoardBaseFields[0]))
			{
				const BoardBaseFieldEntry& aField = gBoardBaseFields[aFieldId - 1];
				if (!ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ aField.mSync(c, theBoard); }))
					theContext.mFailed = true;
			}
		}
		if (!aReader.mOk)
			theContext.mFailed = true;
	}
	else
	{
		std::vector<unsigned char> aBlob;
		for (const BoardBaseFieldEntry& aField : gBoardBaseFields)
			AppendFieldWithSync(aBlob, aField.mFieldId, [&](PortableSaveContext& c){ aField.mSync(c, theBoard); });
		WriteTLVBlob(theContext, aBlob);
	}
}

}
