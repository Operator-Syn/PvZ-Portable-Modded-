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

#include "Board.h"
#include "Plant.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "Projectile.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "../GameConstants.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/Attachment.h"
#include "Widget/AchievementsScreen.h"
#include <algorithm>
#include <array>
#include <cstdint>




#include "TargetingRules.h"
#include "ProjectileRules.h"

void Projectile::Draw(Graphics* g)
{
	const ProjectileDefinition& aProjectileDef = GetProjectileDef();

	Image* aImage = nullptr;
	float aScale = 1.0f;
	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_COBBIG:
		aImage = IMAGE_REANIM_COBCANNON_COB;
		aScale = 0.9f;
		break;
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		aImage = IMAGE_PROJECTILEPEA;
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
		aImage = IMAGE_PROJECTILESNOWPEA;
		break;
	case ProjectileType::PROJECTILE_FIREBALL:
		aImage = nullptr;
		break;
	case ProjectileType::PROJECTILE_SPIKE:
		aImage = IMAGE_PROJECTILECACTUS;
		break;
	case ProjectileType::PROJECTILE_STAR:
		aImage = IMAGE_PROJECTILE_STAR;
		break;
	case ProjectileType::PROJECTILE_PUFF:
		aImage = IMAGE_PUFFSHROOM_PUFF1;
		aScale = PvzpAnimateCurveFloat(0, 30, mProjectileAge, 0.3f, 1.0f, PvzpCurves::CURVE_LINEAR);
		break;
	case ProjectileType::PROJECTILE_BASKETBALL:
		aImage = IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL;
		aScale = 1.1f;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
		aImage = IMAGE_REANIM_CABBAGEPULT_CABBAGE;
		aScale = 1.0f;
		break;
	case ProjectileType::PROJECTILE_KERNEL:
		aImage = IMAGE_REANIM_CORNPULT_KERNAL;
		aScale = 0.95f;
		break;
	case ProjectileType::PROJECTILE_BUTTER:
		aImage = IMAGE_REANIM_CORNPULT_BUTTER;
		aScale = 0.8f;
		break;
	case ProjectileType::PROJECTILE_MELON:
		aImage = IMAGE_REANIM_MELONPULT_MELON;
		aScale = 1.0f;
		break;
	case ProjectileType::PROJECTILE_WINTERMELON:
		aImage = IMAGE_REANIM_WINTERMELON_PROJECTILE;
		aScale = 1.0f;
		break;
	case ProjectileType::PROJECTILE_CHERRYBOMB:
		if (mGatlingCherryShot)
		{
			aImage = IMAGE_PROJECTILEPEA;
			break;
		}
		[[fallthrough]];
	case ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB:
	case ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB:
		aImage = nullptr;
		break;
	default:
		PVZP_ASSERT(false);
		break;
	}

	bool aMirror = false;
	if (mMotionType == ProjectileMotion::MOTION_BEE_BACKWARDS)
	{
		aMirror = true;
	}

	if (mGatlingCherryShot && mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
	{
		Graphics theParticleGraphics(*g);
		MakeParentGraphicsFrame(&theParticleGraphics);
		AttachmentDraw(mAttachmentID, &theParticleGraphics, false);
	}

	if (aImage)
	{
		GraphicsStateGuard aStateGuard(*g);
		if (mGatlingCherryShot)
		{
			g->SetColorizeImages(true);
			g->SetColor(Color(28, 22, 38));
		}
		PVZP_ASSERT(aProjectileDef.mImageRow < aImage->mNumRows);
		PVZP_ASSERT(mFrame < aImage->mNumCols);

		int aCelWidth = aImage->GetCelWidth();
		int aCelHeight = aImage->GetCelHeight();
		Rect aSrcRect(aCelWidth * mFrame, aCelHeight * aProjectileDef.mImageRow, aCelWidth, aCelHeight);
		if (FloatApproxEqual(mRotation, 0.0f) && FloatApproxEqual(aScale, 1.0f))
		{
			Rect aDestRect(0, 0, aCelWidth, aCelHeight);
			g->DrawImageMirror(aImage, aDestRect, aSrcRect, aMirror);
		}
		else
		{
			float aOffsetX = mPosX + aCelWidth * 0.5f;
			float aOffsetY = mPosZ + mPosY + aCelHeight * 0.5f;
			SexyTransform2D aTransform;
			PvzpScaleRotateTransformMatrix(aTransform, aOffsetX + mBoard->mX, aOffsetY + mBoard->mY, mRotation, aScale, aScale);
			PvzpBltMatrix(g, aImage, aTransform, g->mClipRect,
				mGatlingCherryShot ? Color(28, 22, 38) : Color::White, g->mDrawMode, aSrcRect);
		}
	}

	if (!mGatlingCherryShot && mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
	{
		Graphics theParticleGraphics(*g);
		MakeParentGraphicsFrame(&theParticleGraphics);
		AttachmentDraw(mAttachmentID, &theParticleGraphics, false);
	}
}

void Projectile::DrawShadow(Graphics* g)
{
	int aCelCol = 0;
	float aScale = 1.0f;
	float aStretch = 1.0f;
	float aOffsetX = mPosX - mX;
	float aOffsetY = mPosY - mY;

	int aGridX = mBoard->PixelToGridXKeepOnBoard(mX, mY);
	bool isHighGround = false;
	if (mBoard->mGridSquareType[aGridX][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		isHighGround = true;
	}
	if (mOnHighGround && !isHighGround)
	{
		aOffsetY += HIGH_GROUND_HEIGHT;
	}
	else if (!mOnHighGround && isHighGround)
	{
		aOffsetY -= HIGH_GROUND_HEIGHT;
	}

	if (mBoard->StageIsNight())
	{
		aCelCol = 1;
	}

	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		aOffsetX += 3.0f;
		break;

	case ProjectileType::PROJECTILE_SNOWPEA:
		aOffsetX += -1.0f;
		aScale = 1.3f;
		break;

	case ProjectileType::PROJECTILE_STAR:
		aOffsetX += 7.0f;
		break;

	case ProjectileType::PROJECTILE_CABBAGE:
	case ProjectileType::PROJECTILE_KERNEL:
	case ProjectileType::PROJECTILE_BUTTER:
	case ProjectileType::PROJECTILE_MELON:
	case ProjectileType::PROJECTILE_WINTERMELON:
	case ProjectileType::PROJECTILE_CHERRYBOMB:
	case ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB:
	case ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB:
		if (mGatlingCherryShot)
		{
			aOffsetX += 3.0f;
		}
		else
		{
			aOffsetX += 3.0f;
			aOffsetY += 10.0f;
			aScale = 1.6f;
		}
		break;

	case ProjectileType::PROJECTILE_PUFF:
		return;

	case ProjectileType::PROJECTILE_COBBIG:
		aScale = 1.0f;
		aStretch = 3.0f;
		aOffsetX += 57.0f;
		break;

	case ProjectileType::PROJECTILE_FIREBALL:
		aScale = 1.4f;
		break;
	default:
		break;
	}

	if (mMotionType == ProjectileMotion::MOTION_LOBBED)
	{
		float aHeight = std::clamp(-mPosZ, 0.0f, 200.0f);
		aScale *= 200.0f / (aHeight + 200.0f);
	}

	PvzpDrawImageCelScaledF(g, IMAGE_PEA_SHADOWS, aOffsetX, (mShadowY - mPosY + aOffsetY), aCelCol, 0, aScale * aStretch, aScale);
}
