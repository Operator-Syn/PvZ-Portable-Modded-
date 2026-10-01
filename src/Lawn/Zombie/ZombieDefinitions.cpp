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

#include <climits>
#include <algorithm>
#include <array>
#include <format>

#include "../Plant/Plant.h"
#include "../Board/Board.h"
#include "../../ConstEnums.h"
#include "Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Entities/LawnMower.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/PlayerInfo.h"
#include "../System/Zombatar.h"
#include "../System/Music.h"
#include "../Widget/AlmanacDialog.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

constinit const ZombieDefinition gZombieDefs[NUM_ZOMBIE_TYPES] = {
	{ .mZombieType = ZOMBIE_NORMAL, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 1, .mStartingLevel = 1, .mFirstAllowedWave = 1, .mPickWeight = 4000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_FLAG, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 1, .mStartingLevel = 1, .mFirstAllowedWave = 1, .mPickWeight = 0, .mZombieName = "FLAG_ZOMBIE" },
	{ .mZombieType = ZOMBIE_TRAFFIC_CONE, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 2, .mStartingLevel = 3, .mFirstAllowedWave = 1, .mPickWeight = 4000, .mZombieName = "CONEHEAD_ZOMBIE" },
	{ .mZombieType = ZOMBIE_POLEVAULTER, .mReanimationType = REANIM_POLEVAULTER, .mZombieValue = 2, .mStartingLevel = 6, .mFirstAllowedWave = 5, .mPickWeight = 2000, .mZombieName = "POLE_VAULTING_ZOMBIE" },
	{ .mZombieType = ZOMBIE_PAIL, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 4, .mStartingLevel = 8, .mFirstAllowedWave = 1, .mPickWeight = 3000, .mZombieName = "BUCKETHEAD_ZOMBIE" },
	{ .mZombieType = ZOMBIE_NEWSPAPER, .mReanimationType = REANIM_ZOMBIE_NEWSPAPER, .mZombieValue = 2, .mStartingLevel = 11, .mFirstAllowedWave = 1, .mPickWeight = 1000, .mZombieName = "NEWSPAPER_ZOMBIE" },
	{ .mZombieType = ZOMBIE_DOOR, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 4, .mStartingLevel = 13, .mFirstAllowedWave = 5, .mPickWeight = 3500, .mZombieName = "SCREEN_DOOR_ZOMBIE" },
	{ .mZombieType = ZOMBIE_FOOTBALL, .mReanimationType = REANIM_ZOMBIE_FOOTBALL, .mZombieValue = 7, .mStartingLevel = 16, .mFirstAllowedWave = 5, .mPickWeight = 2000, .mZombieName = "FOOTBALL_ZOMBIE" },
	{ .mZombieType = ZOMBIE_DANCER, .mReanimationType = REANIM_DANCER, .mZombieValue = 5, .mStartingLevel = 18, .mFirstAllowedWave = 5, .mPickWeight = 1000, .mZombieName = "DANCING_ZOMBIE" },
	{ .mZombieType = ZOMBIE_BACKUP_DANCER, .mReanimationType = REANIM_BACKUP_DANCER, .mZombieValue = 1, .mStartingLevel = 18, .mFirstAllowedWave = 1, .mPickWeight = 0, .mZombieName = "BACKUP_DANCER" },
	{ .mZombieType = ZOMBIE_DUCKY_TUBE, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 1, .mStartingLevel = 21, .mFirstAllowedWave = 5, .mPickWeight = 0, .mZombieName = "DUCKY_TUBE_ZOMBIE" },
	{ .mZombieType = ZOMBIE_SNORKEL, .mReanimationType = REANIM_SNORKEL, .mZombieValue = 3, .mStartingLevel = 23, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "SNORKEL_ZOMBIE" },
	{ .mZombieType = ZOMBIE_ZAMBONI, .mReanimationType = REANIM_ZOMBIE_ZAMBONI, .mZombieValue = 7, .mStartingLevel = 26, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "ZOMBONI" },
	{ .mZombieType = ZOMBIE_BOBSLED, .mReanimationType = REANIM_BOBSLED, .mZombieValue = 3, .mStartingLevel = 26, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "ZOMBIE_BOBSLED_TEAM" },
	{ .mZombieType = ZOMBIE_DOLPHIN_RIDER, .mReanimationType = REANIM_ZOMBIE_DOLPHINRIDER, .mZombieValue = 3, .mStartingLevel = 28, .mFirstAllowedWave = 10, .mPickWeight = 1500, .mZombieName = "DOLPHIN_RIDER_ZOMBIE" },
	{ .mZombieType = ZOMBIE_JACK_IN_THE_BOX, .mReanimationType = REANIM_JACKINTHEBOX, .mZombieValue = 3, .mStartingLevel = 31, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "JACK_IN_THE_BOX_ZOMBIE" },
	{ .mZombieType = ZOMBIE_BALLOON, .mReanimationType = REANIM_BALLOON, .mZombieValue = 2, .mStartingLevel = 33, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "BALLOON_ZOMBIE" },
	{ .mZombieType = ZOMBIE_DIGGER, .mReanimationType = REANIM_DIGGER, .mZombieValue = 4, .mStartingLevel = 36, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "DIGGER_ZOMBIE" },
	{ .mZombieType = ZOMBIE_POGO, .mReanimationType = REANIM_POGO, .mZombieValue = 4, .mStartingLevel = 38, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "POGO_ZOMBIE" },
	{ .mZombieType = ZOMBIE_YETI, .mReanimationType = REANIM_YETI, .mZombieValue = 4, .mStartingLevel = 40, .mFirstAllowedWave = 1, .mPickWeight = 1, .mZombieName = "ZOMBIE_YETI" },
	{ .mZombieType = ZOMBIE_BUNGEE, .mReanimationType = REANIM_BUNGEE, .mZombieValue = 3, .mStartingLevel = 41, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "BUNGEE_ZOMBIE" },
	{ .mZombieType = ZOMBIE_LADDER, .mReanimationType = REANIM_LADDER, .mZombieValue = 4, .mStartingLevel = 43, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "LADDER_ZOMBIE" },
	{ .mZombieType = ZOMBIE_CATAPULT, .mReanimationType = REANIM_CATAPULT, .mZombieValue = 5, .mStartingLevel = 46, .mFirstAllowedWave = 10, .mPickWeight = 1500, .mZombieName = "CATAPULT_ZOMBIE" },
	{ .mZombieType = ZOMBIE_GARGANTUAR, .mReanimationType = REANIM_GARGANTUAR, .mZombieValue = 10, .mStartingLevel = 48, .mFirstAllowedWave = 15, .mPickWeight = 1500, .mZombieName = "GARGANTUAR" },
	{ .mZombieType = ZOMBIE_IMP, .mReanimationType = REANIM_IMP, .mZombieValue = 10, .mStartingLevel = 48, .mFirstAllowedWave = 1, .mPickWeight = 0, .mZombieName = "IMP" },
	{ .mZombieType = ZOMBIE_BOSS, .mReanimationType = REANIM_BOSS, .mZombieValue = 10, .mStartingLevel = 50, .mFirstAllowedWave = 1, .mPickWeight = 0, .mZombieName = "BOSS" },
	{ .mZombieType = ZOMBIE_PEA_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 1, .mStartingLevel = 99, .mFirstAllowedWave = 1, .mPickWeight = 4000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_WALLNUT_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 4, .mStartingLevel = 99, .mFirstAllowedWave = 1, .mPickWeight = 3000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_JALAPENO_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 3, .mStartingLevel = 99, .mFirstAllowedWave = 10, .mPickWeight = 1000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_GATLING_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 3, .mStartingLevel = 99, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_SQUASH_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 3, .mStartingLevel = 99, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_TALLNUT_HEAD, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 4, .mStartingLevel = 99, .mFirstAllowedWave = 10, .mPickWeight = 2000, .mZombieName = "ZOMBIE" },
	{ .mZombieType = ZOMBIE_REDEYE_GARGANTUAR, .mReanimationType = REANIM_GARGANTUAR, .mZombieValue = 10, .mStartingLevel = 48, .mFirstAllowedWave = 15, .mPickWeight = 6000, .mZombieName = "REDEYED_GARGANTUAR" },
	{ .mZombieType = ZOMBIE_BULWARK_GARGANTUAR, .mReanimationType = REANIM_GARGANTUAR, .mZombieValue = 10, .mStartingLevel = 48, .mFirstAllowedWave = 15, .mPickWeight = 0, .mZombieName = "BULWARK_GARGANTUAR" },
	{ .mZombieType = ZOMBIE_BULWARK_BUCKET, .mReanimationType = REANIM_ZOMBIE, .mZombieValue = 4, .mStartingLevel = 48, .mFirstAllowedWave = 15, .mPickWeight = 750, .mZombieName = "BULWARK_BUCKET_ZOMBIE" },
};



const ZombieDefinition& GetZombieDefinition(ZombieType theZombieType)
{
	PVZP_ASSERT(theZombieType >= 0 && theZombieType < NUM_ZOMBIE_TYPES);
	PVZP_ASSERT(gZombieDefs[theZombieType].mZombieType == theZombieType);

	return gZombieDefs[theZombieType];
}
