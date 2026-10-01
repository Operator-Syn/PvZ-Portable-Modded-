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

#include <climits>
#include <algorithm>
#include <array>
#include <format>

#include "Plant.h"
#include "Board.h"
#include "../ConstEnums.h"
#include "Zombie.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "LawnMower.h"
#include "Challenge.h"
#include "Projectile.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "System/PlayerInfo.h"
#include "System/Zombatar.h"
#include "System/Music.h"
#include "Widget/AlmanacDialog.h"
#include "../PvzpLib/PvzpFoley.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpCommon.h"
#include "../PvzpLib/Reanimator.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

#include "ZombieConstants.h"

void Zombie::BossPlayIdle()
{
	mZombiePhase = ZombiePhase::PHASE_BOSS_IDLE;
	mPhaseCounter = RandRangeInt(100, 200);
	PlayZombieReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 6.0f);
}

void Zombie::DrawBossFireBall(Graphics* g)
{
	MakeParentGraphicsFrame(g);

	Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireBallReanim)
	{
		aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);

		g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
		aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);

		g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
		aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_BOSS_FIREBALL_TOP);
	}
}

void Zombie::DrawBossBackArm(Graphics* g, const ZombieDrawPosition& theDrawPos)
{
	float aImageOffsetX = 0.0f;
	float aImageOffsetY = 0.0f;
	if (mZombiePhase == PHASE_BOSS_DROP_RV)
	{
		aImageOffsetY = (mTargetRow - 1) * 85.0f - mTargetCol * 20.0f;
		aImageOffsetX = mTargetCol * 80.0f;
	}
	else if (mZombiePhase == PHASE_BOSS_BUNGEES_ENTER || mZombiePhase == PHASE_BOSS_BUNGEES_DROP || mZombiePhase == PHASE_BOSS_BUNGEES_LEAVE)
	{
		aImageOffsetX = mTargetCol * 80.0f - 23.0f;
	}

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	aBodyReanim->mOverlayMatrix.m02 += aImageOffsetX;
	aBodyReanim->mOverlayMatrix.m12 += aImageOffsetY;
	DrawReanim(g, theDrawPos, RENDER_GROUP_BOSS_BACK_ARM);
	aBodyReanim->mOverlayMatrix.m02 -= aImageOffsetX;
	aBodyReanim->mOverlayMatrix.m12 -= aImageOffsetY;
}

void Zombie::BossRVAttack()
{
	RemoveColdEffects();
	mZombiePhase = ZombiePhase::PHASE_BOSS_DROP_RV;
#ifdef DO_FIX_BUGS
	mTargetRow = RandRangeInt(0, mBoard->StageHas6Rows() ? 4 : 3);  // pool boss compatibility
#else
	mTargetRow = RandRangeInt(0, 3);
#endif
	mTargetCol = RandRangeInt(0, 2);

	PlayZombieReanim("anim_RV_1", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

void Zombie::BossRVLanding()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow >= mTargetRow && aPlant->mRow <= mTargetRow + 1 && aPlant->mPlantCol >= mTargetCol && aPlant->mPlantCol <= mTargetCol + 2)
		{
			aPlant->Squish();
		}
	}

	mBoard->ShakeBoard(1, 2);
	mApp->PlaySample(SOUND_RVTHROW);

	mSummonCounter = 500;
	mBossHeadCounter = 5000;
	if (mBossMode >= 1)
	{
		mBossStompCounter = 4000;
	}
	if (mBossMode >= 2)
	{
		mBossBungeeCounter = 6500;
	}
}

void Zombie::BossSpawnAttack()
{
	RemoveColdEffects();
	mZombiePhase = ZombiePhase::PHASE_BOSS_SPAWNING;
	if (mBossMode == 0)
	{
		mSummonCounter = RandRangeInt(450, 550);
	}
	else if (mBossMode == 1)
	{
		mSummonCounter = RandRangeInt(350, 450);
	}
	else if (mBossMode == 2)
	{
		mSummonCounter = RandRangeInt(150, 250);
	}

	mTargetRow = mBoard->PickRowForNewZombie(ZombieType::ZOMBIE_NORMAL);

	const char* aTrackName;
	switch (mTargetRow)
	{
	case 0:     aTrackName = "anim_spawn_1";    break;
	case 1:     aTrackName = "anim_spawn_2";    break;
	case 2:     aTrackName = "anim_spawn_3";    break;
	case 3:     aTrackName = "anim_spawn_4";    break;
	case 4:     aTrackName = "anim_spawn_5";    break;
#ifdef DO_FIX_BUGS
	default:    aTrackName = "anim_spawn_5";    break;  // compromise fix for the pool-stage spawn crash without editing the animation
#else
	default:    PVZP_ASSERT(false);                   break;
#endif
	}
	PlayZombieReanim(aTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

void Zombie::BossSpawnContact()
{
	ZombieType aZombieType;
	if (mZombieAge < 3500)
	{
		aZombieType = ZombieType::ZOMBIE_NORMAL;
	}
	else if (mZombieAge < 8000)
	{
		aZombieType = ZombieType::ZOMBIE_TRAFFIC_CONE;
	}
	else if (mZombieAge < 12500)
	{
		aZombieType = ZombieType::ZOMBIE_PAIL;
	}
	else
	{
		int aZombieTypeCount = LENGTH(gBossZombieList);
		if (mTargetRow == 0)
		{
			PVZP_ASSERT(gBossZombieList[aZombieTypeCount - 1] == ZombieType::ZOMBIE_GARGANTUAR);
			aZombieTypeCount--;
		}

		aZombieType = PvzpPickFromArray(gBossZombieList, aZombieTypeCount);
	}

	Zombie* aZombie = mBoard->AddZombieInRow(aZombieType, mTargetRow, 0);
	aZombie->mPosX = 600.0f;
}

void Zombie::BossStompAttack()
{
	RemoveColdEffects();
	mZombiePhase = ZombiePhase::PHASE_BOSS_STOMPING;
	mBossStompCounter = RandRangeInt(5500, 6500);

	int aRowsCount = 0;
	intptr_t aRowArray[4];
	for (int i = 0; i < 4; i++)
	{
		if (BossCanStompRow(i))
		{
			aRowArray[aRowsCount] = i;
			aRowsCount++;
		}
	}

	if (aRowsCount == 0)
		return;

	mTargetRow = PvzpPickFromArray(aRowArray, aRowsCount);

	const char* aTrackName;
	switch (mTargetRow)
	{
	case 0:     aTrackName = "anim_stomp_1";    break;
	case 1:     aTrackName = "anim_stomp_2";    break;
	case 2:     aTrackName = "anim_stomp_3";    break;
	case 3:     aTrackName = "anim_stomp_4";    break;
	default:    PVZP_ASSERT(false);                   break;
	}
	PlayZombieReanim(aTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

bool Zombie::BossCanStompRow(int theRow)
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (!aPlant->NotOnGround() && aPlant->mRow >= theRow && aPlant->mRow <= theRow + 1 && aPlant->mPlantCol >= 5)
		{
			return true;
		}
	}
	return false;
}

void Zombie::BossStompContact()
{
	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow >= mTargetRow && aPlant->mRow <= mTargetRow + 1 && aPlant->mPlantCol >= 5)
		{
			aPlant->Squish();
		}
	}

	mBoard->ShakeBoard(1, 4);
	mApp->PlayFoley(FoleyType::FOLEY_THUMP);
}

void Zombie::BossBungeeAttack()
{
	RemoveColdEffects();
	mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_ENTER;
	mBossBungeeCounter = RandRangeInt(4000, 5000);
	mTargetCol = RandRangeInt(0, 2);

	PlayZombieReanim("anim_bungee_1_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
	mApp->PlayFoley(FoleyType::FOLEY_BUNGEE_SCREAM);
}

void Zombie::BossBungeeSpawn()
{
	mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_DROP;

	for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
	{
		Zombie* aZombie = mBoard->AddZombieInRow(ZombieType::ZOMBIE_BUNGEE, 0, 0);
		aZombie->PickBungeeZombieTarget(mTargetCol + i);
		aZombie->mAltitude = aZombie->mPosY - 30.0f;
		mFollowerZombieID[i] = mBoard->ZombieGetID(aZombie);
	}
}

void Zombie::BossBungeeLeave()
{
	mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_LEAVE;

	for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mFollowerZombieID[i]);
		if (aZombie && aZombie->mButteredCounter > 0)
		{
			aZombie->DieWithLoot();
		}
	}

	PlayZombieReanim("anim_bungee_1_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
}

bool Zombie::BossAreBungeesDone()
{
	int aBungeesRemaining = 0;
	for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
	{
		Zombie* aZombie = mBoard->ZombieTryToGet(mFollowerZombieID[i]);
		if (aZombie)
		{
			if (aZombie->mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
			{
				return true;
			}
			aBungeesRemaining++;
		}
	}

	return aBungeesRemaining == 0;
}

void Zombie::BossHeadAttack()
{
	mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_ENTER;
	mBossHeadCounter = RandRangeInt(4000, 5000);

	PlayZombieReanim("anim_head_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

void Zombie::BossHeadSpit()
{
	Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireBallReanim)
	{
		aFireBallReanim->ReanimationDie();
		mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_SPIT;
#ifdef DO_FIX_BUGS
	mFireballRow = RandRangeInt(0, mBoard->StageHas6Rows() ? 5 : 4);  // pool boss compatibility
#else
	mFireballRow = RandRangeInt(0, 4);
#endif
	mIsFireBall = RandRangeInt(0, 1) == 0;

	const char* aTrackName;
	switch (mFireballRow)
	{
	case 0:     aTrackName = "anim_head_attack_1";      break;
	case 1:     aTrackName = "anim_head_attack_2";      break;
	case 2:     aTrackName = "anim_head_attack_3";      break;
	case 3:     aTrackName = "anim_head_attack_4";      break;
	case 4:     aTrackName = "anim_head_attack_5";      break;
#ifdef DO_FIX_BUGS
	default:    aTrackName = "anim_head_attack_5";      break;  // compromise fix for the pool-stage ball spit without editing the animation
#else
	default:    PVZP_ASSERT(false);                           break;
#endif
	}
	PlayZombieReanim(aTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mIsFireBall)
	{
		aBodyReanim->SetImageOverride("Boss_eyeglow_red", nullptr);
		aBodyReanim->SetImageOverride("Boss_mouthglow_red", nullptr);
	}
	else
	{
		aBodyReanim->SetImageOverride("Boss_eyeglow_red", IMAGE_REANIM_ZOMBIE_BOSS_EYEGLOW_BLUE);
		aBodyReanim->SetImageOverride("Boss_mouthglow_red", IMAGE_REANIM_ZOMBIE_BOSS_MOUTHGLOW_BLUE);
	}

	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
	aHeadReanim->PlayReanim("anim_drive", ReanimLoopType::REANIM_LOOP, 20, 36.0f);
}

void Zombie::BossDestroyIceballInRow()
{
	//if (theRow != mFireballRow)  // the row check happens at the call site, so theRow is no longer a parameter
	//    return;

	Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireBallReanim && !mIsFireBall)
	{
		float aPosX = aFireBallReanim->mOverlayMatrix.m02 + 80.0f;
		float aPosY = aFireBallReanim->mOverlayMatrix.m12 + 80.0f;
		mApp->AddPvzpParticle(aPosX, aPosY, 400000, ParticleEffect::PARTICLE_ICEBALL_DEATH);

		aFireBallReanim->ReanimationDie();
		mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
		mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_ICEBALL_TRAIL);
	}
}

void Zombie::BossDestroyFireball()
{
	Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireBallReanim && mIsFireBall)
	{
		float aPosX = aFireBallReanim->mOverlayMatrix.m02 + 80.0f;
		float aPosY = aFireBallReanim->mOverlayMatrix.m12 + 40.0f;
		for (int i = 0; i < 6; i++)
		{
			float aAngle = 2 * PI * i / 6 + PI / 2;
			Reanimation* aReanim = mApp->AddReanimation(aPosX + 60.0f * sin(aAngle), aPosY + 60.0f * cos(aAngle), 400000, ReanimationType::REANIM_JALAPENO_FIRE);
			aReanim->mAnimTime = 0.2f;
			aReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME;
			aReanim->mAnimRate = RandRangeFloat(20.0f, 25.0f);
		}

		aFireBallReanim->ReanimationDie();
		mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
		mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_FIREBALL_TRAIL);
	}
}

void Zombie::BossHeadSpitEffect()
{
	if (mIsFireBall)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		int aTrackIndex = aBodyReanim->FindTrackIndex("Boss_jaw");
		ReanimatorTransform aTransform;
		aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
		float aFlamePosX = mPosX + aTransform.mTransX + 100.0f;
		float aFlamePosY = mPosY + aTransform.mTransY + 50.0f;
		mApp->AddPvzpParticle(aFlamePosX, aFlamePosY, mRenderOrder + 2, ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL);
	}
	else
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		int aTrackIndex = aBodyReanim->FindTrackIndex("Boss_jaw");
		ReanimatorTransform aTransform;
		aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
		float aFlamePosX = mPosX + aTransform.mTransX + 100.0f;
		float aFlamePosY = mPosY + aTransform.mTransY + 50.0f;
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aFlamePosX, aFlamePosY, mRenderOrder + 2, ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL);
		if (aParticle)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIE_BOSS_ICEBALL_PARTICLES);
		}
	}

	mApp->PlayFoley(FoleyType::FOLEY_BOSS_BOULDER_ATTACK);
}

void Zombie::BossHeadSpitContact()
{
	PVZP_ASSERT(!mApp->ReanimationTryToGet(mBossFireBallReanimID));

	float aPosY = mBoard->GetPosYBasedOnRow(550.0f, mFireballRow) - 90.0f;
	Reanimation* aFireBallReanim;
	if (mIsFireBall)
	{
		aFireBallReanim = mApp->AddReanimation(455.0f, aPosY, mRenderOrder + 1, ReanimationType::REANIM_BOSS_FIREBALL);
		aFireBallReanim->PlayReanim("anim_form", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 16.0f);
		aFireBallReanim->mIsAttachment = true;
		aFireBallReanim->AssignRenderGroupToTrack("additive", RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
		aFireBallReanim->AssignRenderGroupToTrack("superglow", RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
	}
	else
	{
		aFireBallReanim = mApp->AddReanimation(455.0f, aPosY, mRenderOrder + 1, ReanimationType::REANIM_BOSS_ICEBALL);
		aFireBallReanim->PlayReanim("anim_form", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 16.0f);
		aFireBallReanim->mIsAttachment = true;
		aFireBallReanim->AssignRenderGroupToTrack("ice_highlight", RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
	}

	mBossFireBallReanimID = mApp->ReanimationGetID(aFireBallReanim);
	mApp->ReanimationTryToGet(mSpecialHeadReanimID)->PlayReanim("anim_laugh", ReanimLoopType::REANIM_LOOP, 20, 18.0f);
	mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

void Zombie::UpdateBossFireball()
{
	Reanimation* aFireballReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireballReanim == nullptr)
		return;

	float aSpeed = aFireballReanim->GetTrackVelocity("_ground");
	aFireballReanim->mOverlayMatrix.m02 -= aSpeed;
	float aPosX = aFireballReanim->mOverlayMatrix.m02;
	float aPosY = mBoard->GetPosYBasedOnRow(aPosX + 75.0f, mFireballRow) - 90.0f;
	aFireballReanim->mOverlayMatrix.m12 = aPosY;

	if (aPosX < -180.0f)
	{
		aFireballReanim->ReanimationDie();
		mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	SquishAllInSquare(mBoard->PixelToGridX(aPosX + 75, aPosY), mFireballRow, ZombieAttackType::ATTACKTYPE_DRIVE_OVER);

	for (LawnMower* aLawnMower : mBoard->mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED && aLawnMower->mRow == mFireballRow &&
			aLawnMower->mPosX > aPosX && aLawnMower->mPosX < aPosX + 50.0f)
		{
			aLawnMower->SquishMower();
		}
	}

	if (mIsFireBall)
	{
		if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aFireballReanim->mLoopCount > 0)
		{
			aFireballReanim->PlayReanim("anim_role", ReanimLoopType::REANIM_LOOP, 0, 2.0f);
			aFireballReanim->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mFireballRow, 0);
		}

		if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_LOOP && Rand(10) == 0)
		{
			float aBallPosX = aPosX + 100.0f + RandRangeFloat(0.0f, 20.0f);
			float aBallPosY = mBoard->GetPosYBasedOnRow(aBallPosX - 40.0f, mFireballRow) + 90.0f + RandRangeFloat(-50.0f, 0.0f);
			int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mFireballRow, 6);
			mApp->AddPvzpParticle(aBallPosX, aBallPosY, aRenderPosition, ParticleEffect::PARTICLE_FIREBALL_TRAIL);
		}
	}
	else
	{
		if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aFireballReanim->mLoopCount > 0)
		{
			aFireballReanim->PlayReanim("anim_role", ReanimLoopType::REANIM_LOOP, 0, 2.0f);
			aFireballReanim->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mFireballRow, 0);
		}

		if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_LOOP && Rand(10) == 0)
		{
			float aBallPosX = aPosX + 100.0f + RandRangeFloat(0.0f, 20.0f);
			float aBallPosY = mBoard->GetPosYBasedOnRow(aBallPosX - 40.0f, mFireballRow) + 90.0f + RandRangeFloat(-50.0f, 0.0f);
			int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mFireballRow, 6);
			mApp->AddPvzpParticle(aBallPosX, aBallPosY, aRenderPosition, ParticleEffect::PARTICLE_ICEBALL_TRAIL);
		}
	}

	aFireballReanim->Update();
}

void Zombie::BossStartDeath()
{
	mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_LEAVE;
	PlayZombieReanim("anim_head_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

	mApp->AddPvzpParticle(700.0f, 150.0f, 400000, ParticleEffect::PARTICLE_BOSS_EXPLOSION);
	mApp->PlaySample(SOUND_BOSSEXPLOSION);
	mApp->PlayFoley(FoleyType::FOLEY_GARGANTUDEATH);

	BossDie();
}

void Zombie::UpdateBoss()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.24f) || aBodyReanim->ShouldTriggerTimedEvent(0.79f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_THUMP);
			mBoard->ShakeBoard(1, 4);
		}
		return;
	}

	Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
	UpdateBossFireball();
	if (mIceTrapCounter == 0)
	{
		if (mSummonCounter > 0)
		{
			mSummonCounter--;
		}
		if (mBossBungeeCounter > 0)
		{
			mBossBungeeCounter--;
		}
		if (mBossStompCounter > 0)
		{
			mBossStompCounter--;
		}
		if (mBossHeadCounter > 0)
		{
			mBossHeadCounter--;
		}

		if (mChilledCounter > 0)
		{
			aHeadReanim->mAnimRate = 6.0f;
		}
		else if (aHeadReanim->mAnimRate == 0.0f)
		{
			aHeadReanim->mAnimRate = 12.0f;
		}
	}
	else
	{
		aHeadReanim->mAnimRate = 0.0f;
	}

	if (mZombiePhase == ZombiePhase::PHASE_BOSS_ENTER)
	{
		BossPlayIdle();
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_IDLE)
	{
		if (mBodyHealth == 1)
		{
			PlayDeathAnim(0U);
			return;
		}

		if (mPhaseCounter > 0)
			return;

		int aDamageIndex = GetBodyDamageIndex();
		if (aDamageIndex != mBossMode)
		{
			mBossMode = aDamageIndex;
			if (mBossMode == 1)  // on entering damage stage 1, immediately release bungees once
			{
				BossBungeeAttack();
			}
			else  // on entering damage stage 2, immediately throw an RV once
			{
				BossRVAttack();
			}
		}
		else if (mBossStompCounter == 0)
		{
			BossStompAttack();
		}
		else if (mBossBungeeCounter == 0)
		{
			if (Rand(mApp->IsAdventureMode() ? 4 : 2) == 0)  // 1/2 chance to throw an RV (1/4 in adventure mode), otherwise release bungees
			{
				mBossBungeeCounter = RandRangeInt(4000, 5000);
				BossRVAttack();
			}
			else
			{
				BossBungeeAttack();
			}
		}
		else if (mBossHeadCounter == 0)
		{
			BossHeadAttack();
		}
		else if (mSummonCounter == 0)
		{
			BossSpawnAttack();
		}
		else
		{
			mPhaseCounter = RandRangeInt(100, 200);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_SPAWNING)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.6f))
		{
			BossSpawnContact();
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			BossPlayIdle();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_STOMPING)
	{
		float aTrigger = 0.5f;
		if (mTargetRow >= 2)
		{
			aTrigger = 0.55f;
		}
		if (aBodyReanim->ShouldTriggerTimedEvent(aTrigger))
		{
			BossStompContact();
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			BossPlayIdle();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_BUNGEES_ENTER)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
		{
			BossBungeeSpawn();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_BUNGEES_DROP)
	{
		if (BossAreBungeesDone())
		{
			BossBungeeLeave();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_BUNGEES_LEAVE)
	{
		if (aBodyReanim->mLoopCount > 0)
		{
			BossPlayIdle();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_DROP_RV)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.65f))
		{
			BossRVLanding();
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			BossPlayIdle();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_ENTER)
	{
		if (GetBodyDamageIndex() == 2 && aBodyReanim->ShouldTriggerTimedEvent(0.37f))
		{
			ApplyBossSmokeParticles(true);
		}

		if (aBodyReanim->ShouldTriggerTimedEvent(0.55f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT;
			PlayZombieReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			mPhaseCounter = 500;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT)
	{
		if (mBodyHealth == 1)
		{
			BossStartDeath();
		}
		else if (mPhaseCounter == 0)
		{
			BossHeadSpit();
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_SPIT)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.37f))
		{
			BossHeadSpitEffect();
		}

		if (aBodyReanim->ShouldTriggerTimedEvent(0.42f))
		{
			BossHeadSpitContact();
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
			aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 18.0f);
			mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT;
			PlayZombieReanim("anim_head_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			mPhaseCounter = 300;
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT)
	{
		if (mBodyHealth == 1)
		{
			BossStartDeath();
		}
		else if (mPhaseCounter == 0)
		{
			mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_LEAVE;
			PlayZombieReanim("anim_head_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
		}
	}
	else if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_LEAVE)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.23f))
		{
			mChilledCounter = 0;
			UpdateAnimSpeed();
		}

		if (aBodyReanim->ShouldTriggerTimedEvent(0.48f) || aBodyReanim->ShouldTriggerTimedEvent(0.8f))
		{
			mApp->PlayFoley(FoleyType::FOLEY_THUMP);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			ApplyBossSmokeParticles(false);
			BossPlayIdle();
		}
	}
	else
	{
		PVZP_ASSERT(false);
	}
}

void Zombie::BossDie()
{
	if (!IsOnBoard())
		return;

	Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
	if (aFireBallReanim)
	{
		aFireBallReanim->ReanimationDie();
		mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;

		BossDestroyIceballInRow();
		BossDestroyFireball();
	}

	mApp->mMusic->FadeOut(200);

	for (Zombie* aZombie : mBoard->mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie != this && !aZombie->IsDeadOrDying())
		{
			aZombie->DieWithLoot();
		}
	}

	RemoveColdEffects();
}

void Zombie::BossSetupReanim()
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	aBodyReanim->AssignRenderGroupToPrefix("Boss_innerleg", RENDER_GROUP_BOSS_BACK_LEG);
	aBodyReanim->AssignRenderGroupToPrefix("Boss_outerleg", RENDER_GROUP_BOSS_FRONT_LEG);
	aBodyReanim->AssignRenderGroupToPrefix("Boss_body2", RENDER_GROUP_BOSS_FRONT_LEG);
	aBodyReanim->AssignRenderGroupToPrefix("Boss_innerarm", RENDER_GROUP_BOSS_BACK_ARM);
	aBodyReanim->AssignRenderGroupToPrefix("Boss_RV", RENDER_GROUP_BOSS_BACK_ARM);

	Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_BOSS_DRIVER);
	aHeadReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 18.0f);
	mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);

	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Boss_head2");
	AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 28.0f, -84.0f);
	aBodyReanim->mFrameBasePose = 0;
	aAttachEffect->mOffset.m00 = 1.2f;
	aAttachEffect->mOffset.m11 = 1.2f;
	aAttachEffect->mDontDrawIfParentHidden = true;
}

void Zombie::DrawBossPart(Graphics* g, BossPart theBossPart)
{
	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);

	switch (theBossPart)
	{
	case BossPart::BOSS_PART_BACK_LEG:      DrawReanim(g, aDrawPos, RENDER_GROUP_BOSS_BACK_LEG);    break;
	case BossPart::BOSS_PART_FRONT_LEG:     DrawReanim(g, aDrawPos, RENDER_GROUP_BOSS_FRONT_LEG);   break;
	case BossPart::BOSS_PART_MAIN:          DrawReanim(g, aDrawPos, 0);                             break;
	case BossPart::BOSS_PART_BACK_ARM:      DrawBossBackArm(g, aDrawPos);                           break;
	case BossPart::BOSS_PART_FIREBALL:      DrawBossFireBall(g);                          break;
	default:                                                                                        break;
	}
}
