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

#include <time.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <SDL.h>
#include <format>
#include "../Modes/ZenGarden.h"
#include "BoardInclude.h"
#include "../Entities/LawnCommon.h"
#include "../System/Music.h"
#include "../System/SaveGame.h"
#include "../Widget/LawnDialog.h"
#include "../System/PlayerInfo.h"
#include "../System/PoolEffect.h"
#include "../System/TypingCheck.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/AwardScreen.h"
#include "../../PvzpLib/Trail.h"
#include "../Widget/ChallengeScreen.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "../Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "../Zombie/ZombieStrengthRules.h"
#include "../Plant/PlantRules.h"

int Board::MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset)
{
	return theRow * static_cast<int>(RenderLayer::RENDER_LAYER_ROW_OFFSET) + theRenderLayer + theLayerOffset;
}

void Board::UpdateProgressMeter()
{
	if (mApp->IsFinalBossLevel())
	{
		Zombie* aBoss = GetBossZombie();
		if (aBoss && !aBoss->IsDeadOrDying())
		{
			mProgressMeterWidth = 150 * (aBoss->mBodyMaxHealth - aBoss->mBodyHealth) / aBoss->mBodyMaxHealth;
		}
		else
		{
			mProgressMeterWidth = 150;
		}
	}
	else if (mCurrentWave != 0)
	{
		if (mFlagRaiseCounter > 0)
			mFlagRaiseCounter--;

		int aTotalWidth = 150;  // total meter length distributed across the waves
		int aNumWavesPerFlag = GetNumWavesPerFlag();
		bool aHasFlags = ProgressMeterHasFlags();  // flag waves occupy extra meter length when flags are drawn
		if (aHasFlags)
		{
			aTotalWidth -= 12 * mNumWaves / aNumWavesPerFlag;
		}

		int aWaveLength = aTotalWidth / (mNumWaves - 1);
		int aCurrentWaveLength = (mCurrentWave - 1) * aTotalWidth / (mNumWaves - 1);
		int aNextWaveLength = mCurrentWave * aTotalWidth / (mNumWaves - 1);
		if (aHasFlags)
		{
			int anExtraLength = mCurrentWave / aNumWavesPerFlag * 12;  // add back the flag length of completed flag waves
			aCurrentWaveLength += anExtraLength;
			aNextWaveLength += anExtraLength;
		}

		float aFraction = (mZombieCountDownStart - mZombieCountDown) / static_cast<float>(mZombieCountDownStart);
		if (mZombieHealthToNextWave != -1)
		{
			int aHealthCurrent = TotalZombiesHealthInWave(mCurrentWave - 1);
			// damage that must be dealt to this wave to trigger the next wave
			int aDamageTarget = mZombieHealthWaveStart - mZombieHealthToNextWave;
			if (aDamageTarget < 1)
			{
				aDamageTarget = 1;
			}
			// health fraction = damage dealt to this wave / damage needed to trigger the next wave
			float aHealthFraction = (aDamageTarget - aHealthCurrent + mZombieHealthToNextWave) / static_cast<float>(aDamageTarget);
			aFraction = std::max(aHealthFraction, aFraction);
		}

		int aLength = std::clamp(aCurrentWaveLength + FloatRoundToInt((aNextWaveLength - aCurrentWaveLength) * aFraction), 1, 150);
		int aDelta = aLength - mProgressMeterWidth;
		// adjust every 20cs, or every 5cs when more than a wave length behind
		if ((aDelta > aWaveLength && (mMainCounter % 5 == 0)) || (aDelta > 0 && (mMainCounter % 20 == 0)))
		{
			mProgressMeterWidth++;
		}
	}
}

void Board::DrawIce(Graphics* g, int theGridY)
{
	int aPosY = GridToPixelY(8, theGridY) + 20;
	int aHeight = Sexy::IMAGE_ICE->GetHeight();
	int aWidth = Sexy::IMAGE_ICE->GetWidth();
	int anAlpha = std::clamp(255 * mIceTimer[theGridY] / 10, 0, 255);
	if (anAlpha < 255)
	{
		g->SetColorizeImages(true);
		g->SetColor(Color(255, 255, 255, anAlpha));
	}

	int aBeginningX = mIceMinX[theGridY] + 13, aDeltaX;
	for (int aPosX = aBeginningX; aPosX < mApp->mWidth; aPosX += aDeltaX)
	{
		if (aPosX == aBeginningX)
		{
			aDeltaX = (mApp->mWidth - aBeginningX) % aWidth;
			if (!aDeltaX) aDeltaX = aWidth;
		}
		else aDeltaX = aWidth;
		Rect aRepeatSrcRect(aWidth - aDeltaX, 0, aDeltaX, aHeight);
		Rect aRepeatDstRect(aPosX, aPosY, aDeltaX, aHeight);
		g->DrawImage(Sexy::IMAGE_ICE, aRepeatDstRect, aRepeatSrcRect);
	}
	g->DrawImage(Sexy::IMAGE_ICE_CAP, mIceMinX[theGridY], aPosY);
	g->SetColorizeImages(false);
}

void Board::DrawBackdrop(Graphics* g)
{
	Image* aBgImage = nullptr;
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:				aBgImage = Sexy::IMAGE_BACKGROUND1;						break;
	case BackgroundType::BACKGROUND_2_NIGHT:			aBgImage = Sexy::IMAGE_BACKGROUND2;						break;
	case BackgroundType::BACKGROUND_3_POOL:				aBgImage = Sexy::IMAGE_BACKGROUND3;						break;
	case BackgroundType::BACKGROUND_4_FOG:				aBgImage = Sexy::IMAGE_BACKGROUND4;						break;
	case BackgroundType::BACKGROUND_5_ROOF:				aBgImage = Sexy::IMAGE_BACKGROUND5;						break;
	case BackgroundType::BACKGROUND_6_BOSS:				aBgImage = Sexy::IMAGE_BACKGROUND6BOSS;					break;
	case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:	aBgImage = Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN;		break;
	case BackgroundType::BACKGROUND_GREENHOUSE:			aBgImage = Sexy::IMAGE_BACKGROUND_GREENHOUSE;			break;
	case BackgroundType::BACKGROUND_ZOMBIQUARIUM:		aBgImage = Sexy::IMAGE_AQUARIUM1;						break;
	case BackgroundType::BACKGROUND_TREEOFWISDOM:		aBgImage = nullptr;										break;
	default:											PVZP_ASSERT(false);											break;
	}

	if (mLevel == 1 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD1ROW->GetWidth(), PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD1ROW->GetHeight());
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265, aSrcRect);
	}
	else if (((mLevel == 2 || mLevel == 3) && mApp->IsFirstTimeAdventureMode()) || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RESODDED)
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD3ROW->GetWidth(), PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD3ROW->GetHeight());
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149, aSrcRect);
	}
	else if (mLevel == 4 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149);
		int aWidth = PvzpAnimateCurve(0, 1000, mSodPosition, 0, 773, PvzpCurves::CURVE_LINEAR);
		Rect aSrcRect(232, 0, aWidth, Sexy::IMAGE_BACKGROUND1->GetHeight());
		g->DrawImage(Sexy::IMAGE_BACKGROUND1, 232 - BOARD_OFFSET, 0, aSrcRect);
	}
	else if (aBgImage)
	{
		if (aBgImage == Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN || aBgImage == Sexy::IMAGE_BACKGROUND_GREENHOUSE || aBgImage == Sexy::IMAGE_AQUARIUM1)
		{
			g->DrawImage(aBgImage, 0, 0);
		}
		else
		{
			g->DrawImage(aBgImage, -BOARD_OFFSET, 0);
		}
	}
	const bool aTallWideEndlessPool = LawnApp::IsSurvivalEndless(mApp->mGameMode) && StageHasPool() &&
		mApp->mWidth > BOARD_WIDTH && mApp->mHeight > BOARD_HEIGHT;
	if (aTallWideEndlessPool && aBgImage)
	{
		constexpr int aAddedColumnCount = MAX_GRID_SIZE_X - CLASSIC_GRID_SIZE_X;
		constexpr int aAddedColumnWidth = 80;
		constexpr int aAddedColumnsWidth = aAddedColumnCount * aAddedColumnWidth;
		constexpr int aBackgroundExtensionStart = LAWN_XMIN + CLASSIC_GRID_SIZE_X * aAddedColumnWidth;
		constexpr int aExtensionSourceX = BOARD_OFFSET + aBackgroundExtensionStart - 3 * aAddedColumnWidth;
		constexpr int aExtensionPatternWidth = 2 * aAddedColumnWidth;
		constexpr int aBorderSourceX = BOARD_OFFSET + BOARD_WIDTH;
		const int aRightBorderStart = aBackgroundExtensionStart + aAddedColumnsWidth;
		const int aRightBorderWidth = mApp->mWidth - aRightBorderStart;
		const int aBaseHeight = std::min(aBgImage->GetHeight(), mApp->mHeight);

		auto DrawExtendedBackgroundStrip = [&](Graphics& theGraphics, int theDestY, int theSourceY, int theHeight)
		{
			for (int aColumnOffset = 0; aColumnOffset < aAddedColumnsWidth; aColumnOffset += aAddedColumnWidth)
			{
				const int aTileWidth = std::min(aAddedColumnWidth, aAddedColumnsWidth - aColumnOffset);
				const int aTileSourceX = aExtensionSourceX + aColumnOffset % aExtensionPatternWidth;
				theGraphics.DrawImage(aBgImage,
					Rect(aBackgroundExtensionStart + aColumnOffset, theDestY, aTileWidth, theHeight),
					Rect(aTileSourceX, theSourceY, aTileWidth, theHeight));
			}
			if (aRightBorderWidth > 0)
			{
				theGraphics.DrawImage(aBgImage,
					Rect(aRightBorderStart, theDestY, aRightBorderWidth, theHeight),
					Rect(aBorderSourceX, theSourceY, aRightBorderWidth, theHeight));
			}
		};

		constexpr int aJoinBlendWidth = 8;
		constexpr int aJoinBlendBandWidth = 2;
		Graphics aColumnBlendGraphics(*g);
		aColumnBlendGraphics.SetColorizeImages(true);
		auto BlendColumnJoin = [&](int theDestX, int theSourceX, int theDestY, int theSourceY, int theHeight)
		{
			for (int aBand = 0; aBand < aJoinBlendWidth / aJoinBlendBandWidth; aBand++)
			{
				int anAlpha = (aBand + 1) * 255 / (aJoinBlendWidth / aJoinBlendBandWidth);
				aColumnBlendGraphics.SetColor(Color(255, 255, 255, anAlpha));
				aColumnBlendGraphics.DrawImage(aBgImage,
					Rect(theDestX - aJoinBlendWidth + aBand * aJoinBlendBandWidth,
						theDestY, aJoinBlendBandWidth, theHeight),
					Rect(theSourceX + aBand * aJoinBlendBandWidth, theSourceY,
						aJoinBlendBandWidth, theHeight));
			}
		};
		DrawExtendedBackgroundStrip(*g, 0, 0, aBaseHeight);
		BlendColumnJoin(aBackgroundExtensionStart, aExtensionSourceX, 0, 0, aBaseHeight);
		BlendColumnJoin(aBackgroundExtensionStart + 2 * aAddedColumnWidth, aExtensionSourceX,
			0, 0, aBaseHeight);
		if (aRightBorderWidth > 0)
			BlendColumnJoin(aRightBorderStart, aBorderSourceX, 0, 0, aBaseHeight);

		// The upper lawn is a clean background strip above the original pool rim. Repeat two lane heights
		// below the original viewport and blend the joins over a wider strip.
		const int aRowSpacing = GetGridRowSpacing();
		const int aLawnTileHeight = 2 * aRowSpacing;
		const int aSourcePoolTop = LAWN_YMIN + 199;
		const int aLawnSourceY = std::max(0, aSourcePoolTop - aLawnTileHeight - 8);
		constexpr int aBlendHeight = 24;
		constexpr int aBlendBandHeight = 2;
		Graphics aBlendGraphics(*g);
		aBlendGraphics.SetColorizeImages(true);
		auto DrawLawnBackgroundStrip = [&](Graphics& theGraphics, int theDestY, int theSourceY, int theHeight)
		{
			theGraphics.DrawImage(aBgImage,
				Rect(-BOARD_OFFSET, theDestY, aBgImage->GetWidth(), theHeight),
				Rect(0, theSourceY, aBgImage->GetWidth(), theHeight));
			DrawExtendedBackgroundStrip(theGraphics, theDestY, theSourceY, theHeight);
		};
		auto BlendLawnSeam = [&](int theDestY)
		{
			for (int aBand = 0; aBand < aBlendHeight / aBlendBandHeight; aBand++)
			{
				const int aSourceY = aLawnSourceY + aBand * aBlendBandHeight;
				const int aBandDestY = theDestY + aBand * aBlendBandHeight;
				const int anAlpha = (aBand + 1) * 255 / (aBlendHeight / aBlendBandHeight);
				aBlendGraphics.SetColor(Color(255, 255, 255, anAlpha));
				DrawLawnBackgroundStrip(aBlendGraphics, aBandDestY, aSourceY, aBlendBandHeight);
			}
		};
		auto DrawLawnTile = [&](int theDestY, int theSourceY, int theHeight)
		{
			g->DrawImage(aBgImage,
				Rect(-BOARD_OFFSET, theDestY, aBgImage->GetWidth(), theHeight),
				Rect(0, theSourceY, aBgImage->GetWidth(), theHeight));
			DrawExtendedBackgroundStrip(*g, theDestY, theSourceY, theHeight);
			BlendColumnJoin(aBackgroundExtensionStart, aExtensionSourceX, theDestY, theSourceY, theHeight);
			BlendColumnJoin(aBackgroundExtensionStart + 2 * aAddedColumnWidth, aExtensionSourceX,
				theDestY, theSourceY, theHeight);
			if (aRightBorderWidth > 0)
				BlendColumnJoin(aRightBorderStart, aBorderSourceX, theDestY, theSourceY, theHeight);
		};

		BlendLawnSeam(BOARD_HEIGHT - aBlendHeight);
		DrawLawnTile(BOARD_HEIGHT, aLawnSourceY, std::min(aLawnTileHeight, mApp->mHeight - BOARD_HEIGHT));
		for (int aDestY = BOARD_HEIGHT + aLawnTileHeight; aDestY < mApp->mHeight; aDestY += aLawnTileHeight)
		{
			BlendLawnSeam(aDestY - aBlendHeight);
			DrawLawnTile(aDestY, aLawnSourceY + aBlendHeight,
				std::min(aLawnTileHeight - aBlendHeight, mApp->mHeight - aDestY));
		}
	}

	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		DrawHouseDoorBottom(g);
	}
	if (StageHasPool())
	{
		int aFirstPoolRow = 0;
		int aPoolRowCount = 0;
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (mPlantRow[y] == PlantRowType::PLANTROW_POOL)
			{
				if (aPoolRowCount == 0)
					aFirstPoolRow = y;
				++aPoolRowCount;
			}
		}
		mApp->mPoolEffect->PoolEffectDraw(g, StageIsNight(), aFirstPoolRow, aPoolRowCount, GetGridRowSpacing());
		if (aTallWideEndlessPool && aPoolRowCount == 4 && aBgImage)
		{
			constexpr int aPoolFrameLeft = 20;
			constexpr int aPoolWaterLeft = 35;
			constexpr int aPoolEdgeHeight = 18;
			constexpr int aOriginalPoolWaterWidth = 704;
			constexpr int aRightRailWidth = 41;
			const int aPoolFrameRight = mApp->mWidth - aPoolFrameLeft;
			const int aPoolWaterWidth = aPoolFrameRight - aPoolWaterLeft - aRightRailWidth;
			const int aOriginalFrameWidth = BOARD_WIDTH - 2 * aPoolFrameLeft;
			const int aRowSpacing = GetGridRowSpacing();
			const int aPoolTop = LAWN_YMIN + aFirstPoolRow * aRowSpacing + aRowSpacing / 3 + 1;
			constexpr int aSourcePoolTop = LAWN_YMIN + 199;
			const int aSourcePoolBottom = aSourcePoolTop + Sexy::IMAGE_POOL->GetHeight();
			const int aExtendedPoolBottom = aPoolTop + aPoolRowCount * aRowSpacing;
			const int aSideTileHeight = std::min(aRowSpacing, std::max(0, Sexy::IMAGE_POOL->GetHeight() - aPoolEdgeHeight * 2));
			const int aSideSourceY = aSourcePoolBottom - aPoolEdgeHeight - aSideTileHeight;
			const int aLeftSourceX = BOARD_OFFSET + aPoolFrameLeft;
			const int aLeftRailWidth = aPoolWaterLeft - aPoolFrameLeft;
			const int aPoolFrameWidth = aPoolFrameRight - aPoolFrameLeft;
			const int aLowerEdgeSourceY = std::clamp(aSourcePoolBottom, 0,
				aBgImage->GetHeight() - aPoolEdgeHeight);
			const int aPoolTopDelta = aPoolTop - aSourcePoolTop;
			const int aSourceFrameX = BOARD_OFFSET + aPoolFrameLeft;
			const int aRightSourceX = BOARD_OFFSET + aPoolWaterLeft + aOriginalPoolWaterWidth;
				auto DrawHorizontalFrameStrip = [&](int theDestY, int theSourceY, int theHeight)
			{
				constexpr int aFrameJoinBlendWidth = 8;
				constexpr int aFrameJoinBlendBandWidth = 2;
				const int aBaseWidth = std::min(aOriginalFrameWidth, aPoolFrameWidth);
				g->DrawImage(aBgImage,
					Rect(aPoolFrameLeft, theDestY, aBaseWidth, theHeight),
					Rect(aSourceFrameX, theSourceY, aBaseWidth, theHeight));
				const int aDestX = aPoolFrameLeft + aBaseWidth;
				const int aExtensionWidth = aPoolFrameRight - aDestX;
				if (aExtensionWidth > 0)
				{
					const int aExtensionSourceX = aSourceFrameX + aBaseWidth - aExtensionWidth;
					Graphics aFrameBlendGraphics(*g);
					aFrameBlendGraphics.SetColorizeImages(true);
					for (int aBand = 0; aBand < aFrameJoinBlendWidth / aFrameJoinBlendBandWidth; aBand++)
					{
						const int anAlpha = (aBand + 1) * 255 / (aFrameJoinBlendWidth / aFrameJoinBlendBandWidth);
						aFrameBlendGraphics.SetColor(Color(255, 255, 255, anAlpha));
						aFrameBlendGraphics.DrawImage(aBgImage,
							Rect(aDestX - aFrameJoinBlendWidth + aBand * aFrameJoinBlendBandWidth, theDestY,
								aFrameJoinBlendBandWidth, theHeight),
							Rect(aExtensionSourceX + aBand * aFrameJoinBlendBandWidth, theSourceY,
								aFrameJoinBlendBandWidth, theHeight));
					}
					g->DrawImage(aBgImage,
						Rect(aDestX, theDestY, aExtensionWidth, theHeight),
						Rect(aExtensionSourceX, theSourceY, aExtensionWidth, theHeight));
				}
			};

			// The original background has its pool rim one lane higher. Restore that strip to lawn,
			// then reuse its original upper rim at the new four-row basin position.
			if (aPoolTopDelta > 0)
			{
				const int aLawnSourceY = std::clamp(aSourcePoolTop - aPoolEdgeHeight - aPoolTopDelta, 0,
					aBgImage->GetHeight() - aPoolTopDelta);
				DrawHorizontalFrameStrip(aSourcePoolTop - aPoolEdgeHeight, aLawnSourceY, aPoolTopDelta);
				DrawHorizontalFrameStrip(aPoolTop - aPoolEdgeHeight,
					aSourcePoolTop - aPoolEdgeHeight, aPoolEdgeHeight);
			}

			// Continue the existing vertical stone rails down to the relocated lower lip.
			if (aPoolTop < aExtendedPoolBottom + aPoolEdgeHeight && aSideTileHeight > 0)
			{
				constexpr int aRailBlendHeight = 8;
				constexpr int aRailBlendBandHeight = 2;
				Graphics aRailBlendGraphics(*g);
				aRailBlendGraphics.SetColorizeImages(true);
				auto DrawRailStrip = [&](Graphics& theGraphics, int theDestY, int theSourceY, int theHeight)
				{
					theGraphics.DrawImage(aBgImage,
						Rect(aPoolFrameLeft, theDestY, aLeftRailWidth, theHeight),
						Rect(aLeftSourceX, theSourceY, aLeftRailWidth, theHeight));
					theGraphics.DrawImage(aBgImage,
						Rect(aPoolFrameRight - aRightRailWidth, theDestY, aRightRailWidth, theHeight),
						Rect(aRightSourceX, theSourceY, aRightRailWidth, theHeight));
				};
				auto BlendRailSeam = [&](int theDestY)
				{
					for (int aBand = 0; aBand < aRailBlendHeight / aRailBlendBandHeight; aBand++)
					{
						int aSourceY = aSideSourceY + aBand * aRailBlendBandHeight;
						int aBandDestY = theDestY + aBand * aRailBlendBandHeight;
						int anAlpha = (aBand + 1) * 255 / (aRailBlendHeight / aRailBlendBandHeight);
						aRailBlendGraphics.SetColor(Color(255, 255, 255, anAlpha));
						DrawRailStrip(aRailBlendGraphics, aBandDestY, aSourceY, aRailBlendBandHeight);
					}
				};

				const int aRailEnd = aExtendedPoolBottom + aPoolEdgeHeight;
				DrawRailStrip(*g, aPoolTop, aSideSourceY, std::min(aSideTileHeight, aRailEnd - aPoolTop));
				for (int aRailY = aPoolTop + aSideTileHeight; aRailY < aRailEnd; aRailY += aSideTileHeight)
				{
					BlendRailSeam(aRailY - aRailBlendHeight);
					const int aRailHeight = std::min(aSideTileHeight - aRailBlendHeight, aRailEnd - aRailY);
					if (aRailHeight > 0)
						DrawRailStrip(*g, aRailY, aSideSourceY + aRailBlendHeight, aRailHeight);
				}
			}

			// Place the lower stone lip just below the water and use the same matched edge tile pattern.
			DrawHorizontalFrameStrip(aExtendedPoolBottom, aLowerEdgeSourceY, aPoolEdgeHeight);
		}
	}
	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		Graphics aClipG(*g);
		aClipG.SetColorizeImages(true);
		aClipG.SetColor(GetFlashingColor(mMainCounter, 75));
		aClipG.DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		aClipG.SetColorizeImages(false);
	}
	mChallenge->DrawBackdrop(g);
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && StageHasGraveStones())
	{
		g->DrawImage(Sexy::IMAGE_NIGHT_GRAVE_GRAPHIC, 1092, 40);
	}
}

bool Board::HasProgressMeter()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->IsFinalBossLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return true;

	if (mProgressMeterWidth == 0)
		return false;

	if (mApp->IsContinuousChallenge() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->IsScaryPotterLevel())
		return false;

	return true;
}

bool Board::ProgressMeterHasFlags()
{
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
		return false;

	if (mApp->IsWhackAZombieLevel() ||
		mApp->IsFinalBossLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return false;

	return true;
}

void Board::DrawProgressMeter(Graphics* g)
{
	if (!HasProgressMeter())
		return;

	const int aBottomOffset = mApp->mHeight - BOARD_HEIGHT;
	const int aRightOffset = mApp->mWidth - BOARD_WIDTH;
	g->DrawImageCel(Sexy::IMAGE_FLAGMETER, 600 + aRightOffset, 575 + aBottomOffset, 0);
	int aCelWidth = Sexy::IMAGE_FLAGMETER->GetCelWidth();
	int aCelHeight = Sexy::IMAGE_FLAGMETER->GetCelHeight();
	int aClipWidth = PvzpAnimateCurve(0, PROGRESS_METER_COUNTER, mProgressMeterWidth, 0, 143, PvzpCurves::CURVE_LINEAR);
	Rect aSrcRect(aCelWidth - aClipWidth - 7, aCelHeight, aClipWidth, aCelHeight);
	Rect aDstRect(aCelWidth - aClipWidth + 593 + aRightOffset, 575 + aBottomOffset, aClipWidth, aCelHeight);
	g->DrawImage(Sexy::IMAGE_FLAGMETER, aDstRect, aSrcRect);

	// Draw mode-specific text or flags on the meter
	int aPosX = aCelWidth / 2 + 600 + aRightOffset;
	Color aColor(224, 187, 98);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 75, PvzpStringTranslate("[MATCHES]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589 + aBottomOffset, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->IsSquirrelLevel())
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 7, PvzpStringTranslate("[SQUIRRELS]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589 + aBottomOffset, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE)
	{
		int aSunMoney = std::clamp(mSunMoney, 0, 2000);
		std::string aMatchStr = std::format("{}/{} {}", aSunMoney, 2000, PvzpStringTranslate("[SUN]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589 + aBottomOffset, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM)
	{
		int aSunMoney = std::clamp(mSunMoney, 0, 1000);
		std::string aMatchStr = std::format("{}/{} {}", aSunMoney, 1000, PvzpStringTranslate("[SUN]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589 + aBottomOffset, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (mApp->IsIZombieLevel())
	{
		std::string aMatchStr = std::format("{}/{} {}", mChallenge->mChallengeScore, 5, PvzpStringTranslate("[BRAINS]"));
		PvzpDrawString(g, aMatchStr, aPosX, 589 + aBottomOffset, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (ProgressMeterHasFlags())
	{
		int aNumWavesPerFlag = GetNumWavesPerFlag();
		int aNumFlagWaves = mNumWaves / aNumWavesPerFlag;
		int aFlagsPosEnd = 590 + aCelWidth + aRightOffset;
		for (int aFlagWave = 1; aFlagWave <= aNumFlagWaves; aFlagWave++)
		{
			int aHeight = 0;
			int aTotalWavesAtFlag = aFlagWave * aNumWavesPerFlag;
			if (aTotalWavesAtFlag < mCurrentWave)
			{
				aHeight = 14;
			}
			else if (aTotalWavesAtFlag == mCurrentWave)
			{
				aHeight = PvzpAnimateCurve(100, 0, mFlagRaiseCounter, 0, 14, PvzpCurves::CURVE_LINEAR);
			}
			int aPosX = PvzpAnimateCurve(0, mNumWaves, aTotalWavesAtFlag, aFlagsPosEnd, 606 + aRightOffset, PvzpCurves::CURVE_LINEAR);
			g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 571 + aBottomOffset, 1, 0);
			g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 572 - aHeight + aBottomOffset, 2, 0);
		}
	}

	g->DrawImage(Sexy::IMAGE_FLAGMETERLEVELPROGRESS, 638 + aRightOffset, 589 + aBottomOffset);
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM ||
		mApp->IsSquirrelLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsFinalBossLevel())
		return;
	int aHeadProgress = PvzpAnimateCurve(0, 150, mProgressMeterWidth, 0, 135, CURVE_LINEAR);
	g->DrawImageCel(Sexy::IMAGE_FLAGMETERPARTS, aCelWidth - aHeadProgress + 580 + aRightOffset, 572 + aBottomOffset, 0, 0);
}

void Board::DrawHouseDoorBottom(Graphics* g)
{
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:		g->DrawImage(Sexy::IMAGE_BACKGROUND1_GAMEOVER_INTERIOR_OVERLAY, -126, 225);		break;
	case BackgroundType::BACKGROUND_2_NIGHT:	g->DrawImage(Sexy::IMAGE_BACKGROUND2_GAMEOVER_INTERIOR_OVERLAY, -125, 196);		break;
	case BackgroundType::BACKGROUND_3_POOL:		g->DrawImage(Sexy::IMAGE_BACKGROUND3_GAMEOVER_INTERIOR_OVERLAY, -171, 241);		break;
	case BackgroundType::BACKGROUND_4_FOG:		g->DrawImage(Sexy::IMAGE_BACKGROUND4_GAMEOVER_INTERIOR_OVERLAY, -172, 246);		break;
	default:																													break;
	}
}

void Board::DrawHouseDoorTop(Graphics* g)
{
	switch (mBackground)
	{
	case BackgroundType::BACKGROUND_1_DAY:		g->DrawImage(Sexy::IMAGE_BACKGROUND1_GAMEOVER_MASK, -130, 202);		break;
	case BackgroundType::BACKGROUND_2_NIGHT:	g->DrawImage(Sexy::IMAGE_BACKGROUND2_GAMEOVER_MASK, -128, 207);		break;
	case BackgroundType::BACKGROUND_3_POOL:		g->DrawImage(Sexy::IMAGE_BACKGROUND3_GAMEOVER_MASK, -172, 234);		break;
	case BackgroundType::BACKGROUND_4_FOG:		g->DrawImage(Sexy::IMAGE_BACKGROUND4_GAMEOVER_MASK, -173, 133);		break;
	case BackgroundType::BACKGROUND_5_ROOF:		g->DrawImage(Sexy::IMAGE_BACKGROUND5_GAMEOVER_MASK, -220, 81);		break;
	case BackgroundType::BACKGROUND_6_BOSS:		g->DrawImage(Sexy::IMAGE_BACKGROUND6_GAMEOVER_MASK, -220, 81);		break;
	default:																										break;
	}
}

void Board::DrawLevel(Graphics* g)
{
	std::string aLevelStr;
	if (mApp->IsAdventureMode())
	{
		aLevelStr = std::string(PvzpStringTranslate("[LEVEL]")) + " " + mApp->GetStageString(mLevel);
	}
	else
	{
		aLevelStr = mApp->GetCurrentChallengeDef().mChallengeName;
		if (mApp->IsSurvivalMode() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
		{
			int aFlags = GetSurvivalFlagsCompleted();
			if (aFlags > 0)
			{
				std::string aFlagStr = mApp->Pluralize(aFlags, "[ONE_FLAG]", "[COUNT_FLAGS]");
				std::string aCompletedStr = PvzpReplaceString("[FLAGS_COMPLETED]", "{FLAGS}", aFlagStr);
				aLevelStr = std::format("{} - {}", PvzpStringTranslate(aLevelStr), aCompletedStr);
			}
		}
		else if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
		{
			int aStreak = mChallenge->mSurvivalStage;
			if (mNextSurvivalStageCounter > 0)
			{
				aStreak++;
			}
			if (aStreak > 0)
			{
				std::string aStreakStr = PvzpReplaceNumberString("[ENDLESS_STREAK]", "{STREAK}", aStreak);
				aLevelStr = std::format("{} - {}", PvzpStringTranslate(aLevelStr), aStreakStr);
			}
		}
	}

	const int aRightOffset = mApp->mWidth - BOARD_WIDTH;
	int aPosX = 780 + aRightOffset;
	int aPosY = 595 + mApp->mHeight - BOARD_HEIGHT;
	if (HasProgressMeter())
	{
		aPosX = 593 + aRightOffset;
	}
	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
	{
		aPosY += PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 0, 50, PvzpCurves::CURVE_EASE_IN_OUT);
	}
	PvzpDrawString(g, aLevelStr, aPosX, aPosY, Sexy::FONT_HOUSEOFTERROR16, Color(224, 187, 98), DrawStringJustification::DS_ALIGN_RIGHT);
}

void Board::DrawZenWheelBarrowButton(Graphics* g, int theOffsetY)
{
	Rect aButtonRect = GetShovelButtonRect();
	GetZenButtonRect(GameObjectType::OBJECT_TYPE_WHEELBARROW, aButtonRect);
	PottedPlant* aPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
	if (aPlant && mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
	{
		if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
		}
		else
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY + 4);
		}

		if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 23, aButtonRect.mY + theOffsetY - 8, aPlant, 0.6f, true);
		}
		else if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 28, aButtonRect.mY + theOffsetY + 2, aPlant, 0.5f, true);
		}
		else
		{
			mApp->mZenGarden->DrawPottedPlant(g, aButtonRect.mX + 34, aButtonRect.mY + theOffsetY + 12, aPlant, 0.4f, true);
		}
	}
	else
	{
		g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
	}
}

void Board::DrawZenButtons(Graphics* g)
{
	int aOffsetY = 0;
	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
	{
		aOffsetY = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 0, -72, PvzpCurves::CURVE_EASE_IN_OUT);
	}

	for (GameObjectType aTool = GameObjectType::OBJECT_TYPE_WATERING_CAN; aTool <= GameObjectType::OBJECT_TYPE_NEXT_GARDEN; aTool = (GameObjectType)(aTool + 1))
	{
		if (!CanUseGameObject(aTool))
			continue;

		Rect aButtonRect = GetShovelButtonRect();
		if (aTool == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
		{
			aButtonRect.mX = 564;
			if (!mMenuButton->mBtnNoDraw)
			{
				g->DrawImage(Sexy::IMAGE_ZEN_NEXTGARDEN, aButtonRect.mX + 2, aButtonRect.mY + aOffsetY);
			}
		}
		else
		{
			GetZenButtonRect(aTool, aButtonRect);
			g->DrawImage(Sexy::IMAGE_SHOVELBANK, aButtonRect.mX, aButtonRect.mY + aOffsetY);
			if (static_cast<int>(mCursorObject->mCursorType) == static_cast<int>(CursorType::CURSOR_TYPE_WATERING_CAN) + static_cast<int>(aTool) - 6)
			{
				continue;  // skip drawing a tool currently held by the cursor
			}

			switch (aTool)
			{
			case GameObjectType::OBJECT_TYPE_WATERING_CAN:
				if (mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
				{
					g->DrawImage(Sexy::IMAGE_WATERINGCANGOLD, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
				}
				else
				{
					g->DrawImage(Sexy::IMAGE_WATERINGCAN, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
				}
				break;
			case GameObjectType::OBJECT_TYPE_FERTILIZER:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(96, 96, 96));
				}
				else if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS)
				{
					g->SetColorizeImages(true);
					g->SetColor(GetFlashingColor(mMainCounter, 75));
				}
				g->DrawImage(Sexy::IMAGE_FERTILIZER, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_BUG_SPRAY, aButtonRect.mX, aButtonRect.mY + aOffsetY - 1);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
				g->DrawImage(Sexy::IMAGE_PHONOGRAPH, aButtonRect.mX + 2, aButtonRect.mY + aOffsetY + 2);
				break;
			case GameObjectType::OBJECT_TYPE_CHOCOLATE:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_CHOCOLATE, aButtonRect.mX + 6, aButtonRect.mY + aOffsetY + 4);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			case GameObjectType::OBJECT_TYPE_GLOVE:
				if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE &&
					mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
				{
					g->DrawImage(Sexy::IMAGE_ZEN_GARDENGLOVE, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 4);
				}
				break;
			case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
				g->DrawImage(Sexy::IMAGE_ZEN_MONEYSIGN, aButtonRect.mX - 5, aButtonRect.mY + aOffsetY - 4);
				break;
			case GameObjectType::OBJECT_TYPE_WHEELBARROW:
				DrawZenWheelBarrowButton(g, aOffsetY);
				break;
			case GameObjectType::OBJECT_TYPE_TREE_FOOD:
			{
				uint32_t aPurchase = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD];
				int aCharges = aPurchase > PURCHASE_COUNT_OFFSET ? aPurchase - PURCHASE_COUNT_OFFSET : 0;
				if (aCharges == 0)
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				if (!mChallenge->TreeOfWisdomCanFeed())
				{
					g->SetColorizeImages(true);
					g->SetColor(Color(128, 128, 128));
				}
				g->DrawImage(Sexy::IMAGE_TREEFOOD, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
				g->SetColorizeImages(false);

				std::string aChargeString = std::format("x{}", aCharges);
				PvzpDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
				break;
			}
			default:
				break;
			}
		}
	}
}

void Board::DrawShovel(Graphics* g)
{
	if (mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mShowShovel)
		{
			Rect aShovelRect = GetShovelButtonRect();
			g->DrawImage(Sexy::IMAGE_SHOVELBANK, aShovelRect.mX, aShovelRect.mY);

			if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_SHOVEL)
			{
				if (mChallenge->mChallengeState == (ChallengeState)15)
				{
					g->SetColorizeImages(true);
					g->SetColor(GetFlashingColor(mMainCounter, 75));
				}
				g->DrawImage(Sexy::IMAGE_SHOVEL, aShovelRect.mX - 7, aShovelRect.mY - 3);
				g->SetColorizeImages(false);
			}
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawZenButtons(g);
	}
}

void Board::DrawDebugText(Graphics* g)
{
	std::string aText;

	switch (mDebugTextMode)
	{
	case DebugTextMode::DEBUG_TEXT_NONE:
		break;

	case DebugTextMode::DEBUG_TEXT_ZOMBIE_SPAWN:
	{
		int aTime = mZombieCountDownStart - mZombieCountDown;
		float aCountDownFraction = static_cast<float>(aTime) / static_cast<float>(mZombieCountDownStart);

		aText += std::format("ZOMBIE SPAWNING DEBUG\n");
		aText += std::format("CurrentWave: {} of {}\n", mCurrentWave, mNumWaves);
		aText += std::format("TimeSinseLastSpawn: {} {}\n", aTime, aTime > 400 ? "" : "(too soon)");
		aText += std::format("ZombieCountDown: {}/{} ({:.0f}%)\n", mZombieCountDown, mZombieCountDownStart, aCountDownFraction);

		if (mZombieHealthToNextWave != -1)
		{
			int aTotalHealth = TotalZombiesHealthInWave(mCurrentWave - 1);
			int aHealthRange = std::max(mZombieHealthWaveStart - mZombieHealthToNextWave, 1);
			float aHealthFraction = static_cast<float>(mZombieHealthToNextWave - aTotalHealth + aHealthRange) / static_cast<float>(aHealthRange);
			aText += std::format("ZombieHealth: CurZombieHealth {} trigger {} ({:.0f}%)\n", aTotalHealth, mZombieHealthToNextWave, aHealthFraction * 100);
		}
		else
		{
			aText += std::format("ZombieHealth: before first wave\n");
		}

		if (mHugeWaveCountDown > 0)
		{
			aText += std::format("HugeWaveCountDown: {}\n", mHugeWaveCountDown);
		}

		Zombie* aBossZombie = GetBossZombie();
		if (aBossZombie)
		{
			aText += std::format("\nSpawn: {}\n", aBossZombie->mSummonCounter);
			aText += std::format("Stomp: {}\n", aBossZombie->mBossStompCounter);
			aText += std::format("Bungee: {}\n", aBossZombie->mBossBungeeCounter);
			aText += std::format("Head: {}\n", aBossZombie->mBossHeadCounter);
			aText += std::format("Health: {} of {}\n", aBossZombie->mBodyHealth, aBossZombie->mBodyMaxHealth);
		}

		break;
	}

	case DebugTextMode::DEBUG_TEXT_MUSIC:
	{
		aText += std::format("MUSIC DEBUG\n");
		aText += std::format("CurrentWave: {} of {}\n", mCurrentWave, mNumWaves);

		if (mApp->mMusic->mCurMusicFileMain == MusicFile::MUSIC_FILE_NONE)
		{
			aText += std::format("No music");
		}
		else
		{
			aText += std::format("Music Burst: ");

			if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_OFF)
			{
				aText += std::format("Off");
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_STARTING)
			{
				aText += std::format("Starting {}/{}", mApp->mMusic->mBurstStateCounter, 400);
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_ON)
			{
				aText += std::format("On at least until {}/{}", mApp->mMusic->mBurstStateCounter, 800);
			}
			else if (mApp->mMusic->mMusicBurstState == MusicBurstState::MUSIC_BURST_FINISHING)
			{
				aText += std::format("Finishing {}/{}", mApp->mMusic->mBurstStateCounter, 400);
			}

			if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
			{
				aText += std::format(", Drums off");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON_QUEUED)
			{
				aText += std::format(", Drums queued on");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON)
			{
				aText += std::format(", Drums on");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED)
			{
				aText += std::format(", Drums queued off");
			}
			else if (mApp->mMusic->mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_FADING)
			{
				aText += std::format(", Drums fading off {}/{}", mApp->mMusic->mDrumsStateCounter, 50);
			}
			aText += std::format("\n");
		}

		break;
	}

	case DebugTextMode::DEBUG_TEXT_MEMORY:
		aText += std::format("MEMORY DEBUG\n");
		aText += std::format("attachments {}\n", mApp->mEffectSystem->mAttachmentHolder->mAttachments.mSize);
		aText += std::format("emitters {}\n", mApp->mEffectSystem->mParticleHolder->mEmitters.mSize);
		aText += std::format("particles {}\n", mApp->mEffectSystem->mParticleHolder->mParticles.mSize);
		aText += std::format("particle systems {}\n", mApp->mEffectSystem->mParticleHolder->mParticleSystems.mSize);
		aText += std::format("trails {}\n", mApp->mEffectSystem->mTrailHolder->mTrails.mSize);
		aText += std::format("reanimation {}\n", mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize);
		aText += std::format("zombies {}\n", mZombies.mSize);
		aText += std::format("plants {}\n", mPlants.mSize);
		aText += std::format("projectiles {}\n", mProjectiles.mSize);
		aText += std::format("coins {}\n", mCoins.mSize);
		aText += std::format("lawn mowers {}\n", mLawnMowers.mSize);
		aText += std::format("grid items {}\n", mGridItems.mSize);
		break;

	case DebugTextMode::DEBUG_TEXT_COLLISION:
		aText += std::format("COLLISION DEBUG\n");
		break;

	default:
		PVZP_ASSERT(false);
		break;
	}

	g->SetFont(FONT_PICO129);
	if (!aText.empty())
	{
		g->SetColor(Color::Black);
		g->DrawStringWordWrapped(aText, 10, 89);
		g->DrawStringWordWrapped(aText, 11, 91);
		g->DrawStringWordWrapped(aText, 9, 90);
		g->DrawStringWordWrapped(aText, 11, 90);
		g->SetColor(Color(255, 255, 255));
		g->DrawStringWordWrapped(aText, 10, 90);
	}
}

void Board::DrawDebugObjectRects(Graphics* g)
{
	if (mDebugTextMode != DebugTextMode::DEBUG_TEXT_COLLISION)
		return;

	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
				continue;
			Rect aRect = aPlant->GetPlantRect();
			g->SetColor(Color(0, 255, 0));
			g->DrawRect(aRect);

			Rect aAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
			if (aAttackRect.mWidth < mApp->mWidth)
			{
				g->SetColor(Color(255, 0, 0));
				g->DrawRect(aAttackRect);
			}

			Rect aSecondaryRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_SECONDARY);
			if (aSecondaryRect.mWidth < mApp->mWidth)
			{
				g->SetColor(Color(255, 0, 128));
				g->DrawRect(aSecondaryRect);
			}
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
				continue;
			if (!aZombie->IsDeadOrDying())
			{
				Rect aRect = aZombie->GetZombieRect();
				g->SetColor(Color(0, 255, 0));
				g->DrawRect(aRect);

				Rect aAttackRect = aZombie->GetZombieAttackRect();
				g->SetColor(Color(255, 0, 0));
				g->DrawRect(aAttackRect);
			}
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
				continue;
			Rect aAttackRect = aLawnMower->GetLawnMowerAttackRect();
			g->SetColor(Color(255, 0, 0));
			g->DrawRect(aAttackRect);
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
				continue;
			g->SetColor(Color(255, 0, 0));
			Rect aDamageRect = aProjectile->GetProjectileRect();
			g->DrawRect(aDamageRect);
		}
	}
}

void Board::DrawFadeOut(Graphics* g)
{
	if (mBoardFadeOutCounter < 0 || IsSurvivalStageWithRepick())
		return;

	int anAlpha = PvzpAnimateCurve(200, 0, mBoardFadeOutCounter, 0, 255, PvzpCurves::CURVE_LINEAR);
	if (mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49)
	{
		g->SetColor(Color(0, 0, 0, anAlpha));
	}
	else
	{
		g->SetColor(Color(255, 255, 255, anAlpha));
	}
	g->FillRect(0, 0, mWidth, mHeight);
}

void Board::DrawTopRightUI(Graphics* g)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		if (mChallenge->mChallengeState == STATECHALLENGE_ZEN_FADING)
		{
			mMenuButton->mY = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, -10, -50, PvzpCurves::CURVE_EASE_IN_OUT);
			mStoreButton->mX = PvzpAnimateCurve(50, 0, mChallenge->mChallengeStateCounter, 678, 800, PvzpCurves::CURVE_EASE_IN_OUT);
		}
		else
		{
			mMenuButton->mY = -10;
			mStoreButton->mX = 678;
		}
	}

	if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED)
	{
		g->SetColorizeImages(true);
		g->SetColor(GetFlashingColor(mMainCounter, 75));
	}
	mMenuButton->Draw(g);
	g->SetColorizeImages(false);

	if (mStoreButton && mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE)
		{
			g->SetColorizeImages(true);
			g->SetColor(GetFlashingColor(mMainCounter, 75));
		}
		mStoreButton->Draw(g);
		g->SetColorizeImages(false);
	}
}

void Board::DrawUIBottom(Graphics* g)
{
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		int aWaveTime = std::abs(static_cast<int>((mMainCounter / 8) % 22) - 11);
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImageCel(Sexy::IMAGE_WAVESIDE, 0, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 160, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 320, 40, aWaveTime);
		g->DrawImageCel(Sexy::IMAGE_WAVECENTER, 480, 40, aWaveTime);
		//PvzpDrawImageCelScaled(g, Sexy::IMAGE_WAVESIDE, 800, 40, 0, aWaveTime, -1.0f, 1.0f);
		PvzpDrawImageCelScaled(
			g, Sexy::IMAGE_WAVESIDE, 800, 40, aWaveTime % Sexy::IMAGE_WAVESIDE->mNumCols,
			aWaveTime / Sexy::IMAGE_WAVESIDE->mNumCols, -1.0f, 1.0f
		);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE || mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		g->DrawImage(
			IMAGE_BACKGROUND_GREENHOUSE_OVERLAY,
			Rect(0, 0, BOARD_WIDTH, BOARD_HEIGHT),
			Rect(0, 0, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mWidth, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mHeight)
		);
		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
	}

	if (mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		if (mSeedBank->BeginDraw(g))
		{
			mSeedBank->Draw(g);
			mSeedBank->EndDraw(g);
		}

		if (mAdvice->mMessageStyle == MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
		{
			mAdvice->Draw(g);
		}
	}

	DrawShovel(g);
	if (!StageHasFog())
	{
		DrawTopRightUI(g);
	}
}

void Board::DrawUICoinBank(Graphics* g)
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF)
		return;

	if (mCoinBankFadeCount <= 0)
		return;

	int aPosX = 57;
	int aPosY = mApp->mHeight - 1 - Sexy::IMAGE_COINBANK->GetHeight();
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
	{
		aPosX = 450 - mX;
	}

	g->SetColorizeImages(true);
	int anAlpha = std::clamp(255 * mCoinBankFadeCount / 15, 0, 255);
	g->SetColor(Color(255, 255, 255, anAlpha));
	g->DrawImage(Sexy::IMAGE_COINBANK, aPosX, aPosY);

	g->SetColor(Color(180, 255, 90, anAlpha));
	g->SetFont(Sexy::FONT_CONTINUUMBOLD14);
	std::string aCoinLabel = mApp->GetMoneyString(mApp->mPlayerInfo->mCoins);
	g->DrawString(aCoinLabel, aPosX + 116 - Sexy::FONT_CONTINUUMBOLD14->StringWidth(aCoinLabel), aPosY + 24);
	g->SetColorizeImages(false);
}

void Board::DrawFog(Graphics* g)
{
	bool aIs3DAccelerated = mApp->Is3DAccelerated();
	Image* aImageFog = aIs3DAccelerated ? Sexy::IMAGE_FOG : Sexy::IMAGE_FOG_SOFTWARE;
	// the fog animation uses 900- and 500-frame periods; mod by their lcm (4500) to avoid float precision loss on large counters
	constexpr uint32_t FOG_ANIM_PERIOD = 4500;
	float aTime = static_cast<float>(mMainCounter % FOG_ANIM_PERIOD) * PI * 2;
	g->SetColorizeImages(true);
	for (int x = 0; x < GetNumPlayableColumns(); x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
		{
			int aFadeAmount = mGridCelFog[x][y];
			if (aFadeAmount == 0)
				continue;

			// fog shape of the cell; the extra row 6 reuses row 0's shape
			int aCelLook = mGridCelLook[x][y % MAX_GRID_SIZE_Y];
			int aCelCol = aCelLook % 8;
			float aPosX = x * 80 + mFogOffset - 15;
			float aPosY = GetGridRowY(std::min(y, MAX_GRID_SIZE_Y - 1)) + 20;
			float aPhaseX = 6 * PI * x / GetNumPlayableColumns();
			float aPhaseY = 6 * PI * y / (MAX_GRID_SIZE_Y + 1);
			float aMotion = 13 + 4 * sin(aTime / 900 + aPhaseY) + 8 * sin(aTime / 500 + aPhaseX);

			int aColorVariant = 255 - aCelLook * 1.5 - aMotion * 1.5;
			int aLightnessVariant = 255 - aCelLook - aMotion;
			if (!aIs3DAccelerated)
			{
				aPosX += 10;
				aPosY += 3;
				aCelCol = aCelLook % Sexy::IMAGE_FOG_SOFTWARE->mNumCols;
				aColorVariant = 255;
				aLightnessVariant = 255;
			}

			g->SetColor(Color(aColorVariant, aColorVariant, aLightnessVariant, aFadeAmount));
			g->DrawImageCel(aImageFog, aPosX, aPosY, aCelCol, 0);

			if (x == GetNumPlayableColumns() - 1)
			{
				g->DrawImageCel(aImageFog, aPosX + 80, aPosY, aCelCol, 0);
			}
		}
	}
	g->SetColorizeImages(false);
}

bool Board::IsScaryPotterDaveTalking()
{
	return mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter > 0 && mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF;
}

void Board::DrawUITop(Graphics* g)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_PLANT_PRACTICE && mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		g->SetColor(Color(80, 110, 48, 230));
		g->FillRect(mApp->mWidth - 119, 40, 117, 30);
		PvzpDrawString(g, "Spawn Zombie", mApp->mWidth - 60, 60, Sexy::FONT_DWARVENTODCRAFT12, Color::White, DrawStringJustification::DS_ALIGN_CENTER);
		if (mPracticeZombiePickerOpen)
		{
			g->SetColor(Color(24, 35, 28, 235));
			g->FillRect(35, 70, 730, 390);
			PvzpDrawString(g, "Choose a zombie to spawn in the middle lane", 400, 94, Sexy::FONT_HOUSEOFTERROR16, Color::White, DrawStringJustification::DS_ALIGN_CENTER);
			int aVisibleIndex = 0;
			for (ZombieType aType = ZombieType::ZOMBIE_NORMAL; aType < ZombieType::NUM_ZOMBIE_TYPES;
				aType = static_cast<ZombieType>(static_cast<int>(aType) + 1))
			{
				if (aType == ZombieType::ZOMBIE_BOSS || (aType >= ZombieType::ZOMBIE_PEA_HEAD && aType <= ZombieType::ZOMBIE_TALLNUT_HEAD) ||
					IsZombieTypePoolOnly(aType) || GetZombieDefinition(aType).mPickWeight <= 0)
					continue;
				int aColumn = aVisibleIndex % 5;
				int aRow = aVisibleIndex / 5;
				int aX = 50 + aColumn * 140;
				int aY = 105 + aRow * 36;
				g->SetColor(Color(66, 80, 54, 255));
				g->FillRect(aX, aY, 132, 30);
				PvzpDrawString(g, GetZombieDefinition(aType).mZombieName, aX + 66, aY + 20, Sexy::FONT_DWARVENTODCRAFT12, Color::White, DrawStringJustification::DS_ALIGN_CENTER);
				aVisibleIndex++;
			}
			PvzpDrawString(g, "Click outside to close", 400, 450, Sexy::FONT_DWARVENTODCRAFT12, Color(220, 220, 180), DrawStringJustification::DS_ALIGN_CENTER);
		}
	}

	if (StageHasFog())
	{
		DrawTopRightUI(g);
	}

	if (mTimeStopCounter > 0)
	{
		g->SetColor(Color(200, 200, 200, 210));
		g->FillRect(0, 0, mApp->mWidth, mApp->mHeight);
	}

	if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawProgressMeter(g);
		DrawLevel(g);
	}
	if (mStoreButton && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		mStoreButton->Draw(g);
	}

	if ((mApp->mGameMode == GameMode::GAMEMODE_UPSELL || mApp->mGameMode == GameMode::GAMEMODE_INTRO) && mCutScene->mUpsellHideBoard)
	{
		g->SetColor(Color(0, 0, 0));
		g->FillRect(0, 0, mApp->mWidth, mApp->mHeight);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		mCutScene->DrawUpsell(g);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		mCutScene->DrawIntro(g);
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		IsScaryPotterDaveTalking())
	{
		Graphics aScreenSpace(*g);
		aScreenSpace.mTransX -= mX;
		aScreenSpace.mTransY -= mY;
		mApp->DrawCrazyDave(&aScreenSpace);
	}

	if (mAdvice->mMessageStyle != MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
	{
		mAdvice->Draw(g);
	}

	if (mTimeStopCounter == 0 && mCursorObject->BeginDraw(g))
	{
		mCursorObject->Draw(g);
		mCursorObject->EndDraw(g);
	}

	mToolTip->Draw(g);
	DrawDebugText(g);
	DrawDebugObjectRects(g);
}

void Board::Draw(Graphics* g)
{
	Sexy::FrameProfileScope aProfileScope(Sexy::FrameProfileMetric::BOARD_DRAW);
	if (mApp->GetDialog(Dialogs::DIALOG_STORE) || mApp->GetDialog(Dialogs::DIALOG_ALMANAC))
		return;

	g->SetLinearBlend(true);

	if (mDrawCount && mCutScene->mPreloaded)
	{
		int64_t aTickCount = SDL_GetTicks();
		int64_t aIntervalDraws = mDrawCount - mIntervalDrawCountStart;
		int64_t aInterval = aTickCount - mIntervalDrawTime;
		if (aInterval > 10000)
		{
			float aIntervalFPS = (aIntervalDraws * 1000 + 500) / aInterval;
			if (mMinFPS > aIntervalFPS)
			{
				mMinFPS = aIntervalFPS;
			}
			mIntervalDrawCountStart = mDrawCount;
			mIntervalDrawTime = aTickCount;
		}
	}
	else
	{
		ResetFPSStats();
	}

	mDrawCount++;
	DrawGameObjects(g);
	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead || aPlant->mSeedType != SeedType::SEED_PLANTERN || aPlant->mTargetX < 0 ||
			!aPlant->HasPlanternTarget())
			continue;
		g->DrawImageCel(IMAGE_COBCANNON_TARGET, aPlant->mTargetX - 11 + mX, aPlant->mTargetY + 7 + mY, 0);
	}
}
