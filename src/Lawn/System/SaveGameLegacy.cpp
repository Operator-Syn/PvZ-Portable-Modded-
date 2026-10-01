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

#include "Music.h"
#include "SaveGame.h"
#include "../Board/Board.h"
#include "../Modes/Challenge.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../Widget/MessageWidget.h"
#include "../../PvzpLib/Trail.h"
#include "zlib.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/DataArray.h"
#include "../../PvzpLib/PvzpList.h"
#include "DataSync.h"
#include "../../GameConstants.h"
#include "misc/Buffer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>


#include "SaveGameInternal.h"

namespace SaveGameInternal
{
static constexpr const char* FILE_COMPILE_TIME_STRING = "Jul  2 201011:47:03"; // save files are tied to this exact timestamp string
static constexpr const uint32_t SAVE_FILE_MAGIC_NUMBER = 0xFEEDDEAD;
static constexpr const uint32_t SAVE_FILE_VERSION = 3U;
static const uint32_t SAVE_FILE_DATE = crc32(0, (Bytef*)FILE_COMPILE_TIME_STRING, strlen(FILE_COMPILE_TIME_STRING));
struct SaveFileHeader
{
	uint32_t	mMagicNumber;
	uint32_t	mBuildVersion;
	uint32_t	mBuildDate;
};


// Legacy mid-level save support
class SaveGameContext
{
public:
	Sexy::Buffer	mBuffer;
	bool			mFailed;
	bool			mReading;

public:
	inline int		ByteLeftToRead() { return (mBuffer.mDataBitSize - mBuffer.mReadBitPos + 7) / 8; }
	void			SyncBytes(void* theDest, int theReadSize);
	void			SyncInt32(int32_t& theInt32);
	void			SyncUInt32(uint32_t& theUInt32);
	inline void		SyncInt(int& theInt)
	{
		int32_t aValue = theInt;
		SyncInt32(aValue);
		if (mReading)
			theInt = aValue;
	}
	void			SyncReanimationDef(ReanimatorDefinition*& theDefinition);
	void			SyncParticleDef(PvzpParticleDefinition*& theDefinition);
	void			SyncTrailDef(TrailDefinition*& theDefinition);
	void			SyncImage(Image*& theImage);
};

void SaveGameContext::SyncBytes(void* theDest, int theReadSize)
{
	int aReadSize = theReadSize;
	if (mReading)
	{
		if (ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		aReadSize = mFailed ? 0 : mBuffer.ReadInt32();
	}
	else
	{
		mBuffer.WriteInt32(theReadSize);
	}

	if (mReading)
	{
		if (aReadSize != theReadSize || ByteLeftToRead() < theReadSize)
		{
			mFailed = true;
		}

		if (mFailed)
		{
			memset(theDest, 0, theReadSize);
		}
		else
		{
			mBuffer.ReadBytes((uchar*)theDest, theReadSize);
		}
	}
	else
	{
		mBuffer.WriteBytes((uchar*)theDest, theReadSize);
	}
}

void SaveGameContext::SyncInt32(int32_t& theInt32)
{
	if (mReading)
	{
		if (ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		theInt32 = mFailed ? 0 : mBuffer.ReadInt32();
	}
	else
	{
		mBuffer.WriteInt32(theInt32);
	}
}

void SaveGameContext::SyncUInt32(uint32_t& theUInt32)
{
	if (mReading)
	{
		if (ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		theUInt32 = mFailed ? 0 : mBuffer.ReadUInt32();
	}
	else
	{
		mBuffer.WriteUInt32(theUInt32);
	}
}

void SaveGameContext::SyncReanimationDef(ReanimatorDefinition*& theDefinition)
{
	if (mReading)
	{
		int aReanimType;
		SyncInt(aReanimType);
		if (aReanimType == static_cast<int>(ReanimationType::REANIM_NONE))
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < static_cast<int>(ReanimationType::NUM_REANIMS))
		{
			ReanimatorEnsureDefinitionLoaded(static_cast<ReanimationType>(aReanimType), true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aReanimType = static_cast<int>(ReanimationType::REANIM_NONE);
		for (int i = 0; i < static_cast<int>(ReanimationType::NUM_REANIMS); i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		SyncInt(aReanimType);
	}
}

void SaveGameContext::SyncParticleDef(PvzpParticleDefinition*& theDefinition)
{
	if (mReading)
	{
		int aParticleType;
		SyncInt(aParticleType);
		if (aParticleType == static_cast<int>(ParticleEffect::PARTICLE_NONE))
		{
			theDefinition = nullptr;
		}
		else if (aParticleType >= 0 && aParticleType < static_cast<int>(ParticleEffect::NUM_PARTICLES))
		{
			theDefinition = &gParticleDefArray[aParticleType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aParticleType = static_cast<int>(ParticleEffect::PARTICLE_NONE);
		for (int i = 0; i < static_cast<int>(ParticleEffect::NUM_PARTICLES); i++)
		{
			PvzpParticleDefinition* aDef = &gParticleDefArray[i];
			if (theDefinition == aDef)
			{
				aParticleType = i;
				break;
			}
		}
		SyncInt(aParticleType);
	}
}

void SaveGameContext::SyncTrailDef(TrailDefinition*& theDefinition)
{
	if (mReading)
	{
		int aTrailType;
		SyncInt(aTrailType);
		if (aTrailType == TrailType::TRAIL_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aTrailType >= 0 && aTrailType < TrailType::NUM_TRAILS)
		{
			theDefinition = &gTrailDefArray[aTrailType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aTrailType = TrailType::TRAIL_NONE;
		for (int i = 0; i < TrailType::NUM_TRAILS; i++)
		{
			TrailDefinition* aDef = &gTrailDefArray[i];
			if (theDefinition == aDef)
			{
				aTrailType = i;
				break;
			}
		}
		SyncInt(aTrailType);
	}
}

void SaveGameContext::SyncImage(Image*& theImage)
{
	if (mReading)
	{
		ResourceId aResID;
		SyncInt((int&)aResID);
		if (aResID == Sexy::ResourceId::RESOURCE_ID_MAX)
		{
			theImage = nullptr;
		}
		else
		{
			theImage = GetImageById(aResID);
		}
	}
	else
	{
		ResourceId aResID;
		if (theImage != nullptr)
		{
			aResID = GetIdByImage(theImage);
		}
		else
		{
			aResID = Sexy::ResourceId::RESOURCE_ID_MAX;
		}
		SyncInt((int&)aResID);
	}
}

static void SyncDataIDList(PvzpList<uint32_t>* theDataIDList, SaveGameContext& theContext, PvzpAllocator* theAllocator)
{
	try
	{
		if (theContext.mReading)
		{
			if (theDataIDList)
			{
				theDataIDList->mHead = nullptr;
				theDataIDList->mTail = nullptr;
				theDataIDList->mSize = 0;
				theDataIDList->SetAllocator(theAllocator);
			}

			int aCount;
			theContext.SyncInt(aCount);
			for (int i = 0; i < aCount; i++)
			{
				uint32_t aDataID;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt(aCount);
			for (PvzpListNode<uint32_t>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				uint32_t aDataID = aNode->mValue;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

static void SyncParticleEmitter(PvzpParticleSystem* theParticleSystem, PvzpParticleEmitter* theParticleEmitter, SaveGameContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = (reinterpret_cast<intptr_t>(theParticleEmitter->mEmitterDef) -
			reinterpret_cast<intptr_t>(theParticleSystem->mParticleDef->mEmitterDefs)) / sizeof(PvzpEmitterDefinition);
		theContext.SyncInt(aEmitterDefIndex);
	}

	theContext.SyncImage(theParticleEmitter->mImageOverride);
	SyncDataIDList((PvzpList<uint32_t>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	for (PvzpListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<uint32_t>(aNode->mValue));
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
	}
}

static void SyncParticleSystem(Board* theBoard, PvzpParticleSystem* theParticleSystem, SaveGameContext& theContext)
{
	theContext.SyncParticleDef(theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder.get();
	}

	SyncDataIDList((PvzpList<uint32_t>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (PvzpListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet(static_cast<uint32_t>(aNode->mValue));
		SyncParticleEmitter(theParticleSystem, aEmitter, theContext);
	}
}

static void SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext)
{
	theContext.SyncReanimationDef(theReanimation->mDefinition);
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder.get();
	}

	if (theReanimation->mDefinition->mTracks.count != 0)
	{
		int aSize = theReanimation->mDefinition->mTracks.count * sizeof(ReanimatorTrackInstance);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aSize)->Calloc(aSize);
		}
		theContext.SyncBytes(theReanimation->mTrackInstances, aSize);

		for (int aTrackIndex = 0; aTrackIndex < theReanimation->mDefinition->mTracks.count; aTrackIndex++)
		{
			ReanimatorTrackInstance& aTrackInstance = theReanimation->mTrackInstances[aTrackIndex];
			theContext.SyncImage(aTrackInstance.mImageOverride);

			if (theContext.mReading)
			{
				aTrackInstance.mBlendTransform.mText = "";
				PVZP_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				PVZP_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
			else
			{
				PVZP_ASSERT(aTrackInstance.mBlendTransform.mText[0] == 0);
				PVZP_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				PVZP_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
		}
	}
}

static void SyncTrail(Board* theBoard, Trail* theTrail, SaveGameContext& theContext)
{
	theContext.SyncTrailDef(theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder.get();
	}
}

template <typename T>
struct LegacyDataArrayItem
{
	alignas(T) unsigned char mItem[sizeof(T)];
	unsigned int mID;
};

template <typename T> inline static void SyncDataArray(SaveGameContext& theContext, DataArray<T>& theDataArray)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	auto aBlock = std::make_unique<LegacyDataArrayItem<T>[]>(theDataArray.mMaxUsedCount);
	if (!theContext.mReading)
	{
		for (uint32_t i = 0; i < theDataArray.mMaxUsedCount; i++)
		{
			auto& aSlot = aBlock[i];
			std::copy_n(reinterpret_cast<unsigned char*>(&theDataArray.DataArrayGetItemAt(i)), sizeof(T), aSlot.mItem);
			aSlot.mID = theDataArray.DataArrayGetIDAt(i);
		}
	}
	theContext.SyncBytes(aBlock.get(), theDataArray.mMaxUsedCount * sizeof(aBlock[0]));
	if (!theContext.mReading)
		return;

	for (uint32_t i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		auto& aSlot = aBlock[i];
		theDataArray.DataArrayGetIDAt(i) = aSlot.mID;
		if (aSlot.mID & DATA_ARRAY_KEY_MASK)
			std::copy_n(aSlot.mItem, sizeof(T), reinterpret_cast<unsigned char*>(&theDataArray.DataArrayGetItemAt(i)));
	}
}

static void SyncBoard(SaveGameContext& theContext, Board* theBoard)
{
	size_t offset = size_t(&theBoard->mPaused) - size_t(theBoard);
	theContext.SyncBytes(&theBoard->mPaused, sizeof(Board) - offset);

	SyncDataArray(theContext, theBoard->mZombies);
	SyncDataArray(theContext, theBoard->mPlants);
	SyncDataArray(theContext, theBoard->mProjectiles);
	SyncDataArray(theContext, theBoard->mCoins);
	SyncDataArray(theContext, theBoard->mLawnMowers);
	SyncDataArray(theContext, theBoard->mGridItems);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails);
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);

	{
		for (PvzpParticleSystem* aParticle : theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems)
		{
			SyncParticleSystem(theBoard, aParticle, theContext);
		}
	}
	{
		for (Reanimation* aReanimation : theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations)
		{
			SyncReanimation(theBoard, aReanimation, theContext);
		}
	}
	{
		for (Trail* aTrail : theBoard->mApp->mEffectSystem->mTrailHolder->mTrails)
		{
			SyncTrail(theBoard, aTrail, theContext);
		}
	}

	theContext.SyncBytes(theBoard->mCursorObject.get(), sizeof(CursorObject));
	theContext.SyncBytes(theBoard->mCursorPreview.get(), sizeof(CursorPreview));
	theContext.SyncBytes(theBoard->mAdvice.get(), sizeof(MessageWidget));
	theContext.SyncBytes(theBoard->mSeedBank.get(), sizeof(SeedBank));
	theContext.SyncBytes(theBoard->mChallenge.get(), sizeof(Challenge));
	theContext.SyncBytes(theBoard->mApp->mMusic.get(), sizeof(Music));

	if (theContext.mReading)
	{
		if (theContext.ByteLeftToRead() < 4)
		{
			theContext.mFailed = true;
		}

		if (theContext.mFailed || theContext.mBuffer.ReadUInt32() != SAVE_FILE_MAGIC_NUMBER)
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		theContext.mBuffer.WriteUInt32(SAVE_FILE_MAGIC_NUMBER);
	}
}

Result ReadLegacySave(Board* theBoard, Sexy::Buffer& theBuffer)
{
	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = true;
	aContext.mBuffer = theBuffer;

	SaveFileHeader aHeader{};
	aContext.SyncBytes(&aHeader, sizeof(aHeader));
	if (aHeader.mMagicNumber != SAVE_FILE_MAGIC_NUMBER || aHeader.mBuildVersion != SAVE_FILE_VERSION || aHeader.mBuildDate != SAVE_FILE_DATE)
	{
		return {Error::InvalidHeader};
	}

	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
	{
		return {Error::InvalidState};
	}

	return {};
}
}
