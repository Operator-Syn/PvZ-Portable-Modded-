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
enum SaveChunkTypeV4
{
	SAVE4_CHUNK_BOARD_BASE = 1,
	SAVE4_CHUNK_ZOMBIES = 2,
	SAVE4_CHUNK_PLANTS = 3,
	SAVE4_CHUNK_PROJECTILES = 4,
	SAVE4_CHUNK_COINS = 5,
	SAVE4_CHUNK_MOWERS = 6,
	SAVE4_CHUNK_GRIDITEMS = 7,
	SAVE4_CHUNK_PARTICLE_EMITTERS = 8,
	SAVE4_CHUNK_PARTICLE_PARTICLES = 9,
	SAVE4_CHUNK_PARTICLE_SYSTEMS = 10,
	SAVE4_CHUNK_REANIMATIONS = 11,
	SAVE4_CHUNK_TRAILS = 12,
	SAVE4_CHUNK_ATTACHMENTS = 13,
	SAVE4_CHUNK_CURSOR = 14,
	SAVE4_CHUNK_CURSOR_PREVIEW = 15,
	SAVE4_CHUNK_ADVICE = 16,
	SAVE4_CHUNK_SEEDBANK = 17,
	SAVE4_CHUNK_SEEDPACKETS = 18,
	SAVE4_CHUNK_CHALLENGE = 19,
	SAVE4_CHUNK_MUSIC = 20
};

static constexpr const uint32_t SAVE4_CHUNK_VERSION = 1U;

static void SyncColorPortable(PortableSaveContext& theContext, Color& theColor)
{
	theContext.SyncInt32(theColor.mRed);
	theContext.SyncInt32(theColor.mGreen);
	theContext.SyncInt32(theColor.mBlue);
	theContext.SyncInt32(theColor.mAlpha);
}

static void SyncVector2Portable(PortableSaveContext& theContext, SexyVector2& theVector)
{
	theContext.SyncFloat(theVector.x);
	theContext.SyncFloat(theVector.y);
}

static void SyncMatrixPortable(PortableSaveContext& theContext, SexyMatrix3& theMatrix)
{
	theContext.SyncFloat(theMatrix.m00);
	theContext.SyncFloat(theMatrix.m01);
	theContext.SyncFloat(theMatrix.m02);
	theContext.SyncFloat(theMatrix.m10);
	theContext.SyncFloat(theMatrix.m11);
	theContext.SyncFloat(theMatrix.m12);
	theContext.SyncFloat(theMatrix.m20);
	theContext.SyncFloat(theMatrix.m21);
	theContext.SyncFloat(theMatrix.m22);
}

static void SyncRectPortable(PortableSaveContext& theContext, Rect& theRect)
{
	theContext.SyncInt32(theRect.mX);
	theContext.SyncInt32(theRect.mY);
	theContext.SyncInt32(theRect.mWidth);
	theContext.SyncInt32(theRect.mHeight);
}

static void SyncReanimationDefPortable(PortableSaveContext& theContext, ReanimatorDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aReanimType = 0;
		theContext.SyncInt32(aReanimType);
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
			theContext.mFailed = true;
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
		theContext.SyncInt32(aReanimType);
	}
}

static void SyncParticleDefPortable(PortableSaveContext& theContext, PvzpParticleDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aParticleType = 0;
		theContext.SyncInt32(aParticleType);
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
			theContext.mFailed = true;
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
		theContext.SyncInt32(aParticleType);
	}
}

static void SyncTrailDefPortable(PortableSaveContext& theContext, TrailDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aTrailType = 0;
		theContext.SyncInt32(aTrailType);
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
			theContext.mFailed = true;
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
		theContext.SyncInt32(aTrailType);
	}
}

static void SyncImagePortable(PortableSaveContext& theContext, Image*& theImage)
{
	if (theContext.mReading)
	{
		ResourceId aResID;
		theContext.SyncInt32(reinterpret_cast<int32_t&>(aResID));
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
		theContext.SyncInt32(reinterpret_cast<int32_t&>(aResID));
	}
}

static void SyncDataIDListPortable(PvzpList<uint32_t>* theDataIDList, PortableSaveContext& theContext, PvzpAllocator* theAllocator)
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

			int aCount = 0;
			theContext.SyncInt32(aCount);
			for (int i = 0; i < aCount; i++)
			{
				uint32_t aDataID = 0;
				theContext.SyncUInt32(aDataID);
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt32(aCount);
			for (PvzpListNode<uint32_t>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				uint32_t aDataID = aNode->mValue;
				theContext.SyncUInt32(aDataID);
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

static void SyncGameObjectPortable(PortableSaveContext& theContext, GameObject& theObject)
{
	theContext.SyncInt32(theObject.mX);
	theContext.SyncInt32(theObject.mY);
	theContext.SyncInt32(theObject.mWidth);
	theContext.SyncInt32(theObject.mHeight);
	theContext.SyncBool(theObject.mVisible);
	theContext.SyncInt32(theObject.mRow);
	theContext.SyncInt32(theObject.mRenderOrder);
}

static constexpr const uint32_t PORTABLE_FIELD_TAIL = 100U;

static void SyncPvzpSmoothArray(PortableSaveContext& theContext, PvzpSmoothArray& theArray)
{
	theContext.SyncInt32(theArray.mItem);
	theContext.SyncFloat(theArray.mWeight);
	theContext.SyncFloat(theArray.mLastPicked);
	theContext.SyncFloat(theArray.mSecondLastPicked);
}

void SyncPvzpSmoothArrayList(PortableSaveContext& theContext, PvzpSmoothArray* theData, size_t theCount)
{
	for (size_t i = 0; i < theCount; i++)
		SyncPvzpSmoothArray(theContext, theData[i]);
}

static void SyncPottedPlantPortable(PortableSaveContext& theContext, PottedPlant& thePlant)
{
	SyncEnum32(theContext, thePlant.mSeedType);
	SyncEnum32(theContext, thePlant.mWhichZenGarden);
	theContext.SyncInt32(thePlant.mX);
	theContext.SyncInt32(thePlant.mY);
	SyncEnum32(theContext, thePlant.mFacing);
	theContext.SyncInt64(thePlant.mLastWateredTime);
	SyncEnum32(theContext, thePlant.mDrawVariation);
	SyncEnum32(theContext, thePlant.mPlantAge);
	theContext.SyncInt32(thePlant.mTimesFed);
	theContext.SyncInt32(thePlant.mFeedingsPerGrow);
	SyncEnum32(theContext, thePlant.mPlantNeed);
	theContext.SyncInt64(thePlant.mLastNeedFulfilledTime);
	theContext.SyncInt64(thePlant.mLastFertilizedTime);
	theContext.SyncInt64(thePlant.mLastChocolateTime);
	theContext.SyncInt64(thePlant.mFutureAttribute[0]);
}

static void SyncMotionTrailFramePortable(PortableSaveContext& theContext, MotionTrailFrame& theFrame)
{
	theContext.SyncFloat(theFrame.mPosX);
	theContext.SyncFloat(theFrame.mPosY);
	theContext.SyncFloat(theFrame.mAnimTime);
}

void SyncMagnetItemPortable(PortableSaveContext& theContext, MagnetItem& theItem)
{
	theContext.SyncFloat(theItem.mPosX);
	theContext.SyncFloat(theItem.mPosY);
	theContext.SyncFloat(theItem.mDestOffsetX);
	theContext.SyncFloat(theItem.mDestOffsetY);
	SyncEnum32(theContext, theItem.mItemType);
}

static void SyncAttachEffectPortable(PortableSaveContext& theContext, AttachEffect& theEffect)
{
	theContext.SyncUInt32(theEffect.mEffectID);
	SyncEnum32(theContext, theEffect.mEffectType);
	SyncMatrixPortable(theContext, theEffect.mOffset);
	theContext.SyncBool(theEffect.mDontDrawIfParentHidden);
	theContext.SyncBool(theEffect.mDontPropogateColor);
}

static void SyncAttachmentTailPortable(PortableSaveContext& theContext, Attachment& theAttachment)
{
	for (int i = 0; i < MAX_EFFECTS_PER_ATTACHMENT; i++)
		SyncAttachEffectPortable(theContext, theAttachment.mEffectArray[i]);
	theContext.SyncInt32(theAttachment.mNumEffects);
	theContext.SyncBool(theAttachment.mDead);
}

static void SyncCursorObjectTailPortable(PortableSaveContext& theContext, CursorObject& theObject)
{
	theContext.SyncInt32(theObject.mSeedBankIndex);
	SyncEnum32(theContext, theObject.mType);
	SyncEnum32(theContext, theObject.mImitaterType);
	SyncEnum32(theContext, theObject.mCursorType);
	SyncEnumU32(theContext, theObject.mCoinID);
	SyncEnumU32(theContext, theObject.mGlovePlantID);
	SyncEnumU32(theContext, theObject.mDuplicatorPlantID);
	SyncEnumU32(theContext, theObject.mCobCannonPlantID);
	theContext.SyncInt32(theObject.mHammerDownCounter);
	SyncEnumU32(theContext, theObject.mReanimCursorID);
}

static void SyncCursorPreviewTailPortable(PortableSaveContext& theContext, CursorPreview& thePreview)
{
	theContext.SyncInt32(thePreview.mGridX);
	theContext.SyncInt32(thePreview.mGridY);
}

static void SyncMessageWidgetTailPortable(PortableSaveContext& theContext, MessageWidget& theWidget)
{
	theContext.SyncBytes(theWidget.mLabel, sizeof(theWidget.mLabel));
	theContext.SyncInt32(theWidget.mDisplayTime);
	theContext.SyncInt32(theWidget.mDuration);
	SyncEnum32(theContext, theWidget.mMessageStyle);
	SyncEnumU32Array(theContext, &theWidget.mTextReanimID[0], MAX_MESSAGE_LENGTH);
	SyncEnum32(theContext, theWidget.mReanimType);
	theContext.SyncInt32(theWidget.mSlideOffTime);
	theContext.SyncBytes(theWidget.mLabelNext, sizeof(theWidget.mLabelNext));
	SyncEnum32(theContext, theWidget.mMessageStyleNext);
}

static void SyncSeedBankTailPortable(PortableSaveContext& theContext, SeedBank& theSeedBank)
{
	theContext.SyncInt32(theSeedBank.mNumPackets);
	theContext.SyncInt32(theSeedBank.mCutSceneDarken);
	theContext.SyncInt32(theSeedBank.mConveyorBeltCounter);
}

static void SyncSeedPacketTailPortable(PortableSaveContext& theContext, SeedPacket& thePacket)
{
	theContext.SyncInt32(thePacket.mRefreshCounter);
	theContext.SyncInt32(thePacket.mRefreshTime);
	theContext.SyncInt32(thePacket.mIndex);
	theContext.SyncInt32(thePacket.mOffsetX);
	SyncEnum32(theContext, thePacket.mPacketType);
	SyncEnum32(theContext, thePacket.mImitaterType);
	theContext.SyncInt32(thePacket.mSlotMachineCountDown);
	SyncEnum32(theContext, thePacket.mSlotMachiningNextSeed);
	theContext.SyncFloat(thePacket.mSlotMachiningPosition);
	theContext.SyncBool(thePacket.mActive);
	theContext.SyncBool(thePacket.mRefreshing);
	theContext.SyncInt32(thePacket.mTimesUsed);
}

static void SyncChallengeTailPortable(PortableSaveContext& theContext, Challenge& theChallenge)
{
	theContext.SyncInt32(theChallenge.mBeghouledMouseCapture);
	theContext.SyncInt32(theChallenge.mBeghouledMouseDownX);
	theContext.SyncInt32(theChallenge.mBeghouledMouseDownY);
	SyncInt32Array(theContext, &theChallenge.mBeghouledEated[0][0], 9 * 6);
	SyncInt32Array(theContext, &theChallenge.mBeghouledPurcasedUpgrade[0], NUM_BEGHOULED_UPGRADES);
	theContext.SyncInt32(theChallenge.mBeghouledMatchesThisMove);
	SyncEnum32(theContext, theChallenge.mChallengeState);
	theContext.SyncInt32(theChallenge.mChallengeStateCounter);
	theContext.SyncInt32(theChallenge.mConveyorBeltCounter);
	theContext.SyncInt32(theChallenge.mChallengeScore);
	theContext.SyncInt32(theChallenge.mShowBowlingLine);
	SyncEnum32(theContext, theChallenge.mLastConveyorSeedType);
	theContext.SyncInt32(theChallenge.mSurvivalStage);
	theContext.SyncInt32(theChallenge.mSlotMachineRollCount);
	SyncEnumU32(theContext, theChallenge.mReanimChallenge);
	SyncEnumU32Array(theContext, &theChallenge.mReanimClouds[0], 6);
	SyncInt32Array(theContext, &theChallenge.mCloudsCounter[0], 6);
	theContext.SyncInt32(theChallenge.mChallengeGridX);
	theContext.SyncInt32(theChallenge.mChallengeGridY);
	theContext.SyncInt32(theChallenge.mScaryPotterPots);
	theContext.SyncInt32(theChallenge.mRainCounter);
	theContext.SyncInt32(theChallenge.mTreeOfWisdomTalkIndex);
}

static void SyncMusicTailPortable(PortableSaveContext& theContext, Music& theMusic)
{
	SyncEnum32(theContext, theMusic.mCurMusicTune);
	SyncEnum32(theContext, theMusic.mCurMusicFileMain);
	SyncEnum32(theContext, theMusic.mCurMusicFileDrums);
	SyncEnum32(theContext, theMusic.mCurMusicFileHihats);
	theContext.SyncInt32(theMusic.mBurstOverride);
	theContext.SyncFloat(theMusic.mBaseBPM);
	theContext.SyncFloat(theMusic.mBaseModSpeed);
	SyncEnum32(theContext, theMusic.mMusicBurstState);
	theContext.SyncInt32(theMusic.mBurstStateCounter);
	SyncEnum32(theContext, theMusic.mMusicDrumsState);
	theContext.SyncInt32(theMusic.mQueuedDrumTrackPackedOrder);
	theContext.SyncInt32(theMusic.mDrumsStateCounter);
	theContext.SyncInt32(theMusic.mPauseOffset);
	theContext.SyncInt32(theMusic.mPauseOffsetDrums);
	theContext.SyncBool(theMusic.mPaused);
	// When loading, do not override a runtime music-disable flag that may have been set
	// because this platform don't have audio support; keep it if already true.
	if (theContext.mReading)
	{
		bool aSavedMusicDisabled = false;
		theContext.SyncBool(aSavedMusicDisabled); // Just read and discard
		// Completely ignore the saved value. mMusicDisabled is a runtime capability flag
		// (set when audio assets fail to load). It should never be transferred from a save.
	}
	else
	{
		theContext.SyncBool(theMusic.mMusicDisabled);
	}
	theContext.SyncInt32(theMusic.mFadeOutCounter);
	theContext.SyncInt32(theMusic.mFadeOutDuration);
}

static void SyncZombieTailPortable(PortableSaveContext& theContext, Zombie& theZombie)
{
	SyncEnum32(theContext, theZombie.mZombieType);
	SyncEnum32(theContext, theZombie.mZombiePhase);
	theContext.SyncFloat(theZombie.mPosX);
	theContext.SyncFloat(theZombie.mPosY);
	theContext.SyncFloat(theZombie.mVelX);
	theContext.SyncInt32(theZombie.mAnimCounter);
	theContext.SyncInt32(theZombie.mGroanCounter);
	theContext.SyncInt32(theZombie.mAnimTicksPerFrame);
	theContext.SyncInt32(theZombie.mAnimFrames);
	theContext.SyncInt32(theZombie.mFrame);
	theContext.SyncInt32(theZombie.mPrevFrame);
	theContext.SyncBool(theZombie.mVariant);
	theContext.SyncBool(theZombie.mIsEating);
	theContext.SyncInt32(theZombie.mJustGotShotCounter);
	theContext.SyncInt32(theZombie.mShieldJustGotShotCounter);
	theContext.SyncInt32(theZombie.mShieldRecoilCounter);
	theContext.SyncInt32(theZombie.mZombieAge);
	SyncEnum32(theContext, theZombie.mZombieHeight);
	theContext.SyncInt32(theZombie.mPhaseCounter);
	theContext.SyncInt32(theZombie.mFromWave);
	theContext.SyncBool(theZombie.mDroppedLoot);
	theContext.SyncInt32(theZombie.mZombieFade);
	theContext.SyncBool(theZombie.mFlatTires);
	theContext.SyncInt32(theZombie.mUseLadderCol);
	theContext.SyncInt32(theZombie.mTargetCol);
	theContext.SyncFloat(theZombie.mAltitude);
	theContext.SyncBool(theZombie.mHitUmbrella);
	SyncRectPortable(theContext, theZombie.mZombieRect);
	SyncRectPortable(theContext, theZombie.mZombieAttackRect);
	theContext.SyncInt32(theZombie.mChilledCounter);
	theContext.SyncInt32(theZombie.mButteredCounter);
	theContext.SyncInt32(theZombie.mIceTrapCounter);
	theContext.SyncBool(theZombie.mMindControlled);
	theContext.SyncBool(theZombie.mBlowingAway);
	theContext.SyncBool(theZombie.mHasHead);
	theContext.SyncBool(theZombie.mHasArm);
	theContext.SyncBool(theZombie.mHasObject);
	theContext.SyncBool(theZombie.mInPool);
	theContext.SyncBool(theZombie.mOnHighGround);
	theContext.SyncBool(theZombie.mYuckyFace);
	theContext.SyncInt32(theZombie.mYuckyFaceCounter);
	SyncEnum32(theContext, theZombie.mHelmType);
	theContext.SyncInt32(theZombie.mBodyHealth);
	theContext.SyncInt32(theZombie.mBodyMaxHealth);
	theContext.SyncInt32(theZombie.mHelmHealth);
	theContext.SyncInt32(theZombie.mHelmMaxHealth);
	SyncEnum32(theContext, theZombie.mShieldType);
	theContext.SyncInt32(theZombie.mShieldHealth);
	theContext.SyncInt32(theZombie.mShieldMaxHealth);
	theContext.SyncInt32(theZombie.mFlyingHealth);
	theContext.SyncInt32(theZombie.mFlyingMaxHealth);
	theContext.SyncBool(theZombie.mDead);
	SyncEnumU32(theContext, theZombie.mRelatedZombieID);
	SyncEnumU32Array(theContext, &theZombie.mFollowerZombieID[0], MAX_ZOMBIE_FOLLOWERS);
	theContext.SyncBool(theZombie.mPlayingSong);
	theContext.SyncInt32(theZombie.mParticleOffsetX);
	theContext.SyncInt32(theZombie.mParticleOffsetY);
	SyncEnum32(theContext, theZombie.mAttachmentID);
	theContext.SyncInt32(theZombie.mSummonCounter);
	SyncEnumU32(theContext, theZombie.mBodyReanimID);
	theContext.SyncFloat(theZombie.mScaleZombie);
	theContext.SyncFloat(theZombie.mVelZ);
	theContext.SyncFloat(theZombie.mOriginalAnimRate);
	SyncEnumU32(theContext, theZombie.mTargetPlantID);
	theContext.SyncInt32(theZombie.mBossMode);
	theContext.SyncInt32(theZombie.mTargetRow);
	theContext.SyncInt32(theZombie.mBossBungeeCounter);
	theContext.SyncInt32(theZombie.mBossStompCounter);
	theContext.SyncInt32(theZombie.mBossHeadCounter);
	SyncEnumU32(theContext, theZombie.mBossFireBallReanimID);
	SyncEnumU32(theContext, theZombie.mSpecialHeadReanimID);
	theContext.SyncInt32(theZombie.mFireballRow);
	theContext.SyncBool(theZombie.mIsFireBall);
	SyncEnumU32(theContext, theZombie.mMoweredReanimID);
	theContext.SyncInt32(theZombie.mLastPortalX);
	SyncEnumU32(theContext, theZombie.mZombatarHeadReanimID);
}

static void SyncPlantTailPortable(PortableSaveContext& theContext, Plant& thePlant)
{
	SyncEnum32(theContext, thePlant.mSeedType);
	theContext.SyncInt32(thePlant.mPlantCol);
	theContext.SyncInt32(thePlant.mAnimCounter);
	theContext.SyncInt32(thePlant.mFrame);
	theContext.SyncInt32(thePlant.mFrameLength);
	theContext.SyncInt32(thePlant.mNumFrames);
	SyncEnum32(theContext, thePlant.mState);
	theContext.SyncInt32(thePlant.mPlantHealth);
	theContext.SyncInt32(thePlant.mPlantMaxHealth);
	theContext.SyncInt32(thePlant.mSubclass);
	theContext.SyncInt32(thePlant.mDisappearCountdown);
	theContext.SyncInt32(thePlant.mDoSpecialCountdown);
	theContext.SyncInt32(thePlant.mStateCountdown);
	theContext.SyncInt32(thePlant.mLaunchCounter);
	theContext.SyncInt32(thePlant.mLaunchRate);
	SyncRectPortable(theContext, thePlant.mPlantRect);
	SyncRectPortable(theContext, thePlant.mPlantAttackRect);
	theContext.SyncInt32(thePlant.mTargetX);
	theContext.SyncInt32(thePlant.mTargetY);
	theContext.SyncInt32(thePlant.mStartRow);
	SyncEnumU32(theContext, thePlant.mParticleID);
	theContext.SyncInt32(thePlant.mShootingCounter);
	SyncEnumU32(theContext, thePlant.mBodyReanimID);
	SyncEnumU32(theContext, thePlant.mHeadReanimID);
	SyncEnumU32(theContext, thePlant.mHeadReanimID2);
	SyncEnumU32(theContext, thePlant.mHeadReanimID3);
	SyncEnumU32(theContext, thePlant.mBlinkReanimID);
	SyncEnumU32(theContext, thePlant.mLightReanimID);
	SyncEnumU32(theContext, thePlant.mSleepingReanimID);
	theContext.SyncInt32(thePlant.mBlinkCountdown);
	theContext.SyncInt32(thePlant.mRecentlyEatenCountdown);
	theContext.SyncInt32(thePlant.mEatenFlashCountdown);
	theContext.SyncInt32(thePlant.mBeghouledFlashCountdown);
	theContext.SyncFloat(thePlant.mShakeOffsetX);
	theContext.SyncFloat(thePlant.mShakeOffsetY);
	for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		SyncMagnetItemPortable(theContext, thePlant.mMagnetItems[i]);
	SyncEnumU32(theContext, thePlant.mTargetZombieID);
	theContext.SyncInt32(thePlant.mWakeUpCounter);
	SyncEnum32(theContext, thePlant.mOnBungeeState);
	SyncEnum32(theContext, thePlant.mImitaterType);
	theContext.SyncInt32(thePlant.mPottedPlantIndex);
	theContext.SyncBool(thePlant.mAnimPing);
	theContext.SyncBool(thePlant.mDead);
	theContext.SyncBool(thePlant.mSquished);
	theContext.SyncBool(thePlant.mIsAsleep);
	theContext.SyncBool(thePlant.mIsOnBoard);
	theContext.SyncBool(thePlant.mHighlighted);

	if (theContext.mReading)
	{
		// Upgrade the earlier rendering-only sniper saved as a non-shooter.
		if (thePlant.mSeedType == SeedType::SEED_SNIPER_FEMALE && thePlant.mLaunchRate == 0)
		{
			thePlant.mSubclass = PlantSubClass::SUBCLASS_SHOOTER;
			thePlant.mLaunchRate = GetPlantDefinition(SeedType::SEED_SNIPER_FEMALE).mLaunchRate;
			thePlant.mLaunchCounter = 0;
		}
		int aLegacyMaxHealth = 0;
		if (thePlant.mSeedType == SeedType::SEED_SPIKEWEED)
			aLegacyMaxHealth = 300;
		else if (thePlant.mSeedType == SeedType::SEED_SPIKEROCK)
			aLegacyMaxHealth = 450;

		if (aLegacyMaxHealth != 0 && thePlant.mPlantMaxHealth == aLegacyMaxHealth)
		{
			thePlant.mPlantHealth *= 3;
			thePlant.mPlantMaxHealth *= 3;
		}

		if (thePlant.mSeedType == SeedType::SEED_TWINSUNFLOWER && thePlant.mLaunchRate == 1250)
		{
			thePlant.mLaunchCounter = static_cast<int32_t>(std::clamp<int64_t>(
				(static_cast<int64_t>(thePlant.mLaunchCounter) * 625 + 625) / 1250, 0, 625));
			thePlant.mLaunchRate = 625;
		}
		if (thePlant.mSeedType == SeedType::SEED_PLANTERN && thePlant.mLaunchRate == 2500)
		{
			thePlant.mLaunchCounter = static_cast<int32_t>(std::clamp<int64_t>(
				(static_cast<int64_t>(thePlant.mLaunchCounter) * 625 + 1250) / 2500, 0, 625));
			thePlant.mLaunchRate = 625;
			thePlant.mStateCountdown = 2500;
		}
	}
}

static void SyncProjectileTailPortable(PortableSaveContext& theContext, Projectile& theProjectile)
{
	theContext.SyncInt32(theProjectile.mFrame);
	theContext.SyncInt32(theProjectile.mNumFrames);
	theContext.SyncInt32(theProjectile.mAnimCounter);
	theContext.SyncFloat(theProjectile.mPosX);
	theContext.SyncFloat(theProjectile.mPosY);
	theContext.SyncFloat(theProjectile.mPosZ);
	theContext.SyncFloat(theProjectile.mVelX);
	theContext.SyncFloat(theProjectile.mVelY);
	theContext.SyncFloat(theProjectile.mVelZ);
	theContext.SyncFloat(theProjectile.mAccZ);
	theContext.SyncFloat(theProjectile.mShadowY);
	theContext.SyncBool(theProjectile.mDead);
	theContext.SyncInt32(theProjectile.mAnimTicksPerFrame);
	SyncEnum32(theContext, theProjectile.mMotionType);
	SyncEnum32(theContext, theProjectile.mProjectileType);
	theContext.SyncInt32(theProjectile.mProjectileAge);
	theContext.SyncInt32(theProjectile.mClickBackoffCounter);
	theContext.SyncFloat(theProjectile.mRotation);
	theContext.SyncFloat(theProjectile.mRotationSpeed);
	theContext.SyncBool(theProjectile.mOnHighGround);
	theContext.SyncInt32(theProjectile.mDamageRangeFlags);
	theContext.SyncInt32(theProjectile.mHitTorchwoodGridX);
	SyncEnum32(theContext, theProjectile.mAttachmentID);
	theContext.SyncFloat(theProjectile.mCobTargetX);
	theContext.SyncInt32(theProjectile.mCobTargetRow);
	SyncEnumU32(theContext, theProjectile.mTargetZombieID);
	theContext.SyncInt32(theProjectile.mLastPortalX);
	if (theContext.mReading && theProjectile.mProjectileType == ProjectileType::PROJECTILE_SNIPER_ARROW)
	{
		theProjectile.mWidth = Plant::SNIPER_ARROW_WIDTH;
		theProjectile.mHeight = Plant::SNIPER_ARROW_HEIGHT;
	}
}

static void SyncCoinTailPortable(PortableSaveContext& theContext, Coin& theCoin)
{
	theContext.SyncFloat(theCoin.mPosX);
	theContext.SyncFloat(theCoin.mPosY);
	theContext.SyncFloat(theCoin.mVelX);
	theContext.SyncFloat(theCoin.mVelY);
	theContext.SyncFloat(theCoin.mScale);
	theContext.SyncBool(theCoin.mDead);
	theContext.SyncInt32(theCoin.mFadeCount);
	theContext.SyncFloat(theCoin.mCollectX);
	theContext.SyncFloat(theCoin.mCollectY);
	theContext.SyncInt32(theCoin.mGroundY);
	theContext.SyncInt32(theCoin.mCoinAge);
	theContext.SyncBool(theCoin.mIsBeingCollected);
	theContext.SyncInt32(theCoin.mDisappearCounter);
	SyncEnum32(theContext, theCoin.mType);
	SyncEnum32(theContext, theCoin.mCoinMotion);
	SyncEnum32(theContext, theCoin.mAttachmentID);
	theContext.SyncFloat(theCoin.mCollectionDistance);
	SyncEnum32(theContext, theCoin.mUsableSeedType);
	SyncPottedPlantPortable(theContext, theCoin.mPottedPlantSpec);
	theContext.SyncBool(theCoin.mNeedsBouncyArrow);
	theContext.SyncBool(theCoin.mHasBouncyArrow);
	theContext.SyncBool(theCoin.mHitGround);
	theContext.SyncInt32(theCoin.mTimesDropped);
}

static void SyncLawnMowerTailPortable(PortableSaveContext& theContext, LawnMower& theMower)
{
	theContext.SyncFloat(theMower.mPosX);
	theContext.SyncFloat(theMower.mPosY);
	theContext.SyncInt32(theMower.mRenderOrder);
	theContext.SyncInt32(theMower.mRow);
	theContext.SyncInt32(theMower.mAnimTicksPerFrame);
	SyncEnumU32(theContext, theMower.mReanimID);
	theContext.SyncInt32(theMower.mChompCounter);
	theContext.SyncInt32(theMower.mRollingInCounter);
	theContext.SyncInt32(theMower.mSquishedCounter);
	SyncEnum32(theContext, theMower.mMowerState);
	theContext.SyncBool(theMower.mDead);
	theContext.SyncBool(theMower.mVisible);
	SyncEnum32(theContext, theMower.mMowerType);
	theContext.SyncFloat(theMower.mAltitude);
	SyncEnum32(theContext, theMower.mMowerHeight);
	theContext.SyncInt32(theMower.mLastPortalX);
}

static void SyncGridItemTailPortable(PortableSaveContext& theContext, GridItem& theItem)
{
	SyncEnum32(theContext, theItem.mGridItemType);
	SyncEnum32(theContext, theItem.mGridItemState);
	theContext.SyncInt32(theItem.mGridX);
	theContext.SyncInt32(theItem.mGridY);
	theContext.SyncInt32(theItem.mGridItemCounter);
	theContext.SyncInt32(theItem.mRenderOrder);
	theContext.SyncBool(theItem.mDead);
	theContext.SyncFloat(theItem.mPosX);
	theContext.SyncFloat(theItem.mPosY);
	theContext.SyncFloat(theItem.mGoalX);
	theContext.SyncFloat(theItem.mGoalY);
	SyncEnumU32(theContext, theItem.mGridItemReanimID);
	SyncEnumU32(theContext, theItem.mGridItemParticleID);
	SyncEnum32(theContext, theItem.mZombieType);
	SyncEnum32(theContext, theItem.mSeedType);
	SyncEnum32(theContext, theItem.mScaryPotType);
	theContext.SyncBool(theItem.mHighlighted);
	theContext.SyncInt32(theItem.mTransparentCounter);
	theContext.SyncInt32(theItem.mSunCount);
	for (int i = 0; i < NUM_MOTION_TRAIL_FRAMES; i++)
		SyncMotionTrailFramePortable(theContext, theItem.mMotionTrailFrames[i]);
	theContext.SyncInt32(theItem.mMotionTrailCount);
}

static void WriteGameObjectField(std::vector<unsigned char>& theOut, uint32_t theFieldId, GameObject& theObject)
{
	AppendFieldWithSync(theOut, theFieldId, [&](PortableSaveContext& aContext)
	{
		SyncGameObjectPortable(aContext, theObject);
	});
}

static bool ReadGameObjectField(const unsigned char* theData, size_t theSize, GameObject& theObject)
{
	return ApplyFieldWithSync(theData, theSize, [&](PortableSaveContext& aContext)
	{
		SyncGameObjectPortable(aContext, theObject);
	});
}

void WriteTLVBlob(PortableSaveContext& theContext, const std::vector<unsigned char>& theBlob)
{
	uint32_t aSize = static_cast<uint32_t>(theBlob.size());
	theContext.SyncUInt32(aSize);
	if (aSize > 0)
		theContext.SyncBytes(theBlob.data(), aSize);
}

bool ReadTLVBlob(PortableSaveContext& theContext, std::vector<unsigned char>& theBlob)
{
	uint32_t aSize = 0;
	theContext.SyncUInt32(aSize);
	if (theContext.mFailed)
		return false;
	theBlob.resize(aSize);
	if (aSize > 0)
		theContext.SyncBytes(theBlob.data(), aSize);
	return !theContext.mFailed;
}

// Syncs a single object as a TLV blob: GameObject field (1U) only when TObject derives from GameObject, plus the tail field.
template <typename TObject, typename TTailSync>
static void SyncSingleObjectTLV(PortableSaveContext& theContext, TObject& theObject, TTailSync theTailSync)
{
	static constexpr bool HAS_GAME_OBJECT_FIELD = std::is_base_of_v<GameObject, TObject>;
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U:
				if constexpr (HAS_GAME_OBJECT_FIELD)
					ReadGameObjectField(aFieldData, aFieldSize, theObject);
				break;
			case PORTABLE_FIELD_TAIL:
				ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ theTailSync(c, theObject); });
				break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		if constexpr (HAS_GAME_OBJECT_FIELD)
			WriteGameObjectField(aBlob, 1U, theObject);
		AppendFieldWithSync(aBlob, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ theTailSync(c, theObject); });
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncReanimTransformPortable(PortableSaveContext& theContext, ReanimatorTransform& theTransform)
{
	theContext.SyncFloat(theTransform.mTransX);
	theContext.SyncFloat(theTransform.mTransY);
	theContext.SyncFloat(theTransform.mSkewX);
	theContext.SyncFloat(theTransform.mSkewY);
	theContext.SyncFloat(theTransform.mScaleX);
	theContext.SyncFloat(theTransform.mScaleY);
	theContext.SyncFloat(theTransform.mFrame);
	theContext.SyncFloat(theTransform.mAlpha);
	if (theContext.mReading)
	{
		theTransform.mImage = nullptr;
		theTransform.mFont = nullptr;
		theTransform.mText = "";
	}
}

static void SyncReanimTrackInstancePortable(PortableSaveContext& theContext, ReanimatorTrackInstance& theTrackInstance)
{
	theContext.SyncInt32(theTrackInstance.mBlendCounter);
	theContext.SyncInt32(theTrackInstance.mBlendTime);
	SyncReanimTransformPortable(theContext, theTrackInstance.mBlendTransform);
	theContext.SyncFloat(theTrackInstance.mShakeOverride);
	theContext.SyncFloat(theTrackInstance.mShakeX);
	theContext.SyncFloat(theTrackInstance.mShakeY);
	theContext.SyncInt32(reinterpret_cast<int32_t&>(theTrackInstance.mAttachmentID));
	SyncImagePortable(theContext, theTrackInstance.mImageOverride);
	theContext.SyncInt32(theTrackInstance.mRenderGroup);
	SyncColorPortable(theContext, theTrackInstance.mTrackColor);
	theContext.SyncBool(theTrackInstance.mIgnoreClipRect);
	theContext.SyncBool(theTrackInstance.mTruncateDisappearingFrames);
	theContext.SyncBool(theTrackInstance.mIgnoreColorOverride);
	theContext.SyncBool(theTrackInstance.mIgnoreExtraAdditiveColor);
}

static void SyncReanimationPortable(Board* theBoard, Reanimation* theReanimation, PortableSaveContext& theContext)
{
	SyncReanimationDefPortable(theContext, theReanimation->mDefinition);
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder.get();
	}

	ReanimatorDefinition* aDef = theReanimation->mDefinition;
	ReanimatorDefinition* aDefStart = gReanimatorDefArray.get();
	ReanimatorDefinition* aDefEnd = gReanimatorDefArray.get() + static_cast<int>(ReanimationType::NUM_REANIMS);
	if (aDef == nullptr || aDef < aDefStart || aDef >= aDefEnd)
	{
		int aType = static_cast<int>(theReanimation->mReanimationType);
		if (aType >= 0 && aType < static_cast<int>(ReanimationType::NUM_REANIMS))
		{
			ReanimatorEnsureDefinitionLoaded(static_cast<ReanimationType>(aType), true);
			aDef = &gReanimatorDefArray[aType];
			if (theContext.mReading)
				theReanimation->mDefinition = aDef;
		}
		else
		{
			aDef = nullptr;
		}
	}

	theContext.SyncEnum(theReanimation->mReanimationType);
	theContext.SyncFloat(theReanimation->mAnimTime);
	theContext.SyncFloat(theReanimation->mAnimRate);
	theContext.SyncEnum(theReanimation->mLoopType);
	theContext.SyncBool(theReanimation->mDead);
	theContext.SyncInt32(theReanimation->mFrameStart);
	theContext.SyncInt32(theReanimation->mFrameCount);
	theContext.SyncInt32(theReanimation->mFrameBasePose);
	SyncMatrixPortable(theContext, theReanimation->mOverlayMatrix);
	SyncColorPortable(theContext, theReanimation->mColorOverride);
	theContext.SyncInt32(theReanimation->mLoopCount);
	theContext.SyncBool(theReanimation->mIsAttachment);
	theContext.SyncInt32(theReanimation->mRenderOrder);
	SyncColorPortable(theContext, theReanimation->mExtraAdditiveColor);
	theContext.SyncBool(theReanimation->mEnableExtraAdditiveDraw);
	SyncColorPortable(theContext, theReanimation->mExtraOverlayColor);
	theContext.SyncBool(theReanimation->mEnableExtraOverlayDraw);
	theContext.SyncFloat(theReanimation->mLastFrameTime);
	theContext.SyncEnum(theReanimation->mFilterEffect);

	if (aDef && aDef->mTracks.count != 0)
	{
		int aCount = aDef->mTracks.count;
		bool aUseTemp = (theReanimation->mTrackInstances == nullptr);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = reinterpret_cast<ReanimatorTrackInstance*>(
				FindGlobalAllocator(aCount * sizeof(ReanimatorTrackInstance))->Calloc(aCount * sizeof(ReanimatorTrackInstance)));
			if (theReanimation->mTrackInstances == nullptr)
			{
				aUseTemp = true;
			}
			else
			{
				aUseTemp = false;
			}
		}
		else
		{
			PvzpAllocator* aAllocator = FindGlobalAllocator(aCount * sizeof(ReanimatorTrackInstance));
			if (aAllocator == nullptr || !aAllocator->IsPointerFromAllocator(theReanimation->mTrackInstances) || aAllocator->IsPointerOnFreeList(theReanimation->mTrackInstances))
			{
				aUseTemp = true;
			}
		}
		if (aUseTemp)
		{
			std::vector<ReanimatorTrackInstance> aTemp;
			aTemp.resize(aCount);
			memset(aTemp.data(), 0, sizeof(ReanimatorTrackInstance) * aCount);
			for (int aTrackIndex = 0; aTrackIndex < aCount; aTrackIndex++)
			{
				SyncReanimTrackInstancePortable(theContext, aTemp[aTrackIndex]);
			}
		}
		else
		{
			for (int aTrackIndex = 0; aTrackIndex < aCount; aTrackIndex++)
			{
				SyncReanimTrackInstancePortable(theContext, theReanimation->mTrackInstances[aTrackIndex]);
			}
		}
	}
}

static void SyncParticlePortable(PvzpParticle* theParticle, PortableSaveContext& theContext)
{
	theContext.SyncInt32(theParticle->mParticleDuration);
	theContext.SyncInt32(theParticle->mParticleAge);
	theContext.SyncFloat(theParticle->mParticleTimeValue);
	theContext.SyncFloat(theParticle->mParticleLastTimeValue);
	theContext.SyncFloat(theParticle->mAnimationTimeValue);
	SyncVector2Portable(theContext, theParticle->mVelocity);
	SyncVector2Portable(theContext, theParticle->mPosition);
	theContext.SyncInt32(theParticle->mImageFrame);
	theContext.SyncFloat(theParticle->mSpinPosition);
	theContext.SyncFloat(theParticle->mSpinVelocity);
	theContext.SyncInt32(reinterpret_cast<int32_t&>(theParticle->mCrossFadeParticleID));
	theContext.SyncInt32(theParticle->mCrossFadeDuration);
	for (int i = 0; i < ParticleTracks::NUM_PARTICLE_TRACKS; i++)
		theContext.SyncFloat(theParticle->mParticleInterp[i]);
	for (int i = 0; i < MAX_PARTICLE_FIELDS; i++)
	{
		for (int j = 0; j < 2; j++)
			theContext.SyncFloat(theParticle->mParticleFieldInterp[i][j]);
	}
}

static void SyncParticleEmitterPortable(PvzpParticleSystem* theParticleSystem, PvzpParticleEmitter* theParticleEmitter, PortableSaveContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt32(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = (reinterpret_cast<intptr_t>(theParticleEmitter->mEmitterDef) -
			reinterpret_cast<intptr_t>(theParticleSystem->mParticleDef->mEmitterDefs)) / sizeof(PvzpEmitterDefinition);
		theContext.SyncInt32(aEmitterDefIndex);
	}

	SyncDataIDListPortable((PvzpList<uint32_t>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	SyncVector2Portable(theContext, theParticleEmitter->mSystemCenter);
	SyncColorPortable(theContext, theParticleEmitter->mColorOverride);
	SyncImagePortable(theContext, theParticleEmitter->mImageOverride);
	theContext.SyncFloat(theParticleEmitter->mSpawnAccum);
	theContext.SyncInt32(theParticleEmitter->mParticlesSpawned);
	theContext.SyncInt32(theParticleEmitter->mSystemAge);
	theContext.SyncInt32(theParticleEmitter->mSystemDuration);
	theContext.SyncFloat(theParticleEmitter->mSystemTimeValue);
	theContext.SyncFloat(theParticleEmitter->mSystemLastTimeValue);
	theContext.SyncBool(theParticleEmitter->mDead);
	theContext.SyncBool(theParticleEmitter->mExtraAdditiveDrawOverride);
	theContext.SyncFloat(theParticleEmitter->mScaleOverride);
	theContext.SyncInt32(reinterpret_cast<int32_t&>(theParticleEmitter->mCrossFadeEmitterID));
	theContext.SyncInt32(theParticleEmitter->mEmitterCrossFadeCountDown);
	theContext.SyncInt32(theParticleEmitter->mFrameOverride);
	for (int i = 0; i < ParticleSystemTracks::NUM_SYSTEM_TRACKS; i++)
		theContext.SyncFloat(theParticleEmitter->mTrackInterp[i]);
	for (int i = 0; i < MAX_PARTICLE_FIELDS; i++)
	{
		for (int j = 0; j < 2; j++)
			theContext.SyncFloat(theParticleEmitter->mSystemFieldInterp[i][j]);
	}

	for (PvzpListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<uint32_t>(aNode->mValue));
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
		SyncParticlePortable(aParticle, theContext);
	}
}

static void SyncParticleSystemPortable(Board* theBoard, PvzpParticleSystem* theParticleSystem, PortableSaveContext& theContext)
{
	SyncParticleDefPortable(theContext, theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder.get();
	}

	SyncDataIDListPortable((PvzpList<uint32_t>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (PvzpListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet(static_cast<uint32_t>(aNode->mValue));
		SyncParticleEmitterPortable(theParticleSystem, aEmitter, theContext);
	}

	theContext.SyncEnum(theParticleSystem->mEffectType);
	theContext.SyncBool(theParticleSystem->mDead);
	theContext.SyncBool(theParticleSystem->mIsAttachment);
	theContext.SyncInt32(theParticleSystem->mRenderOrder);
	theContext.SyncBool(theParticleSystem->mDontUpdate);
}

static void SyncTrailPortable(Board* theBoard, Trail* theTrail, PortableSaveContext& theContext)
{
	SyncTrailDefPortable(theContext, theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder.get();
	}

	for (int i = 0; i < 20; i++)
		SyncVector2Portable(theContext, theTrail->mTrailPoints[i].aPos);
	theContext.SyncInt32(theTrail->mNumTrailPoints);
	theContext.SyncBool(theTrail->mDead);
	theContext.SyncInt32(theTrail->mRenderOrder);
	theContext.SyncInt32(theTrail->mTrailAge);
	theContext.SyncInt32(theTrail->mTrailDuration);
	for (int i = 0; i < 4; i++)
		theContext.SyncFloat(theTrail->mTrailInterp[i]);
	SyncVector2Portable(theContext, theTrail->mTrailCenter);
	theContext.SyncBool(theTrail->mIsAttachment);
	SyncColorPortable(theContext, theTrail->mColorOverride);
}

template <typename T, typename TSyncFn>
static void SyncDataArrayPortable(PortableSaveContext& theContext, DataArray<T>& theDataArray, TSyncFn theSyncFn)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && (aMaxSize == 0 || aMaxSize > theDataArray.mMaxSize ||
		theDataArray.mMaxUsedCount > aMaxSize || theDataArray.mMaxUsedCount > theDataArray.mMaxSize ||
		theDataArray.mSize > theDataArray.mMaxUsedCount || theDataArray.mFreeListHead > theDataArray.mMaxUsedCount))
	{
		theContext.mFailed = true;
		return;
	}

	for (uint32_t i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.DataArrayGetIDAt(i));
		theSyncFn(theDataArray.DataArrayGetItemAt(i));
	}
}

template <typename T>
static void SyncDataArrayIdsOnlyPortable(PortableSaveContext& theContext, DataArray<T>& theDataArray)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && aMaxSize != theDataArray.mMaxSize)
	{
		theContext.mFailed = true;
	}

	for (uint32_t i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.DataArrayGetIDAt(i));
	}
}

template <typename T, typename TWriteFn, typename TReadFn>
static void SyncDataArrayPortableTLV(PortableSaveContext& theContext, DataArray<T>& theDataArray, TWriteFn theWriteFn, TReadFn theReadFn)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && (aMaxSize == 0 || aMaxSize > theDataArray.mMaxSize ||
		theDataArray.mMaxUsedCount > aMaxSize || theDataArray.mMaxUsedCount > theDataArray.mMaxSize ||
		theDataArray.mSize > theDataArray.mMaxUsedCount || theDataArray.mFreeListHead > theDataArray.mMaxUsedCount))
	{
		theContext.mFailed = true;
		return;
	}

	for (uint32_t i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.DataArrayGetIDAt(i));
		if (theContext.mReading)
		{
			uint32_t aItemSize = 0;
			theContext.SyncUInt32(aItemSize);
			T& anItem = theDataArray.DataArrayResetItemAt(i);
			std::vector<unsigned char> aItemData;
			aItemData.resize(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
			TLVReader aReader(aItemData.data(), aItemSize);
			while (aReader.mOk && aReader.mPos < aReader.mSize)
			{
				uint32_t aFieldId = 0;
				uint32_t aFieldSize = 0;
				if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
					break;
				const unsigned char* aFieldData = nullptr;
				if (!aReader.ReadBytes(aFieldData, aFieldSize))
					break;
				theReadFn(aFieldId, aFieldData, aFieldSize, anItem);
			}
		}
		else
		{
			bool aActive = (theDataArray.DataArrayGetIDAt(i) & DATA_ARRAY_KEY_MASK) != 0;
			uint32_t aItemSize = 0;
			std::vector<unsigned char> aItemData;
			if (aActive)
			{
				theWriteFn(aItemData, theDataArray.DataArrayGetItemAt(i));
				aItemSize = static_cast<uint32_t>(aItemData.size());
			}
			theContext.SyncUInt32(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
		}
	}
}

// Syncs a DataArray of entities: GameObject field (1U) only when T derives from GameObject, plus the tail field.
template <typename T, typename TTailSync>
static void SyncDataArrayObjectsTLV(PortableSaveContext& theContext, DataArray<T>& theDataArray, TTailSync theTailSync)
{
	SyncDataArrayPortableTLV(theContext, theDataArray,
		[&](std::vector<unsigned char>& aOut, T& anItem)
		{
			if constexpr (std::is_base_of_v<GameObject, T>)
				WriteGameObjectField(aOut, 1U, anItem);
			AppendFieldWithSync(aOut, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ theTailSync(c, anItem); });
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, T& anItem)
		{
			switch (aFieldId)
			{
			case 1U:
				if constexpr (std::is_base_of_v<GameObject, T>)
					ReadGameObjectField(aData, aSize, anItem);
				break;
			case PORTABLE_FIELD_TAIL:
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ theTailSync(c, anItem); });
				break;
			default: break;
			}
		});
}

static void SyncZombiesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mZombies,
		[&](std::vector<unsigned char>& aOut, Zombie& aZombie)
		{
			WriteGameObjectField(aOut, 1U, aZombie);
			AppendFieldWithSync(aOut, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ SyncZombieTailPortable(c, aZombie); });
			AppendFieldWithSync(aOut, 101U, [&](PortableSaveContext& c)
			{
				c.SyncInt32(aZombie.mTierBucketArmorHealth);
				c.SyncInt32(aZombie.mTierBucketArmorMaxHealth);
			});
			AppendFieldWithSync(aOut, 102U, [&](PortableSaveContext& c){ SyncEnumU32(c, aZombie.mFirstIgnoredSpikyPlantID); });
			AppendFieldWithSync(aOut, 103U, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mDolphinFirstLeapComplete); });
			AppendFieldWithSync(aOut, 104U, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mBloverKnockbackDistanceRemaining); });
			AppendFieldWithSync(aOut, 105U, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mThreeMillionSunDurabilityApplied); });
			AppendFieldWithSync(aOut, 106U, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mSpawnedByZombieRain); });
			AppendFieldWithSync(aOut, 107U, [&](PortableSaveContext& c){ c.SyncFloat(aZombie.mContinuousHealthRemainder); });
			AppendFieldWithSync(aOut, 108U, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mEphraimStaggerCounter); });
			AppendFieldWithSync(aOut, 109U, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mEphraimKnockbackDistanceRemaining); });
			AppendFieldWithSync(aOut, 110U, [&](PortableSaveContext& c)
			{
				c.SyncInt32(aZombie.mSniperWoundCounter);
				c.SyncInt32(aZombie.mSniperDotRemainder);
			});
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Zombie& aZombie)
		{
			if (aFieldId == 1U)
				ReadGameObjectField(aData, aSize, aZombie);
			else if (aFieldId == PORTABLE_FIELD_TAIL)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncZombieTailPortable(c, aZombie); });
			else if (aFieldId == 101U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					c.SyncInt32(aZombie.mTierBucketArmorHealth);
					c.SyncInt32(aZombie.mTierBucketArmorMaxHealth);
				});
			else if (aFieldId == 102U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncEnumU32(c, aZombie.mFirstIgnoredSpikyPlantID); });
			else if (aFieldId == 103U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mDolphinFirstLeapComplete); });
			else if (aFieldId == 104U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mBloverKnockbackDistanceRemaining); });
			else if (aFieldId == 105U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mThreeMillionSunDurabilityApplied); });
			else if (aFieldId == 106U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aZombie.mSpawnedByZombieRain); });
			else if (aFieldId == 107U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncFloat(aZombie.mContinuousHealthRemainder); });
			else if (aFieldId == 108U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mEphraimStaggerCounter); });
			else if (aFieldId == 109U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aZombie.mEphraimKnockbackDistanceRemaining); });
			else if (aFieldId == 110U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					c.SyncInt32(aZombie.mSniperWoundCounter);
					c.SyncInt32(aZombie.mSniperDotRemainder);
				});
			if (aZombie.mSniperWoundCounter < 0 || aZombie.mSniperWoundCounter > Zombie::SNIPER_WOUND_DURATION_TICKS)
				aZombie.mSniperWoundCounter = 0;
			if (aZombie.mSniperWoundCounter == 0 || (aZombie.mSniperDotRemainder < 0 || aZombie.mSniperDotRemainder >= 100000))
				aZombie.mSniperDotRemainder = 0;
			if (aZombie.mEphraimKnockbackDistanceRemaining < -BOARD_WIDTH || aZombie.mEphraimKnockbackDistanceRemaining > BOARD_WIDTH)
				aZombie.mEphraimKnockbackDistanceRemaining = 0;
			if (aZombie.mEphraimStaggerCounter < 0 || aZombie.mEphraimStaggerCounter > Plant::EPHRAIM_JAVELIN_STAGGER_TICKS)
				aZombie.mEphraimStaggerCounter = 0;
			if (!(aZombie.mContinuousHealthRemainder > -1.0f && aZombie.mContinuousHealthRemainder < 1.0f))
				aZombie.mContinuousHealthRemainder = 0.0f;
			if (aZombie.mTierBucketArmorHealth < 0 || aZombie.mTierBucketArmorMaxHealth < 0 ||
				aZombie.mTierBucketArmorMaxHealth > 1100 * 124 ||
				aZombie.mTierBucketArmorHealth > aZombie.mTierBucketArmorMaxHealth ||
				theBoard->mZombieStrengthTier < 5 || aZombie.mZombieType == ZombieType::ZOMBIE_BUNGEE)
			{
				aZombie.mTierBucketArmorHealth = 0;
				aZombie.mTierBucketArmorMaxHealth = 0;
			}
		});
}

static void SyncEphraimAfterimagePortable(PortableSaveContext& c, Plant::EphraimAfterimage& theEcho)
{
	c.SyncInt32(theEcho.mAttackSet);
	c.SyncInt32(theEcho.mElapsedTicks);
	c.SyncInt32(theEcho.mDelayTicks);
	c.SyncInt32(theEcho.mHitStopTicks);
	c.SyncInt32(theEcho.mPauseFlags);
	c.SyncInt32(theEcho.mTargetX);
	c.SyncInt32(theEcho.mTrailOffset);
}

static void SyncPlantsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mPlants,
		[&](std::vector<unsigned char>& aOut, Plant& aPlant)
		{
			WriteGameObjectField(aOut, 1U, aPlant);
			AppendFieldWithSync(aOut, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ SyncPlantTailPortable(c, aPlant); });
			AppendFieldWithSync(aOut, 101U, [&](PortableSaveContext& c){ SyncEnum32(c, aPlant.mGatlingPeaVolleyProjectileType); });
			AppendFieldWithSync(aOut, 102U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mSunMagnetCoffeeTicksRemaining); c.SyncInt32(aPlant.mSunMagnetCoffeeTicksUntilDamage); });
			AppendFieldWithSync(aOut, 103U, [&](PortableSaveContext& c){ SyncEnumU32(c, aPlant.mCattailTargetZombieID); });
			AppendFieldWithSync(aOut, 104U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mTwinSunflowerBombCountdown); });
			AppendFieldWithSync(aOut, 105U, [&](PortableSaveContext& c){ c.SyncBool(aPlant.mGatlingPeaMillionSunVolley); });
			AppendFieldWithSync(aOut, 106U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAttackSet); });
			AppendFieldWithSync(aOut, 107U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimHitStopCounter); });
			AppendFieldWithSync(aOut, 109U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAttackPauseFlags); });
			AppendFieldWithSync(aOut, 110U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageFrame); });
			AppendFieldWithSync(aOut, 111U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageChancePercent); });
			AppendFieldWithSync(aOut, 112U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageFailureCount); });
			AppendFieldWithSync(aOut, 113U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimagesRemaining); });
			AppendFieldWithSync(aOut, 114U, [&](PortableSaveContext& c)
			{
				uint32_t aCount = static_cast<uint32_t>(aPlant.mEphraimAfterimages.size());
				c.SyncUInt32(aCount);
				for (Plant::EphraimAfterimage& anEcho : aPlant.mEphraimAfterimages)
					SyncEphraimAfterimagePortable(c, anEcho);
			});
			// Preserve the original echo payload; store nesting in a separate field.
			AppendFieldWithSync(aOut, 115U, [&](PortableSaveContext& c)
			{
				uint32_t aCount = static_cast<uint32_t>(aPlant.mEphraimAfterimages.size());
				c.SyncUInt32(aCount);
				for (Plant::EphraimAfterimage& anEcho : aPlant.mEphraimAfterimages)
					c.SyncInt32(anEcho.mNestingDepth);
			});
			AppendFieldWithSync(aOut, 116U, [&](PortableSaveContext& c)
			{
				uint32_t aCount = static_cast<uint32_t>(aPlant.mEphraimAfterimages.size());
				c.SyncUInt32(aCount);
				for (Plant::EphraimAfterimage& anEcho : aPlant.mEphraimAfterimages)
				{
					int32_t anOriginX = aPlant.GetEphraimAfterimageOriginX(anEcho);
					c.SyncInt32(anOriginX);
				}
			});
			AppendFieldWithSync(aOut, 117U, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mSniperHitStopCounter); });
			AppendFieldWithSync(aOut, 108U, [&](PortableSaveContext& c){ c.SyncFloat(aPlant.mContinuousHealthRemainder); });
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Plant& aPlant)
		{
			if (aFieldId == 1U)
				ReadGameObjectField(aData, aSize, aPlant);
			else if (aFieldId == PORTABLE_FIELD_TAIL)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncPlantTailPortable(c, aPlant); });
			else if (aFieldId == 101U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncEnum32(c, aPlant.mGatlingPeaVolleyProjectileType); });
			else if (aFieldId == 102U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mSunMagnetCoffeeTicksRemaining); c.SyncInt32(aPlant.mSunMagnetCoffeeTicksUntilDamage); });
			else if (aFieldId == 103U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncEnumU32(c, aPlant.mCattailTargetZombieID); });
			else if (aFieldId == 104U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mTwinSunflowerBombCountdown); });
			else if (aFieldId == 105U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aPlant.mGatlingPeaMillionSunVolley); });
			else if (aFieldId == 106U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAttackSet); });
			else if (aFieldId == 107U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimHitStopCounter); });
			else if (aFieldId == 109U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAttackPauseFlags); });
			else if (aFieldId == 110U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageFrame); });
			else if (aFieldId == 111U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageChancePercent); });
				// Recover the tally for saves made before the explicit count was added.
				const int aChance = std::clamp(aPlant.mEphraimAfterimageChancePercent, Plant::EPHRAIM_AFTERIMAGE_CHANCE_PERCENT, 100);
				aPlant.mEphraimAfterimageFailureCount = std::clamp((aChance -
					Plant::EPHRAIM_AFTERIMAGE_CHANCE_PERCENT + Plant::EPHRAIM_AFTERIMAGE_CHANCE_INCREMENT_PERCENT - 1) /
					Plant::EPHRAIM_AFTERIMAGE_CHANCE_INCREMENT_PERCENT, 0, Plant::EPHRAIM_MAX_AFTERIMAGES);
			}
			else if (aFieldId == 112U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimageFailureCount); });
			else if (aFieldId == 113U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mEphraimAfterimagesRemaining); });
			else if (aFieldId == 114U)
			{
				if (!ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					uint32_t aCount = 0;
					c.SyncUInt32(aCount);
					// Seven int32 fields per echo; bound allocation by the actual payload.
					if (c.mFailed || aSize < 4 || aCount > (aSize - 4) / 28)
					{
						c.mFailed = true;
						return;
					}
					std::vector<Plant::EphraimAfterimage> anEchoes(aCount);
					for (Plant::EphraimAfterimage& anEcho : anEchoes)
					{
						SyncEphraimAfterimagePortable(c, anEcho);
						const int aPauseMask = Plant::EPHRAIM_ATTACK_FLAG_AFTERIMAGE_IMPACT | Plant::EPHRAIM_ATTACK_FLAG_AFTERIMAGE_ANTICIPATION |
							Plant::EPHRAIM_ATTACK_FLAG_RANGED | Plant::EPHRAIM_ATTACK_FLAG_RECOIL | Plant::EPHRAIM_ATTACK_FLAG_RELEASE;
						if (c.mFailed || anEcho.mAttackSet < 0 || anEcho.mAttackSet >= Plant::EPHRAIM_ATTACK_VARIANT_COUNT ||
							((anEcho.mPauseFlags & Plant::EPHRAIM_ATTACK_FLAG_RANGED) != 0 && anEcho.mAttackSet < 2) ||
							anEcho.mElapsedTicks < 0 || anEcho.mElapsedTicks >= Plant::EPHRAIM_ATTACK_DURATION_TICKS[anEcho.mAttackSet] ||
							anEcho.mDelayTicks < 0 || anEcho.mDelayTicks > (Plant::EPHRAIM_MAX_AFTERIMAGES - 1) * 12 ||
							anEcho.mHitStopTicks < 0 || anEcho.mHitStopTicks > Plant::EPHRAIM_HIT_STOP_TICKS ||
							(anEcho.mPauseFlags & ~aPauseMask) != 0 || anEcho.mTrailOffset < Plant::EPHRAIM_AFTERIMAGE_TRAIL_OFFSET ||
							anEcho.mTrailOffset > Plant::EPHRAIM_MAX_AFTERIMAGE_TRAIL_OFFSET)
						{
							c.mFailed = true;
							return;
						}
					}
					aPlant.mEphraimAfterimages = std::move(anEchoes);
					aPlant.mEphraimAfterimageFrame = -1;
				}))
					theContext.mFailed = true;
			}
			else if (aFieldId == 115U)
			{
				if (!ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					uint32_t aCount = 0;
					c.SyncUInt32(aCount);
					if (c.mFailed || aSize < 4 || aCount != aPlant.mEphraimAfterimages.size() || aCount > (aSize - 4) / 4)
					{
						c.mFailed = true;
						return;
					}
					for (Plant::EphraimAfterimage& anEcho : aPlant.mEphraimAfterimages)
					{
						c.SyncInt32(anEcho.mNestingDepth);
						if (anEcho.mNestingDepth < 0 || anEcho.mNestingDepth > Plant::EPHRAIM_MAX_AFTERIMAGE_NESTING)
							c.mFailed = true;
					}
				}))
					theContext.mFailed = true;
			}
			else if (aFieldId == 116U)
			{
				if (!ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					uint32_t aCount = 0;
					c.SyncUInt32(aCount);
					if (c.mFailed || aSize < 4 || aCount != aPlant.mEphraimAfterimages.size() || aCount > (aSize - 4) / 4)
					{
						c.mFailed = true;
						return;
					}
					for (Plant::EphraimAfterimage& anEcho : aPlant.mEphraimAfterimages)
					{
						c.SyncInt32(anEcho.mOriginX);
						if (std::abs(static_cast<int64_t>(anEcho.mOriginX)) > 2 * BOARD_WIDTH + Plant::EPHRAIM_MAX_AFTERIMAGE_TRAIL_OFFSET)
							c.mFailed = true;
					}
				}))
					theContext.mFailed = true;
			}
			else if (aFieldId == 117U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aPlant.mSniperHitStopCounter); });
			else if (aFieldId == 108U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncFloat(aPlant.mContinuousHealthRemainder); });
			if (aPlant.mSniperHitStopCounter < 0 || aPlant.mSniperHitStopCounter > Plant::SNIPER_HITSTOP_TICKS[1])
				aPlant.mSniperHitStopCounter = 0;
			if (!(aPlant.mContinuousHealthRemainder > -1.0f && aPlant.mContinuousHealthRemainder < 1.0f))
				aPlant.mContinuousHealthRemainder = 0.0f;
			if (aPlant.mEphraimAttackSet < 0 || aPlant.mEphraimAttackSet >= Plant::EPHRAIM_ATTACK_VARIANT_COUNT)
				aPlant.mEphraimAttackSet = 0;
			if (aPlant.mEphraimHitStopCounter < 0 || aPlant.mEphraimHitStopCounter > Plant::EPHRAIM_HIT_STOP_TICKS)
				aPlant.mEphraimHitStopCounter = 0;
			if (aPlant.mEphraimAttackPauseFlags < 0 || aPlant.mEphraimAttackPauseFlags > Plant::EPHRAIM_ATTACK_FLAGS_MASK)
				aPlant.mEphraimAttackPauseFlags = 0;
			if (aPlant.mEphraimAttackSet < 2)
				aPlant.mEphraimAttackPauseFlags &= ~Plant::EPHRAIM_ATTACK_FLAG_RANGED;
			const int anAfterimageSet = (aPlant.mEphraimAttackPauseFlags & Plant::EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_MASK) >> Plant::EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_SHIFT;
			if (anAfterimageSet >= Plant::EPHRAIM_ATTACK_VARIANT_COUNT)
				aPlant.mEphraimAttackPauseFlags &= ~Plant::EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_MASK;
			if (aPlant.mEphraimAfterimageFrame < -1 || aPlant.mEphraimAfterimageFrame >= Plant::EPHRAIM_ATTACK_ANIMATION_TICKS)
				aPlant.mEphraimAfterimageFrame = -1;
			if (aPlant.mEphraimAfterimageChancePercent < Plant::EPHRAIM_AFTERIMAGE_CHANCE_PERCENT || aPlant.mEphraimAfterimageChancePercent > 100)
			{
				aPlant.mEphraimAfterimageChancePercent = Plant::EPHRAIM_AFTERIMAGE_CHANCE_PERCENT;
				aPlant.mEphraimAfterimageFailureCount = 0;
			}
			if (aPlant.mEphraimAfterimageFailureCount < 0 || aPlant.mEphraimAfterimageFailureCount > Plant::EPHRAIM_MAX_AFTERIMAGES)
				aPlant.mEphraimAfterimageFailureCount = 0;
			if (aPlant.mEphraimAfterimagesRemaining < 0 || aPlant.mEphraimAfterimagesRemaining > Plant::EPHRAIM_MAX_AFTERIMAGES)
				aPlant.mEphraimAfterimagesRemaining = 0;
			if (aPlant.mGatlingPeaVolleyProjectileType != ProjectileType::PROJECTILE_PEA &&
				aPlant.mGatlingPeaVolleyProjectileType != ProjectileType::PROJECTILE_BUTTER &&
				aPlant.mGatlingPeaVolleyProjectileType != ProjectileType::PROJECTILE_CHERRYBOMB &&
				aPlant.mGatlingPeaVolleyProjectileType != ProjectileType::PROJECTILE_MELON)
				aPlant.mGatlingPeaVolleyProjectileType = ProjectileType::PROJECTILE_PEA;
			if (aPlant.mSunMagnetCoffeeTicksRemaining < 0 || aPlant.mSunMagnetCoffeeTicksRemaining > 300 ||
				aPlant.mSunMagnetCoffeeTicksUntilDamage < 0 || aPlant.mSunMagnetCoffeeTicksUntilDamage > 60 ||
				(aPlant.mSunMagnetCoffeeTicksRemaining > 0 && aPlant.mSunMagnetCoffeeTicksUntilDamage == 0))
			{
				aPlant.mSunMagnetCoffeeTicksRemaining = 0;
				aPlant.mSunMagnetCoffeeTicksUntilDamage = 0;
			}
			else if (aPlant.mSunMagnetCoffeeTicksRemaining == 0)
			{
				aPlant.mSunMagnetCoffeeTicksUntilDamage = 0;
			}
			if (aPlant.mTwinSunflowerBombCountdown < 0 || aPlant.mTwinSunflowerBombCountdown > TWIN_SUNFLOWER_ASSAULT_INTERVAL)
				aPlant.mTwinSunflowerBombCountdown = TWIN_SUNFLOWER_ASSAULT_INTERVAL;
		});
}

static void SyncProjectilesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mProjectiles,
		[&](std::vector<unsigned char>& aOut, Projectile& aProjectile)
		{
			WriteGameObjectField(aOut, 1U, aProjectile);
			AppendFieldWithSync(aOut, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ SyncProjectileTailPortable(c, aProjectile); });
			AppendFieldWithSync(aOut, 101U, [&](PortableSaveContext& c)
			{
				c.SyncBool(aProjectile.mPiercesZombies);
				c.SyncInt32(aProjectile.mPiercedZombieCount);
				SyncEnumU32Array(c, aProjectile.mPiercedZombieIDs, Projectile::MAX_PIERCING_HITS);
			});
			AppendFieldWithSync(aOut, 102U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mPlanternCob); });
			AppendFieldWithSync(aOut, 103U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mTargetTrackingEnded); });
			AppendFieldWithSync(aOut, 104U, [&](PortableSaveContext& c){ c.SyncInt32(aProjectile.mCattailRedirectionCount); });
			AppendFieldWithSync(aOut, 105U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mGatlingCherryShot); });
			AppendFieldWithSync(aOut, 106U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mMillionSunDamage); });
			AppendFieldWithSync(aOut, 107U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mWintermelonCherryShot); });
			AppendFieldWithSync(aOut, 108U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mTwoMillionSunCatTailDamage); });
			AppendFieldWithSync(aOut, 109U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mPlanternAutoCoffeeBean); });
			AppendFieldWithSync(aOut, 110U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mEphraimChargedJavelin); });
			AppendFieldWithSync(aOut, 111U, [&](PortableSaveContext& c){ SyncEnumU32(c, aProjectile.mSniperSourcePlantID); });
			AppendFieldWithSync(aOut, 112U, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mSniperCriticalArrow); });
			AppendFieldWithSync(aOut, 113U, [&](PortableSaveContext& c)
			{
				uint32_t aCount = static_cast<uint32_t>(aProjectile.mSniperPiercedZombieIDs.size());
				c.SyncUInt32(aCount);
				for (ZombieID& anID : aProjectile.mSniperPiercedZombieIDs)
					SyncEnumU32(c, anID);
			});
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Projectile& aProjectile)
		{
			if (aFieldId == 1U)
				ReadGameObjectField(aData, aSize, aProjectile);
			else if (aFieldId == PORTABLE_FIELD_TAIL)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncProjectileTailPortable(c, aProjectile); });
			else if (aFieldId == 101U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					c.SyncBool(aProjectile.mPiercesZombies);
					c.SyncInt32(aProjectile.mPiercedZombieCount);
					SyncEnumU32Array(c, aProjectile.mPiercedZombieIDs, Projectile::MAX_PIERCING_HITS);
				});
			else if (aFieldId == 102U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mPlanternCob); });
			else if (aFieldId == 103U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mTargetTrackingEnded); });
			else if (aFieldId == 104U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aProjectile.mCattailRedirectionCount); });
			else if (aFieldId == 105U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mGatlingCherryShot); });
			else if (aFieldId == 106U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mMillionSunDamage); });
			else if (aFieldId == 107U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mWintermelonCherryShot); });
			else if (aFieldId == 108U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mTwoMillionSunCatTailDamage); });
			else if (aFieldId == 109U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mPlanternAutoCoffeeBean); });
			else if (aFieldId == 110U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mEphraimChargedJavelin); });
			else if (aFieldId == 111U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncEnumU32(c, aProjectile.mSniperSourcePlantID); });
			else if (aFieldId == 112U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncBool(aProjectile.mSniperCriticalArrow); });
			else if (aFieldId == 113U)
			{
				if (!ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c)
				{
					uint32_t aCount = 0;
					c.SyncUInt32(aCount);
					if (c.mFailed || aSize < 4 || aCount > (aSize - 4) / 4)
					{
						c.mFailed = true;
						return;
					}
					aProjectile.mSniperPiercedZombieIDs.resize(aCount);
					for (ZombieID& anID : aProjectile.mSniperPiercedZombieIDs)
						SyncEnumU32(c, anID);
				}))
					theContext.mFailed = true;
			}
				if (aProjectile.mCattailRedirectionCount < 0)
					aProjectile.mCattailRedirectionCount = 0;
				else if (aProjectile.mCattailRedirectionCount > 1)
					aProjectile.mCattailRedirectionCount = 1;
			if (aProjectile.mPiercedZombieCount < 0 || aProjectile.mPiercedZombieCount > Projectile::MAX_PIERCING_HITS)
			{
				aProjectile.mPiercesZombies = false;
				aProjectile.mPiercedZombieCount = 0;
			}
		});
}

static void SyncCoinsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mCoins,
		[](std::vector<unsigned char>& aOut, Coin& aCoin)
		{
			WriteGameObjectField(aOut, 1U, aCoin);
			AppendFieldWithSync(aOut, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ SyncCoinTailPortable(c, aCoin); });
			AppendFieldWithSync(aOut, 101U, [&](PortableSaveContext& c){ SyncEnumU32(c, aCoin.mSunMagnetClaimID); c.SyncBool(aCoin.mSunMagnetPickupPending); });
			AppendFieldWithSync(aOut, 102U, [&](PortableSaveContext& c){ c.SyncInt32(aCoin.mSunValueOverride); });
		},
		[](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Coin& aCoin)
		{
			if (aFieldId == 1U)
				ReadGameObjectField(aData, aSize, aCoin);
			else if (aFieldId == PORTABLE_FIELD_TAIL)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncCoinTailPortable(c, aCoin); });
			else if (aFieldId == 101U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ SyncEnumU32(c, aCoin.mSunMagnetClaimID); c.SyncBool(aCoin.mSunMagnetPickupPending); });
			else if (aFieldId == 102U)
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(aCoin.mSunValueOverride); });
			if (aCoin.mSunValueOverride < 0)
				aCoin.mSunValueOverride = 0;
		});
}

static void SyncMowersPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayObjectsTLV(theContext, theBoard->mLawnMowers, SyncLawnMowerTailPortable);
}

static void SyncGridItemsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayObjectsTLV(theContext, theBoard->mGridItems, SyncGridItemTailPortable);
}

static void SyncParticleEmittersPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayIdsOnlyPortable(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
}

static void SyncParticlesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayIdsOnlyPortable(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
}

static void SyncParticleSystemsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems,
		[theBoard](std::vector<unsigned char>& aOut, PvzpParticleSystem& theSystem)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncParticleSystemPortable(theBoard, &theSystem, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, PvzpParticleSystem& theSystem)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncParticleSystemPortable(theBoard, &theSystem, aContext);
				});
			}
		});
}

static void SyncReanimationsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations,
		[theBoard](std::vector<unsigned char>& aOut, Reanimation& theReanimation)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncReanimationPortable(theBoard, &theReanimation, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Reanimation& theReanimation)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncReanimationPortable(theBoard, &theReanimation, aContext);
				});
			}
		});
}

static void SyncTrailsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails,
		[theBoard](std::vector<unsigned char>& aOut, Trail& theTrail)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncTrailPortable(theBoard, &theTrail, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Trail& theTrail)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncTrailPortable(theBoard, &theTrail, aContext);
				});
			}
		});
}

static void SyncAttachmentsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayObjectsTLV(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments, SyncAttachmentTailPortable);
}

static void SyncCursorPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mCursorObject, SyncCursorObjectTailPortable);
}

static void SyncCursorPreviewPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mCursorPreview, SyncCursorPreviewTailPortable);
}

static void SyncAdvicePortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mAdvice, SyncMessageWidgetTailPortable);
}

static void SyncSeedBankPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mSeedBank, SyncSeedBankTailPortable);
}

static void SyncSeedPacketsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	int aCount = SEEDBANK_MAX;
	theContext.SyncInt32(aCount);
	for (int i = 0; i < aCount && i < SEEDBANK_MAX; i++)
	{
		if (theContext.mReading)
		{
			uint32_t aItemSize = 0;
			theContext.SyncUInt32(aItemSize);
			std::vector<unsigned char> aItemData;
			aItemData.resize(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
			TLVReader aReader(aItemData.data(), aItemSize);
			while (aReader.mOk && aReader.mPos < aReader.mSize)
			{
				uint32_t aFieldId = 0;
				uint32_t aFieldSize = 0;
				if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
					break;
				const unsigned char* aFieldData = nullptr;
				if (!aReader.ReadBytes(aFieldData, aFieldSize))
					break;
				switch (aFieldId)
				{
				case 1U: ReadGameObjectField(aFieldData, aFieldSize, theBoard->mSeedBank->mSeedPackets[i]); break;
				case PORTABLE_FIELD_TAIL: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ SyncSeedPacketTailPortable(c, theBoard->mSeedBank->mSeedPackets[i]); }); break;
				default: break;
				}
			}
		}
		else
		{
			std::vector<unsigned char> aItemData;
			WriteGameObjectField(aItemData, 1U, theBoard->mSeedBank->mSeedPackets[i]);
			AppendFieldWithSync(aItemData, PORTABLE_FIELD_TAIL, [&](PortableSaveContext& c){ SyncSeedPacketTailPortable(c, theBoard->mSeedBank->mSeedPackets[i]); });
			uint32_t aItemSize = static_cast<uint32_t>(aItemData.size());
			theContext.SyncUInt32(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
		}
	}
}

static void SyncChallengePortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mChallenge, SyncChallengeTailPortable);
}

static void SyncMusicPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncSingleObjectTLV(theContext, *theBoard->mApp->mMusic, SyncMusicTailPortable);
}

static void SyncBoardPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncBoardBasePortable(theContext, theBoard);
	SyncZombiesPortable(theContext, theBoard);
	SyncPlantsPortable(theContext, theBoard);
	SyncProjectilesPortable(theContext, theBoard);
	SyncCoinsPortable(theContext, theBoard);
	SyncMowersPortable(theContext, theBoard);
	SyncGridItemsPortable(theContext, theBoard);
	SyncParticleEmittersPortable(theContext, theBoard);
	SyncParticlesPortable(theContext, theBoard);
	SyncParticleSystemsPortable(theContext, theBoard);
	SyncReanimationsPortable(theContext, theBoard);
	SyncTrailsPortable(theContext, theBoard);
	SyncAttachmentsPortable(theContext, theBoard);
	SyncCursorPortable(theContext, theBoard);
	SyncCursorPreviewPortable(theContext, theBoard);
	SyncAdvicePortable(theContext, theBoard);
	SyncSeedBankPortable(theContext, theBoard);
	SyncSeedPacketsPortable(theContext, theBoard);
	SyncChallengePortable(theContext, theBoard);
	SyncMusicPortable(theContext, theBoard);
}


typedef void (*ChunkSyncFn)(PortableSaveContext&, Board*);

struct ChunkDescriptor
{
	uint32_t mType;
	ChunkSyncFn mSync;
};

// Stable order and IDs match every save written before the modularization.
constexpr std::array<ChunkDescriptor, 20> SAVE_CHUNKS = {{
	{SAVE4_CHUNK_BOARD_BASE, SyncBoardBasePortable},
	{SAVE4_CHUNK_ZOMBIES, SyncZombiesPortable},
	{SAVE4_CHUNK_PLANTS, SyncPlantsPortable},
	{SAVE4_CHUNK_PROJECTILES, SyncProjectilesPortable},
	{SAVE4_CHUNK_COINS, SyncCoinsPortable},
	{SAVE4_CHUNK_MOWERS, SyncMowersPortable},
	{SAVE4_CHUNK_GRIDITEMS, SyncGridItemsPortable},
	{SAVE4_CHUNK_PARTICLE_EMITTERS, SyncParticleEmittersPortable},
	{SAVE4_CHUNK_PARTICLE_PARTICLES, SyncParticlesPortable},
	{SAVE4_CHUNK_PARTICLE_SYSTEMS, SyncParticleSystemsPortable},
	{SAVE4_CHUNK_REANIMATIONS, SyncReanimationsPortable},
	{SAVE4_CHUNK_TRAILS, SyncTrailsPortable},
	{SAVE4_CHUNK_ATTACHMENTS, SyncAttachmentsPortable},
	{SAVE4_CHUNK_CURSOR, SyncCursorPortable},
	{SAVE4_CHUNK_CURSOR_PREVIEW, SyncCursorPreviewPortable},
	{SAVE4_CHUNK_ADVICE, SyncAdvicePortable},
	{SAVE4_CHUNK_SEEDBANK, SyncSeedBankPortable},
	{SAVE4_CHUNK_SEEDPACKETS, SyncSeedPacketsPortable},
	{SAVE4_CHUNK_CHALLENGE, SyncChallengePortable},
	{SAVE4_CHUNK_MUSIC, SyncMusicPortable},
}};

static ChunkSyncFn GetChunkSyncFn(uint32_t theChunkType)
{
	for (const auto& aChunk : SAVE_CHUNKS)
		if (aChunk.mType == theChunkType)
			return aChunk.mSync;
	return nullptr;
}

static bool WriteChunkV4(std::vector<unsigned char>& thePayload, uint32_t theChunkType, Board* theBoard)
{
	ChunkSyncFn aSyncFn = GetChunkSyncFn(theChunkType);
	if (!aSyncFn)
		return false;

	DataWriter aFieldWriter;
	aFieldWriter.OpenMemory(0x4000);
	PortableSaveContext aFieldContext(aFieldWriter);
	aSyncFn(aFieldContext, theBoard);
	if (aFieldContext.mFailed)
		return false;

	DataWriter aChunkWriter;
	aChunkWriter.OpenMemory(0x200);
	aChunkWriter.WriteUInt32(SAVE4_CHUNK_VERSION);
	aChunkWriter.WriteUInt32(1U);
	aChunkWriter.WriteUInt32(aFieldWriter.GetDataLen());
	aChunkWriter.WriteBytes(aFieldWriter.GetDataPtr(), aFieldWriter.GetDataLen());

	std::vector<unsigned char> aChunk;
	aChunk.resize(aChunkWriter.GetDataLen());
	memcpy(aChunk.data(), aChunkWriter.GetDataPtr(), aChunkWriter.GetDataLen());
	AppendChunk(thePayload, theChunkType, aChunk);
	return true;
}

static bool ReadChunkV4(uint32_t theChunkType, const unsigned char* theData, size_t theSize, Board* theBoard)
{
	ChunkSyncFn aSyncFn = GetChunkSyncFn(theChunkType);
	if (!aSyncFn)
		return true;
	if (theSize < 4)
		return false;

	TLVReader aReader(theData, theSize);
	uint32_t aChunkVersion = 0;
	if (!aReader.ReadU32(aChunkVersion))
		return false;
	if (aChunkVersion != SAVE4_CHUNK_VERSION)
		return false;

	bool aApplied = false;
	while (aReader.mOk && aReader.mPos < aReader.mSize)
	{
		uint32_t aFieldId = 0;
		uint32_t aFieldSize = 0;
		if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
			break;
		const unsigned char* aFieldData = nullptr;
		if (!aReader.ReadBytes(aFieldData, aFieldSize))
			break;

		if (aFieldId == 1U)
		{
			DataReader aFieldReader;
			aFieldReader.OpenMemory(aFieldData, static_cast<uint32_t>(aFieldSize), false);
			PortableSaveContext aContext(aFieldReader);
			aSyncFn(aContext, theBoard);
			if (aContext.mFailed)
				return false;
			aApplied = true;
		}
	}

	return aReader.mOk && aApplied;
}


Result ReadPortablePayload(Board* theBoard, std::span<const unsigned char> thePayload)
{
	TLVReader aReader(thePayload.data(), thePayload.size());
	bool aBaseLoaded = false;
	while (aReader.mOk && aReader.mPos < aReader.mSize)
	{
		const size_t aChunkOffset = aReader.mPos;
		uint32_t aChunkType = 0;
		uint32_t aChunkSize = 0;
		if (!aReader.ReadU32(aChunkType) || !aReader.ReadU32(aChunkSize))
			break;
		const unsigned char* aChunkData = nullptr;
		if (!aReader.ReadBytes(aChunkData, aChunkSize))
			break;

		if (!ReadChunkV4(aChunkType, aChunkData, aChunkSize, theBoard))
			return {Error::InvalidState, aChunkOffset, aChunkType};
		if (aChunkType == SAVE4_CHUNK_BOARD_BASE)
			aBaseLoaded = true;
	}

	if (!aReader.mOk)
		return {Error::InvalidRecord, aReader.mPos};
	if (!aBaseLoaded)
		return {Error::MissingBoard};

	return {};
}

Result WritePortablePayload(Board* theBoard, std::vector<unsigned char>& thePayload)
{
	for (const auto& aChunk : SAVE_CHUNKS)
		if (!WriteChunkV4(thePayload, aChunk.mType, theBoard))
			return {Error::InvalidState, 0, aChunk.mType};
	return {};
}
}
