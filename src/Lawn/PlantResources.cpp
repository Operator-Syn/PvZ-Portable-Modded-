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

#include "Coin.h"
#include "Plant.h"
#include "Board.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "ZenGarden.h"
#include "Challenge.h"
#include "Projectile.h"
#include "SeedPacket.h"
#include "../LawnApp.h"
#include "CursorObject.h"
#include "../GameConstants.h"
#include <array>
#include "System/PlayerInfo.h"
#include "System/ReanimationLawn.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "TargetingRules.h"

void Plant::PreloadPlantResources(SeedType theSeedType)
{
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
	{
		ReanimatorEnsureDefinitionLoaded(aPlantDef.mReanimationType, true);
	}
	if (theSeedType == SeedType::SEED_CHOMPERNUT)
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CHOMPER, true);

	if (theSeedType == SeedType::SEED_CHERRYBOMB)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED, true);
	}
	else if (theSeedType == SeedType::SEED_PLANTERN)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CHERRYBOMB, true);
	}
	else if (theSeedType == SeedType::SEED_JALAPENO)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_JALAPENO_FIRE, true);
	}
	else if (theSeedType == SeedType::SEED_TORCHWOOD)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FIRE_PEA, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_JALAPENO_FIRE, true);
	}
	else if (Plant::IsNocturnal(theSeedType))
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SLEEPING, true);
	}
}
