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

#include "PvzpDebug.h"
#include "Definition.h"
#include "PvzpParticle.h"
#include "EffectSystem.h"
#include "misc/FrameProfiler.h"
#include "../LawnApp.h"
#include "../GameConstants.h"
#include "graphics/Graphics.h"
#include "graphics/GLInterface.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <format>
#include <mutex>
#include <thread>


float CrossFadeLerp(float theFrom, float theTo, bool theFromIsSet, bool theToIsSet, float theFraction)
{
	if (!theFromIsSet)
		return theTo;
	if (!theToIsSet)
		return theFrom;
	return theFrom + (theTo - theFrom) * theFraction;
}

bool PvzpParticleEmitter::GetRenderParams(PvzpParticle* theParticle, ParticleRenderParams* theParams)
{
	PvzpParticleEmitter* aEmitter = theParticle->mParticleEmitter;
	PvzpEmitterDefinition* aDef = aEmitter->mEmitterDef;

	// Color: a channel counts as set when its system track, particle track, or override is defined
	theParams->mRedIsSet = false;
	theParams->mRedIsSet |= FloatTrackIsSet(aDef->mSystemRed);
	theParams->mRedIsSet |= FloatTrackIsSet(aDef->mParticleRed);
	theParams->mRedIsSet |= aEmitter->mColorOverride.mRed != 1.0f;
	theParams->mGreenIsSet = false;
	theParams->mGreenIsSet |= FloatTrackIsSet(aDef->mSystemGreen);
	theParams->mGreenIsSet |= FloatTrackIsSet(aDef->mParticleGreen);
	theParams->mGreenIsSet |= aEmitter->mColorOverride.mGreen != 1.0f;
	theParams->mBlueIsSet = false;
	theParams->mBlueIsSet |= FloatTrackIsSet(aDef->mSystemBlue);
	theParams->mBlueIsSet |= FloatTrackIsSet(aDef->mParticleBlue);
	theParams->mBlueIsSet |= aEmitter->mColorOverride.mBlue != 1.0f;
	theParams->mAlphaIsSet = false;
	theParams->mAlphaIsSet |= FloatTrackIsSet(aDef->mSystemAlpha);
	theParams->mAlphaIsSet |= FloatTrackIsSet(aDef->mParticleAlpha);
	theParams->mAlphaIsSet |= aEmitter->mColorOverride.mAlpha != 1.0f;
	theParams->mParticleScaleIsSet = false;
	theParams->mParticleScaleIsSet |= FloatTrackIsSet(aDef->mParticleScale);
	theParams->mParticleScaleIsSet |= (aEmitter->mScaleOverride != 1.0f);
	theParams->mParticleStretchIsSet = FloatTrackIsSet(aDef->mParticleStretch);
	// Spin: also counts as set when using a random or launch-aligned initial spin
	theParams->mSpinPositionIsSet = false;
	theParams->mSpinPositionIsSet |= FloatTrackIsSet(aDef->mParticleSpinSpeed);
	theParams->mSpinPositionIsSet |= FloatTrackIsSet(aDef->mParticleSpinAngle);
	theParams->mSpinPositionIsSet |= TestBit(aDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_RANDOM_LAUNCH_SPIN));
	theParams->mSpinPositionIsSet |= TestBit(aDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_ALIGN_LAUNCH_SPIN));
	theParams->mPositionIsSet = false;
	theParams->mPositionIsSet |= (aDef->mParticleFields.count > 0.0f);
	theParams->mPositionIsSet |= FloatTrackIsSet(aDef->mEmitterRadius);
	theParams->mPositionIsSet |= FloatTrackIsSet(aDef->mEmitterOffsetX);
	theParams->mPositionIsSet |= FloatTrackIsSet(aDef->mEmitterOffsetY);
	theParams->mPositionIsSet |= FloatTrackIsSet(aDef->mEmitterBoxX);
	theParams->mPositionIsSet |= FloatTrackIsSet(aDef->mEmitterBoxY);

	float aSystemRed = aEmitter->SystemTrackEvaluate(aDef->mSystemRed, ParticleSystemTracks::TRACK_SYSTEM_RED);
	float aSystemGreen = aEmitter->SystemTrackEvaluate(aDef->mSystemGreen, ParticleSystemTracks::TRACK_SYSTEM_GREEN);
	float aSystemBlue = aEmitter->SystemTrackEvaluate(aDef->mSystemBlue, ParticleSystemTracks::TRACK_SYSTEM_BLUE);
	float aSystemAlpha = aEmitter->SystemTrackEvaluate(aDef->mSystemAlpha, ParticleSystemTracks::TRACK_SYSTEM_ALPHA);
	float aSystemBrightness = aEmitter->SystemTrackEvaluate(aDef->mSystemBrightness, ParticleSystemTracks::TRACK_SYSTEM_BRIGHTNESS);
	float aParticleRed = aEmitter->ParticleTrackEvaluate(aDef->mParticleRed, theParticle, ParticleTracks::TRACK_PARTICLE_RED);
	float aParticleGreen = aEmitter->ParticleTrackEvaluate(aDef->mParticleGreen, theParticle, ParticleTracks::TRACK_PARTICLE_GREEN);
	float aParticleBlue = aEmitter->ParticleTrackEvaluate(aDef->mParticleBlue, theParticle, ParticleTracks::TRACK_PARTICLE_BLUE);
	float aParticleAlpha = aEmitter->ParticleTrackEvaluate(aDef->mParticleAlpha, theParticle, ParticleTracks::TRACK_PARTICLE_ALPHA);
	float aParticleBrightness = aEmitter->ParticleTrackEvaluate(aDef->mParticleBrightness, theParticle, ParticleTracks::TRACK_PARTICLE_BRIGHTNESS);
	float aBrightness = aParticleBrightness * aSystemBrightness;
	// final color = particle color * system color * override color * brightness
	theParams->mRed = aParticleRed * aSystemRed * aEmitter->mColorOverride.mRed * aBrightness;
	theParams->mGreen = aParticleGreen * aSystemGreen * aEmitter->mColorOverride.mGreen * aBrightness;
	theParams->mBlue = aParticleBlue * aSystemBlue * aEmitter->mColorOverride.mBlue * aBrightness;
	theParams->mAlpha = aParticleAlpha * aSystemAlpha * aEmitter->mColorOverride.mAlpha * aBrightness;
	theParams->mPosX = theParticle->mPosition.x;
	theParams->mPosY = theParticle->mPosition.y;
	float aParticleScale = aEmitter->ParticleTrackEvaluate(aDef->mParticleScale, theParticle, ParticleTracks::TRACK_PARTICLE_SCALE);
	theParams->mParticleStretch = aEmitter->ParticleTrackEvaluate(aDef->mParticleStretch, theParticle, ParticleTracks::TRACK_PARTICLE_STRETCH);
	theParams->mParticleScale = aParticleScale * aEmitter->mScaleOverride;
	theParams->mSpinPosition = theParticle->mSpinPosition;

	PvzpParticle* aCrossFadeParticle = aEmitter->mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(static_cast<unsigned int>(theParticle->mCrossFadeParticleID));
	if (aCrossFadeParticle != nullptr)  // blend render params with the cross-fade source (from aCrossFadeParticle to theParticle)
	{
		ParticleRenderParams aCrossFadeParams;
		if (PvzpParticleEmitter::GetRenderParams(aCrossFadeParticle, &aCrossFadeParams))
		{
			float aFraction = theParticle->mParticleAge / static_cast<float>(aCrossFadeParticle->mCrossFadeDuration - 1);
			theParams->mRed = CrossFadeLerp(aCrossFadeParams.mRed, theParams->mRed, aCrossFadeParams.mRedIsSet, theParams->mRedIsSet, aFraction);
			theParams->mGreen = CrossFadeLerp(aCrossFadeParams.mGreen, theParams->mGreen, aCrossFadeParams.mGreenIsSet, theParams->mGreenIsSet, aFraction);
			theParams->mBlue = CrossFadeLerp(aCrossFadeParams.mBlue, theParams->mBlue, aCrossFadeParams.mBlueIsSet, theParams->mBlueIsSet, aFraction);
			theParams->mAlpha = CrossFadeLerp(aCrossFadeParams.mAlpha, theParams->mAlpha, aCrossFadeParams.mAlphaIsSet, theParams->mAlphaIsSet, aFraction);
			theParams->mParticleScale = CrossFadeLerp(
				aCrossFadeParams.mParticleScale, theParams->mParticleScale, aCrossFadeParams.mParticleScaleIsSet, theParams->mParticleScaleIsSet, aFraction);
			theParams->mParticleStretch = CrossFadeLerp(
				aCrossFadeParams.mParticleStretch, theParams->mParticleStretch, aCrossFadeParams.mParticleStretchIsSet, theParams->mParticleStretchIsSet, aFraction);
			theParams->mSpinPosition = CrossFadeLerp(
				aCrossFadeParams.mSpinPosition, theParams->mSpinPosition, aCrossFadeParams.mSpinPositionIsSet, theParams->mSpinPositionIsSet, aFraction);
			theParams->mPosX = CrossFadeLerp(aCrossFadeParams.mPosX, theParams->mPosX, aCrossFadeParams.mPositionIsSet, theParams->mPositionIsSet, aFraction);
			theParams->mPosY = CrossFadeLerp(aCrossFadeParams.mPosY, theParams->mPosY, aCrossFadeParams.mPositionIsSet, theParams->mPositionIsSet, aFraction);
			// a field set on the source also counts as set on this particle
			theParams->mRedIsSet |= aCrossFadeParams.mRedIsSet;
			theParams->mGreenIsSet |= aCrossFadeParams.mGreenIsSet;
			theParams->mBlueIsSet |= aCrossFadeParams.mBlueIsSet;
			theParams->mAlphaIsSet |= aCrossFadeParams.mAlphaIsSet;
			theParams->mParticleScaleIsSet |= aCrossFadeParams.mParticleScaleIsSet;
			theParams->mParticleStretchIsSet |= aCrossFadeParams.mParticleStretchIsSet;
			theParams->mSpinPositionIsSet |= aCrossFadeParams.mSpinPositionIsSet;
			theParams->mPositionIsSet |= aCrossFadeParams.mPositionIsSet;
		}
	}
	return true;
}

void RenderParticle(Graphics* g, PvzpParticle* theParticle, const Color& theColor, ParticleRenderParams* theParams, PvzpTriangleGroup* theTriangleGroup)
{
	PvzpParticleEmitter* aEmitter = theParticle->mParticleEmitter;
	PvzpEmitterDefinition* aEmitterDef = aEmitter->mEmitterDef;
	Image* aImage = aEmitter->mImageOverride != nullptr ? aEmitter->mImageOverride : aEmitterDef->mImage;
	if (aImage == nullptr)
		return;

	int aCelWidth = aImage->GetCelWidth();
	int aCelHeight = aImage->GetCelHeight();
	int aFrame = aEmitter->mFrameOverride;
	if (aFrame == -1)
	{
		if (FloatTrackIsSet(aEmitterDef->mAnimationRate))
			aFrame = std::clamp(static_cast<int>(theParticle->mAnimationTimeValue * aEmitterDef->mImageFrames), 0, aEmitterDef->mImageFrames - 1);
		else if (aEmitterDef->mAnimated)
			aFrame = std::clamp(static_cast<int>(theParticle->mParticleTimeValue * aEmitterDef->mImageFrames), 0, aEmitterDef->mImageFrames - 1);
		else
			aFrame = theParticle->mImageFrame;
	}
	aFrame += aEmitterDef->mImageCol;
	if (aFrame >= aImage->mNumCols)
		aFrame = aImage->mNumCols - 1;

	Rect aSrcRect(aFrame * aCelWidth, std::min(aEmitterDef->mImageRow, aImage->mNumRows - 1) * aCelHeight, aCelWidth, aCelHeight);
	float aClipTop = PvzpParticleEmitter::ParticleTrackEvaluate(aEmitterDef->mClipTop, theParticle, ParticleTracks::TRACK_PARTICLE_CLIP_TOP);
	float aClipBottom = PvzpParticleEmitter::ParticleTrackEvaluate(aEmitterDef->mClipBottom, theParticle, ParticleTracks::TRACK_PARTICLE_CLIP_BOTTOM);
	float aClipLeft = PvzpParticleEmitter::ParticleTrackEvaluate(aEmitterDef->mClipLeft, theParticle, ParticleTracks::TRACK_PARTICLE_CLIP_LEFT);
	float aClipRight = PvzpParticleEmitter::ParticleTrackEvaluate(aEmitterDef->mClipRight, theParticle, ParticleTracks::TRACK_PARTICLE_CLIP_RIGHT);
	PVZP_ASSERT(aClipTop >= 0.0f && aClipTop <= 1.0f);
	PVZP_ASSERT(aClipBottom >= 0.0f && aClipBottom <= 1.0f);
	PVZP_ASSERT(aClipLeft >= 0.0f && aClipLeft <= 1.0f);
	PVZP_ASSERT(aClipRight >= 0.0f && aClipRight <= 1.0f);
	theParams->mPosX += aClipLeft * aCelWidth;
	theParams->mPosY += aClipTop * aCelHeight;
	aSrcRect.mX += FloatRoundToInt(aClipLeft * aCelWidth);
	aSrcRect.mY += FloatRoundToInt(aClipTop * aCelHeight);
	aSrcRect.mWidth -= FloatRoundToInt(aCelWidth * (aClipLeft + aClipRight));
	aSrcRect.mHeight -= FloatRoundToInt(aCelHeight * (aClipBottom + aClipTop));  // adjust the source rect by the clip ratio of each side
	PVZP_ASSERT(aSrcRect.mX == aCelWidth * aFrame + FloatRoundToInt(aClipLeft * aCelWidth));
	PVZP_ASSERT(aSrcRect.mY == aCelHeight * aEmitterDef->mImageRow + FloatRoundToInt(aClipTop * aCelHeight));
	PVZP_ASSERT(aSrcRect.mX >= 0 && aSrcRect.mX < 10000);
	PVZP_ASSERT(aSrcRect.mY >= 0 && aSrcRect.mY < 10000);

	if (TestBit(aEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_ALIGN_TO_PIXELS)))
	{
		theParams->mPosX = FloatRoundToInt(theParams->mPosX);
		theParams->mPosY = FloatRoundToInt(theParams->mPosY);
	}
	int aDrawMode = g->mDrawMode;
	if (TestBit(aEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_ADDITIVE)))
		aDrawMode = Graphics::DRAWMODE_ADDITIVE;
	if (TestBit(aEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_FULLSCREEN)))
	{
		theTriangleGroup->DrawGroup(g);
		Color anOldColor = g->GetColor();
		int anOldDrawMode = g->GetDrawMode();
		g->SetColor(theColor);
		g->FillRect(-g->mTransX, -g->mTransY, BOARD_WIDTH, BOARD_HEIGHT);
		g->SetColor(anOldColor);
		g->SetDrawMode(anOldDrawMode);
	}
	else
	{
		SexyMatrix3 aTransform;
		PvzpScaleRotateTransformMatrix(
			aTransform,
			theParams->mPosX,
			theParams->mPosY,
			theParams->mSpinPosition,
			theParams->mParticleScale,
			theParams->mParticleStretch * theParams->mParticleScale
		);
		theTriangleGroup->AddTriangle(g, aImage, aTransform, g->mClipRect, theColor, aDrawMode, aSrcRect);
		if (aEmitter->mExtraAdditiveDrawOverride)
			theTriangleGroup->AddTriangle(g, aImage, aTransform, g->mClipRect, theColor, Graphics::DRAWMODE_ADDITIVE, aSrcRect);
	}
}

void PvzpParticleEmitter::DrawParticle(Graphics* g, PvzpParticle* theParticle, PvzpTriangleGroup* theTriangleGroup,
	uint32_t& theCullCount)
{
	if (theParticle->mCrossFadeDuration > 0)  // cross-fade source particles are not drawn
		return;
	if (mImageOverride == nullptr && mEmitterDef->mImage != nullptr && mCullRadius > 0.0f &&
		theParticle->mCrossFadeParticleID == ParticleID::PARTICLEID_NULL &&
		!TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_FULLSCREEN)))
	{
		const float aRadius = mCullRadius * std::abs(mScaleOverride);
		const float aPosX = theParticle->mPosition.x + g->mTransX;
		const float aPosY = theParticle->mPosition.y + g->mTransY;
		const Rect& aClip = g->mClipRect;
		if (aPosX + aRadius < aClip.mX || aPosX - aRadius > aClip.mX + aClip.mWidth ||
			aPosY + aRadius < aClip.mY || aPosY - aRadius > aClip.mY + aClip.mHeight)
		{
			++theCullCount;
			return;
		}
	}

	ParticleRenderParams aParams;
	if (GetRenderParams(theParticle, &aParams))
	{
		Color aColor(
			std::clamp(FloatRoundToInt(aParams.mRed), 0, 255),
			std::clamp(FloatRoundToInt(aParams.mGreen), 0, 255),
			std::clamp(FloatRoundToInt(aParams.mBlue), 0, 255),
			std::clamp(FloatRoundToInt(aParams.mAlpha), 0, 255)
		);
		if (aColor.mAlpha > 0)
		{
			aParams.mPosX += g->mTransX;
			aParams.mPosY += g->mTransY;

			PvzpParticle* aParticle;
			if (mImageOverride || mEmitterDef->mImage)
				aParticle = theParticle;
			else  // no image of its own: try the cross-fade source particle
				aParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(static_cast<unsigned int>(theParticle->mCrossFadeParticleID));
			if (aParticle != nullptr)
				RenderParticle(g, aParticle, aColor, &aParams, theTriangleGroup);
		}
	}
}

void PvzpParticleSystem::Draw(Graphics* g)
{
	Sexy::FrameProfiler& aProfiler = Sexy::FrameProfiler::Get();
	const uint64_t aProfileStart = aProfiler.BeginParticleEffect();
	PvzpTriangleGroup aTriangleGroup;
	uint32_t aParticlesVisited = 0;
	uint32_t aParticlesCulled = 0;
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
		mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue))->Draw(g, &aTriangleGroup,
			aParticlesVisited, aParticlesCulled);
	aTriangleGroup.DrawGroup(g);
	if (aProfileStart != 0)
		aProfiler.RecordParticleEffectDraw(static_cast<int>(mEffectType), aProfileStart, aParticlesVisited,
			aParticlesCulled, aTriangleGroup.mTrianglesFlushed, aTriangleGroup.mBatchFlushes);
}

void PvzpParticleEmitter::Draw(Graphics* g, PvzpTriangleGroup* theTriangleGroup, uint32_t& theParticlesVisited,
	uint32_t& theParticlesCulled)
{
	bool aHardWare = gSexyAppBase->Is3DAccelerated();
	if ((TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_SOFTWARE_ONLY)) && aHardWare) ||
		(TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_HARDWARE_ONLY)) && !aHardWare))
		return;

	for (PvzpListNode<ParticleID>* aNode = mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		++theParticlesVisited;
		DrawParticle(g, mParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<unsigned int>(aNode->mValue)),
			theTriangleGroup, theParticlesCulled);
	}
}

