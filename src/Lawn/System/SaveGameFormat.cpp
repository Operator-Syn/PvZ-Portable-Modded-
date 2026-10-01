// SPDX-License-Identifier: LGPL-3.0-or-later
#include "SaveGameFormat.h"

#include <cstring>
#include <limits>
#include <zlib.h>

namespace SaveGameFormat
{
void AppendU32LE(std::vector<unsigned char>& theOut, uint32_t theValue)
{
	for (unsigned int aShift = 0; aShift < 32; aShift += 8)
		theOut.push_back(static_cast<unsigned char>((theValue >> aShift) & 0xFF));
}

void AppendBytes(std::vector<unsigned char>& theOut, const void* theData, size_t theLen)
{
	if (theLen == 0)
		return;
	const auto* aBytes = static_cast<const unsigned char*>(theData);
	theOut.insert(theOut.end(), aBytes, aBytes + theLen);
}

void AppendChunk(std::vector<unsigned char>& theOut, uint32_t theType, const std::vector<unsigned char>& theData)
{
	AppendU32LE(theOut, theType);
	AppendU32LE(theOut, static_cast<uint32_t>(theData.size()));
	AppendBytes(theOut, theData.data(), theData.size());
}

bool TLVReader::ReadU32(uint32_t& theValue)
{
	if (!mOk || mPos > mSize || mSize - mPos < 4)
	{
		mOk = false;
		theValue = 0;
		return false;
	}
	theValue = static_cast<uint32_t>(mData[mPos]) |
		(static_cast<uint32_t>(mData[mPos + 1]) << 8) |
		(static_cast<uint32_t>(mData[mPos + 2]) << 16) |
		(static_cast<uint32_t>(mData[mPos + 3]) << 24);
	mPos += 4;
	return true;
}

bool TLVReader::ReadBytes(const unsigned char*& thePtr, size_t theLen)
{
	if (!mOk || mPos > mSize || theLen > mSize - mPos)
	{
		mOk = false;
		thePtr = nullptr;
		return false;
	}
	thePtr = mData ? mData + mPos : nullptr;
	mPos += theLen;
	return true;
}

PortablePayload ValidatePortableSave(std::span<const unsigned char> theBytes)
{
	if (theBytes.size() < HEADER_SIZE_V4)
		return {{Error::InvalidHeader}, {}};
	if (std::memcmp(theBytes.data(), MAGIC_V4, sizeof(MAGIC_V4)) != 0)
		return {{Error::InvalidHeader}, {}};
	TLVReader aHeader(theBytes.data() + sizeof(MAGIC_V4), HEADER_SIZE_V4 - sizeof(MAGIC_V4));
	uint32_t aVersion, aSize, aCrc;
	aHeader.ReadU32(aVersion);
	aHeader.ReadU32(aSize);
	aHeader.ReadU32(aCrc);
	if (aVersion != VERSION_V4)
		return {{Error::UnsupportedVersion, 12}, {}};
	if (aSize > theBytes.size() - HEADER_SIZE_V4)
		return {{Error::TruncatedPayload, HEADER_SIZE_V4}, {}};
	auto aPayload = theBytes.subspan(HEADER_SIZE_V4, aSize);
	if (crc32(0, aPayload.data(), aSize) != aCrc)
		return {{Error::ChecksumMismatch, 20}, {}};

	// Validate all outer records before the serializer mutates any live board state.
	TLVReader aReader(aPayload.data(), aPayload.size());
	bool aHasBoard = false;
	while (aReader.mPos < aReader.mSize)
	{
		const size_t anOffset = HEADER_SIZE_V4 + aReader.mPos;
		uint32_t aType = 0, aLength = 0;
		const unsigned char* aData = nullptr;
		if (!aReader.ReadU32(aType) || !aReader.ReadU32(aLength) || !aReader.ReadBytes(aData, aLength))
			return {{Error::InvalidRecord, anOffset, aType}, {}};
		aHasBoard |= aType == 1;
	}
	if (!aHasBoard)
		return {{Error::MissingBoard, HEADER_SIZE_V4}, {}};
	return {{}, aPayload};
}

Result EncodePortableSave(std::span<const unsigned char> thePayload, std::vector<unsigned char>& theBytes)
{
	// The filesystem adapter writes an int-sized byte count.
	if (thePayload.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - HEADER_SIZE_V4)
		return {Error::PayloadTooLarge};
	std::vector<unsigned char> aBytes;
	aBytes.reserve(HEADER_SIZE_V4 + thePayload.size());
	AppendBytes(aBytes, MAGIC_V4, sizeof(MAGIC_V4));
	AppendU32LE(aBytes, VERSION_V4);
	AppendU32LE(aBytes, static_cast<uint32_t>(thePayload.size()));
	AppendU32LE(aBytes, crc32(0, thePayload.data(), static_cast<uint32_t>(thePayload.size())));
	AppendBytes(aBytes, thePayload.data(), thePayload.size());
	theBytes.swap(aBytes);
	return {};
}

std::string_view ErrorMessage(Error theError)
{
	switch (theError)
	{
	case Error::None: return "success";
	case Error::InvalidBoard: return "board or application is unavailable";
	case Error::FileRead: return "could not read save file";
	case Error::FileWrite: return "could not write save file";
	case Error::InvalidHeader: return "invalid save header";
	case Error::UnsupportedVersion: return "unsupported save version";
	case Error::TruncatedPayload: return "truncated save payload";
	case Error::ChecksumMismatch: return "save checksum mismatch";
	case Error::InvalidRecord: return "invalid save record";
	case Error::MissingBoard: return "save has no board record";
	case Error::InvalidState: return "save contains invalid game state";
	case Error::PayloadTooLarge: return "save exceeds supported size";
	}
	return "unknown save error";
}
}
