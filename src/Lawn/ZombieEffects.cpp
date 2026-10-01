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

bool ZombieEffects::CanDolphinBeButterStunned(Zombie* theZombie)
{
	return theZombie != nullptr && theZombie->mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER &&
		!theZombie->IsSunTierInvulnerable() && !theZombie->IsDeadOrDying() && !theZombie->mMindControlled &&
		theZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_INTO_POOL &&
		theZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_IN_JUMP && !theZombie->IsFlying();
}

void Zombie::ApplyChill(bool theIsIceTrap)
{
	if (!CanBeChilled())
		return;

	if (mChilledCounter == 0)
	{
		mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
	}

	int aChillTime = ZombieRules::IsGargantuarType(mZombieType) ? 1500 : 1000;
	if (theIsIceTrap)
	{
		aChillTime = ZombieRules::IsGargantuarType(mZombieType) ? 3000 : 2000;
	}
	mChilledCounter = std::max(aChillTime, mChilledCounter);

	UpdateAnimSpeed();
}

PvzpParticleSystem* Zombie::AddAttachedParticle(int thePosX, int thePosY, ParticleEffect theEffect)
{
	if (mDead)
		return nullptr;

	if (IsFullOfAttachments(mAttachmentID))
		return nullptr;

	PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(mX + thePosX, mY + thePosY, 0, theEffect);
	if (aParticle)
	{
		AttachParticle(mAttachmentID, aParticle, thePosX, thePosY);
	}

	return aParticle;
}

Reanimation* Zombie::AddAttachedReanim(int thePosX, int thePosY, ReanimationType theReanimType)
{
	if (mDead)
		return nullptr;

	Reanimation* aReanim = mApp->AddReanimation(mX + thePosX, mY + thePosY, 0, theReanimType);
	if (aReanim)
	{
		AttachReanim(mAttachmentID, aReanim, thePosX, thePosY);
	}

	return aReanim;
}

void Zombie::RemoveIceTrap()
{
	mIceTrapCounter = 0;
	if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		BalloonPropellerHatSpin(true);
	}

	UpdateAnimSpeed();
	StartZombieSound();
}

void Zombie::HitIceTrap()
{
	if (!CanBeChilled())
		return;

	bool cold = false;
	if (mChilledCounter > 0 || mIceTrapCounter != 0)
	{
		cold = true;
	}

	ApplyChill(true);
	if (!CanBeFrozen())
		return;

	if (mInPool)
	{
		mIceTrapCounter = 300;
	}
	else if (cold)
	{
		mIceTrapCounter = RandRangeInt(300, 400);
	}
	else
	{
		mIceTrapCounter = RandRangeInt(400, 600);
	}

	StopZombieSound();
	if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		BalloonPropellerHatSpin(false);
	}
	if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_SPIT)
	{
		mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL);
	}

	TakeDamage(20, 1U);
	UpdateAnimSpeed();
}

bool Zombie::IsTangleKelpTarget()
{
	if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
		return true;

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mSeedType == SeedType::SEED_TANGLEKELP && aPlant->mTargetZombieID == mBoard->ZombieGetID(this))
		{
			return true;
		}
	}

	return false;
}

bool Zombie::IsSquashTarget(Plant* theExcept)
{
	ZombieID anId = mBoard->ZombieGetID(this);

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant != theExcept && aPlant->mSeedType == SeedType::SEED_SQUASH && aPlant->mTargetZombieID == anId)
		{
			return true;
		}
	}

	return false;
}

bool Zombie::IsFireResistant()
{
	return
		mZombieType == ZombieType::ZOMBIE_CATAPULT ||
		mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
		mShieldType == ShieldType::SHIELDTYPE_DOOR ||
		mShieldType == ShieldType::SHIELDTYPE_LADDER;
}

void Zombie::BalloonPropellerHatSpin(bool theSpinning)
{
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	ReanimatorTrackInstance* aHatTrackInstance = aBodyReanim->GetTrackInstanceByName("hat");
	Reanimation* aPropellerReanim = FindReanimAttachment(aHatTrackInstance->mAttachmentID);
	if (aPropellerReanim)
	{
		if (theSpinning)
		{
			aPropellerReanim->mAnimRate = aPropellerReanim->mDefinition->mFPS;
		}
		else
		{
			aPropellerReanim->mAnimRate = 0.0f;
		}
	}
}

void Zombie::RemoveButter()
{
	if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		BalloonPropellerHatSpin(true);
	}

	if (Zombie::IsZombotany(mZombieType))
	{
		Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
		if (aHeadReanim)
		{
			if (mZombieType == ZombieType::ZOMBIE_PEA_HEAD && aHeadReanim->IsAnimPlaying("anim_shooting"))
			{
				aHeadReanim->mAnimRate = 35.0f;
			}
			else if (mZombieType == ZombieType::ZOMBIE_GATLING_HEAD && aHeadReanim->IsAnimPlaying("anim_shooting"))
			{
				aHeadReanim->mAnimRate = 38.0f;
			}
			else
			{
				aHeadReanim->mAnimRate = 15.0f;
			}
		}
	}

	UpdateAnimSpeed();
	StartZombieSound();
}

void Zombie::ApplyButter()
{
	if (IsSunTierInvulnerable())
		return;
	if (mBoard != nullptr && mBoard->mZombieTierSunMoney >= TWO_AND_HALF_MILLION_SUN_THRESHOLD &&
		mZombieType != ZombieType::ZOMBIE_BUNGEE && !ZombieEffects::CanDolphinBeButterStunned(this))
		return;
	if (ZombieRules::IsBulwarkType(mZombieType))
		return;
	if ((ZombieRules::IsGargantuarType(mZombieType) || ZombieRules::IsBulwarkType(mZombieType)) && mBoard != nullptr &&
		mBoard->mZombieTierSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD)
		return;
	bool aCanBeButterStunned = CanBeFrozen();
	if (!aCanBeButterStunned && mZombieType == ZombieType::ZOMBIE_BUNGEE && mBoard != nullptr &&
		mBoard->mZombieTierSunMoney >= THREE_AND_HALF_MILLION_SUN_THRESHOLD && !IsDeadOrDying() && !mMindControlled &&
		mZombiePhase == ZombiePhase::PHASE_BUNGEE_AT_BOTTOM)
	{
		// Cold slows are disabled at this tier, but grounded Bungees retain their butter-stun exception.
		aCanBeButterStunned = true;
	}
	if (!aCanBeButterStunned && mBoard != nullptr &&
		mBoard->mZombieTierSunMoney >= THREE_AND_HALF_MILLION_SUN_THRESHOLD && ZombieEffects::CanDolphinBeButterStunned(this))
	{
		// Dolphin Riders keep the 2m pre-leap immunity; after the first leap, butter still stuns them.
		aCanBeButterStunned = true;
	}
	if (!mHasHead || !aCanBeButterStunned)
		return;

	bool aIsGargantuar = ZombieRules::IsGargantuarType(mZombieType);
	if (aIsGargantuar && mBoard != nullptr && mBoard->mZombieStrengthTier >= 3)
		return;

	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_BOSS || IsTangleKelpTarget() || IsBobsledTeamWithSled() || IsFlying())
		return;

	mButteredCounter = mBoard != nullptr && mBoard->mZombieStrengthTier >= 3 ? 200 : 400;
	Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
	if (aZombie)
	{
		aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
		mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
	}

	if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		mAltitude = 0.0f;
		if (mOnHighGround)
		{
			mAltitude += HIGH_GROUND_HEIGHT;
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		BalloonPropellerHatSpin(false);
	}
	else if (Zombie::IsZombotany(mZombieType))
	{
		Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
		if (aHeadReanim)
		{
			aHeadReanim->mAnimRate = 0.0f;
		}
	}

	UpdateAnimSpeed();
	StopZombieSound();
}

void Zombie::MowDown()
{
	if (mDead || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED || mZombieType == ZombieType::ZOMBIE_BOSS)
		return;

	if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION);
		mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
		DieWithLoot();
		return;
	}

	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION);
		mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
		DieWithLoot();
		return;
	}

	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
		mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED ||
		ZombieRules::IsGargantuarType(mZombieType) ||
		mZombieType == ZombieType::ZOMBIE_BUNGEE ||
		mZombieType == ZombieType::ZOMBIE_DIGGER ||
		mZombieType == ZombieType::ZOMBIE_IMP ||
		mZombieType == ZombieType::ZOMBIE_YETI ||
		mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
		IsBobsledTeamWithSled() ||
		IsFlying() ||
		mInPool)
	{
		Reanimation* aPuffReanim = mApp->AddReanimation(mPosX - 73.0f, mPosY - 56.0f, mRenderOrder + 2, ReanimationType::REANIM_PUFF);
		aPuffReanim->SetFramesForLayer("anim_puff");
		mApp->AddPvzpParticle(mPosX + 110.0f, mPosY + 0.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MOWER_CLOUD);

		if (mBoard->mPlantRow[mRow] != PlantRowType::PLANTROW_POOL)
		{
			DropHead(0U);
			DropArm(0U);
			DropHelm(0U);
			DropShield(0U);
		}

		DieWithLoot();
		return;
	}

	if (mIceTrapCounter > 0)
	{
		RemoveIceTrap();
	}
	mButteredCounter = std::min(mButteredCounter, 0);

	DropShield(0U);
	DropHelm(0U);
	if (mZombieType == ZombieType::ZOMBIE_FLAG)
	{
		DropFlag();
	}
	else if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		DropPole();
	}
	else if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER || mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		DropHead(0U);
	}
	else if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		DropHead(0U);
		mAltitude = 0.0f;
	}

	Reanimation* aMoweredReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder, ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE);
	aMoweredReanim->mAnimRate = 8.0f;
	aMoweredReanim->mIsAttachment = false;
	aMoweredReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
	mMoweredReanimID = mApp->ReanimationGetID(aMoweredReanim);
	mZombiePhase = ZombiePhase::PHASE_ZOMBIE_MOWERED;
	DropLoot();
}

void Zombie::RemoveColdEffects()
{
	if (mIceTrapCounter > 0)
	{
		RemoveIceTrap();
	}

	if (mChilledCounter > 0)
	{
		mChilledCounter = 0;
		UpdateAnimSpeed();
	}
}

void Zombie::ApplyBurn()
{
	if (mDead || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
		return;

	if (mBoard != nullptr && mBoard->mZombieStrengthTier > 0)
	{
		TakeDamage(mBoard->GetZombieExplosiveDamage(), 18U);
		return;
	}

	if (mBodyHealth >= 1800 || mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		TakeDamage(1800, 18U);
		return;
	}

	if (mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD && !mHasHead)
	{
		mApp->RemoveReanimation(mSpecialHeadReanimID);
		mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	if (mIceTrapCounter > 0)
	{
		RemoveIceTrap();
	}
	mButteredCounter = std::min(mButteredCounter, 0);

	AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, nullptr);
	BungeeDropPlant();

	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
		mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
		mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
		mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
		mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
		mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
		mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
		mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED ||
		mInPool)
	{
		DieWithLoot();
	}
	else if (mZombieType == ZOMBIE_BUNGEE || mZombieType == ZOMBIE_YETI || Zombie::IsZombotany(mZombieType) || IsBobsledTeamWithSled() || IsFlying() || !mHasHead)
	{
		SetAnimRate(0.0f);
		Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
		if (aHeadReanim)
		{
			aHeadReanim->mAnimRate = 0.0f;
		}

		mZombiePhase = ZombiePhase::PHASE_ZOMBIE_BURNED;
		mPhaseCounter = 300;
		mJustGotShotCounter = 0;
		DropLoot();

		if (mZombieType == ZombieType::ZOMBIE_BALLOON)
		{
			BalloonPropellerHatSpin(false);
		}
	}
	else
	{
		ReanimationType aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED;
		float aCharredPosX = mPosX + 22.0f;
		float aCharredPosY = mPosY - 10.0f;
		if (mZombieType == ZombieType::ZOMBIE_BALLOON)
		{
			aCharredPosY += 31.0f;
		}
		if (mZombieType == ZombieType::ZOMBIE_IMP)
		{
			aCharredPosX -= 6.0f;
			aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_IMP;
		}
		if (mZombieType == ZombieType::ZOMBIE_DIGGER)
		{
			if (IsWalkingBackwards())
			{
				aCharredPosX += 14.0f;
			}
			aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER;
		}
		if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
		{
			aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI;
			aCharredPosX += 61.0f;
			aCharredPosY -= 16.0f;
		}
		if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
		{
			aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT;
			aCharredPosX -= 36.0f;
			aCharredPosY -= 20.0f;
		}
		if (ZombieRules::IsGargantuarType(mZombieType))
		{
			aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR;
			aCharredPosX -= 15.0f;
			aCharredPosY -= 10.0f;
		}

		Reanimation* aCharredReanim = mApp->AddReanimation(aCharredPosX, aCharredPosY, mRenderOrder, aReanimType);
		aCharredReanim->mAnimRate *= RandRangeFloat(0.9f, 1.1f);
		if (mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING_WITHOUT_AXE)
		{
			aCharredReanim->SetFramesForLayer("anim_crumble_noaxe");
		}
		else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
		{
			aCharredReanim->SetFramesForLayer("anim_crumble");
		}
		else if (ZombieRules::IsGargantuarType(mZombieType) && !mHasObject)
		{
			aCharredReanim->SetImageOverride("impblink", IMAGE_BLANK);
			aCharredReanim->SetImageOverride("imphead", IMAGE_BLANK);
		}

		float aCharredScale = mScaleZombie;
		if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
		{
			aCharredScale = 1.0f;
		}

		if (aCharredScale != 1.0f)
		{
			aCharredReanim->mOverlayMatrix.m00 = aCharredScale;
			aCharredReanim->mOverlayMatrix.m11 = aCharredScale;
			aCharredReanim->mOverlayMatrix.m02 += 20.0f - aCharredScale * 20.0f;
			aCharredReanim->mOverlayMatrix.m12 += 120.0f - aCharredScale * 120.0f;
			aCharredReanim->OverrideScale(aCharredScale, aCharredScale);
		}

		if (IsWalkingBackwards())
		{
			aCharredReanim->OverrideScale(-aCharredScale, aCharredScale);
			aCharredReanim->mOverlayMatrix.m02 += 60.0f * aCharredScale;
		}

		DieWithLoot();
	}

	if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		BobsledBurn();
	}
}

void Zombie::AttachShield()
{
	const char* aTrackName;
	Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
	if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
	{
		ShowDoorArms(true);
		ReanimShowPrefix("Zombie_outerarm_screendoor", RENDER_GROUP_OVER_SHIELD);
		aTrackName = "anim_screendoor";
	}
	else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
	{
		ReanimShowPrefix("Zombie_paper_hands", RENDER_GROUP_OVER_SHIELD);
		aTrackName = "Zombie_paper_paper";
	}
	else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
	{
		ReanimShowPrefix("Zombie_outerarm", RENDER_GROUP_OVER_SHIELD);
		aTrackName = "Zombie_ladder_1";
	}
	else
	{
		PVZP_ASSERT(false);
	}

	aBodyReanim->AssignRenderGroupToTrack(aTrackName, RENDER_GROUP_SHIELD);
}

void Zombie::DetachShield()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
		{
			ShowDoorArms(false);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
		{
			ReanimShowPrefix("Zombie_paper_hands", RENDER_GROUP_NORMAL);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
		{
#ifdef DO_FIX_BUGS
			if (mHasArm)  // fixes lost arms regrowing after a ladder zombie places its ladder
			{
				ReanimShowPrefix("Zombie_outerarm", RENDER_GROUP_NORMAL);
			}
#else
			ReanimShowPrefix("Zombie_outerarm", RENDER_GROUP_NORMAL);
#endif
			mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
			if (mIsEating)
			{
				PlayZombieReanim("anim_eat", ReanimLoopType::REANIM_LOOP, 20, 0.0f);
			}
			else
			{
				StartWalkAnim(0);
			}
		}
		else
		{
			PVZP_ASSERT(false);
		}
	}

	mShieldType = ShieldType::SHIELDTYPE_NONE;
	mShieldHealth = 0;
}

void Zombie::ReanimShowPrefix(const char* theTrackPrefix, int theRenderGroup)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		aBodyReanim->AssignRenderGroupToPrefix(theTrackPrefix, theRenderGroup);
	}
}

void Zombie::ReanimShowTrack(const char* theTrackName, int theRenderGroup)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		aBodyReanim->AssignRenderGroupToTrack(theTrackName, theRenderGroup);
	}
}
