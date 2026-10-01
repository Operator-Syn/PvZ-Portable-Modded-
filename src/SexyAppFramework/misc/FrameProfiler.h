/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#ifndef __SEXY_FRAMEPROFILER_H__
#define __SEXY_FRAMEPROFILER_H__

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace Sexy
{

enum class FrameProfileMetric : uint8_t
{
	UPDATE,
	UPDATE_INTERPOLATION,
	BOARD_UPDATE,
	EFFECTS_UPDATE,
	PARTICLE_SYSTEMS_UPDATE,
	TRAILS_UPDATE,
	REANIMATIONS_UPDATE,
	BOARD_DRAW,
	SCREEN_DRAW,
	RENDER_GATHER,
	RENDER_SORT,
	RENDER_ITEMS,
	PRESENT,
	SWAP_WAIT,
	PLANT_TARGETING,
	PROJECTILE_IMPACT,
	PROJECTILE_SPLASH,
	SUN_MAGNET_ASSIGNMENT,
	COUNT
};

struct FrameProfilePool
{
	uint32_t mLive = 0;
	uint32_t mHighWater = 0;
	uint32_t mCapacity = 0;
};

struct FrameProfileBoardCounts
{
	FrameProfilePool mPlants;
	FrameProfilePool mZombies;
	FrameProfilePool mProjectiles;
	FrameProfilePool mCoins;
	FrameProfilePool mParticles;
	FrameProfilePool mEmitters;
	FrameProfilePool mReanimations;
	FrameProfilePool mAttachments;
	FrameProfilePool mGridItems;
	FrameProfilePool mMowers;
	uint32_t mRenderItems = 0;
	uint32_t mRenderCapacity = 0;
};

struct FrameProfileParticleEffect
{
	uint64_t mUpdateTicks = 0;
	uint64_t mDrawTicks = 0;
	uint32_t mParticlesVisited = 0;
	uint32_t mParticlesSpawned = 0;
	uint32_t mLiveParticles = 0;
	uint32_t mEmitters = 0;
	uint32_t mDrawParticlesVisited = 0;
	uint32_t mParticlesCulled = 0;
	uint32_t mTrianglesSubmitted = 0;
	uint32_t mBatchFlushes = 0;
	uint32_t mParallelBatches = 0;
	uint32_t mParallelParticles = 0;
};

struct FrameProfileParticleEffectSample
{
	uint16_t mEffectId = 0;
	FrameProfileParticleEffect mMetrics;
};

class FrameProfiler
{
public:
	static FrameProfiler& Get();

	void Initialize(const std::filesystem::path& thePath, uint64_t theSessionID, int theBuildNumber,
		std::string_view theCommitDate, bool theDetailed);
	void SetFrameBudget(double theMilliseconds);
	void SetSchedulerState(double theUpdateBacklogMs, double thePendingUpdates, double theUpdateIntervalMs);
	void SetBoardCounts(const FrameProfileBoardCounts& theCounts);
	bool IsDetailed() const { return mDetailed; }
	int GetVisualQualityTier() const { return mVisualQualityTier; }
	uint64_t BeginParticleEffect();
	void RecordParticleEffectUpdate(int theEffect, uint64_t theStart, uint32_t theParticlesVisited,
		uint32_t theParticlesSpawned, uint32_t theLiveParticles, uint32_t theEmitters);
	void RecordParticleEffectDraw(int theEffect, uint64_t theStart, uint32_t theParticlesVisited,
		uint32_t theParticlesCulled, uint32_t theTrianglesSubmitted, uint32_t theBatchFlushes);
	void RecordParticleCalculation(int theEffect, uint32_t theParticles, uint64_t theStartCounter, bool theParallel);
	void RecordGameplayEvent(std::string_view theEvent, std::string_view theDataJson);

	uint64_t Begin(FrameProfileMetric theMetric);
	void End(FrameProfileMetric theMetric, uint64_t theStart);
	void RecordFrame();

private:
	static constexpr size_t METRIC_COUNT = static_cast<size_t>(FrameProfileMetric::COUNT);
	static constexpr size_t PARTICLE_EFFECT_COUNT = 128;
	static constexpr size_t SAMPLE_RING_SIZE = 256;
	static constexpr size_t SLOW_CAPTURE_PRE_SAMPLES = 120;
	static constexpr size_t SLOW_CAPTURE_POST_SAMPLES = 60;
	static constexpr uint64_t MAX_LOG_BYTES = 8 * 1024 * 1024;

	struct FrameSample
	{
		uint64_t mSequence = 0;
		double mFrameMs = 0.0;
		double mUpdateMs = 0.0;
		double mInterpolationUpdateMs = 0.0;
		double mBoardUpdateMs = 0.0;
		double mEffectsUpdateMs = 0.0;
		double mParticleSystemsUpdateMs = 0.0;
		double mTrailsUpdateMs = 0.0;
		double mReanimationsUpdateMs = 0.0;
		double mScreenDrawMs = 0.0;
		double mBoardDrawMs = 0.0;
		double mRenderGatherMs = 0.0;
		double mRenderSortMs = 0.0;
		double mRenderItemsMs = 0.0;
		double mPresentMs = 0.0;
		double mSwapWaitMs = 0.0;
		double mPlantTargetingMs = 0.0;
		double mProjectileImpactMs = 0.0;
		double mProjectileSplashMs = 0.0;
		double mSunMagnetMs = 0.0;
		double mUpdateBacklogMs = 0.0;
		double mPendingUpdates = 0.0;
		uint32_t mUpdates = 0;
		uint32_t mInterpolationUpdates = 0;
		FrameProfileBoardCounts mCounts;
		std::array<FrameProfileParticleEffectSample, 8> mParticleEffects{};
		int mVisualQualityTier = 0;
	};

	struct TimingSummary
	{
		double mTotalMs = 0.0;
		double mMaxMs = 0.0;
		std::array<uint32_t, 16> mHistogram{};
		uint32_t mSamples = 0;
		void Add(double theMilliseconds);
		double Percentile(double theFraction) const;
		void Reset();
	};

	FrameProfiler() = default;
	static uint64_t Counter();
	static double ToMilliseconds(uint64_t theTicks);
	static const char* MetricName(FrameProfileMetric theMetric);
	static void AppendSampleJson(std::string& theOutput, const FrameSample& theSample);
	static void AppendParticleEffectsJson(std::string& theOutput,
		const std::array<FrameProfileParticleEffectSample, 8>& theEffects);
	void AddSample(const FrameSample& theSample);
	void WriteSummary(uint64_t theNow);
	void StartSlowCapture(const FrameSample& theSample);
	void CompleteSlowCapture();
	void WriteLine(const std::string& theLine);
	void WriteSessionMetadata();
	void RotateLogIfNeeded(size_t theIncomingBytes);

	std::filesystem::path mPath;
	std::ofstream mLog;
	std::string mCommitDate;
	std::array<uint64_t, METRIC_COUNT> mMetricTicks{};
	std::array<FrameSample, SAMPLE_RING_SIZE> mSamples{};
	std::array<FrameSample, SLOW_CAPTURE_PRE_SAMPLES + SLOW_CAPTURE_POST_SAMPLES> mSlowCapture{};
	TimingSummary mFrameSummary;
	TimingSummary mUpdateSummary;
	TimingSummary mDrawSummary;
	TimingSummary mPresentSummary;
	TimingSummary mSwapWaitSummary;
	FrameProfileBoardCounts mBoardCounts;
	std::array<FrameProfileParticleEffect, PARTICLE_EFFECT_COUNT> mParticleEffects{};
	uint64_t mLastFrameCounter = 0;
	uint64_t mLastSummaryCounter = 0;
	uint64_t mNextSlowCaptureAllowed = 0;
	uint64_t mSlowCaptureTriggerSequence = 0;
	uint64_t mFrameSequence = 0;
	uint64_t mSessionID = 0;
	size_t mSampleWriteIndex = 0;
	size_t mSamplesStored = 0;
	size_t mSlowCaptureCount = 0;
	uint32_t mUpdatesSinceSample = 0;
	uint32_t mInterpolationUpdatesSinceSample = 0;
	uint32_t mLogSegmentIndex = 0;
	int mBuildNumber = 0;
	double mFrameBudgetMs = 10.0;
	double mUpdateBacklogMs = 0.0;
	double mPendingUpdates = 0.0;
	double mUpdateIntervalMs = 10.0;
	uint32_t mVisualQualityRecoveryFrames = 0;
	int mVisualQualityTier = 0;
	bool mInitialized = false;
	bool mDetailed = false;
	bool mParticleParallelEnabled = false;
	bool mSlowCaptureActive = false;
};

class FrameProfileScope
{
public:
	FrameProfileScope(FrameProfileMetric theMetric, bool theDetailedOnly = false);
	~FrameProfileScope();
	FrameProfileScope(const FrameProfileScope&) = delete;
	FrameProfileScope& operator=(const FrameProfileScope&) = delete;
	void Stop();

private:
	FrameProfileMetric mMetric;
	uint64_t mStart = 0;
	bool mActive = false;
};

} // namespace Sexy

#endif
