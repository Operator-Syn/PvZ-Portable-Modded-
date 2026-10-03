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

#include "../Entities/Coin.h"
#include "Plant.h"
#include "../Board/Board.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Modes/ZenGarden.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../GameConstants.h"
#include <array>
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "../Rules/TargetingRules.h"

// Ephraim uses a cel atlas instead of a reanimation. Plant::GetImage
// reads mPlantImage[0], so the entry is filled during resource loading.
Image* gEphraimPlantImages[1] = { nullptr };
Image* gSniperFemalePlantImages[1] = { nullptr };
Image* gSerraBishopPlantImages[1] = { nullptr };
Image* gSerraBishopSequences[2] = { nullptr, nullptr };
Image* gSerraDivineBlessing = nullptr;

constinit const PlantDefinition gPlantDefs[SeedType::NUM_SEED_TYPES] = {
	{ .mSeedType = SeedType::SEED_PEASHOOTER,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PEASHOOTER,    .mPacketIndex = 0,  .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "PEASHOOTER" },
	{ .mSeedType = SeedType::SEED_SUNFLOWER,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SUNFLOWER,     .mPacketIndex = 1,  .mSeedCost = 50,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 1250, .mPlantName = "SUNFLOWER" },
	{ .mSeedType = SeedType::SEED_CHERRYBOMB,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CHERRYBOMB,    .mPacketIndex = 3,  .mSeedCost = 150, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "CHERRY_BOMB" },
	{ .mSeedType = SeedType::SEED_WALLNUT,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "WALL_NUT" },
	{ .mSeedType = SeedType::SEED_POTATOMINE,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_POTATOMINE,    .mPacketIndex = 37, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "POTATO_MINE" },
	{ .mSeedType = SeedType::SEED_SNOWPEA,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SNOWPEA,       .mPacketIndex = 4,  .mSeedCost = 175, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SNOW_PEA" },
	{ .mSeedType = SeedType::SEED_CHOMPER,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CHOMPER,       .mPacketIndex = 31, .mSeedCost = 150, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "CHOMPER" },
	{ .mSeedType = SeedType::SEED_REPEATER,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_REPEATER,      .mPacketIndex = 5,  .mSeedCost = 200, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "REPEATER" },
	{ .mSeedType = SeedType::SEED_PUFFSHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PUFFSHROOM,    .mPacketIndex = 6,  .mSeedCost = 0,   .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "PUFF_SHROOM" },
	{ .mSeedType = SeedType::SEED_SUNSHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SUNSHROOM,     .mPacketIndex = 7,  .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 1250, .mPlantName = "SUN_SHROOM" },
	{ .mSeedType = SeedType::SEED_FUMESHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_FUMESHROOM,    .mPacketIndex = 9,  .mSeedCost = 75,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "FUME_SHROOM" },
	{ .mSeedType = SeedType::SEED_GRAVEBUSTER,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GRAVE_BUSTER,  .mPacketIndex = 40, .mSeedCost = 75,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GRAVE_BUSTER" },
	{ .mSeedType = SeedType::SEED_HYPNOSHROOM,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_HYPNOSHROOM,   .mPacketIndex = 10, .mSeedCost = 75,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "HYPNO_SHROOM" },
	{ .mSeedType = SeedType::SEED_SCAREDYSHROOM,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SCRAREYSHROOM, .mPacketIndex = 33, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SCAREDY_SHROOM" },
	{ .mSeedType = SeedType::SEED_ICESHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_ICESHROOM,     .mPacketIndex = 36, .mSeedCost = 75,  .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "ICE_SHROOM" },
	{ .mSeedType = SeedType::SEED_DOOMSHROOM,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_DOOMSHROOM,    .mPacketIndex = 20, .mSeedCost = 125, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "DOOM_SHROOM" },
	{ .mSeedType = SeedType::SEED_LILYPAD,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_LILYPAD,       .mPacketIndex = 19, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "LILY_PAD" },
	{ .mSeedType = SeedType::SEED_SQUASH,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SQUASH,        .mPacketIndex = 21, .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SQUASH" },
	{ .mSeedType = SeedType::SEED_THREEPEATER,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_THREEPEATER,   .mPacketIndex = 12, .mSeedCost = 325, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "THREEPEATER" },
	{ .mSeedType = SeedType::SEED_TANGLEKELP,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TANGLEKELP,    .mPacketIndex = 17, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TANGLE_KELP" },
	{ .mSeedType = SeedType::SEED_JALAPENO,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_JALAPENO,      .mPacketIndex = 11, .mSeedCost = 125, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "JALAPENO" },
	{ .mSeedType = SeedType::SEED_SPIKEWEED,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPIKEWEED,     .mPacketIndex = 22, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPIKEWEED" },
	{ .mSeedType = SeedType::SEED_TORCHWOOD,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TORCHWOOD,     .mPacketIndex = 29, .mSeedCost = 175, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TORCHWOOD" },
	{ .mSeedType = SeedType::SEED_TALLNUT,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TALLNUT,       .mPacketIndex = 28, .mSeedCost = 125, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "TALL_NUT" },
	{ .mSeedType = SeedType::SEED_SEASHROOM,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SEASHROOM,     .mPacketIndex = 39, .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SEA_SHROOM" },
	{ .mSeedType = SeedType::SEED_PLANTERN,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PLANTERN,      .mPacketIndex = 38, .mSeedCost = 25,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 625,  .mPlantName = "PLANTERN" },
	{ .mSeedType = SeedType::SEED_CACTUS,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CACTUS,        .mPacketIndex = 15, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "CACTUS" },
	{ .mSeedType = SeedType::SEED_BLOVER,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_BLOVER,        .mPacketIndex = 18, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "BLOVER" },
	{ .mSeedType = SeedType::SEED_SPLITPEA,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPLITPEA,      .mPacketIndex = 32, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "SPLIT_PEA" },
	{ .mSeedType = SeedType::SEED_STARFRUIT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_STARFRUIT,     .mPacketIndex = 30, .mSeedCost = 125, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "STARFRUIT" },
	{ .mSeedType = SeedType::SEED_PUMPKINSHELL,      .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_PUMPKIN,       .mPacketIndex = 25, .mSeedCost = 125, .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "PUMPKIN" },
	{ .mSeedType = SeedType::SEED_MAGNETSHROOM,      .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MAGNETSHROOM,  .mPacketIndex = 35, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "MAGNET_SHROOM" },
	{ .mSeedType = SeedType::SEED_CABBAGEPULT,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CABBAGEPULT,   .mPacketIndex = 13, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "CABBAGE_PULT" },
	{ .mSeedType = SeedType::SEED_FLOWERPOT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_FLOWER_POT,    .mPacketIndex = 33, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "FLOWER_POT" },
	{ .mSeedType = SeedType::SEED_KERNELPULT,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_KERNELPULT,    .mPacketIndex = 13, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "KERNEL_PULT" },
	{ .mSeedType = SeedType::SEED_INSTANT_COFFEE,    .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_COFFEEBEAN,    .mPacketIndex = 33, .mSeedCost = 75,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "COFFEE_BEAN" },
	{ .mSeedType = SeedType::SEED_GARLIC,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GARLIC,        .mPacketIndex = 8,  .mSeedCost = 50,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GARLIC" },
	{ .mSeedType = SeedType::SEED_UMBRELLA,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_UMBRELLALEAF,  .mPacketIndex = 23, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "UMBRELLA_LEAF" },
	{ .mSeedType = SeedType::SEED_MARIGOLD,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MARIGOLD,      .mPacketIndex = 24, .mSeedCost = 50,  .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 2500, .mPlantName = "MARIGOLD" },
	{ .mSeedType = SeedType::SEED_MELONPULT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_MELONPULT,     .mPacketIndex = 14, .mSeedCost = 300, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "MELON_PULT" },
	{ .mSeedType = SeedType::SEED_GATLINGPEA,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GATLINGPEA,    .mPacketIndex = 5,  .mSeedCost = 250, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 40,   .mPlantName = "GATLING_PEA" },
	{ .mSeedType = SeedType::SEED_TWINSUNFLOWER,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TWIN_SUNFLOWER,.mPacketIndex = 1,  .mSeedCost = 150, .mRefreshTime = 1667,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 625,  .mPlantName = "TWIN_SUNFLOWER" },
	{ .mSeedType = SeedType::SEED_GLOOMSHROOM,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GLOOMSHROOM,   .mPacketIndex = 27, .mSeedCost = 150, .mRefreshTime = 500,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 67,   .mPlantName = "GLOOM_SHROOM" },
	{ .mSeedType = SeedType::SEED_CATTAIL,           .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_CATTAIL,       .mPacketIndex = 27, .mSeedCost = 75,  .mRefreshTime = 1667,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 75,   .mPlantName = "CATTAIL" },
	{ .mSeedType = SeedType::SEED_WINTERMELON,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WINTER_MELON,  .mPacketIndex = 27, .mSeedCost = 200, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 300,  .mPlantName = "WINTER_MELON" },
	{ .mSeedType = SeedType::SEED_GOLD_MAGNET,       .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GOLD_MAGNET,   .mPacketIndex = 27, .mSeedCost = 50,  .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GOLD_MAGNET" },
	{ .mSeedType = SeedType::SEED_SPIKEROCK,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_SPIKEROCK,     .mPacketIndex = 27, .mSeedCost = 125, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPIKEROCK" },
	{ .mSeedType = SeedType::SEED_COBCANNON,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_COBCANNON,     .mPacketIndex = 16, .mSeedCost = 500, .mRefreshTime = 5000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 600,  .mPlantName = "COB_CANNON" },
	{ .mSeedType = SeedType::SEED_IMITATER,          .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_IMITATER,      .mPacketIndex = 33, .mSeedCost = 0,   .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "IMITATER" },
	{ .mSeedType = SeedType::SEED_EXPLODE_O_NUT,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "EXPLODE_O_NUT" },
	{ .mSeedType = SeedType::SEED_GIANT_WALLNUT,     .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_WALLNUT,       .mPacketIndex = 2,  .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "GIANT_WALLNUT" },
	{ .mSeedType = SeedType::SEED_SPROUT,            .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_ZENGARDEN_SPROUT, .mPacketIndex = 33, .mSeedCost = 0,   .mRefreshTime = 3000,   .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SPROUT" },
	{ .mSeedType = SeedType::SEED_LEFTPEATER,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_REPEATER,      .mPacketIndex = 5,  .mSeedCost = 200, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 150,  .mPlantName = "REPEATER" },
	{ .mSeedType = SeedType::SEED_SUN_MAGNET,        .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_GOLD_MAGNET,  .mPacketIndex = 27, .mSeedCost = 25,  .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "SUN_MAGNET" },
	{ .mSeedType = SeedType::SEED_CHOMPERNUT,         .mPlantImage = nullptr, .mReanimationType = ReanimationType::REANIM_TALLNUT,        .mPacketIndex = -1, .mSeedCost = 0,   .mRefreshTime = 0,      .mSubClass = PlantSubClass::SUBCLASS_NORMAL,  .mLaunchRate = 0,    .mPlantName = "CHOMPER_NUT" },
	{ .mSeedType = SeedType::SEED_EPHRAIM,            .mPlantImage = gEphraimPlantImages, .mReanimationType = ReanimationType::REANIM_NONE,           .mPacketIndex = -1, .mSeedCost = 300, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER, .mLaunchRate = 100,  .mPlantName = "EPHRAIM" },
	{ .mSeedType = SeedType::SEED_SNIPER_FEMALE,      .mPlantImage = gSniperFemalePlantImages, .mReanimationType = ReanimationType::REANIM_NONE,      .mPacketIndex = -1, .mSeedCost = 100, .mRefreshTime = 750,    .mSubClass = PlantSubClass::SUBCLASS_SHOOTER,  .mLaunchRate = 55,    .mPlantName = "SNIPER_FEMALE" },
	{ .mSeedType = SeedType::SEED_SERRA_BISHOP, .mPlantImage = gSerraBishopPlantImages, .mReanimationType = ReanimationType::REANIM_NONE, .mPacketIndex = -1, .mSeedCost = 100, .mRefreshTime = 750, .mSubClass = PlantSubClass::SUBCLASS_NORMAL, .mLaunchRate = Plant::SERRA_PRODUCTION_RATE_TICKS, .mPlantName = "SERRA_BISHOP" }
};

const PlantDefinition& GetPlantDefinition(SeedType theSeedType)
{
	PVZP_ASSERT(gPlantDefs[theSeedType].mSeedType == theSeedType);
	PVZP_ASSERT(theSeedType >= 0 && theSeedType < static_cast<int>(SeedType::NUM_SEED_TYPES));

	return gPlantDefs[theSeedType];
}

int Plant::GetCost(SeedType theSeedType, SeedType theImitaterType)
{
	if (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		if (theSeedType == SeedType::SEED_REPEATER)
		{
			return 1000;
		}
		else if (theSeedType == SeedType::SEED_FUMESHROOM)
		{
			return 500;
		}
		else if (theSeedType == SeedType::SEED_TALLNUT)
		{
			return 250;
		}
		else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
		{
			return 100;
		}
		else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			return 200;
		}
	}

	switch (theSeedType)
	{
	case SeedType::SEED_SLOT_MACHINE_SUN:           return 0;
	case SeedType::SEED_SLOT_MACHINE_DIAMOND:       return 0;
	case SeedType::SEED_ZOMBIQUARIUM_SNORKLE:       return 100;
	case SeedType::SEED_ZOMBIQUARIUM_TROPHY:        return 1000;
	case SeedType::SEED_ZOMBIE_NORMAL:              return 50;
	case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:        return 75;
	case SeedType::SEED_ZOMBIE_POLEVAULTER:         return 75;
	case SeedType::SEED_ZOMBIE_PAIL:                return 125;
	case SeedType::SEED_ZOMBIE_LADDER:              return 150;
	case SeedType::SEED_ZOMBIE_DIGGER:              return 125;
	case SeedType::SEED_ZOMBIE_BUNGEE:              return 125;
	case SeedType::SEED_ZOMBIE_FOOTBALL:            return 175;
	case SeedType::SEED_ZOMBIE_BALLOON:             return 150;
	case SeedType::SEED_ZOMBIE_SCREEN_DOOR:         return 100;
	case SeedType::SEED_ZOMBONI:                    return 175;
	case SeedType::SEED_ZOMBIE_POGO:                return 200;
	case SeedType::SEED_ZOMBIE_DANCER:              return 350;
	case SeedType::SEED_ZOMBIE_GARGANTUAR:          return 300;
	case SeedType::SEED_ZOMBIE_IMP:                 return 50;
	default:
	{
		if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
		{
			const PlantDefinition& aPlantDef = GetPlantDefinition(theImitaterType);
			return aPlantDef.mSeedCost;
		}
		else
		{
			const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
			return aPlantDef.mSeedCost;
		}
	}
	}
}

std::string Plant::GetNameString(SeedType theSeedType, SeedType theImitaterType)
{
	if (theSeedType == SeedType::SEED_EPHRAIM)
		return "Ephraim";
	if (theSeedType == SeedType::SEED_SERRA_BISHOP)
		return "Serra Bishop";
	if (theSeedType == SeedType::SEED_SNIPER_FEMALE)
		return "Female Sniper";
	if (theSeedType == SeedType::SEED_CHOMPERNUT)
		return "Chomper Nut";

	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	std::string aName = std::format("[{}]", aPlantDef.mPlantName);
	std::string aTranslatedName(PvzpStringTranslate(aName));

	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		std::string aTranslatedImitaterName = GetNameString(theImitaterType);
		return std::format("{} {}", aTranslatedName, aTranslatedImitaterName);
	}

	return aTranslatedName;
}

std::string Plant::GetToolTip(SeedType theSeedType)
{
	if (theSeedType == SeedType::SEED_EPHRAIM)
		return "Ephraim";
	if (theSeedType == SeedType::SEED_SERRA_BISHOP)
		return "Serra Bishop";
	if (theSeedType == SeedType::SEED_SNIPER_FEMALE)
		return "Female Sniper";
	const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
	std::string aToolTip = std::format("[{}_TOOLTIP]", aPlantDef.mPlantName);
	return std::string(PvzpStringTranslate(aToolTip));
}

int Plant::GetRefreshTime(SeedType theSeedType, SeedType theImitaterType)
{
	if (Challenge::IsZombieSeedType(theSeedType))
	{
		return 0;
	}

	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		const PlantDefinition& aPlantDef = GetPlantDefinition(theImitaterType);
		return (aPlantDef.mRefreshTime + 2) / 3;
	}
	else
	{
		const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
		return (aPlantDef.mRefreshTime + 2) / 3;
	}
}

bool Plant::IsNocturnal(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_PUFFSHROOM ||
		theSeedtype == SeedType::SEED_SEASHROOM ||
		theSeedtype == SeedType::SEED_SUNSHROOM ||
		theSeedtype == SeedType::SEED_FUMESHROOM ||
		theSeedtype == SeedType::SEED_HYPNOSHROOM ||
		theSeedtype == SeedType::SEED_DOOMSHROOM ||
		theSeedtype == SeedType::SEED_ICESHROOM ||
		theSeedtype == SeedType::SEED_MAGNETSHROOM ||
		theSeedtype == SeedType::SEED_SCAREDYSHROOM ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM;
}

bool Plant::IsFungus(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_PUFFSHROOM ||
		theSeedtype == SeedType::SEED_SUNSHROOM ||
		theSeedtype == SeedType::SEED_FUMESHROOM ||
		theSeedtype == SeedType::SEED_HYPNOSHROOM ||
		theSeedtype == SeedType::SEED_SCAREDYSHROOM ||
		theSeedtype == SeedType::SEED_ICESHROOM ||
		theSeedtype == SeedType::SEED_DOOMSHROOM ||
		theSeedtype == SeedType::SEED_SEASHROOM ||
		theSeedtype == SeedType::SEED_MAGNETSHROOM ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM;
}

bool Plant::IsAquatic(SeedType theSeedType)
{
	return
		theSeedType == SeedType::SEED_LILYPAD ||
		theSeedType == SeedType::SEED_TANGLEKELP ||
		theSeedType == SeedType::SEED_SEASHROOM ||
		theSeedType == SeedType::SEED_CATTAIL;
}

bool Plant::IsFlying(SeedType theSeedtype)
{
	return theSeedtype == SeedType::SEED_INSTANT_COFFEE;
}

bool Plant::IsUpgrade(SeedType theSeedtype)
{
	return
		theSeedtype == SeedType::SEED_GATLINGPEA ||
		theSeedtype == SeedType::SEED_WINTERMELON ||
		theSeedtype == SeedType::SEED_TWINSUNFLOWER ||
		theSeedtype == SeedType::SEED_SPIKEROCK ||
		theSeedtype == SeedType::SEED_COBCANNON ||
		theSeedtype == SeedType::SEED_GOLD_MAGNET ||
		theSeedtype == SeedType::SEED_GLOOMSHROOM ||
		theSeedtype == SeedType::SEED_CATTAIL;
}
