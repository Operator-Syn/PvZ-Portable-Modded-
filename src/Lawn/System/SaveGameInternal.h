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

#include "SaveGamePortableContext.h"
#include "../Board.h"
#include "misc/Buffer.h"

namespace SaveGameInternal
{
using SaveGameFormat::AppendBytes;
using SaveGameFormat::AppendU32LE;
using SaveGameFormat::AppendChunk;
using SaveGameFormat::TLVReader;
using SaveGameFormat::Result;
using SaveGameFormat::Error;

// These dimensions remain part of older portable and raw save compatibility.
constexpr int LEGACY_BOARD_GRID_SIZE_X = CLASSIC_GRID_SIZE_X;
constexpr int LEGACY_BOARD_GRID_SIZE_Y = 6;

void SyncPvzpSmoothArrayList(PortableSaveContext& theContext, PvzpSmoothArray* theData, size_t theCount);
void SyncMagnetItemPortable(PortableSaveContext& theContext, MagnetItem& theItem);
void WriteTLVBlob(PortableSaveContext& theContext, const std::vector<unsigned char>& theBlob);
bool ReadTLVBlob(PortableSaveContext& theContext, std::vector<unsigned char>& theBlob);
void SyncBoardBasePortable(PortableSaveContext& theContext, Board* theBoard);
Result ReadPortablePayload(Board* theBoard, std::span<const unsigned char> thePayload);
Result WritePortablePayload(Board* theBoard, std::vector<unsigned char>& thePayload);
Result ReadLegacySave(Board* theBoard, Sexy::Buffer& theBuffer);
void FixBoardAfterLoad(Board* theBoard);
}
