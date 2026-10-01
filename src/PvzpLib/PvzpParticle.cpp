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

namespace
{
constexpr uint32_t ZAMBONI_SMOKE_PARTICLE_LIMIT = 12000;
constexpr float ZAMBONI_SMOKE_SPAWN_RATE_SCALE = 0.5f;
}


PvzpParticleSystem::PvzpParticleSystem()
{
	mEffectType = ParticleEffect::PARTICLE_NONE;
	mParticleDef = nullptr;
	mParticleHolder = nullptr;
	mDead = false;
	mDontUpdate = false;
	mIsAttachment = false;
	mRenderOrder = 0;
}

static bool IsLowPriorityParticleEffect(ParticleEffect theEffect)
{
	// Only explicitly decorative ambient effects shed quality; unknown and gameplay-feedback effects stay protected.
	switch (theEffect)
	{
	case ParticleEffect::PARTICLE_ICEBALL_TRAIL:
	case ParticleEffect::PARTICLE_FIREBALL_TRAIL:
	case ParticleEffect::PARTICLE_SNOWPEA_TRAIL:
	case ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL:
	case ParticleEffect::PARTICLE_ZAMBONI_SMOKE:
	case ParticleEffect::PARTICLE_CREDIT_STROBE:
	case ParticleEffect::PARTICLE_CREDITS_RAYSWIPE:
	case ParticleEffect::PARTICLE_CREDITS_FOG:
		return true;
	default:
		return false;
	}
}

static float GetGloomCloudQualityScale(int theQualityTier)
{
	switch (theQualityTier)
	{
	case 1: return 0.75f;
	case 2: return 0.50f;
	case 3: return 0.25f;
	default: return 1.0f;
	}
}

#if !defined(__EMSCRIPTEN__)
class ParticleCalculationWorkers
{
public:
	ParticleCalculationWorkers()
	{
		const unsigned int aHardwareThreads = std::thread::hardware_concurrency();
		const unsigned int aWorkerCount = std::min(16U, aHardwareThreads > 1 ? aHardwareThreads - 1 : 0U);
		mWorkers.reserve(aWorkerCount);
		for (unsigned int i = 0; i < aWorkerCount; ++i)
			mWorkers.emplace_back([this] { WorkerLoop(); });
	}

	~ParticleCalculationWorkers()
	{
		{
			std::lock_guard<std::mutex> aLock(mMutex);
			mStopping = true;
			++mGeneration;
		}
		mWake.notify_all();
		for (std::thread& aWorker : mWorkers)
			if (aWorker.joinable())
				aWorker.join();
	}

	bool Available() const { return !mWorkers.empty(); }

	void Run(const std::vector<PvzpParticleCalculationTask>& theTasks, size_t theBegin, size_t theEnd)
	{
		{
			std::lock_guard<std::mutex> aLock(mMutex);
			mTasks = &theTasks;
			mBatchEnd = theEnd;
			mNextParticle.store(theBegin, std::memory_order_relaxed);
			mWorkersRemaining = mWorkers.size();
			++mGeneration;
		}
		mWake.notify_all();
		ProcessParticles();
		std::unique_lock<std::mutex> aLock(mMutex);
		mFinished.wait(aLock, [this] { return mWorkersRemaining == 0; });
		mTasks = nullptr;
	}

private:
	void ProcessParticles()
	{
		constexpr size_t aChunkSize = 128;
		while (true)
		{
			const size_t aStart = mNextParticle.fetch_add(aChunkSize, std::memory_order_relaxed);
			if (aStart >= mBatchEnd)
				return;
			const size_t anEnd = std::min(aStart + aChunkSize, mBatchEnd);
			for (size_t i = aStart; i < anEnd; ++i)
			{
				const auto& aTask = (*mTasks)[i];
				aTask.mEmitter->CalculateParticleState(aTask.mParticle);
			}
		}
	}

	void WorkerLoop()
	{
		uint64_t aSeenGeneration = 0;
		std::unique_lock<std::mutex> aLock(mMutex);
		while (true)
		{
			mWake.wait(aLock, [this, aSeenGeneration] { return mStopping || mGeneration != aSeenGeneration; });
			if (mStopping)
				return;
			aSeenGeneration = mGeneration;
			aLock.unlock();
			ProcessParticles();
			aLock.lock();
			if (--mWorkersRemaining == 0)
				mFinished.notify_one();
		}
	}

	std::vector<std::thread> mWorkers;
	std::mutex mMutex;
	std::condition_variable mWake;
	std::condition_variable mFinished;
	std::atomic<size_t> mNextParticle{0};
	const std::vector<PvzpParticleCalculationTask>* mTasks = nullptr;
	size_t mBatchEnd = 0;
	size_t mWorkersRemaining = 0;
	uint64_t mGeneration = 0;
	bool mStopping = false;
};

static bool IsParticleParallelEnabled()
{
	static const bool anEnabled = []
	{
		const char* anEnvironment = std::getenv("PVZ_PARTICLE_PARALLEL");
		return anEnvironment != nullptr && std::strcmp(anEnvironment, "1") == 0;
	}();
	return anEnabled;
}

static ParticleCalculationWorkers& GetParticleCalculationWorkers()
{
	static ParticleCalculationWorkers aWorkers;
	return aWorkers;
}
#else
static bool IsParticleParallelEnabled()
{
	return false;
}
#endif

void PvzpParticleHolder::ProcessParticleUpdateBatch()
{
	auto& aTasks = mParticleUpdateScratch;
	if (aTasks.empty())
		return;

	size_t aValidTaskCount = 0;
	for (size_t i = 0; i < aTasks.size(); ++i)
	{
		auto& aTask = aTasks[i];
		PvzpParticleEmitter* anEmitter = mEmitters.DataArrayTryToGet(static_cast<unsigned int>(aTask.mEmitterId));
		PvzpParticle* aParticle = mParticles.DataArrayTryToGet(static_cast<unsigned int>(aTask.mParticleId));
		if (anEmitter == nullptr || aParticle == nullptr || anEmitter->mEmitterDef == nullptr ||
			anEmitter->mParticleSystem == nullptr || anEmitter->mParticleSystem->mParticleHolder != this ||
			anEmitter->mDead || anEmitter->mParticleSystem->mDead ||
			aParticle->mParticleEmitter != anEmitter || aParticle->mCrossFadeDuration > 0 ||
			aParticle->mCrossFadeParticleID != ParticleID::PARTICLEID_NULL ||
			anEmitter->mParticleSystem->mEffectType != aTask.mEffect)
			continue;

		aTask.mEmitter = anEmitter;
		aTask.mParticle = aParticle;
		if (aValidTaskCount != i)
			aTasks[aValidTaskCount] = aTask;
		++aValidTaskCount;
	}
	aTasks.resize(aValidTaskCount);
	if (aTasks.empty())
		return;

#if !defined(__EMSCRIPTEN__)
	constexpr size_t MINIMUM_PARALLEL_BATCH_SIZE = 1024;
	constexpr size_t MINIMUM_PARALLEL_EFFECT_SIZE = 512;
	Sexy::FrameProfiler& aProfiler = Sexy::FrameProfiler::Get();
	const bool aNeedsEffectTiming = aProfiler.IsDetailed();
	const bool aBatchLargeEnough = aTasks.size() >= MINIMUM_PARALLEL_BATCH_SIZE;
	if (aBatchLargeEnough || aNeedsEffectTiming)
	{
		std::sort(aTasks.begin(), aTasks.end(), [](const auto& theLeft, const auto& theRight)
			{ return theLeft.mEffect < theRight.mEffect; });
		ParticleCalculationWorkers* aWorkers = aBatchLargeEnough ? &GetParticleCalculationWorkers() : nullptr;
		const bool aParallelWorkersAvailable = aWorkers != nullptr && aWorkers->Available();
		for (size_t aBegin = 0; aBegin < aTasks.size(); )
		{
			size_t anEnd = aBegin + 1;
			while (anEnd < aTasks.size() && aTasks[anEnd].mEffect == aTasks[aBegin].mEffect)
				++anEnd;
			const bool aRunParallel = aParallelWorkersAvailable && anEnd - aBegin >= MINIMUM_PARALLEL_EFFECT_SIZE;
			const uint64_t aStart = aProfiler.BeginParticleEffect();
			if (aRunParallel)
			{
				aWorkers->Run(aTasks, aBegin, anEnd);
			}
			else
				for (size_t i = aBegin; i < anEnd; ++i)
					aTasks[i].mEmitter->CalculateParticleState(aTasks[i].mParticle);
			aProfiler.RecordParticleCalculation(static_cast<int>(aTasks[aBegin].mEffect),
				static_cast<uint32_t>(anEnd - aBegin), aStart, aRunParallel);
			aBegin = anEnd;
		}
		aTasks.clear();
		return;
	}
#endif

	for (const auto& aTask : aTasks)
		aTask.mEmitter->CalculateParticleState(aTask.mParticle);
	aTasks.clear();
}

PvzpParticleSystem::~PvzpParticleSystem()
{
	ParticleSystemDie();
	mEmitterList.RemoveAll();
}

void PvzpParticleSystem::PvzpParticleInitializeFromDef(float theX, float theY, int theRenderOrder, PvzpParticleDefinition* theDefinition, ParticleEffect theEffectType)
{
	PVZP_ASSERT(mParticleHolder);
	mEmitterList.SetAllocator(&mParticleHolder->mEmitterListNodeAllocator);
	mParticleDef = theDefinition;
	mEffectType = theEffectType;
	mRenderOrder = theRenderOrder;

	for (int i = 0; i < theDefinition->mEmitterDefCount; i++)
	{
		PvzpEmitterDefinition& aDef = theDefinition->mEmitterDefs[i];
		if (!FloatTrackIsSet(aDef.mCrossFadeDuration))
		{
			if (TestBit(aDef.mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_DIE_IF_OVERLOADED)) && mParticleHolder->IsOverLoaded())
			{
				ParticleSystemDie();
				break;
			}
			PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayAlloc();
			aEmitter->PvzpEmitterInitialize(theX, theY, this, &aDef);
			mEmitterList.AddTail(static_cast<ParticleEmitterID>(mParticleHolder->mEmitters.DataArrayGetID(aEmitter)));
		}
	}
}

void PvzpParticleEmitter::PvzpEmitterInitialize(float theX, float theY, PvzpParticleSystem* theSystem, PvzpEmitterDefinition* theEmitterDef)
{
	mSpawnAccum = 0.0f;
	mSpawnScaleAccum = 0.0f;
	mParticlesSpawned = 0;
	mSystemTimeValue = -1.0f;
	mSystemLastTimeValue = -1.0f;
	mSystemAge = -1;
	mDead = false;
	mColorOverride = Sexy::Color::White;
	mSystemCenter.x = theX;
	mSystemCenter.y = theY;
	mFrameOverride = -1;
	mCurrentQualityTier = 0;
	mParticleSystem = theSystem;
	mScaleOverride = 1.0f;
	mExtraAdditiveDrawOverride = false;
	mImageOverride = nullptr;
	mSystemDuration = 0;
	mEmitterDef = theEmitterDef;
	mCullRadius = 0.0f;
	if (mEmitterDef->mImage != nullptr)
	{
		const float aCelWidth = static_cast<float>(mEmitterDef->mImage->GetCelWidth());
		const float aCelHeight = static_cast<float>(mEmitterDef->mImage->GetCelHeight());
		float aScaleBound = 1.0f;
		for (int i = 0; mEmitterDef->mParticleScale.mNodes != nullptr && i < mEmitterDef->mParticleScale.mCountNodes; ++i)
			aScaleBound = std::max({ aScaleBound, std::abs(mEmitterDef->mParticleScale.mNodes[i].mLowValue),
				std::abs(mEmitterDef->mParticleScale.mNodes[i].mHighValue) });
		float aStretchBound = 1.0f;
		for (int i = 0; mEmitterDef->mParticleStretch.mNodes != nullptr && i < mEmitterDef->mParticleStretch.mCountNodes; ++i)
			aStretchBound = std::max({ aStretchBound, std::abs(mEmitterDef->mParticleStretch.mNodes[i].mLowValue),
				std::abs(mEmitterDef->mParticleStretch.mNodes[i].mHighValue) });
		mCullRadius = std::hypot(aCelWidth, aCelHeight) * aScaleBound * aStretchBound + 2.0f;
	}
	if (mEmitterDef->mParticleFields.count < 0 || mEmitterDef->mParticleFields.count > MAX_PARTICLE_FIELDS)
	{
		PvzpLogLn("Emitter '{}' has {} particle fields; limiting to {}",
			mEmitterDef->mName, mEmitterDef->mParticleFields.count, MAX_PARTICLE_FIELDS);
		mEmitterDef->mParticleFields.count = std::clamp(mEmitterDef->mParticleFields.count, 0, MAX_PARTICLE_FIELDS);
	}
	if (mEmitterDef->mSystemFields.count < 0 || mEmitterDef->mSystemFields.count > MAX_PARTICLE_FIELDS)
	{
		PvzpLogLn("Emitter '{}' has {} system fields; limiting to {}",
			mEmitterDef->mName, mEmitterDef->mSystemFields.count, MAX_PARTICLE_FIELDS);
		mEmitterDef->mSystemFields.count = std::clamp(mEmitterDef->mSystemFields.count, 0, MAX_PARTICLE_FIELDS);
	}
	mParticleList.SetAllocator(&theSystem->mParticleHolder->mParticleListNodeAllocator);

	if (FloatTrackIsSet(mEmitterDef->mSystemDuration))
		mSystemDuration = FloatTrackEvaluate(mEmitterDef->mSystemDuration, 0.0f, Sexy::Rand(1.0f));
	else
		mSystemDuration = FloatTrackEvaluate(mEmitterDef->mParticleDuration, 0.0f, 1.0f);
	mSystemDuration = std::max(1, mSystemDuration);

	for (int i = 0; i < mEmitterDef->mSystemFields.count; i++)
	{
		mSystemFieldInterp[i][0] = Sexy::Rand(1.0f);
		mSystemFieldInterp[i][1] = Sexy::Rand(1.0f);
	}
	for (int j = 0; j < 10; j++)
		mTrackInterp[j] = Sexy::Rand(1.0f);

	Update();
}

void PvzpParticleSystem::ParticleSystemDie()
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		aEmitter->DeleteAll();
		mParticleHolder->mEmitters.DataArrayFree(aEmitter);
	}
	mEmitterList.RemoveAll();
	mDead = true;
}

PvzpParticle* PvzpParticleEmitter::SpawnParticle(int theIndex, int theSpawnCount)
{
	DataArray<PvzpParticle>& aDataArray = mParticleSystem->mParticleHolder->mParticles;
	PvzpParticleHolder* aParticleHolder = mParticleSystem->mParticleHolder;
	if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_ZAMBONI_SMOKE &&
		aParticleHolder->mZamboniSmokeLiveParticles >= ZAMBONI_SMOKE_PARTICLE_LIMIT)
		return nullptr;
	if (aDataArray.mSize >= aDataArray.mMaxSize)
	{
		if (!aParticleHolder->mParticleCapacityWarningLogged)
		{
			int anEffectIndex = static_cast<int>(mParticleSystem->mEffectType);
			const char* anEffectName = anEffectIndex >= 0 && anEffectIndex < gParticleDefCount
				? gLawnParticleArray[anEffectIndex].mParticleFileName : "unknown";
			PvzpLogLn("Particle pool exhausted for effect '{}' emitter '{}': live {}/{}, high-water slots {}",
				anEffectName, mEmitterDef->mName, aDataArray.mSize, aDataArray.mMaxSize, aDataArray.mMaxUsedCount);
			aParticleHolder->mParticleCapacityWarningLogged = true;
		}
		return nullptr;
	}
	if (aDataArray.mSize < aDataArray.mMaxSize * 3U / 4U)
		aParticleHolder->mParticleCapacityWarningLogged = false;

	PvzpParticle* aParticle = aDataArray.DataArrayAlloc();
	if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_ZAMBONI_SMOKE)
		++aParticleHolder->mZamboniSmokeLiveParticles;
	PVZP_ASSERT(mEmitterDef->mParticleFields.count <= MAX_PARTICLE_FIELDS);
	for (int i = 0; i < mEmitterDef->mParticleFields.count; i++)
	{
		aParticle->mParticleFieldInterp[i][0] = Sexy::Rand(1.0f);  // random X interp for each particle field
		aParticle->mParticleFieldInterp[i][1] = Sexy::Rand(1.0f);  // random Y interp for each particle field
	}
	for (int i = 0; i < static_cast<int>(ParticleTracks::NUM_PARTICLE_TRACKS); i++)
		aParticle->mParticleInterp[i] = Sexy::Rand(1.0f);  // random interp for each track

	float aParticleDurationInterp = Sexy::Rand(1.0f);
	float aLaunchSpeedInterp = Sexy::Rand(1.0f);
	float aEmitterOffsetXInterp = Sexy::Rand(1.0f);
	float aEmitterOffsetYInterp = Sexy::Rand(1.0f);
	aParticle->mParticleDuration = FloatTrackEvaluate(mEmitterDef->mParticleDuration, mSystemTimeValue, aParticleDurationInterp);
	aParticle->mParticleDuration = std::max(1, aParticle->mParticleDuration);  // duration is at least 1
	aParticle->mParticleAge = 0;
	aParticle->mParticleEmitter = this;
	aParticle->mParticleTimeValue = -1.0f;
	aParticle->mParticleLastTimeValue = -1.0f;
	if (TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_RANDOM_START_TIME)))
		aParticle->mParticleAge = Sexy::Rand(aParticle->mParticleDuration);  // start at a random age
	float aLaunchSpeed = FloatTrackEvaluate(mEmitterDef->mLaunchSpeed, mSystemTimeValue, aLaunchSpeedInterp) * 0.01f;
	float aLaunchAngleInterp = Sexy::Rand(1.0f);

	float aLaunchAngle;
	if (mEmitterDef->mEmitterType == EmitterType::EMITTER_CIRCLE_PATH)
	{
		// launch angle = base angle on the circle from the path definition + offset from the launch angle definition
		aLaunchAngle = FloatTrackEvaluate(mEmitterDef->mEmitterPath, mSystemTimeValue, mTrackInterp[ParticleSystemTracks::TRACK_EMITTER_PATH]) * 2 * PI;
		aLaunchAngle += DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, mSystemTimeValue, aLaunchAngleInterp));
	}
	else if (mEmitterDef->mEmitterType == EmitterType::EMITTER_CIRCLE_EVEN_SPACING)
		// base angle spreads theSpawnCount particles evenly around the circle
		aLaunchAngle = 2 * PI * theIndex / theSpawnCount + DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, mSystemTimeValue, aLaunchAngleInterp));
	else if (FloatTrackIsConstantZero(mEmitterDef->mLaunchAngle))
		// no track defined: use a random launch angle in [0, 2π]
		aLaunchAngle = Sexy::Rand(static_cast<float>(2 * PI));
	else
		aLaunchAngle = DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, mSystemTimeValue, aLaunchAngleInterp));

	float aPosX, aPosY;
	switch (mEmitterDef->mEmitterType)
	{
	case EmitterType::EMITTER_CIRCLE:
	case EmitterType::EMITTER_CIRCLE_PATH:
	case EmitterType::EMITTER_CIRCLE_EVEN_SPACING:
	{
		float aEmitterRadiusInterp = Sexy::Rand(1.0f);
		float aRadius = FloatTrackEvaluate(mEmitterDef->mEmitterRadius, mSystemTimeValue, aEmitterRadiusInterp);
		// angle 0 points straight down
		aPosX = sin(aLaunchAngle) * aRadius;
		aPosY = cos(aLaunchAngle) * aRadius;
		break;
	}
	case EmitterType::EMITTER_BOX:
	{
		float aEmitterBoxXInterp = Sexy::Rand(1.0f);
		float aEmitterBoxYInterp = Sexy::Rand(1.0f);
		aPosX = FloatTrackEvaluate(mEmitterDef->mEmitterBoxX, mSystemTimeValue, aEmitterBoxXInterp);
		aPosY = FloatTrackEvaluate(mEmitterDef->mEmitterBoxY, mSystemTimeValue, aEmitterBoxYInterp);
		break;
	}
	case EmitterType::EMITTER_BOX_PATH:
	{
		float aEmitterPathPosition = FloatTrackEvaluate(mEmitterDef->mEmitterPath, mSystemTimeValue, mTrackInterp[ParticleSystemTracks::TRACK_EMITTER_PATH]);
		float aMinX = FloatTrackEvaluate(mEmitterDef->mEmitterBoxX, mSystemTimeValue, 0.0f);
		float aMaxX = FloatTrackEvaluate(mEmitterDef->mEmitterBoxX, mSystemTimeValue, 1.0f);
		float aMinY = FloatTrackEvaluate(mEmitterDef->mEmitterBoxY, mSystemTimeValue, 0.0f);
		float aMaxY = FloatTrackEvaluate(mEmitterDef->mEmitterBoxY, mSystemTimeValue, 1.0f);
		float aDistanceX = aMaxX - aMinX;  // width of the path rectangle
		float aDistanceY = aMaxY - aMinY;  // height of the path rectangle
		float aPathPos = aEmitterPathPosition * (aDistanceY + aDistanceX + aDistanceY + aDistanceX);  // spawn position along the rectangle's edges
		// Label the vertices A, B, C, D counter-clockwise starting from the top-left corner, and the spawn point P;
		// aPathPos is the distance from A to P along the path. X points right and Y points down.
		if (aPathPos < aDistanceY)  // spawn point on edge AB
		{
			aPosX = aMinX;
			aPosY = aMinY + aPathPos;
		}
		else if (aPathPos < aDistanceY + aDistanceX)  // spawn point on edge BC
		{
			aPosX = aMinX + (aPathPos - aDistanceY);
			aPosY = aMaxY;
		}
		else if (aPathPos < aDistanceY + aDistanceX + aDistanceY)  // spawn point on edge CD
		{
			aPosX = aMaxX;
			aPosY = aMaxY - (aPathPos - aDistanceY - aDistanceX);
		}
		else  // spawn point on edge AD
		{
			aPosX = aMaxX - (aPathPos - aDistanceY - aDistanceX - aDistanceY);
			aPosY = aMinY;
		}
		break;
	}
	default:
		PVZP_ASSERT(false);
		break;
	}
	float aEmitterSkewXInterp = Sexy::Rand(1.0f);
	float aEmitterSkewYInterp = Sexy::Rand(1.0f);
	float aSkewX = FloatTrackEvaluate(mEmitterDef->mEmitterSkewX, mSystemTimeValue, aEmitterSkewXInterp);
	float aSkewY = FloatTrackEvaluate(mEmitterDef->mEmitterSkewY, mSystemTimeValue, aEmitterSkewYInterp);
	aParticle->mPosition.x = mSystemCenter.x + aPosX + aPosY * aSkewX;  // X skew scales with the Y coordinate
	aParticle->mPosition.y = mSystemCenter.y + aPosY + aPosX * aSkewY;  // Y skew scales with the X coordinate
	aParticle->mVelocity.x = sin(aLaunchAngle) * aLaunchSpeed;
	aParticle->mVelocity.y = cos(aLaunchAngle) * aLaunchSpeed;
	aParticle->mPosition.x += FloatTrackEvaluate(mEmitterDef->mEmitterOffsetX, mSystemTimeValue, aEmitterOffsetXInterp);
	aParticle->mPosition.y += FloatTrackEvaluate(mEmitterDef->mEmitterOffsetY, mSystemTimeValue, aEmitterOffsetYInterp);

	aParticle->mAnimationTimeValue = 0.0f;
	if (mEmitterDef->mAnimated || FloatTrackIsSet(mEmitterDef->mAnimationRate))
		aParticle->mImageFrame = 0;  // frame computed later from the time value; init to 0 for now
	else
		aParticle->mImageFrame = Sexy::Rand(mEmitterDef->mImageFrames);  // fixed-frame particle: pick a random frame once

	if (TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_RANDOM_LAUNCH_SPIN)))
		aParticle->mSpinPosition = Sexy::Rand(static_cast<float>(2 * PI));  // random initial spin in [0, 2π]
	else if (TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_ALIGN_LAUNCH_SPIN)))
		aParticle->mSpinPosition = aLaunchAngle;  // spin aligns with the launch angle
	else
		aParticle->mSpinPosition = 0.0f;  // no initial spin
	aParticle->mSpinVelocity = 0.0f;
	aParticle->mCrossFadeDuration = 0;
	aParticle->mCrossFadeParticleID = ParticleID::PARTICLEID_NULL;

	ParticleID aParticleID = static_cast<ParticleID>(aDataArray.DataArrayGetID(aParticle));
	mParticleList.AddHead(aParticleID);
	mParticlesSpawned++;
	UpdateParticle(aParticle);
	return aParticle;
}

float PvzpParticleEmitter::ParticleTrackEvaluate(FloatParameterTrack& theTrack, PvzpParticle* theParticle, ParticleTracks theParticleTrack)
{
	return FloatTrackEvaluate(theTrack, theParticle->mParticleTimeValue, theParticle->mParticleInterp[theParticleTrack]);
}

void PvzpParticleEmitter::UpdateParticleField(PvzpParticle* theParticle, ParticleField* theParticleField, float theParticleTimeValue, int theFieldIndex)
{
	PVZP_ASSERT(theFieldIndex < MAX_PARTICLE_FIELDS);
	float aInterpX = theParticle->mParticleFieldInterp[theFieldIndex][0];
	float aInterpY = theParticle->mParticleFieldInterp[theFieldIndex][1];
	float x = FloatTrackEvaluate(theParticleField->mX, theParticleTimeValue, aInterpX);
	float y = FloatTrackEvaluate(theParticleField->mY, theParticleTimeValue, aInterpY);

	switch (theParticleField->mFieldType)
	{
	case ParticleFieldType::FIELD_INVALID:
		break;
	case ParticleFieldType::FIELD_FRICTION:
		theParticle->mVelocity.x *= 1 - x;
		theParticle->mVelocity.y *= 1 - y;
		break;
	case ParticleFieldType::FIELD_ACCELERATION:
		theParticle->mVelocity.x += 0.01f * x;
		theParticle->mVelocity.y += 0.01f * y;
		break;
	case ParticleFieldType::FIELD_ATTRACTOR:
	{
		float aDiffX = x - (theParticle->mPosition.x - mSystemCenter.x);
		float aDiffY = y - (theParticle->mPosition.y - mSystemCenter.y);
		// acceleration points from the particle toward the target position
		theParticle->mVelocity.x += 0.01f * aDiffX;
		theParticle->mVelocity.y += 0.01f * aDiffY;
		break;
	}
	case ParticleFieldType::FIELD_MAX_VELOCITY:
		theParticle->mVelocity.x = std::clamp(theParticle->mVelocity.x, -x, x);
		theParticle->mVelocity.y = std::clamp(theParticle->mVelocity.y, -y, y);
		break;
	case ParticleFieldType::FIELD_VELOCITY:
		theParticle->mPosition.x += 0.01 * x;
		theParticle->mPosition.y += 0.01 * y;
		break;
	case ParticleFieldType::FIELD_POSITION:
	{
		float aLastX = FloatTrackEvaluateFromLastTime(theParticleField->mX, theParticle->mParticleLastTimeValue, aInterpX);
		float aLastY = FloatTrackEvaluateFromLastTime(theParticleField->mY, theParticle->mParticleLastTimeValue, aInterpY);
		theParticle->mPosition.x += x - aLastX;
		theParticle->mPosition.y += y - aLastY;
		break;
	}
	case ParticleFieldType::FIELD_GROUND_CONSTRAINT:
		if (theParticle->mPosition.y > mSystemCenter.y + y)  // check for ground contact
		{
			theParticle->mPosition.y = mSystemCenter.y + y;  // reset the position to the ground
			float aCollisionReflect = FloatTrackEvaluate(
				mEmitterDef->mCollisionReflect, theParticleTimeValue, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_COLLISION_REFLECT]
			);
			float aCollisionSpin = FloatTrackEvaluate(
				mEmitterDef->mCollisionSpin, theParticleTimeValue, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_COLLISION_SPIN]
			) / 1000.0f;
			theParticle->mSpinVelocity = theParticle->mVelocity.y * aCollisionSpin;
			theParticle->mVelocity.x *= aCollisionReflect;
			theParticle->mVelocity.y *= -aCollisionReflect;
		}
		break;
	case ParticleFieldType::FIELD_SHAKE:
	{
		float aLastX = FloatTrackEvaluateFromLastTime(theParticleField->mX, theParticle->mParticleLastTimeValue, aInterpX);
		float aLastY = FloatTrackEvaluateFromLastTime(theParticleField->mY, theParticle->mParticleLastTimeValue, aInterpY);
		float aPreviousShakeScale = mParticleSystem->mParticleHolder->mPreviousScreenShakeScale;
		float aCurrentShakeScale = gLawnApp ? gLawnApp->GetScreenShakeScale() : 1.0f;
		if (theParticle->mParticleLastTimeValue < 0.0f)
		{
			aLastX = 0.0f;
			aLastY = 0.0f;
		}
		// undo the previous frame's shake offset
		int aLastRandSeed = theParticle->mParticleAge - 1;
		if (aLastRandSeed == -1)
			aLastRandSeed = theParticle->mParticleDuration - 1;
		srand(aLastRandSeed * reinterpret_cast<uintptr_t>(theParticle));
		theParticle->mPosition.x -= aLastX * aPreviousShakeScale * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
		theParticle->mPosition.y -= aLastY * aPreviousShakeScale * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
		// apply this frame's random shake offset
		srand(theParticle->mParticleAge * reinterpret_cast<uintptr_t>(theParticle));
		theParticle->mPosition.x += x * aCurrentShakeScale * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
		theParticle->mPosition.y += y * aCurrentShakeScale * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
		break;
	}
	case ParticleFieldType::FIELD_CIRCLE:
	{
		SexyVector2 aToCenter = theParticle->mPosition - mSystemCenter;
		SexyVector2 aMotion = aToCenter.Perp().Normalize();
		float aRadius = aToCenter.Magnitude();
		aMotion *= 0.01 * (x + aRadius * y);
		theParticle->mPosition += aMotion;
		break;
	}
	case ParticleFieldType::FIELD_AWAY:
	{
		SexyVector2 aToCenter = theParticle->mPosition - mSystemCenter;
		SexyVector2 aMotion = aToCenter.Normalize();
		float aRadius = aToCenter.Magnitude();
		aMotion *= 0.01 * (x + aRadius * y);
		theParticle->mPosition += aMotion;
		break;
	}
	default:
		PVZP_ASSERT(0);
		break;
	}
}

float PvzpParticleEmitter::SystemTrackEvaluate(FloatParameterTrack& theTrack, ParticleSystemTracks theSystemTrack)
{
	return FloatTrackEvaluate(theTrack, mSystemTimeValue, mTrackInterp[theSystemTrack]);
}

void PvzpParticleEmitter::UpdateSystemField(ParticleField* theParticleField, float theParticleTimeValue, int theFieldIndex)
{
	PVZP_ASSERT(theFieldIndex < MAX_PARTICLE_FIELDS);
	float aInterpX = mSystemFieldInterp[theFieldIndex][0];
	float aInterpY = mSystemFieldInterp[theFieldIndex][1];
	float x = FloatTrackEvaluate(theParticleField->mX, theParticleTimeValue, aInterpX);
	float y = FloatTrackEvaluate(theParticleField->mY, theParticleTimeValue, aInterpY);

	switch (theParticleField->mFieldType)
	{
	case ParticleFieldType::FIELD_SYSTEM_POSITION:
	{
		float aLastX = FloatTrackEvaluateFromLastTime(theParticleField->mX, mSystemLastTimeValue, aInterpX);
		float aLastY = FloatTrackEvaluateFromLastTime(theParticleField->mY, mSystemLastTimeValue, aInterpY);
		mSystemCenter.x += x - aLastX;
		mSystemCenter.y += y - aLastY;
		break;
	}
	default:
		PVZP_ASSERT(0);
		break;
	}
}

bool PvzpParticleEmitter::CrossFadeParticleToName(PvzpParticle* theParticle, const char* theEmitterName)
{
	PvzpEmitterDefinition* aDef = mParticleSystem->FindEmitterDefByName(theEmitterName);
	if (aDef == nullptr)
	{
		PvzpLogLn("Can't find emitter to cross fade: {}", theEmitterName);
		return false;
	}
	if (mParticleSystem->mParticleHolder->mEmitters.mSize == mParticleSystem->mParticleHolder->mEmitters.mMaxSize)
	{
		PvzpLogLn("Too many emitters to cross fade");
		return false;
	}

	PvzpParticleEmitter* aEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayAlloc();
	aEmitter->PvzpEmitterInitialize(mSystemCenter.x, mSystemCenter.y, mParticleSystem, aDef);
	ParticleEmitterID aEmitterID = static_cast<ParticleEmitterID>(mParticleSystem->mParticleHolder->mEmitters.DataArrayGetID(aEmitter));
	mParticleSystem->mEmitterList.AddTail(aEmitterID);
	return CrossFadeParticle(theParticle, aEmitter);
}

bool PvzpParticleEmitter::UpdateParticle(PvzpParticle* theParticle, bool theDeferCalculation)
{
	if (theParticle->mParticleAge >= theParticle->mParticleDuration)  // particle reached the end of its lifetime
	{
		const bool aGloomCloudLifetimeReduced = mParticleSystem->mEffectType == ParticleEffect::PARTICLE_GLOOMCLOUD && mCurrentQualityTier >= 2;
		if (TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_PARTICLE_LOOPS)) &&
			!((IsLowPriorityParticleEffect(mParticleSystem->mEffectType) || aGloomCloudLifetimeReduced) && mCurrentQualityTier >= 2))
			theParticle->mParticleAge = 0;
		else if (theParticle->mCrossFadeDuration > 0)
			theParticle->mParticleAge = theParticle->mParticleDuration - 1;  // hold the particle on its last frame
		else if (*mEmitterDef->mOnDuration == '\0' || !CrossFadeParticleToName(theParticle, mEmitterDef->mOnDuration))
			return false;
	}
	if (theParticle->mCrossFadeParticleID != ParticleID::PARTICLEID_NULL &&
		mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(theParticle->mCrossFadeParticleID) == nullptr)
		return false;  // the cross-fade source is gone; the particle can be deleted

	if (theDeferCalculation && theParticle->mCrossFadeDuration <= 0 &&
		theParticle->mCrossFadeParticleID == ParticleID::PARTICLEID_NULL)
	{
		PvzpParticleHolder* aHolder = mParticleSystem->mParticleHolder;
		const auto anEmitterId = static_cast<ParticleEmitterID>(aHolder->mEmitters.DataArrayGetID(this));
		const auto aParticleId = static_cast<ParticleID>(aHolder->mParticles.DataArrayGetID(theParticle));
		aHolder->mParticleUpdateScratch.push_back({ anEmitterId, aParticleId, this, theParticle, mParticleSystem->mEffectType });
	}
	else
		CalculateParticleState(theParticle);
	return true;
}

void PvzpParticleEmitter::CalculateParticleState(PvzpParticle* theParticle)
{
	// This touches one particle and immutable emitter/system data; workers skip emitters with FIELD_SHAKE because it uses global rand().
	theParticle->mParticleTimeValue = theParticle->mParticleAge / (static_cast<float>(theParticle->mParticleDuration) - 1);
	for (int i = 0; i < mEmitterDef->mParticleFields.count; i++)
		UpdateParticleField(theParticle, &mEmitterDef->mParticleFields.Fields[i], theParticle->mParticleTimeValue, i);
	theParticle->mPosition += theParticle->mVelocity;
	float aSpinSpeed = ParticleTrackEvaluate(mEmitterDef->mParticleSpinSpeed, theParticle, ParticleTracks::TRACK_PARTICLE_SPIN_SPEED) * 0.01;
	float aSpinAngle = ParticleTrackEvaluate(mEmitterDef->mParticleSpinAngle, theParticle, ParticleTracks::TRACK_PARTICLE_SPIN_ANGLE);
	float aLastSpinAngle = FloatTrackEvaluateFromLastTime(
		mEmitterDef->mParticleSpinAngle, theParticle->mParticleLastTimeValue, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_SPIN_ANGLE]);
	theParticle->mSpinPosition += DEG_TO_RAD(aSpinSpeed + aSpinAngle - aLastSpinAngle) + theParticle->mSpinVelocity;

	if (FloatTrackIsSet(mEmitterDef->mAnimationRate))
	{
		float aAnimTime = ParticleTrackEvaluate(mEmitterDef->mAnimationRate, theParticle, ParticleTracks::TRACK_PARTICLE_ANIMATION_RATE) * 0.01;
		theParticle->mAnimationTimeValue += aAnimTime;
		while (theParticle->mAnimationTimeValue >= 1.0f)
			theParticle->mAnimationTimeValue -= 1.0f;
		while (theParticle->mAnimationTimeValue < 0.0f)
			theParticle->mAnimationTimeValue += 1.0f;
	}

	if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_GLOOMCLOUD)
		theParticle->mParticleAge += mCurrentQualityTier >= 3 ? 4 : mCurrentQualityTier >= 2 ? 2 : 1;
	else if (IsLowPriorityParticleEffect(mParticleSystem->mEffectType))
		theParticle->mParticleAge += mCurrentQualityTier >= 3 ? 4 : mCurrentQualityTier == 2 ? 2 : 1;
	else
		++theParticle->mParticleAge;
	theParticle->mParticleLastTimeValue = theParticle->mParticleTimeValue;
}

void PvzpParticleEmitter::UpdateSpawning()
{
	PvzpParticleEmitter* aCrossFadeEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayTryToGet(static_cast<unsigned int>(mCrossFadeEmitterID));
	PvzpParticleEmitter* aSpawningEmitter = !aCrossFadeEmitter ? this : aCrossFadeEmitter;  // all spawn data is taken from this "primary" emitter
	const bool aIsGloomCloud = mParticleSystem->mEffectType == ParticleEffect::PARTICLE_GLOOMCLOUD;
	const float aGloomCloudQualityScale = aIsGloomCloud ? GetGloomCloudQualityScale(mCurrentQualityTier) : 1.0f;
	const float aSpawnRate = aSpawningEmitter->SystemTrackEvaluate(aSpawningEmitter->mEmitterDef->mSpawnRate, ParticleSystemTracks::TRACK_SPAWN_RATE);
	mSpawnAccum += aSpawnRate * 0.01 * aGloomCloudQualityScale;
	int aSpawnCount = static_cast<int>(mSpawnAccum);
	mSpawnAccum -= aSpawnCount;

	int aSpawnMinActive = static_cast<int>(aSpawningEmitter->SystemTrackEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMinActive, ParticleSystemTracks::TRACK_SPAWN_MIN_ACTIVE));
	if (aIsGloomCloud && aSpawnMinActive > 0 && aGloomCloudQualityScale < 1.0f)
		aSpawnMinActive = std::max(1, static_cast<int>(std::ceil(aSpawnMinActive * aGloomCloudQualityScale)));
	if (aSpawnMinActive >= 0 && aSpawnCount < aSpawnMinActive - mParticleList.mSize)
		aSpawnCount = aSpawnMinActive - mParticleList.mSize;  // spawn at least enough to reach aSpawnMinActive
	int aSpawnMaxActive = static_cast<int>(aSpawningEmitter->SystemTrackEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMaxActive, ParticleSystemTracks::TRACK_SPAWN_MAX_ACTIVE));
	if (aIsGloomCloud && aSpawnMaxActive > 0 && aGloomCloudQualityScale < 1.0f)
		aSpawnMaxActive = std::max(1, static_cast<int>(std::floor(aSpawnMaxActive * aGloomCloudQualityScale)));
	if (aSpawnMaxActive >= 0 && aSpawnCount > aSpawnMaxActive - mParticleList.mSize)
		aSpawnCount = aSpawnMaxActive - mParticleList.mSize;  // cap the active count at aSpawnMaxActive
	bool aSpawnMaxLaunchedAllowsParticle = true;
	if (FloatTrackIsSet(aSpawningEmitter->mEmitterDef->mSpawnMaxLaunched))
	{
		int aSpawnMaxLaunched = aSpawningEmitter->SystemTrackEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMaxLaunched, ParticleSystemTracks::TRACK_SPAWN_MAX_LAUNCHED);
		if (aIsGloomCloud && aSpawnMaxLaunched > 0 && aGloomCloudQualityScale < 1.0f)
			aSpawnMaxLaunched = std::max(1, static_cast<int>(std::floor(aSpawnMaxLaunched * aGloomCloudQualityScale)));
		aSpawnMaxLaunchedAllowsParticle = mParticlesSpawned < aSpawnMaxLaunched;
		if (aSpawnCount > aSpawnMaxLaunched - mParticlesSpawned)
			aSpawnCount = aSpawnMaxLaunched - mParticlesSpawned;  // cap at the emitter's total launch limit
	}
	if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_ZAMBONI_SMOKE && aSpawnCount > 0)
	{
		mSpawnScaleAccum += aSpawnCount * ZAMBONI_SMOKE_SPAWN_RATE_SCALE;
		aSpawnCount = static_cast<int>(mSpawnScaleAccum);
		mSpawnScaleAccum -= aSpawnCount;
	}
	if (IsLowPriorityParticleEffect(mParticleSystem->mEffectType) && mCurrentQualityTier > 0)
	{
		const uint32_t anEmitterId = mParticleSystem->mParticleHolder->mEmitters.DataArrayGetID(this);
		const uint32_t aPhase = static_cast<uint32_t>(mSystemAge) + anEmitterId;
		if (mCurrentQualityTier >= 3 || (mCurrentQualityTier == 2 && (aPhase & 1U) == 0) ||
			(mCurrentQualityTier == 1 && (aPhase & 3U) == 0))
			aSpawnCount = 0;
	}
	if (aIsGloomCloud && mCurrentQualityTier > 0 && mSystemAge == 0 && mParticleList.mSize == 0 &&
		aSpawnCount <= 0 && aSpawnRate > 0.0f && aSpawnMaxActive != 0 && aSpawnMaxLaunchedAllowsParticle)
		aSpawnCount = 1;

	for (int i = 0; i < aSpawnCount; i++)
	{
		PvzpParticle* aParticle = SpawnParticle(i, aSpawnCount);
		if (aParticle == nullptr)
			break;
		if (aCrossFadeEmitter != nullptr)
			CrossFadeParticle(aParticle, aCrossFadeEmitter);
	}
}

void PvzpParticleEmitter::DeleteNonCrossFading()
{
	for (PvzpListNode<ParticleID>* aNode = mParticleList.mHead; aNode != nullptr; )
	{
		PvzpListNode<ParticleID>* aNext = aNode->mNext;
		PvzpParticle* aParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (aParticle->mCrossFadeDuration <= 0)
			DeleteParticle(aParticle);
		aNode = aNext;
	}
}

void PvzpParticleEmitter::DeleteAll()
{
	while (mParticleList.mSize != 0)
	{
		ParticleID anId = mParticleList.RemoveHead();
		DataArray<PvzpParticle>& aDataArray = mParticleSystem->mParticleHolder->mParticles;
		aDataArray.DataArrayFree(aDataArray.DataArrayGet(anId));
		if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_ZAMBONI_SMOKE &&
			mParticleSystem->mParticleHolder->mZamboniSmokeLiveParticles > 0)
			--mParticleSystem->mParticleHolder->mZamboniSmokeLiveParticles;
	}
}

void PvzpParticleSystem::Update(bool theCollectParallelTasks)
{
	Sexy::FrameProfiler& aProfiler = Sexy::FrameProfiler::Get();
	const uint64_t aProfileStart = aProfiler.BeginParticleEffect();
	uint32_t aParticlesVisited = 0;
	uint32_t aParticlesSpawned = 0;
	uint32_t aLiveParticles = 0;
	if (!mDontUpdate)
	{
		bool aEmitterAlive = false;
		for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
		{
			PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
			const uint32_t aParticlesBefore = aProfileStart == 0 ? 0 : aEmitter->mParticleList.mSize;
			const int32_t aSpawnedBefore = aProfileStart == 0 ? 0 : aEmitter->mParticlesSpawned;
			aEmitter->Update(theCollectParallelTasks);
			if (aProfileStart != 0)
			{
				const uint32_t aSpawnedThisUpdate = static_cast<uint32_t>(std::max(0, aEmitter->mParticlesSpawned - aSpawnedBefore));
				aParticlesVisited += aParticlesBefore + aSpawnedThisUpdate;
				aParticlesSpawned += aSpawnedThisUpdate;
				aLiveParticles += aEmitter->mParticleList.mSize;
			}
			if ((FloatTrackIsSet(aEmitter->mEmitterDef->mCrossFadeDuration) && aEmitter->mParticleList.mSize > 0) || !aEmitter->mDead)
				aEmitterAlive = true;
		}
		if (!aEmitterAlive)
			mDead = true;
	}
	if (aProfileStart != 0)
		aProfiler.RecordParticleEffectUpdate(static_cast<int>(mEffectType), aProfileStart, aParticlesVisited,
			aParticlesSpawned, aLiveParticles, mEmitterList.mSize);
}

bool PvzpParticleEmitter::CrossFadeParticle(PvzpParticle* theParticle, PvzpParticleEmitter* theToEmitter)
{
	if (theParticle == nullptr || theToEmitter == nullptr)
		return false;
	if (theParticle->mCrossFadeDuration > 0)
	{
		PvzpLogLn("We don't support cross fading more than one at a time");
		return false;
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mCrossFadeDuration))
	{
		PvzpLogLn("Can't cross fade to emitter that doesn't have CrossFadeDuration");
		return false;
	}
	PVZP_ASSERT(theToEmitter != this);

	PvzpParticle* aToParticle = theToEmitter->SpawnParticle(0, 1);
	if (aToParticle == nullptr)
		return false;
	if (mEmitterCrossFadeCountDown > 0)
		theParticle->mCrossFadeDuration = mEmitterCrossFadeCountDown;  // inherit the source emitter's remaining cross-fade time
	else
	{
		float aCrossFadeDurationInterp = Sexy::Rand(1);
		int aCrossFadeDuration = FloatTrackEvaluate(theToEmitter->mEmitterDef->mCrossFadeDuration, mSystemTimeValue, aCrossFadeDurationInterp);
		theParticle->mCrossFadeDuration = std::max(1, aCrossFadeDuration);  // random cross-fade duration, at least 1 frame
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mParticleDuration))
		aToParticle->mParticleDuration = theParticle->mCrossFadeDuration;
	aToParticle->mCrossFadeParticleID = static_cast<ParticleID>(mParticleSystem->mParticleHolder->mParticles.DataArrayGetID(theParticle));
	return true;
}

void PvzpParticleEmitter::DeleteParticle(PvzpParticle* theParticle)
{
	PvzpParticle* aCrossFadeParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(static_cast<unsigned int>(theParticle->mCrossFadeParticleID));
	if (aCrossFadeParticle != nullptr)
	{
		aCrossFadeParticle->mParticleEmitter->DeleteParticle(aCrossFadeParticle);  // also delete the cross-fade source particle
		theParticle->mCrossFadeParticleID = ParticleID::PARTICLEID_NULL;
	}

	ParticleID aParticleID = static_cast<ParticleID>(mParticleSystem->mParticleHolder->mParticles.DataArrayGetID(theParticle));
	mParticleList.RemoveAt(mParticleList.Find(aParticleID));
	if (mParticleSystem->mEffectType == ParticleEffect::PARTICLE_ZAMBONI_SMOKE &&
		mParticleSystem->mParticleHolder->mZamboniSmokeLiveParticles > 0)
		--mParticleSystem->mParticleHolder->mZamboniSmokeLiveParticles;
	mParticleSystem->mParticleHolder->mParticles.DataArrayFree(theParticle);
}

static bool EmitterUsesSharedRandomShake(const PvzpEmitterDefinition& theDefinition)
{
	for (int i = 0; i < theDefinition.mParticleFields.count; ++i)
		if (theDefinition.mParticleFields.Fields[i].mFieldType == ParticleFieldType::FIELD_SHAKE)
			return true;
	return false;
}

void PvzpParticleEmitter::Update(bool theCollectParallelTasks)
{
	if (mDead)
		return;
	mCurrentQualityTier = mParticleSystem->mParticleHolder->mVisualQualityTier;

	mSystemAge++;
	bool aDie = false;
	if (mSystemAge >= mSystemDuration)  // emitter reached the end of its lifetime
	{
		if (TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_SYSTEM_LOOPS)))
			mSystemAge = 0;
		else
		{
			mSystemAge = mSystemDuration - 1;  // hold the emitter on its last frame
			aDie = true;
		}
	}

	if (mEmitterCrossFadeCountDown > 0)
	{
		mEmitterCrossFadeCountDown--;
		if (mEmitterCrossFadeCountDown == 0)
			aDie = true;
	}
	if (mCrossFadeEmitterID != ParticleEmitterID::PARTICLEEMITTERID_NULL)
	{
		PvzpParticleEmitter* aCrossFadeEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayTryToGet(mCrossFadeEmitterID);
		if (aCrossFadeEmitter == nullptr || aCrossFadeEmitter->mDead)
			aDie = true;
	}

	mSystemTimeValue = mSystemAge / static_cast<float>(mSystemDuration - 1);
	for (int i = 0; i < mEmitterDef->mSystemFields.count; i++)
		UpdateSystemField(&mEmitterDef->mSystemFields.Fields[i], mSystemTimeValue, i);
	PvzpParticleHolder* aHolder = mParticleSystem->mParticleHolder;
	const bool aCanDeferCalculations = theCollectParallelTasks && IsParticleParallelEnabled() && !aDie &&
		mCrossFadeEmitterID == ParticleEmitterID::PARTICLEEMITTERID_NULL && !EmitterUsesSharedRandomShake(*mEmitterDef);
	for (PvzpListNode<ParticleID>* aNode = mParticleList.mHead; aNode != nullptr; )
	{
		PvzpListNode<ParticleID>* aNext = aNode->mNext;
		PvzpParticle* aParticle = aHolder->mParticles.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		const bool aDeferCalculation = aCanDeferCalculations && aParticle->mCrossFadeDuration <= 0 &&
			aParticle->mCrossFadeParticleID == ParticleID::PARTICLEID_NULL;
		if (!UpdateParticle(aParticle, aDeferCalculation))
			DeleteParticle(aParticle);
		aNode = aNext;
	}
	UpdateSpawning();

	if (aDie)
	{
		DeleteNonCrossFading();
		if (mParticleList.mSize == 0)
		{
			mDead = true;
			return;
		}
	}
	mSystemLastTimeValue = mSystemTimeValue;
}

void PvzpParticleSystem::SystemMove(float theX, float theY)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
		mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue))->SystemMove(theX, theY);
}

void PvzpParticleEmitter::SystemMove(float theX, float theY)
{
	float aDeltaX = theX - mSystemCenter.x;
	float aDeltaY = theY - mSystemCenter.y;
	if (FloatApproxEqual(aDeltaX, 0.0f) && FloatApproxEqual(aDeltaY, 0.0f))
		return;

	mSystemCenter.x = theX;
	mSystemCenter.y = theY;
	if (!TestBit(mEmitterDef->mParticleFlags, static_cast<int>(ParticleFlags::PARTICLE_PARTICLES_DONT_FOLLOW)))
	{
		for (PvzpListNode<ParticleID>* aNode = mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
		{
			PvzpParticle* aParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
			aParticle->mPosition.x += aDeltaX;
			aParticle->mPosition.y += aDeltaY;
		}
	}
}

void PvzpParticleSystem::OverrideColor(const char* theEmitterName, const Color& theColor)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (theEmitterName == nullptr || strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			aEmitter->mColorOverride = theColor;
	}
}

void PvzpParticleSystem::OverrideExtraAdditiveDraw(const char* theEmitterName, bool theEnableExtraAdditiveDraw)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (theEmitterName == nullptr || strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			aEmitter->mExtraAdditiveDrawOverride = theEnableExtraAdditiveDraw;
	}
}

void PvzpParticleSystem::OverrideImage(const char* theEmitterName, Image* theImage)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (theEmitterName == nullptr || strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			aEmitter->mImageOverride = theImage;
	}
}

void PvzpParticleSystem::OverrideFrame(const char* theEmitterName, int theFrame)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (theEmitterName == nullptr || strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			aEmitter->mFrameOverride = theFrame;
	}
}

void PvzpParticleSystem::OverrideScale(const char* theEmitterName, float theScale)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (theEmitterName == nullptr || strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			aEmitter->mScaleOverride = theScale;
	}
}

PvzpParticleEmitter* PvzpParticleSystem::FindEmitterByName(const char* theEmitterName)
{
	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (strcasecmp(theEmitterName, aEmitter->mEmitterDef->mName) == 0)
			return aEmitter;
	}
	return nullptr;
}

PvzpEmitterDefinition* PvzpParticleSystem::FindEmitterDefByName(const char* theEmitterName)
{
	for (int i = 0; i < mParticleDef->mEmitterDefCount; i++)
	{
		PvzpEmitterDefinition* aEmitterDef = &mParticleDef->mEmitterDefs[i];
		if (strcasecmp(theEmitterName, aEmitterDef->mName) == 0)
			return aEmitterDef;
	}
	return nullptr;
}

void PvzpParticleEmitter::CrossFadeEmitter(PvzpParticleEmitter* theToEmitter)
{
	if (mEmitterCrossFadeCountDown > 0)
	{
		PvzpLogLn("We don't support cross fading emitters more than one at a time");
		return;
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mCrossFadeDuration))
	{
		PvzpLogLn("Can't cross fade to emitter that doesn't have CrossFadeDuration");
		return;
	}
	PVZP_ASSERT(theToEmitter != this);

	float aCrossFadeDurationInterp = Sexy::Rand(1.0f);
	mEmitterCrossFadeCountDown = FloatTrackEvaluate(theToEmitter->mEmitterDef->mCrossFadeDuration, mSystemTimeValue, aCrossFadeDurationInterp);
	mEmitterCrossFadeCountDown = std::max(1, mEmitterCrossFadeCountDown);
	mCrossFadeEmitterID = static_cast<ParticleEmitterID>(mParticleSystem->mParticleHolder->mEmitters.DataArrayGetID(theToEmitter));
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mSystemDuration))
		theToEmitter->mSystemDuration = mEmitterCrossFadeCountDown;

	for (PvzpListNode<ParticleID>* aNode = mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
		CrossFadeParticle(mParticleSystem->mParticleHolder->mParticles.DataArrayGet(static_cast<unsigned int>(aNode->mValue)), theToEmitter);
}

void PvzpParticleSystem::CrossFade(const char* theEmitterName)
{
	PvzpEmitterDefinition* aEmitterDef = FindEmitterDefByName(theEmitterName);
	if (aEmitterDef == nullptr)
	{
		PvzpLogLn("Can't find cross fade emitter: {}", theEmitterName);
		return;
	}
	if (!FloatTrackIsSet(aEmitterDef->mCrossFadeDuration))
	{
		PvzpLogLn("Can't cross fade without duration set: {}", theEmitterName);
		return;
	}
	if (mParticleHolder->mEmitters.mSize + mEmitterList.mSize > mParticleHolder->mEmitters.mMaxSize)
	{
		PvzpLogLn("Too many emitters to cross fade");
		ParticleSystemDie();
		return;
	}

	for (PvzpListNode<ParticleEmitterID>* aNode = mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		PvzpParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayGet(static_cast<unsigned int>(aNode->mValue));
		if (aEmitter->mEmitterDef != aEmitterDef)  // don't cross fade between emitters of the same kind
		{
			PvzpParticleEmitter* aCrossFadeEmitter = mParticleHolder->mEmitters.DataArrayAlloc();
			aCrossFadeEmitter->PvzpEmitterInitialize(aEmitter->mSystemCenter.x, aEmitter->mSystemCenter.y, this, aEmitterDef);
			ParticleEmitterID aCrossFadeEmitterID = static_cast<ParticleEmitterID>(mParticleHolder->mEmitters.DataArrayGetID(aCrossFadeEmitter));
			mEmitterList.AddTail(aCrossFadeEmitterID);
			aEmitter->CrossFadeEmitter(aCrossFadeEmitter);
		}
	}
}

PvzpParticleHolder::~PvzpParticleHolder()
{
	DisposeHolder();
}

void PvzpParticleHolder::InitializeHolder()
{
	mParticleUpdateScratch.clear();
	mZamboniSmokeLiveParticles = 0;
	mVisualQualityTier = 0;
	mParticleCapacityWarningLogged = false;
	mParticleSystemCapacityWarningLogged = false;
	mEmitterCapacityWarningLogged = false;
	mSuppressedParticleSystemFailures = 0;
	mSuppressedEmitterFailures = 0;
#ifdef LOW_MEMORY
	mParticleSystems.DataArrayInitialize(1024U, "particle systems");
	mEmitters.DataArrayInitialize(1024U, "emitters");
	mParticles.DataArrayInitialize(8192U, "particles");
#else
	mParticleSystems.DataArrayInitialize(4096U, "particle systems");
	mEmitters.DataArrayInitialize(8192U, "emitters");
	mParticles.DataArrayInitialize(32768U, "particles");
#endif
	mParticleUpdateScratch.reserve(mParticles.mMaxSize);
	mParticleIdScratch.reserve(mParticles.mMaxSize);
	mParticleListNodeAllocator.Initialize(1024, sizeof(PvzpListNode<ParticleID>));
	mEmitterListNodeAllocator.Initialize(1024, sizeof(PvzpListNode<ParticleEmitterID>));
}

void PvzpParticleHolder::DisposeHolder()
{
	mZamboniSmokeLiveParticles = 0;
	mVisualQualityTier = 0;
	mParticleSystems.DataArrayDispose();
	mEmitters.DataArrayDispose();
	mParticles.DataArrayDispose();
	mParticleUpdateScratch.clear();
	mParticleIdScratch.clear();
	mParticleListNodeAllocator.FreeAll();
	mEmitterListNodeAllocator.FreeAll();
	mParticleCapacityWarningLogged = false;
	mParticleSystemCapacityWarningLogged = false;
	mEmitterCapacityWarningLogged = false;
	mSuppressedParticleSystemFailures = 0;
	mSuppressedEmitterFailures = 0;
}

bool PvzpParticleHolder::IsAtUsageThreshold(uint32_t thePercent) const
{
	return (mParticleSystems.mMaxSize > 0 && static_cast<uint64_t>(mParticleSystems.mSize) * 100U >= static_cast<uint64_t>(mParticleSystems.mMaxSize) * thePercent) ||
		(mEmitters.mMaxSize > 0 && static_cast<uint64_t>(mEmitters.mSize) * 100U >= static_cast<uint64_t>(mEmitters.mMaxSize) * thePercent) ||
		(mParticles.mMaxSize > 0 && static_cast<uint64_t>(mParticles.mSize) * 100U >= static_cast<uint64_t>(mParticles.mMaxSize) * thePercent);
}

bool PvzpParticleHolder::IsOverLoaded()
{
	return IsAtUsageThreshold(90U);
}

PvzpParticleSystem* PvzpParticleHolder::AllocParticleSystemFromDef(float theX, float theY, int theRenderOrder, PvzpParticleDefinition* theDefinition, ParticleEffect theParticleEffect)
{
	int anEffectIndex = static_cast<int>(theParticleEffect);
	const char* anEffectName = anEffectIndex >= 0 && anEffectIndex < gParticleDefCount
		? gLawnParticleArray[anEffectIndex].mParticleFileName : "unknown";
	if (mParticleSystems.mSize < mParticleSystems.mMaxSize * 3U / 4U)
		mParticleSystemCapacityWarningLogged = false;
	if (mEmitters.mSize < mEmitters.mMaxSize * 3U / 4U)
		mEmitterCapacityWarningLogged = false;
	if (mParticleSystems.mSize == mParticleSystems.mMaxSize)
	{
		if (!mParticleSystemCapacityWarningLogged)
		{
			PvzpLogLn("Particle system allocation refused: effect '{}', live {}/{}, high-water {}, suppressed failures {}",
				anEffectName, mParticleSystems.mSize, mParticleSystems.mMaxSize, mParticleSystems.mMaxUsedCount,
				mSuppressedParticleSystemFailures);
			mParticleSystemCapacityWarningLogged = true;
			mSuppressedParticleSystemFailures = 0;
		}
		else
		{
			++mSuppressedParticleSystemFailures;
		}
		return nullptr;
	}
	if (theDefinition->mEmitterDefCount + mEmitters.mSize > mEmitters.mMaxSize)
	{
		if (!mEmitterCapacityWarningLogged)
		{
			PvzpLogLn("Particle emitter allocation refused: effect '{}', needs {}, live {}/{}, high-water {}, suppressed failures {}",
				anEffectName, theDefinition->mEmitterDefCount, mEmitters.mSize, mEmitters.mMaxSize,
				mEmitters.mMaxUsedCount, mSuppressedEmitterFailures);
			mEmitterCapacityWarningLogged = true;
			mSuppressedEmitterFailures = 0;
		}
		else
		{
			++mSuppressedEmitterFailures;
		}
		return nullptr;
	}
	PvzpParticleSystem* aPvzpParticle = mParticleSystems.DataArrayAlloc();
	aPvzpParticle->mParticleHolder = this;
	aPvzpParticle->PvzpParticleInitializeFromDef(theX, theY, theRenderOrder, theDefinition, theParticleEffect);
	return aPvzpParticle;
}

PvzpParticleSystem* PvzpParticleHolder::AllocParticleSystem(float theX, float theY, int theRenderOrder, ParticleEffect theParticleEffect)
{
	PVZP_ASSERT(static_cast<int>(theParticleEffect) >= 0 && static_cast<int>(theParticleEffect) < gParticleDefCount);
	return AllocParticleSystemFromDef(theX, theY, theRenderOrder, &gParticleDefArray[theParticleEffect], theParticleEffect);
}
