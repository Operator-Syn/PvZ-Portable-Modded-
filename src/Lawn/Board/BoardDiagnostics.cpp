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

void Board::RecordGameplayEvent(std::string_view theEvent, std::string_view theDetailsJson)
{
	const std::string aDataJson = std::format(
		R"({{"context":{{"game_mode":{},"level":{},"wave":{},"survival_stage":{},"sun":{},"viewport_width":{},"viewport_height":{},"grid_columns":{},"grid_rows":{}}},"details":{}}})",
		static_cast<int>(mApp->mGameMode), mLevel, mCurrentWave,
		mChallenge ? mChallenge->mSurvivalStage : -1, mSunMoney, mApp->mWidth, mApp->mHeight,
		GetNumPlayableColumns(), GetNumPlayableRows(), theDetailsJson);
	Sexy::FrameProfiler::Get().RecordGameplayEvent(theEvent, aDataJson);
}

std::string Board::GetZombieBreachReport() const
{
	if (mLastBreachingZombieRow < 0 ||
		static_cast<int>(mLastBreachingZombieType) >= static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES))
		return {};

	const auto& aZombieDefinition = GetZombieDefinition(mLastBreachingZombieType);
	const std::string aZombieName(PvzpStringTranslate(std::format("[{}]", aZombieDefinition.mZombieName)));
	return std::format("Breached by {} in lane {}", aZombieName, mLastBreachingZombieRow + 1);
}
