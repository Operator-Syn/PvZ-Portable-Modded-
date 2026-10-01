// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace SaveGameFormat
{
inline constexpr char MAGIC_V4[12] = "PVZP_SAVE4";
inline constexpr uint32_t VERSION_V4 = 1;
inline constexpr size_t HEADER_SIZE_V4 = 24;

enum class Error
{
	None,
	InvalidBoard,
	FileRead,
	FileWrite,
	InvalidHeader,
	UnsupportedVersion,
	TruncatedPayload,
	ChecksumMismatch,
	InvalidRecord,
	MissingBoard,
	InvalidState,
	PayloadTooLarge
};

struct Result
{
	Error mError = Error::None;
	size_t mOffset = 0;
	uint32_t mRecordType = 0;

	explicit operator bool() const { return mError == Error::None; }
};

struct PortablePayload
{
	Result mResult;
	std::span<const unsigned char> mBytes;
};

// Input is borrowed only for the lifetime of the caller's byte buffer.
PortablePayload ValidatePortableSave(std::span<const unsigned char> theBytes);
Result EncodePortableSave(std::span<const unsigned char> thePayload, std::vector<unsigned char>& theBytes);
std::string_view ErrorMessage(Error theError);

void AppendU32LE(std::vector<unsigned char>& theOut, uint32_t theValue);
void AppendBytes(std::vector<unsigned char>& theOut, const void* theData, size_t theLen);
void AppendChunk(std::vector<unsigned char>& theOut, uint32_t theType, const std::vector<unsigned char>& theData);

// This reader owns only its cursor; no files, resources, globals, or diagnostics.
class TLVReader
{
public:
	const unsigned char* mData;
	size_t mSize;
	size_t mPos = 0;
	bool mOk = true;

	TLVReader(const unsigned char* theData, size_t theSize) : mData(theData), mSize(theSize), mOk(theData != nullptr || theSize == 0) {}
	bool ReadU32(uint32_t& theValue);
	bool ReadBytes(const unsigned char*& thePtr, size_t theLen);
};
}
