/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#include "FrameProfiler.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <format>
#include <system_error>

#include "../../PvzpLib/PvzpParticle.h"

namespace Sexy
{

static std::string EscapeJsonString(std::string_view theValue)
{
	std::string anEscapedValue;
	anEscapedValue.reserve(theValue.size());
	constexpr char aHexDigits[] = "0123456789abcdef";
	for (unsigned char aCharacter : theValue)
	{
		if (aCharacter == '"' || aCharacter == '\\')
		{
			anEscapedValue += '\\';
			anEscapedValue += static_cast<char>(aCharacter);
		}
		else if (aCharacter < 0x20)
		{
			anEscapedValue += "\\u00";
			anEscapedValue += aHexDigits[aCharacter >> 4];
			anEscapedValue += aHexDigits[aCharacter & 0x0f];
		}
		else
			anEscapedValue += static_cast<char>(aCharacter);
	}
	return anEscapedValue;
}

static void AppendPoolJson(std::string& theOutput, const char* theName, const FrameProfilePool& thePool)
{
	if (!theOutput.empty() && theOutput.back() != '{')
		theOutput += ',';
	theOutput += std::format("\"{}\":{{\"live\":{},\"high_water\":{},\"capacity\":{}}}",
		theName, thePool.mLive, thePool.mHighWater, thePool.mCapacity);
}

static void AppendCountsJson(std::string& theOutput, const FrameProfileBoardCounts& theCounts)
{
	theOutput += ",\"counts\":{";
	AppendPoolJson(theOutput, "plants", theCounts.mPlants);
	AppendPoolJson(theOutput, "zombies", theCounts.mZombies);
	AppendPoolJson(theOutput, "projectiles", theCounts.mProjectiles);
	AppendPoolJson(theOutput, "coins", theCounts.mCoins);
	AppendPoolJson(theOutput, "particles", theCounts.mParticles);
	AppendPoolJson(theOutput, "emitters", theCounts.mEmitters);
	AppendPoolJson(theOutput, "reanimations", theCounts.mReanimations);
	AppendPoolJson(theOutput, "attachments", theCounts.mAttachments);
	AppendPoolJson(theOutput, "grid_items", theCounts.mGridItems);
	AppendPoolJson(theOutput, "mowers", theCounts.mMowers);
	theOutput += std::format(",\"render_items\":{},\"render_capacity\":{}}}", theCounts.mRenderItems, theCounts.mRenderCapacity);
}

FrameProfiler& FrameProfiler::Get()
{
	static FrameProfiler aProfiler;
	return aProfiler;
}

uint64_t FrameProfiler::Counter()
{
	return SDL_GetPerformanceCounter();
}

double FrameProfiler::ToMilliseconds(uint64_t theTicks)
{
	const uint64_t aFrequency = SDL_GetPerformanceFrequency();
	return aFrequency == 0 ? 0.0 : static_cast<double>(theTicks) * 1000.0 / static_cast<double>(aFrequency);
}

void FrameProfiler::Initialize(const std::filesystem::path& thePath, uint64_t theSessionID, int theBuildNumber,
	std::string_view theCommitDate, bool theDetailed)
{
	mPath = thePath;
	mSessionID = theSessionID;
	mBuildNumber = theBuildNumber;
	mCommitDate.assign(theCommitDate);
	mLogSegmentIndex = 0;
	mDetailed = theDetailed;
	mInitialized = false;
	mLastFrameCounter = 0;
	mLastSummaryCounter = Counter();
	mFrameSequence = 0;
	mNextSlowCaptureAllowed = 0;
	mParticleEffects = {};
	mVisualQualityTier = 0;
	mVisualQualityRecoveryFrames = 0;

	std::error_code anError;
	std::filesystem::create_directories(mPath.parent_path(), anError);
	if (anError)
		return;

	if (std::filesystem::exists(mPath, anError) && !anError && std::filesystem::file_size(mPath, anError) >= MAX_LOG_BYTES)
	{
		std::filesystem::path aBackup = mPath;
		aBackup += ".1";
		std::filesystem::remove(aBackup, anError);
		anError.clear();
		std::filesystem::rename(mPath, aBackup, anError);
	}

	mLog.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
	if (!mLog)
		return;
	mInitialized = true;

	const char* aParallelEnv = std::getenv("PVZ_PARTICLE_PARALLEL");
	mParticleParallelEnabled = aParallelEnv != nullptr && std::strcmp(aParallelEnv, "1") == 0;
	WriteSessionMetadata();
}

void FrameProfiler::WriteSessionMetadata()
{
	if (!mLog.is_open())
		return;
	mLog << std::format("{{\"type\":\"session\",\"session_id\":{},\"segment_index\":{},\"build\":{},\"commit_date\":\"{}\",\"detail\":{},\"particle_parallel\":{},\"subsystem_timings_are_inclusive\":true,\"counter_frequency\":{}}}\n",
		mSessionID, mLogSegmentIndex, mBuildNumber, mCommitDate, mDetailed ? "true" : "false",
		mParticleParallelEnabled ? "true" : "false", SDL_GetPerformanceFrequency());
	mLog.flush();
}

void FrameProfiler::RecordGameplayEvent(std::string_view theEvent, std::string_view theDataJson)
{
	if (!mInitialized || !mLog.is_open())
		return;

	const std::string anEscapedEvent = EscapeJsonString(theEvent);
	WriteLine(std::format(
		R"({{"type":"gameplay_event","session_id":{},"frame":{},"event":"{}","data":{}}})",
		mSessionID, mFrameSequence, anEscapedEvent, theDataJson));
}

void FrameProfiler::SetFrameBudget(double theMilliseconds)
{
	if (std::isfinite(theMilliseconds) && theMilliseconds > 0.0)
		mFrameBudgetMs = std::clamp(theMilliseconds, 1.0, 1000.0);
}

void FrameProfiler::SetSchedulerState(double theUpdateBacklogMs, double thePendingUpdates, double theUpdateIntervalMs)
{
	mUpdateBacklogMs = std::max(0.0, theUpdateBacklogMs);
	mPendingUpdates = std::max(0.0, thePendingUpdates);
	mUpdateIntervalMs = std::max(theUpdateIntervalMs, 0.1);
	const double aBacklogTicks = mUpdateBacklogMs / mUpdateIntervalMs;
	// Shed at 2/4/6 queued updates; recover one tier at a time only below the prior threshold for 120 frames.
	const int aRequestedTier = aBacklogTicks >= 6.0 ? 3 : aBacklogTicks >= 4.0 ? 2 : aBacklogTicks >= 2.0 ? 1 : 0;
	if (aRequestedTier > mVisualQualityTier)
	{
		mVisualQualityTier = aRequestedTier;
		mVisualQualityRecoveryFrames = 0;
	}
	else if (aRequestedTier < mVisualQualityTier)
	{
		const double aRecoveryThreshold = mVisualQualityTier * 2 - 1;
		if (aBacklogTicks < aRecoveryThreshold)
		{
			if (++mVisualQualityRecoveryFrames >= 120)
			{
				--mVisualQualityTier;
				mVisualQualityRecoveryFrames = 0;
			}
		}
		else
			mVisualQualityRecoveryFrames = 0;
	}
	else
		mVisualQualityRecoveryFrames = 0;
}

void FrameProfiler::SetBoardCounts(const FrameProfileBoardCounts& theCounts)
{
	mBoardCounts = theCounts;
}

uint64_t FrameProfiler::BeginParticleEffect()
{
	return mInitialized && mDetailed ? Counter() : 0;
}

void FrameProfiler::RecordParticleEffectUpdate(int theEffect, uint64_t theStart, uint32_t theParticlesVisited,
	uint32_t theParticlesSpawned, uint32_t theLiveParticles, uint32_t theEmitters)
{
	if (theStart == 0 || theEffect < 0 || static_cast<size_t>(theEffect) >= PARTICLE_EFFECT_COUNT)
		return;
	auto& aMetrics = mParticleEffects[static_cast<size_t>(theEffect)];
	aMetrics.mUpdateTicks += Counter() - theStart;
	aMetrics.mParticlesVisited += theParticlesVisited;
	aMetrics.mParticlesSpawned += theParticlesSpawned;
	aMetrics.mLiveParticles = std::max(aMetrics.mLiveParticles, theLiveParticles);
	aMetrics.mEmitters += theEmitters;
}

void FrameProfiler::RecordParticleEffectDraw(int theEffect, uint64_t theStart, uint32_t theParticlesVisited,
	uint32_t theParticlesCulled, uint32_t theTrianglesSubmitted, uint32_t theBatchFlushes)
{
	if (theStart == 0 || theEffect < 0 || static_cast<size_t>(theEffect) >= PARTICLE_EFFECT_COUNT)
		return;
	auto& aMetrics = mParticleEffects[static_cast<size_t>(theEffect)];
	aMetrics.mDrawTicks += Counter() - theStart;
	aMetrics.mDrawParticlesVisited += theParticlesVisited;
	aMetrics.mParticlesCulled += theParticlesCulled;
	aMetrics.mTrianglesSubmitted += theTrianglesSubmitted;
	aMetrics.mBatchFlushes += theBatchFlushes;
}

void FrameProfiler::RecordParticleCalculation(int theEffect, uint32_t theParticles, uint64_t theStartCounter, bool theParallel)
{
	if (theStartCounter == 0 || theEffect < 0 || static_cast<size_t>(theEffect) >= PARTICLE_EFFECT_COUNT)
		return;
	auto& aMetrics = mParticleEffects[static_cast<size_t>(theEffect)];
	aMetrics.mUpdateTicks += Counter() - theStartCounter;
	if (theParallel)
	{
		++aMetrics.mParallelBatches;
		aMetrics.mParallelParticles += theParticles;
	}
}

uint64_t FrameProfiler::Begin(FrameProfileMetric theMetric)
{
	if (!mInitialized)
		return 0;
	if (theMetric >= FrameProfileMetric::PLANT_TARGETING && !mDetailed)
		return 0;
	return Counter();
}

void FrameProfiler::End(FrameProfileMetric theMetric, uint64_t theStart)
{
	if (theStart == 0 || theMetric >= FrameProfileMetric::COUNT)
		return;
	mMetricTicks[static_cast<size_t>(theMetric)] += Counter() - theStart;
	if (theMetric == FrameProfileMetric::UPDATE)
		++mUpdatesSinceSample;
	else if (theMetric == FrameProfileMetric::UPDATE_INTERPOLATION)
		++mInterpolationUpdatesSinceSample;
}

void FrameProfiler::TimingSummary::Add(double theMilliseconds)
{
	static constexpr std::array<double, 16> aBucketUpperBounds = {
		1.0, 2.0, 4.0, 8.0, 12.0, 16.0, 20.0, 25.0,
		33.0, 50.0, 75.0, 100.0, 150.0, 200.0, 250.0, 500.0
	};
	const auto anUpperBound = std::lower_bound(aBucketUpperBounds.begin(), aBucketUpperBounds.end(), theMilliseconds);
	const size_t anIndex = static_cast<size_t>(anUpperBound - aBucketUpperBounds.begin());
	++mHistogram[std::min(anIndex, mHistogram.size() - 1)];
	mTotalMs += theMilliseconds;
	mMaxMs = std::max(mMaxMs, theMilliseconds);
	++mSamples;
}

double FrameProfiler::TimingSummary::Percentile(double theFraction) const
{
	if (mSamples == 0)
		return 0.0;
	static constexpr std::array<double, 16> aBucketUpperBounds = {
		1.0, 2.0, 4.0, 8.0, 12.0, 16.0, 20.0, 25.0,
		33.0, 50.0, 75.0, 100.0, 150.0, 200.0, 250.0, 500.0
	};
	const uint32_t aTarget = std::max(1U, static_cast<uint32_t>(std::ceil(mSamples * std::clamp(theFraction, 0.0, 1.0))));
	uint32_t aTotal = 0;
	for (size_t i = 0; i < mHistogram.size(); ++i)
	{
		aTotal += mHistogram[i];
		if (aTotal >= aTarget)
			return i == aBucketUpperBounds.size() - 1 && theFraction > 0.99 ? std::max(aBucketUpperBounds[i], mMaxMs) : aBucketUpperBounds[i];
	}
	return mMaxMs;
}

void FrameProfiler::TimingSummary::Reset()
{
	*this = {};
}

const char* FrameProfiler::MetricName(FrameProfileMetric theMetric)
{
	static constexpr const char* aNames[] = {
		"update", "update_interpolation", "board_update", "effects_update", "particle_systems_update", "trails_update",
		"reanimations_update", "board_draw", "screen_draw", "render_gather", "render_sort",
		"render_items", "present", "swap_wait", "plant_targeting", "projectile_impact", "projectile_splash", "sun_magnet_assignment"
	};
	return aNames[static_cast<size_t>(theMetric)];
}

void FrameProfiler::AppendSampleJson(std::string& theOutput, const FrameSample& theSample)
{
	if (!theOutput.empty() && theOutput.back() != '[')
		theOutput += ',';
	theOutput += std::format("{{\"seq\":{},\"frame_ms\":{:.3f},\"update_ms\":{:.3f},\"interpolation_update_ms\":{:.3f},\"board_update_ms\":{:.3f},\"effects_update_ms\":{:.3f},\"particle_systems_update_ms\":{:.3f},\"trails_update_ms\":{:.3f},\"reanimations_update_ms\":{:.3f},\"screen_draw_ms\":{:.3f},\"board_draw_ms\":{:.3f},\"render_gather_ms\":{:.3f},\"render_sort_ms\":{:.3f},\"render_items_ms\":{:.3f},\"present_ms\":{:.3f},\"swap_wait_ms\":{:.3f},\"plant_targeting_ms\":{:.3f},\"projectile_impact_ms\":{:.3f},\"projectile_splash_ms\":{:.3f},\"sun_magnet_ms\":{:.3f},\"update_backlog_ms\":{:.3f},\"pending_updates\":{:.2f},\"updates\":{},\"interpolation_updates\":{}",
		theSample.mSequence, theSample.mFrameMs, theSample.mUpdateMs, theSample.mInterpolationUpdateMs, theSample.mBoardUpdateMs,
		theSample.mEffectsUpdateMs, theSample.mParticleSystemsUpdateMs, theSample.mTrailsUpdateMs,
		theSample.mReanimationsUpdateMs, theSample.mScreenDrawMs, theSample.mBoardDrawMs, theSample.mRenderGatherMs,
		theSample.mRenderSortMs, theSample.mRenderItemsMs, theSample.mPresentMs, theSample.mSwapWaitMs,
		theSample.mPlantTargetingMs, theSample.mProjectileImpactMs,
		theSample.mProjectileSplashMs, theSample.mSunMagnetMs, theSample.mUpdateBacklogMs, theSample.mPendingUpdates,
		theSample.mUpdates, theSample.mInterpolationUpdates);
	AppendCountsJson(theOutput, theSample.mCounts);
	AppendParticleEffectsJson(theOutput, theSample.mParticleEffects);
	theOutput += std::format(",\"vfx_quality_tier\":{}", theSample.mVisualQualityTier);
	theOutput += '}';
}

void FrameProfiler::AppendParticleEffectsJson(std::string& theOutput,
	const std::array<FrameProfileParticleEffectSample, 8>& theEffects)
{
	theOutput += ",\"particle_effects\":[";
	bool aFirst = true;
	for (const auto& anEntry : theEffects)
	{
		const size_t aIndex = anEntry.mEffectId;
		const auto& aMetric = anEntry.mMetrics;
		if (aMetric.mUpdateTicks == 0 && aMetric.mDrawTicks == 0)
			continue;
		if (!aFirst)
			theOutput += ',';
		aFirst = false;
		const char* aName = aIndex < static_cast<size_t>(ParticleEffect::NUM_PARTICLES) &&
			gLawnParticleArray[aIndex].mParticleFileName != nullptr
			? gLawnParticleArray[aIndex].mParticleFileName : "unknown";
		theOutput += std::format("{{\"id\":{},\"name\":\"{}\",\"update_ms\":{:.3f},\"draw_ms\":{:.3f},\"particles_visited\":{},\"spawned\":{},\"peak_system_live\":{},\"emitter_updates\":{},\"draw_visited\":{},\"culled\":{},\"triangles\":{},\"batch_flushes\":{},\"parallel_batches\":{},\"parallel_particles\":{}}}",
			aIndex, aName, ToMilliseconds(aMetric.mUpdateTicks), ToMilliseconds(aMetric.mDrawTicks), aMetric.mParticlesVisited,
			aMetric.mParticlesSpawned, aMetric.mLiveParticles, aMetric.mEmitters, aMetric.mDrawParticlesVisited,
			aMetric.mParticlesCulled, aMetric.mTrianglesSubmitted, aMetric.mBatchFlushes, aMetric.mParallelBatches,
			aMetric.mParallelParticles);
	}
	theOutput += ']';
}

void FrameProfiler::AddSample(const FrameSample& theSample)
{
	mSamples[mSampleWriteIndex] = theSample;
	mSampleWriteIndex = (mSampleWriteIndex + 1) % mSamples.size();
	mSamplesStored = std::min(mSamplesStored + 1, mSamples.size());
	mFrameSummary.Add(theSample.mFrameMs);
	mUpdateSummary.Add(theSample.mUpdateMs);
	mDrawSummary.Add(theSample.mScreenDrawMs);
	mPresentSummary.Add(theSample.mPresentMs);
	mSwapWaitSummary.Add(theSample.mSwapWaitMs);

	const uint64_t aNow = Counter();
	if (mSlowCaptureActive)
	{
		if (mSlowCaptureCount < SLOW_CAPTURE_POST_SAMPLES)
			mSlowCapture[mSlowCaptureCount++] = theSample;
		if (mSlowCaptureCount == SLOW_CAPTURE_POST_SAMPLES)
			CompleteSlowCapture();
	}
	else if (aNow >= mNextSlowCaptureAllowed && theSample.mFrameMs > mFrameBudgetMs * 1.5)
	{
		StartSlowCapture(theSample);
	}

	if (aNow - mLastSummaryCounter >= SDL_GetPerformanceFrequency())
		WriteSummary(aNow);
}

void FrameProfiler::StartSlowCapture(const FrameSample& theSample)
{
	mSlowCaptureActive = true;
	mSlowCaptureCount = 0;
	const size_t aPreCount = std::min(SLOW_CAPTURE_PRE_SAMPLES, mSamplesStored);
	const size_t aStart = (mSampleWriteIndex + mSamples.size() - aPreCount) % mSamples.size();
	for (size_t i = 0; i < aPreCount; ++i)
		mSlowCapture[mSlowCaptureCount++] = mSamples[(aStart + i) % mSamples.size()];
	if (mSlowCaptureCount == 0 || mSlowCapture[mSlowCaptureCount - 1].mSequence != theSample.mSequence)
		mSlowCapture[mSlowCaptureCount++] = theSample;
	mSlowCaptureTriggerSequence = theSample.mSequence;
	std::string aLine = "{\"type\":\"slow_frame_window\",\"samples\":[";
	for (size_t i = 0; i < mSlowCaptureCount; ++i)
		AppendSampleJson(aLine, mSlowCapture[i]);
	aLine += "]}";
	WriteLine(aLine);
	mSlowCaptureCount = 0;
	mNextSlowCaptureAllowed = Counter() + SDL_GetPerformanceFrequency() * 5;
}

void FrameProfiler::CompleteSlowCapture()
{
	std::string aLine = std::format("{{\"type\":\"slow_frame_followup\",\"trigger_seq\":{},\"samples\":[", mSlowCaptureTriggerSequence);
	for (size_t i = 0; i < mSlowCaptureCount; ++i)
		AppendSampleJson(aLine, mSlowCapture[i]);
	aLine += "]}";
	WriteLine(aLine);
	mSlowCaptureActive = false;
	mSlowCaptureCount = 0;
}

void FrameProfiler::WriteSummary(uint64_t theNow)
{
	const double aWindowSeconds = ToMilliseconds(theNow - mLastSummaryCounter) / 1000.0;
	const FrameSample& aLatest = mSamples[(mSampleWriteIndex + mSamples.size() - 1) % mSamples.size()];
	std::string aLine = std::format("{{\"type\":\"summary\",\"seq\":{},\"window_seconds\":{:.2f},\"target_frame_ms\":{:.3f},\"frame\":{{\"avg_ms\":{:.3f},\"p95_ms\":{:.3f},\"max_ms\":{:.3f}}},\"update\":{{\"avg_ms\":{:.3f},\"p95_ms\":{:.3f},\"max_ms\":{:.3f}}},\"draw_screen\":{{\"avg_ms\":{:.3f},\"p95_ms\":{:.3f},\"max_ms\":{:.3f}}},\"present\":{{\"avg_ms\":{:.3f},\"p95_ms\":{:.3f},\"max_ms\":{:.3f}}},\"swap_wait\":{{\"avg_ms\":{:.3f},\"p95_ms\":{:.3f},\"max_ms\":{:.3f}}},\"board_update_ms\":{:.3f},\"effects_update_ms\":{:.3f},\"particle_systems_update_ms\":{:.3f},\"trails_update_ms\":{:.3f},\"reanimations_update_ms\":{:.3f},\"board_draw_ms\":{:.3f},\"screen_draw_ms\":{:.3f},\"render_gather_ms\":{:.3f},\"render_sort_ms\":{:.3f},\"render_items_ms\":{:.3f},\"update_backlog_ms\":{:.3f},\"pending_updates\":{:.2f},\"updates_last_frame\":{},\"interpolation_updates_last_frame\":{}",
		aLatest.mSequence, aWindowSeconds, mFrameBudgetMs,
		mFrameSummary.mSamples ? mFrameSummary.mTotalMs / mFrameSummary.mSamples : 0.0,
		mFrameSummary.Percentile(0.95), mFrameSummary.mMaxMs,
		mUpdateSummary.mSamples ? mUpdateSummary.mTotalMs / mUpdateSummary.mSamples : 0.0,
		mUpdateSummary.Percentile(0.95), mUpdateSummary.mMaxMs,
		mDrawSummary.mSamples ? mDrawSummary.mTotalMs / mDrawSummary.mSamples : 0.0,
		mDrawSummary.Percentile(0.95), mDrawSummary.mMaxMs,
		mPresentSummary.mSamples ? mPresentSummary.mTotalMs / mPresentSummary.mSamples : 0.0,
		mPresentSummary.Percentile(0.95), mPresentSummary.mMaxMs,
		mSwapWaitSummary.mSamples ? mSwapWaitSummary.mTotalMs / mSwapWaitSummary.mSamples : 0.0,
		mSwapWaitSummary.Percentile(0.95), mSwapWaitSummary.mMaxMs,
		aLatest.mBoardUpdateMs, aLatest.mEffectsUpdateMs, aLatest.mParticleSystemsUpdateMs, aLatest.mTrailsUpdateMs,
		aLatest.mReanimationsUpdateMs, aLatest.mBoardDrawMs, aLatest.mScreenDrawMs, aLatest.mRenderGatherMs,
		aLatest.mRenderSortMs, aLatest.mRenderItemsMs, aLatest.mUpdateBacklogMs, aLatest.mPendingUpdates, aLatest.mUpdates,
		aLatest.mInterpolationUpdates);
	AppendCountsJson(aLine, aLatest.mCounts);
	AppendParticleEffectsJson(aLine, aLatest.mParticleEffects);
	aLine += std::format(",\"vfx_quality_tier\":{}", aLatest.mVisualQualityTier);
	aLine += '}';
	WriteLine(aLine);
	mLastSummaryCounter = theNow;
	mFrameSummary.Reset();
	mUpdateSummary.Reset();
	mDrawSummary.Reset();
	mPresentSummary.Reset();
	mSwapWaitSummary.Reset();
}

void FrameProfiler::RotateLogIfNeeded(size_t theIncomingBytes)
{
	if (!mLog.is_open())
		return;
	mLog.flush();
	std::error_code anError;
	uint64_t aCurrentSize = std::filesystem::file_size(mPath, anError);
	if (anError || aCurrentSize + theIncomingBytes <= MAX_LOG_BYTES)
		return;
	mLog.close();
	std::filesystem::path aBackup = mPath;
	aBackup += ".1";
	std::filesystem::remove(aBackup, anError);
	anError.clear();
	std::filesystem::rename(mPath, aBackup, anError);
	if (anError)
		mLog.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
	else
		mLog.open(mPath, std::ios::out | std::ios::trunc | std::ios::binary);
	if (mLog.is_open())
	{
		++mLogSegmentIndex;
		WriteSessionMetadata();
	}
}

void FrameProfiler::WriteLine(const std::string& theLine)
{
	if (!mLog.is_open())
		return;
	RotateLogIfNeeded(theLine.size() + 1);
	if (mLog.is_open())
		mLog << theLine << '\n' << std::flush;
}

void FrameProfiler::RecordFrame()
{
	if (!mInitialized)
		return;
	const uint64_t aNow = Counter();
	FrameSample aSample;
	aSample.mSequence = ++mFrameSequence;
	aSample.mFrameMs = mLastFrameCounter == 0 ? 0.0 : ToMilliseconds(aNow - mLastFrameCounter);
	aSample.mUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::UPDATE)], 0));
	aSample.mInterpolationUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::UPDATE_INTERPOLATION)], 0));
	aSample.mBoardUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::BOARD_UPDATE)], 0));
	aSample.mEffectsUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::EFFECTS_UPDATE)], 0));
	aSample.mParticleSystemsUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::PARTICLE_SYSTEMS_UPDATE)], 0));
	aSample.mTrailsUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::TRAILS_UPDATE)], 0));
	aSample.mReanimationsUpdateMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::REANIMATIONS_UPDATE)], 0));
	aSample.mBoardDrawMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::BOARD_DRAW)], 0));
	aSample.mScreenDrawMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::SCREEN_DRAW)], 0));
	aSample.mRenderGatherMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::RENDER_GATHER)], 0));
	aSample.mRenderSortMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::RENDER_SORT)], 0));
	aSample.mRenderItemsMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::RENDER_ITEMS)], 0));
	aSample.mPresentMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::PRESENT)], 0));
	aSample.mSwapWaitMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::SWAP_WAIT)], 0));
	aSample.mPlantTargetingMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::PLANT_TARGETING)], 0));
	aSample.mProjectileImpactMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::PROJECTILE_IMPACT)], 0));
	aSample.mProjectileSplashMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::PROJECTILE_SPLASH)], 0));
	aSample.mSunMagnetMs = ToMilliseconds(std::exchange(mMetricTicks[static_cast<size_t>(FrameProfileMetric::SUN_MAGNET_ASSIGNMENT)], 0));
	aSample.mUpdateBacklogMs = mUpdateBacklogMs;
	aSample.mPendingUpdates = mPendingUpdates;
	aSample.mUpdates = std::exchange(mUpdatesSinceSample, 0);
	aSample.mInterpolationUpdates = std::exchange(mInterpolationUpdatesSinceSample, 0);
	aSample.mCounts = mBoardCounts;
	if (mDetailed)
	{
		std::array<size_t, 8> aTopEffects{};
		std::array<uint64_t, 8> aTopScores{};
		for (size_t i = 0; i < mParticleEffects.size(); ++i)
		{
			const auto& aMetric = mParticleEffects[i];
			const uint64_t aScore = aMetric.mUpdateTicks + aMetric.mDrawTicks;
			if (aScore == 0)
				continue;
			for (size_t j = 0; j < aTopEffects.size(); ++j)
			{
				if (aScore <= aTopScores[j])
					continue;
				for (size_t k = aTopEffects.size() - 1; k > j; --k)
				{
					aTopEffects[k] = aTopEffects[k - 1];
					aTopScores[k] = aTopScores[k - 1];
				}
				aTopEffects[j] = i;
				aTopScores[j] = aScore;
				break;
			}
		}
		for (size_t i = 0; i < aTopEffects.size(); ++i)
		{
			aSample.mParticleEffects[i].mEffectId = static_cast<uint16_t>(aTopEffects[i]);
			aSample.mParticleEffects[i].mMetrics = mParticleEffects[aTopEffects[i]];
		}
		mParticleEffects = {};
	}
	aSample.mVisualQualityTier = mVisualQualityTier;
	mLastFrameCounter = aNow;
	AddSample(aSample);
}

FrameProfileScope::FrameProfileScope(FrameProfileMetric theMetric, bool theDetailedOnly)
	: mMetric(theMetric)
{
	FrameProfiler& aProfiler = FrameProfiler::Get();
	if (!theDetailedOnly || aProfiler.IsDetailed())
	{
		mStart = aProfiler.Begin(theMetric);
		mActive = mStart != 0;
	}
}

FrameProfileScope::~FrameProfileScope()
{
	Stop();
}

void FrameProfileScope::Stop()
{
	if (mActive)
	{
		FrameProfiler::Get().End(mMetric, mStart);
		mActive = false;
	}
}

} // namespace Sexy
