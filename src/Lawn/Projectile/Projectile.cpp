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

#include "../Board/Board.h"
#include "../Plant/Plant.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../GameConstants.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <array>
#include <cstdint>




#include "../Rules/TargetingRules.h"
#include "ProjectileRules.h"

Projectile::Projectile()
{
	mPiercesZombies = false;
	mPiercedZombieCount = 0;
	mPlanternCob = false;
	mPlanternAutoCoffeeBean = false;
	mTargetTrackingEnded = false;
	mCattailRedirectionCount = 0;
	std::fill_n(mPiercedZombieIDs, MAX_PIERCING_HITS, ZombieID::ZOMBIEID_NULL);
}

Projectile::~Projectile()
{
	AttachmentDie(mAttachmentID);
}

void Projectile::ProjectileInitialize(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType)
{
	int aGridX = mBoard->PixelToGridXKeepOnBoard(theX, theY);
	mProjectileType = theProjectileType;
	mPosX = theX;
	mPosY = theY;
	mPosZ = 0.0f;
	mVelX = 0.0f;
	mVelY = 0.0f;
	mVelZ = 0.0f;
	mAccZ = 0.0f;
	mShadowY = mBoard->GridToPixelY(aGridX, theRow) + 67.0f;
	mHitTorchwoodGridX = -1;
	mMotionType = ProjectileMotion::MOTION_STRAIGHT;
	mFrame = 0;
	mNumFrames = 1;
	mRow = theRow;
	mCobTargetX = 0.0f;
	mDamageRangeFlags = 0;
	mDead = false;
	mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	mCobTargetRow = 0;
	mTargetZombieID = ZombieID::ZOMBIEID_NULL;
	mSourceZombieID = ZombieID::ZOMBIEID_NULL;
	mSourceZombieType = ZombieType::ZOMBIE_INVALID;
	mTargetTrackingEnded = false;
	mCattailRedirectionCount = 0;
	mPiercesZombies = false;
	mPiercedZombieCount = 0;
	mPlanternCob = false;
	mPlanternAutoCoffeeBean = false;
	mGatlingCherryShot = false;
	mMillionSunDamage = false;
	mTwoMillionSunCatTailDamage = false;
	mWintermelonCherryShot = false;
	mEphraimChargedJavelin = false;
	mSniperSourcePlantID = PlantID::PLANTID_NULL;
	mSniperCriticalArrow = false;
	mSniperMovementStacks = 0;
	mSniperPiercedZombieIDs.clear();
	std::fill(std::begin(mPiercedZombieIDs), std::end(mPiercedZombieIDs), ZombieID::ZOMBIEID_NULL);
	mOnHighGround = mBoard->mGridSquareType[aGridX][theRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
	if (mBoard->StageHasRoof() && theX < 480)
	{
		mShadowY -= 12.0f;
	}
	mRenderOrder = theRenderOrder;
	mRotation = 0.0f;
	mRotationSpeed = 0.0f;
	mWidth = 40;
	mHeight = 40;
	mProjectileAge = 0;
	mClickBackoffCounter = 0;
	mAnimTicksPerFrame = 0;

	switch (mProjectileType)
	{
	case ProjectileType::PROJECTILE_SNIPER_ARROW:
		mWidth = Plant::SNIPER_ARROW_WIDTH;
		mHeight = Plant::SNIPER_ARROW_HEIGHT;
		break;
	case ProjectileType::PROJECTILE_EPHRAIM_JAVELIN:
		mWidth = Plant::EPHRAIM_JAVELIN_WIDTH;
		mHeight = Plant::EPHRAIM_JAVELIN_HEIGHT;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
	case ProjectileType::PROJECTILE_BUTTER:
		mRotation = -7 * PI / 25;  // DEG_TO_RAD(-50.4f);
		mRotationSpeed = RandRangeFloat(-0.08f, -0.02f);
		break;
	case ProjectileType::PROJECTILE_MELON:
	case ProjectileType::PROJECTILE_WINTERMELON:
		mRotation = -2 * PI / 5;  // DEG_TO_RAD(-72.0f);
		mRotationSpeed = RandRangeFloat(-0.08f, -0.02f);
		break;
	case ProjectileType::PROJECTILE_KERNEL:
		mRotation = 0.0f;
		mRotationSpeed = RandRangeFloat(-0.2f, -0.08f);
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
	{
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(mPosX + 8.0f, mPosY + 13.0f, 400000, ParticleEffect::PARTICLE_SNOWPEA_TRAIL);
		AttachParticle(mAttachmentID, aParticle, 8.0f, 13.0f);
		break;
	}
	case ProjectileType::PROJECTILE_FIREBALL:
		PVZP_ASSERT(false);
		break;
	case ProjectileType::PROJECTILE_COBBIG:
		mWidth = IMAGE_REANIM_COBCANNON_COB->GetWidth();
		mHeight = IMAGE_REANIM_COBCANNON_COB->GetHeight();
		mRotation = PI / 2;
		break;
	case ProjectileType::PROJECTILE_PUFF:
	{
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(mPosX + 13.0f, mPosY + 13.0f, 400000, ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL);
		AttachParticle(mAttachmentID, aParticle, 13.0f, 13.0f);
		break;
	}
	case ProjectileType::PROJECTILE_BASKETBALL:
		mRotation = RandRangeFloat(0.0f, 2 * PI);
		mRotationSpeed = RandRangeFloat(0.05f, 0.1f);
		break;
	case ProjectileType::PROJECTILE_STAR:
		mShadowY += 15.0f;
		mRotationSpeed = RandRangeFloat(0.05f, 0.1f);
		if (Rand(2) == 0)
		{
			mRotationSpeed = -mRotationSpeed;
		}
		break;
	default:
		break;
	}

	mAnimCounter = 0;
	mX = static_cast<int>(mPosX);
	mY = static_cast<int>(mPosY);
}

void Projectile::Update()
{
	mProjectileAge++;
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mBoard->mCutScene->ShouldRunUpsellBoard())
		return;

	int aTime = 20;
	if (mProjectileType == ProjectileType::PROJECTILE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SNOWPEA ||
		mProjectileType == ProjectileType::PROJECTILE_CABBAGE ||
		mProjectileType == ProjectileType::PROJECTILE_MELON ||
		mProjectileType == ProjectileType::PROJECTILE_WINTERMELON ||
		mProjectileType == ProjectileType::PROJECTILE_KERNEL ||
		mProjectileType == ProjectileType::PROJECTILE_BUTTER ||
		mProjectileType == ProjectileType::PROJECTILE_COBBIG ||
		mProjectileType == ProjectileType::PROJECTILE_CHERRYBOMB ||
		mProjectileType == ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB ||
		mProjectileType == ProjectileType::PROJECTILE_PLANTERN_CHERRY_BOMB ||
		mProjectileType == ProjectileType::PROJECTILE_ZOMBIE_PEA ||
		mProjectileType == ProjectileType::PROJECTILE_SPIKE)
	{
		aTime = 0;
	}
	if (mProjectileAge > aTime)
	{
		mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
	}

	if (mClickBackoffCounter > 0)
	{
		mClickBackoffCounter--;
	}
	mRotation += mRotationSpeed;

	UpdateMotion();
	AttachmentUpdateAndMove(mAttachmentID, mPosX, mPosY + mPosZ);
	if (mProjectileType == ProjectileType::PROJECTILE_TWIN_SUNFLOWER_BOMB)
	{
		Color aSunBombColor = mBoard->mSunMoney > TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD
			? Color(255, 48, 48) : Color::White;
		AttachmentOverrideColor(mAttachmentID, aSunBombColor);
	}
}

void Projectile::Die()
{
	mDead = true;

	if (mProjectileType == ProjectileType::PROJECTILE_PUFF || mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
	{
		AttachmentCrossFade(mAttachmentID, "FadeOut");
		AttachmentDetach(mAttachmentID);
	}
	else
	{
		AttachmentDie(mAttachmentID);
	}
}

void Projectile::ConvertToFireball(int theGridX)
{
	if (mHitTorchwoodGridX == theGridX)
		return;

	mProjectileType = ProjectileType::PROJECTILE_FIREBALL;
	mHitTorchwoodGridX = theGridX;
	mApp->PlayFoley(FoleyType::FOLEY_FIREPEA);

	float aOffsetX = -25.0f;
	float aOffsetY = -25.0f;
	Reanimation* aFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);
	if (mMotionType == ProjectileMotion::MOTION_BACKWARDS)
	{
		aFirePeaReanim->OverrideScale(-1.0f, 1.0f);
		aOffsetX += 80.0f;
	}

	aFirePeaReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);
	aFirePeaReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
	aFirePeaReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);
	AttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);
}

void Projectile::ConvertToPea(int theGridX)
{
	if (mHitTorchwoodGridX == theGridX)
		return;

	AttachmentDie(mAttachmentID);
	mProjectileType = ProjectileType::PROJECTILE_PEA;
	mHitTorchwoodGridX = theGridX;
	mApp->PlayFoley(FoleyType::FOLEY_THROW);
}
