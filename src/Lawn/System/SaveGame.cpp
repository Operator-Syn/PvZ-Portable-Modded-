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

#include "SaveGame.h"
#include "SaveGameInternal.h"
#include "../../LawnApp.h"
#include <cstring>

SaveGameFormat::Result LawnLoadGameDetailed(Board* theBoard, const std::string& theFilePath)
{
	using namespace SaveGameFormat;
	if (theBoard == nullptr || theBoard->mApp == nullptr || gSexyAppBase == nullptr)
		return {Error::InvalidBoard};
	Sexy::Buffer aBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aBuffer, false))
		return {Error::FileRead};
	std::span<const unsigned char> aBytes(aBuffer.GetDataPtr(), static_cast<size_t>(aBuffer.GetDataLen()));
	Result aResult;
	if (aBytes.size() >= sizeof(MAGIC_V4) && std::memcmp(aBytes.data(), MAGIC_V4, sizeof(MAGIC_V4)) == 0)
	{
		PortablePayload aPayload = ValidatePortableSave(aBytes);
		if (!aPayload.mResult)
			return aPayload.mResult;
		aResult = SaveGameInternal::ReadPortablePayload(theBoard, aPayload.mBytes);
		if (!aResult)
			aResult.mOffset += HEADER_SIZE_V4;
	}
	else
		aResult = SaveGameInternal::ReadLegacySave(theBoard, aBuffer);
	if (!aResult)
		return aResult;
	SaveGameInternal::FixBoardAfterLoad(theBoard);
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return {};
}

SaveGameFormat::Result LawnSaveGameDetailed(Board* theBoard, const std::string& theFilePath)
{
	using namespace SaveGameFormat;
	if (theBoard == nullptr || theBoard->mApp == nullptr || gSexyAppBase == nullptr)
		return {Error::InvalidBoard};
	std::vector<unsigned char> aPayload;
	Result aResult = SaveGameInternal::WritePortablePayload(theBoard, aPayload);
	if (!aResult)
		return aResult;
	std::vector<unsigned char> aBytes;
	aResult = EncodePortableSave(aPayload, aBytes);
	if (!aResult)
		return aResult;
	if (!gSexyAppBase->WriteBytesToFile(theFilePath, aBytes.data(), static_cast<int>(aBytes.size())))
		return {Error::FileWrite};
	return {};
}

bool LawnLoadGame(Board* theBoard, const std::string& theFilePath)
{
	return static_cast<bool>(LawnLoadGameDetailed(theBoard, theFilePath));
}

bool LawnSaveGame(Board* theBoard, const std::string& theFilePath)
{
	return static_cast<bool>(LawnSaveGameDetailed(theBoard, theFilePath));
}
