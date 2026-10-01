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

#pragma once

#include "StoreScreen.h"

constexpr const int STORESCREEN_ITEMOFFSET_1_X = 422;
constexpr const int STORESCREEN_ITEMOFFSET_1_Y = 206;
constexpr const int STORESCREEN_ITEMOFFSET_2_X = 372;
constexpr const int STORESCREEN_ITEMOFFSET_2_Y = 310;
constexpr const int STORESCREEN_ITEMSIZE = 74;
constexpr const int STORESCREEN_COINBANK_X = 650;
constexpr const int STORESCREEN_COINBANK_Y = 559;
constexpr const int STORESCREEN_PAGESTRING_X = 470;
constexpr const int STORESCREEN_PAGESTRING_Y = 500;

inline constexpr StoreItem gStoreItemSpots[NUM_STORE_PAGES][MAX_PAGE_SPOTS] =
{
	{ STORE_ITEM_PACKET_UPGRADE,    STORE_ITEM_POOL_CLEANER,        STORE_ITEM_RAKE,                STORE_ITEM_ROOF_CLEANER,
	  STORE_ITEM_PLANT_GATLINGPEA,  STORE_ITEM_PLANT_TWINSUNFLOWER, STORE_ITEM_PLANT_GLOOMSHROOM,   STORE_ITEM_PLANT_CATTAIL },
	{ STORE_ITEM_PLANT_SPIKEROCK,   STORE_ITEM_PLANT_GOLD_MAGNET,   STORE_ITEM_PLANT_WINTERMELON,   STORE_ITEM_PLANT_COBCANNON,
	  STORE_ITEM_PLANT_IMITATER,    STORE_ITEM_FIRSTAID,            STORE_ITEM_INVALID,             STORE_ITEM_INVALID },
	{ STORE_ITEM_POTTED_MARIGOLD_1, STORE_ITEM_POTTED_MARIGOLD_2,   STORE_ITEM_POTTED_MARIGOLD_3,   STORE_ITEM_GOLD_WATERINGCAN,
	  STORE_ITEM_FERTILIZER,        STORE_ITEM_BUG_SPRAY,           STORE_ITEM_PHONOGRAPH,          STORE_ITEM_GARDENING_GLOVE },
	{ STORE_ITEM_MUSHROOM_GARDEN,   STORE_ITEM_AQUARIUM_GARDEN,     STORE_ITEM_WHEEL_BARROW,        STORE_ITEM_STINKY_THE_SNAIL,
	  STORE_ITEM_TREE_OF_WISDOM,    STORE_ITEM_TREE_FOOD,           STORE_ITEM_INVALID,             STORE_ITEM_INVALID }
};

