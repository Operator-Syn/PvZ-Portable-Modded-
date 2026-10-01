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

#include "DataSync.h"
#include "SaveGameFormat.h"
#include <cstring>
#include <vector>

namespace SaveGameInternal
{
using SaveGameFormat::AppendBytes;
using SaveGameFormat::AppendU32LE;
class PortableSaveContext
{
public:
	bool		mReading = false;
	bool		mFailed = false;
	DataReader*	mReader = nullptr;
	DataWriter*	mWriter = nullptr;

public:
	explicit PortableSaveContext(DataReader& theReader)
	{
		mReading = true;
		mReader = &theReader;
	}

	explicit PortableSaveContext(DataWriter& theWriter)
	{
		mReading = false;
		mWriter = &theWriter;
	}

	void SyncBytes(void* theData, uint32_t theDataLen)
	{
		try
		{
			if (mReading)
			{
				mReader->ReadBytes(theData, theDataLen);
			}
			else
			{
				mWriter->WriteBytes(theData, theDataLen);
			}
		}
		catch (DataReaderException&)
		{
			mFailed = true;
			if (mReading)
				memset(theData, 0, theDataLen);
		}
	}

	void SyncBytes(const void* theData, uint32_t theDataLen)
	{
		if (mReading)
		{
			mFailed = true;
			return;
		}
		mWriter->WriteBytes(theData, theDataLen);
	}

	void SyncBool(bool& theBool)
	{
		if (mReading)
		{
			try
			{
				theBool = mReader->ReadBool();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theBool = false;
			}
		}
		else
		{
			mWriter->WriteBool(theBool);
		}
	}

	void SyncUInt32(uint32_t& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = mReader->ReadUInt32();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0;
			}
		}
		else
		{
			mWriter->WriteUInt32(theValue);
		}
	}

	void SyncInt32(int32_t& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = static_cast<int32_t>(mReader->ReadUInt32());
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0;
			}
		}
		else
		{
			mWriter->WriteUInt32(static_cast<uint32_t>(theValue));
		}
	}

	void SyncFloat(float& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = mReader->ReadFloat();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0.0f;
			}
		}
		else
		{
			mWriter->WriteFloat(theValue);
		}
	}

	void SyncUInt64(uint64_t& theValue)
	{
		uint32_t aLow = static_cast<uint32_t>(theValue & 0xFFFFFFFFULL);
		uint32_t aHigh = static_cast<uint32_t>((theValue >> 32) & 0xFFFFFFFFULL);
		SyncUInt32(aLow);
		SyncUInt32(aHigh);
		if (mReading)
			theValue = (static_cast<uint64_t>(aHigh) << 32) | aLow;
	}

	void SyncInt64(int64_t& theValue)
	{
		uint64_t aValue = static_cast<uint64_t>(theValue);
		SyncUInt64(aValue);
		if (mReading)
			theValue = static_cast<int64_t>(aValue);
	}

	template <typename TEnum>
	void SyncEnum(TEnum& theEnum)
	{
		int32_t aValue = static_cast<int32_t>(theEnum);
		SyncInt32(aValue);
		if (mReading)
			theEnum = static_cast<TEnum>(aValue);
	}
};

template <typename TEnum>
inline void SyncEnum32(PortableSaveContext& theContext, TEnum& theValue)
{
	int32_t aValue = static_cast<int32_t>(theValue);
	theContext.SyncInt32(aValue);
	if (theContext.mReading)
		theValue = static_cast<TEnum>(aValue);
}

template <typename TEnum>
inline void SyncEnumU32(PortableSaveContext& theContext, TEnum& theValue)
{
	uint32_t aValue = static_cast<uint32_t>(theValue);
	theContext.SyncUInt32(aValue);
	if (theContext.mReading)
		theValue = static_cast<TEnum>(aValue);
}

template <typename TEnum>
inline void SyncEnum32Array(PortableSaveContext& theContext, TEnum* theData, size_t theCount)
{
	for (size_t i = 0; i < theCount; i++)
		SyncEnum32(theContext, theData[i]);
}

template <typename TEnum>
inline void SyncEnumU32Array(PortableSaveContext& theContext, TEnum* theData, size_t theCount)
{
	for (size_t i = 0; i < theCount; i++)
		SyncEnumU32(theContext, theData[i]);
}

inline void SyncInt32Array(PortableSaveContext& theContext, int32_t* theData, size_t theCount)
{
	for (size_t i = 0; i < theCount; i++)
		theContext.SyncInt32(theData[i]);
}

inline void SyncBoolArray(PortableSaveContext& theContext, bool* theData, size_t theCount)
{
	for (size_t i = 0; i < theCount; i++)
		theContext.SyncBool(theData[i]);
}

template <typename TWriterFn>
static void AppendFieldWithSync(std::vector<unsigned char>& theOut, uint32_t theFieldId, TWriterFn theWriterFn)
{
	DataWriter aWriter;
	aWriter.OpenMemory(0x100);
	PortableSaveContext aContext(aWriter);
	theWriterFn(aContext);
	if (aContext.mFailed)
		return;

	std::vector<unsigned char> aFieldData;
	aFieldData.resize(aWriter.GetDataLen());
	memcpy(aFieldData.data(), aWriter.GetDataPtr(), aWriter.GetDataLen());
	AppendU32LE(theOut, theFieldId);
	AppendU32LE(theOut, static_cast<uint32_t>(aFieldData.size()));
	AppendBytes(theOut, aFieldData.data(), aFieldData.size());
}

template <typename TReaderFn>
static bool ApplyFieldWithSync(const unsigned char* theData, size_t theSize, TReaderFn theReaderFn)
{
	DataReader aReader;
	aReader.OpenMemory(theData, static_cast<uint32_t>(theSize), false);
	PortableSaveContext aContext(aReader);
	theReaderFn(aContext);
	return !aContext.mFailed;
}

}
