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

#include <memory>
#include <cmath>
#include "PoolEffect.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../GameConstants.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "graphics/GLImage.h"
#include "graphics/Graphics.h"
#include "graphics/GLInterface.h"

constexpr const int CAUSTIC_IMAGE_WIDTH = 128;
constexpr const int CAUSTIC_IMAGE_HEIGHT = 64;

//effect documentation by @windowslover1234

void PoolEffect::PoolEffectInitialize()
{
	//load pool caustics into memory
	PvzpHesitationBracket aHesitation("PoolEffectInitialize");

	mApp = gLawnApp;
	mPoolCounter = 0;

	mCausticImage = std::make_unique<MemoryImage>(gSexyAppBase);
	mCausticImage->mWidth = CAUSTIC_IMAGE_WIDTH;
	mCausticImage->mHeight = CAUSTIC_IMAGE_HEIGHT;
	mCausticImage->mBits = std::make_unique<uint32_t[]>(CAUSTIC_IMAGE_WIDTH * CAUSTIC_IMAGE_HEIGHT + 1);
	mCausticImage->mHasTrans = true;
	mCausticImage->mHasAlpha = true;
	mCausticImage->mRenderFlags |= RenderImageFlag_Repeat;
	memset(mCausticImage->mBits.get(), 0xFF, CAUSTIC_IMAGE_WIDTH * CAUSTIC_IMAGE_HEIGHT * 4); //4
	mCausticImage->mBits[CAUSTIC_IMAGE_WIDTH * CAUSTIC_IMAGE_HEIGHT] = MEMORYCHECK_ID;

	mCausticGrayscaleImage.resize(256 * 256);
	MemoryImage* aCausticGrayscaleImage = reinterpret_cast<MemoryImage*>(IMAGE_POOL_CAUSTIC_EFFECT);
	uint32_t* aCausticBits = aCausticGrayscaleImage->GetBits();
	int index = 0;
	for (int x = 0; x < 256; x++)
	{
		for (int y = 0; y < 256; y++)
		{
			mCausticGrayscaleImage[index] = static_cast<unsigned char>(aCausticBits[index]);
			index++;
		}
	}
}

unsigned int PoolEffect::BilinearLookupFixedPoint(unsigned int u, unsigned int v)
{
	unsigned int timeU = u & 0xFFFF0000;
	unsigned int timeV = v & 0xFFFF0000;
	unsigned int factorU1 = ((u - timeU) & 0x0000FFFE) + 1;
	unsigned int factorV1 = ((v - timeV) & 0x0000FFFE) + 1;
	unsigned int factorU0 = 65536 - factorU1;
	unsigned int factorV0 = 65536 - factorV1;
	unsigned int indexU0 = (timeU >> 16) % 256;
	unsigned int indexU1 = ((timeU >> 16) + 1) % 256;
	unsigned int indexV0 = (timeV >> 16) % 256;
	unsigned int indexV1 = ((timeV >> 16) + 1) % 256;

	return
		((((factorU0 * factorV1) / 65536) * mCausticGrayscaleImage[indexV1 * 256 + indexU0]) / 65536) +
		((((factorU1 * factorV1) / 65536) * mCausticGrayscaleImage[indexV1 * 256 + indexU1]) / 65536) +
		((((factorU0 * factorV0) / 65536) * mCausticGrayscaleImage[indexV0 * 256 + indexU0]) / 65536) +
		((((factorU1 * factorV0) / 65536) * mCausticGrayscaleImage[indexV0 * 256 + indexU1]) / 65536);
}

void PoolEffect::UpdateWaterEffect()
{
	int idx = 0;
	for (int y = 0; y < CAUSTIC_IMAGE_HEIGHT; y++)
	{
		unsigned int timeV1 = (256 - y) << 17;
		unsigned int timeV0 = y << 17;

		for (int x = 0; x < CAUSTIC_IMAGE_WIDTH; x++)
		{
			uint32_t* pix = &mCausticImage->mBits[idx];

			unsigned int timeU = x << 17;
			unsigned int timePool0 = mPoolCounter << 16;
			unsigned int timePool1 = ((mPoolCounter & 65535u) + 1u) << 16;
			int a1 = static_cast<unsigned char>(BilinearLookupFixedPoint(timeU - timePool1 / 6, timeV1 + timePool0 / 8)); //scroll speed
			int a0 = static_cast<unsigned char>(BilinearLookupFixedPoint(timeU + timePool0 / 10, timeV0)); //scroll speed
			unsigned char a = static_cast<unsigned char>((a0 + a1) / 2);

			unsigned char alpha;
			if (a >= 160U)
			{
				alpha = 255 - 2 * (a - 160U);
			}
			else if (a >= 128U)
			{
				alpha = 5 * (a - 128U);
			}
			else
			{
				alpha = 0;
			}

			*pix = (*pix & 0x00FFFFFF) + ((static_cast<int>(alpha) / 3) << 24); //alpha of caustic effect
			idx++;
		}
	}

	++mCausticImage->mBitsChangedCount;
}

void PoolEffect::PoolEffectDraw(Sexy::Graphics* g, bool theIsNight, int theFirstPoolRow, int thePoolRowCount, int theRowSpacing)
{
	PVZP_ASSERT(thePoolRowCount == 2 || thePoolRowCount == 4);
	const int aVerticalCellCount = 5;
	constexpr int aClassicHorizontalCellCount = 15;
	constexpr int aWideHorizontalCellCount = 20;
	// Four gameplay lanes use the same original-height pool art so the basin's top and bottom stone edges stay intact.
	const bool aFullHeightPool = thePoolRowCount == 4 && mApp->mHeight > BOARD_HEIGHT;
	const bool aWidePool = aFullHeightPool && mApp->mWidth > BOARD_WIDTH;
	const int aHorizontalCellCount = aWidePool ? aWideHorizontalCellCount : aClassicHorizontalCellCount;
	const int aPoolTop = LAWN_YMIN + theFirstPoolRow * theRowSpacing + theRowSpacing / 3 + 1;
	const int aPoolHeight = aFullHeightPool ? thePoolRowCount * theRowSpacing : IMAGE_POOL->GetHeight();
	const int aPoolWidth = aWidePool ? mApp->mWidth - 96 : IMAGE_POOL->GetWidth();
	// The source water texture includes a dark pool edge on both sides. Skip that edge
	// when repeating it across the wider basin so the original right edge is not drawn
	// as an interior divider.
	const float aWideTextureInset = aWidePool ? 0.25f / aClassicHorizontalCellCount : 0.0f;
	const float aWideTextureRange = 1.0f - 2.0f * aWideTextureInset;
	if (!mApp->Is3DAccelerated())
	{
		//skip if using software rendering, never true in this port
		Image* aPoolImage = theIsNight ? IMAGE_POOL_NIGHT : IMAGE_POOL;
		if (aWidePool)
		{
			for (int aOffsetX = 0; aOffsetX < aPoolWidth;)
			{
				int aTileWidth = std::min(aPoolImage->GetWidth(), aPoolWidth - aOffsetX);
				g->DrawImage(aPoolImage,
					Rect(34 + aOffsetX, aPoolTop, aTileWidth, aPoolHeight),
					Rect(0, 0, aTileWidth, aPoolImage->GetHeight()));
				aOffsetX += aTileWidth;
			}
			return;
		}
		if (theIsNight)
		{
			g->DrawImage(IMAGE_POOL_NIGHT, Rect(34, aPoolTop, IMAGE_POOL_NIGHT->GetWidth(), aPoolHeight),
				Rect(0, 0, IMAGE_POOL_NIGHT->GetWidth(), IMAGE_POOL_NIGHT->GetHeight()));
		}
		else
		{
			g->DrawImage(IMAGE_POOL, Rect(34, aPoolTop, IMAGE_POOL->GetWidth(), aPoolHeight),
				Rect(0, 0, IMAGE_POOL->GetWidth(), IMAGE_POOL->GetHeight()));
		}
		return;
	}
	//pool background
	float aGridSquareX = static_cast<float>(aPoolWidth) / aHorizontalCellCount;
	float aGridSquareY = static_cast<float>(aPoolHeight) / aVerticalCellCount;
	auto PoolXAtIndex = [&](int theIndex)
	{
		if (!aWidePool)
			return static_cast<float>(theIndex) * aGridSquareX;

		const float aOriginalWaterWidth = static_cast<float>(IMAGE_POOL->GetWidth());
		if (theIndex <= aClassicHorizontalCellCount)
			return theIndex * aOriginalWaterWidth / aClassicHorizontalCellCount;

		const float aExtendedCellWidth = static_cast<float>(aPoolWidth - IMAGE_POOL->GetWidth()) /
			(aHorizontalCellCount - aClassicHorizontalCellCount);
		return aOriginalWaterWidth + (theIndex - aClassicHorizontalCellCount) * aExtendedCellWidth;
	};
	float aOffsetArray[3][aWideHorizontalCellCount + 1][11][2] = {{{{ 0 }}}};
	for (int x = 0; x <= aHorizontalCellCount; x++)
	{
		for (int y = 0; y <= aVerticalCellCount; y++) //handles the caustic effect
		{
			aOffsetArray[2][x][y][0] = 0.0f;
			aOffsetArray[2][x][y][1] = static_cast<float>(y) / aVerticalCellCount;
			if (x != 0 && x != aHorizontalCellCount && y != 0 && y != aVerticalCellCount)
			{
				constexpr unsigned int POOL_PHASE_PERIOD = 316800u; // LCM of all sin wave effective periods (1600, 300, 1800, 220, 3200/3, 200, 720, 640, 88)
				float aPoolPhase = (mPoolCounter % POOL_PHASE_PERIOD) * PI; //speed, * 2 is default
				float aWaveTime1 = aPoolPhase / 800.0;
				float aWaveTime2 = aPoolPhase / 150.0;
				float aWaveTime3 = aPoolPhase / 900.0;
				float aWaveTime4 = aPoolPhase / 800.0;
				float aWaveTime5 = aPoolPhase / 110.0;
				float xPhase = x * 3.0f * 2 * PI / aHorizontalCellCount;
				float yPhase = y * 3.0f * 2 * PI / aVerticalCellCount; //more speed options
				//verticies for rendering, dividing by 1 gives interesting results
				aOffsetArray[0][x][y][0] = sin(yPhase + aWaveTime2) * 0.002f + sin(yPhase + aWaveTime1) * 0.005f;
				aOffsetArray[0][x][y][1] = sin(xPhase + aWaveTime5) * 0.01f + sin(xPhase + aWaveTime3) * 0.015f + sin(xPhase + aWaveTime4) * 0.005f;
				aOffsetArray[1][x][y][0] = sin(yPhase * 0.2f + aWaveTime2) * 0.015f + sin(yPhase * 0.2f + aWaveTime1) * 0.012f;
				aOffsetArray[1][x][y][1] = sin(xPhase * 0.2f + aWaveTime5) * 0.005f + sin(xPhase * 0.2f + aWaveTime3) * 0.015f + sin(xPhase * 0.2f + aWaveTime4) * 0.02f;
				aOffsetArray[2][x][y][0] += sin(yPhase + aWaveTime1 * 1.5f) * 0.004f + sin(yPhase + aWaveTime2 * 1.5f) * 0.005f;
				aOffsetArray[2][x][y][1] += sin(xPhase * 4.0f + aWaveTime5 * 2.5f) * 0.005f + sin(xPhase * 2.0f + aWaveTime3 * 2.5f) * 0.04f + sin(xPhase * 3.0f + aWaveTime4 * 2.5f) * 0.02f;
			}
			else
			{
				//skip animation
				aOffsetArray[0][x][y][0] = 0.0f;
				aOffsetArray[0][x][y][1] = 0.0f;
				aOffsetArray[1][x][y][0] = 0.0f;
				aOffsetArray[1][x][y][1] = 0.0f;
			}
		}
	}

	int aIndexOffsetX[6] = { 0, 0, 1, 0, 1, 1 };
	int aIndexOffsetY[6] = { 0, 1, 1, 0, 1, 0 };
	TriVertex aVertArray[3][aWideHorizontalCellCount * aVerticalCellCount * 2][3];
	auto aLaneToneAtY = [&](float theY)
	{
		const float aRowAnchor = static_cast<float>(LAWN_YMIN + theFirstPoolRow * theRowSpacing);
		const float aRowPhase = (theY - aRowAnchor) * (2.0f * PI / theRowSpacing);
		return 0.5f + 0.5f * std::cos(aRowPhase);
	};

	for (int x = 0; x < aHorizontalCellCount; x++)
	{
		for (int y = 0; y < aVerticalCellCount; y++)
		{
			for (int aLayer = 0; aLayer < 3; aLayer++)
			{
				TriVertex* pVert = &aVertArray[aLayer][x * aVerticalCellCount * 2 + y * 2][0];
				for (int aVertIndex = 0; aVertIndex < 6; aVertIndex++, pVert++)
				{
					int aIndexX = x + aIndexOffsetX[aVertIndex];
					int aIndexY = y + aIndexOffsetY[aVertIndex];
					int aTextureCellX = aIndexX - (x / aClassicHorizontalCellCount) * aClassicHorizontalCellCount;
					if (aLayer == 2) //caustic effect
					{
						pVert->x = PoolXAtIndex(aIndexX) + 45.0f;
						pVert->y = aGridSquareY * aIndexY + aPoolTop + 9.0f; //caustics sit just inside the pool border
						pVert->u = aOffsetArray[2][aIndexX][aIndexY][0] +
							2.0f * static_cast<float>(aTextureCellX) / aClassicHorizontalCellCount;
						pVert->v = aOffsetArray[2][aIndexX][aIndexY][1] + static_cast<float>(aIndexY) / aVerticalCellCount;
						//use correct colors depending on the scene
						if (!g->mClipRect.Contains(pVert->x, pVert->y))
						{
							pVert->color = 0x00FFFFFFUL;
						}
						else if (aIndexX == 0 || aIndexX == aHorizontalCellCount || aIndexY == 0)
						{
							pVert->color = 0x20FFFFFFUL;
						}
						else if (theIsNight)
						{
							pVert->color = 0x30FFFFFFUL;
						}
						else
						{
							pVert->color = aIndexX <= aHorizontalCellCount / 2 ? 0xC0FFFFFFUL : 0x80FFFFFFUL;
						}
					}
					else
					{
						//update water outlines
						pVert->color = 0xFFFFFFFFUL;
						pVert->x = PoolXAtIndex(aIndexX) + 35.0f;
						pVert->y = aIndexY * aGridSquareY + aPoolTop;
						const float aTextureU = aWideTextureInset + aWideTextureRange *
							static_cast<float>(aTextureCellX) / aClassicHorizontalCellCount +
							aOffsetArray[aLayer][aIndexX][aIndexY][0];
						pVert->u = aWidePool
							? std::clamp(aTextureU, aWideTextureInset, 1.0f - aWideTextureInset)
							: aTextureU;
						pVert->v = aOffsetArray[aLayer][aIndexX][aIndexY][1] + static_cast<float>(aIndexY) / aVerticalCellCount;
						if (!g->mClipRect.Contains(pVert->x, pVert->y))
						{
							pVert->color = 0x00FFFFFFUL;
						}
					}
					if (aFullHeightPool)
					{
						const float aLaneTone = aLaneToneAtY(pVert->y);
						if (aLayer == 2)
						{
							const uint32_t aAlpha = (pVert->color >> 24) & 0xFF;
							const uint32_t aShadedAlpha = static_cast<uint32_t>(aAlpha * (0.96f + 0.04f * aLaneTone) + 0.5f);
							pVert->color = (pVert->color & 0x00FFFFFFUL) | (aShadedAlpha << 24);
						}
						else
						{
							const uint32_t aAlpha = (pVert->color >> 24) & 0xFF;
							const uint32_t aRed = 247 + static_cast<uint32_t>(7 * aLaneTone);
							const uint32_t aGreen = 251 + static_cast<uint32_t>(4 * aLaneTone);
							pVert->color = (aAlpha << 24) | (aRed << 16) | (aGreen << 8) | 0xFF;
						}
					}
				}
			}
		}
	}
	//draw correct shading type depending on area.
	if (theIsNight)
	{
		g->DrawTrianglesTex(IMAGE_POOL_BASE_NIGHT, aVertArray[0], aHorizontalCellCount * aVerticalCellCount * 2);
		g->DrawTrianglesTex(IMAGE_POOL_SHADING_NIGHT, aVertArray[1], aHorizontalCellCount * aVerticalCellCount * 2);
	}
	else
	{
		g->DrawTrianglesTex(IMAGE_POOL_BASE, aVertArray[0], aHorizontalCellCount * aVerticalCellCount * 2);
		g->DrawTrianglesTex(IMAGE_POOL_SHADING, aVertArray[1], aHorizontalCellCount * aVerticalCellCount * 2);
	}
	//update positions
	UpdateWaterEffect();
	//send something to OpenGL
	GLInterface* anInterface = ((GLImage*)g->mDestImage)->mGLInterface;

	//Send caustic effect tris to OpenGL (tex, verts, tris)
	g->DrawTrianglesTex(mCausticImage.get(), aVertArray[2], aHorizontalCellCount * aVerticalCellCount * 2);
}

void PoolEffect::PoolEffectUpdate()
{
	++mPoolCounter;
}
