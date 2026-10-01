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

#include "../Plant/Plant.h"
#include "../Board/Board.h"
#include "../../ConstEnums.h"
#include "Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Entities/LawnMower.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/PlayerInfo.h"
#include "../System/Zombatar.h"
#include "../System/Music.h"
#include "../Widget/AlmanacDialog.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/PvzpParticle.h"
#include <algorithm>









#include "ZombieStatusRules.h"
#include "ZombieEffects.h"
#include "ZombieBossTypes.h"

#include "ZombieConstants.h"

void Zombie::DropPole()
{
	if (mZombieType != ZombieType::ZOMBIE_POLEVAULTER)
		return;

	ReanimShowPrefix("Zombie_polevaulter_innerarm", RENDER_GROUP_HIDDEN);
	ReanimShowPrefix("Zombie_polevaulter_innerhand", RENDER_GROUP_HIDDEN);
	ReanimShowPrefix("Zombie_polevaulter_pole", RENDER_GROUP_HIDDEN);
}

bool Zombie::CanLoseBodyParts()
{
	return
		mZombieType != ZombieType::ZOMBIE_ZAMBONI &&
		mZombieType != ZombieType::ZOMBIE_BUNGEE &&
		mZombieType != ZombieType::ZOMBIE_CATAPULT &&
		mZombieType != ZombieType::ZOMBIE_GARGANTUAR &&
		mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR &&
		mZombieType != ZombieType::ZOMBIE_BULWARK_GARGANTUAR &&
		mZombieType != ZombieType::ZOMBIE_BOSS &&
		mZombieHeight != ZombieHeight::HEIGHT_ZOMBIQUARIUM &&
		!IsFlying() &&
		!IsBobsledTeamWithSled();
}

void Zombie::SetupReanimForLostHead()
{
	ReanimShowPrefix("anim_head", RENDER_GROUP_HIDDEN);
	ReanimShowPrefix("anim_hair", RENDER_GROUP_HIDDEN);
	ReanimShowPrefix("anim_tongue", RENDER_GROUP_HIDDEN);
}

void Zombie::DropHead(unsigned int theDamageFlags)
{
	if (!CanLoseBodyParts() || !mHasHead)
		return;

	if (mButteredCounter > 0)
	{
		mButteredCounter = 0;
		UpdateAnimSpeed();
	}

	mHasHead = false;
	SetupReanimForLostHead();
	if (TestBit(theDamageFlags, DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
	{
		return;
	}

	if (Zombie::IsZombotany(mZombieType))
	{
		mApp->ReanimationGet(mSpecialHeadReanimID)->ReanimationDie();
		mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
		return;
	}

	int aRenderOrder = mRenderOrder + 1;
	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	float aPosX = mPosX + aDrawPos.mImageOffsetX + aDrawPos.mHeadX + 11.0f;
	float aPosY = mPosY + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY + 21.0f;
	if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
	{
		GetTrackPosition("anim_head1", aPosX, aPosY);
	}

	ParticleEffect aEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD;
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
	{
		aEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD;
	}
	else if (mInPool)
	{
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL;
	}
	if (mZombieType == ZombieType::ZOMBIE_DANCER)
	{
		aRenderOrder = mRenderOrder - 1;
	}
	if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
	{
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER_HEAD;
	}
	else if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		PogoBreak(theDamageFlags);
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_POGO_HEAD;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		ReanimShowPrefix("anim_hat", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("hat", RENDER_GROUP_HIDDEN);
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_BALLOON_HEAD;
	}
	else if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		DropPole();
	}
	else if (mZombieType == ZombieType::ZOMBIE_FLAG)
	{
		DropFlag();
	}

	PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, aRenderOrder, aEffect);
	OverrideParticleColor(aParticle);
	OverrideParticleScale(aParticle);
	if (aParticle)
	{
		if (mZombieType == ZombieType::ZOMBIE_DANCER)
		{
			ReanimShowPrefix("Zombie_disco_chops", RENDER_GROUP_HIDDEN);
			ReanimShowPrefix("Zombie_disco_glasses", RENDER_GROUP_HIDDEN);
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEDANCERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
		{
			ReanimShowPrefix("Zombie_disco_chops", RENDER_GROUP_HIDDEN);
			ReanimShowPrefix("Zombie_backup_stash", RENDER_GROUP_HIDDEN);
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEBACKUPDANCERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEBOBSLEDHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_LADDER)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIELADDERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_IMP)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEIMPHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEFOOTBALLHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEPOLEVAULTERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
		{
			aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_SNORKLE_HEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEDIGGERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEDOLPHINRIDERHEAD);
		}
		else if (mZombieType == ZombieType::ZOMBIE_YETI)
		{
			aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEYETIHEAD);
		}
	}

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (mBoard->mMustacheMode && aBodyReanim->TrackExists("Zombie_mustache"))
	{
		ReanimShowPrefix("Zombie_mustache", RENDER_GROUP_HIDDEN);

		PvzpParticleSystem* aMustacheParticle = mApp->AddPvzpParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_MUSTACHE);
		OverrideParticleColor(aMustacheParticle);
		OverrideParticleScale(aMustacheParticle);

		Image* aMustacheImage = aBodyReanim->GetImageOverride("Zombie_mustache");
		if (aMustacheParticle && aMustacheImage)
		{
			aMustacheParticle->OverrideImage(nullptr, aMustacheImage);
		}
	}
	if (mBoard->mFutureMode)
	{
		Image* aHeadImage = aBodyReanim->GetImageOverride("anim_head1");
		int aFrame = -1;
		if (aHeadImage)
		{
			if (aHeadImage == IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES1)
			{
				aFrame = 0;
			}
			else if (aHeadImage == IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES2)
			{
				aFrame = 1;
			}
			else if (aHeadImage == IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES3)
			{
				aFrame = 2;
			}
			else if (aHeadImage == IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES4)
			{
				aFrame = 3;
			}
		}

		if (aFrame != -1)
		{
			PvzpParticleSystem* aSunglassParticle = mApp->AddPvzpParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS);
			OverrideParticleColor(aSunglassParticle);
			OverrideParticleScale(aSunglassParticle);
			if (aSunglassParticle)
			{
				aSunglassParticle->OverrideFrame(nullptr, aFrame);
			}
		}
	}
	if (mBoard->mPinataMode && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_MOWERED)
	{
		mApp->AddPvzpParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_PINATA);
		OverrideParticleScale(aParticle); // Weird, TODO: test the Pinata Mode
	}

	mApp->PlayFoley(FoleyType::FOLEY_LIMBS_POP);
}

void Zombie::SetupReanimForLostArm(unsigned int theDamageFlags)
{
	switch (mZombieType)
	{
	case ZombieType::ZOMBIE_FOOTBALL:
		ReanimShowPrefix("Zombie_football_leftarm_lower", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("Zombie_football_leftarm_hand", RENDER_GROUP_HIDDEN);
		break;
	case ZombieType::ZOMBIE_NEWSPAPER:
		ReanimShowTrack("Zombie_paper_hands", RENDER_GROUP_HIDDEN);
		ReanimShowTrack("Zombie_paper_leftarm_lower", RENDER_GROUP_HIDDEN);
		break;
	case ZombieType::ZOMBIE_POLEVAULTER:
		ReanimShowTrack("Zombie_polevaulter_outerarm_lower", RENDER_GROUP_HIDDEN);
		ReanimShowTrack("Zombie_outerarm_hand", RENDER_GROUP_HIDDEN);
		break;
	case ZombieType::ZOMBIE_DANCER:
		ReanimShowTrack("Zombie_disco_outerarm_lower", RENDER_GROUP_HIDDEN);
		ReanimShowTrack("Zombie_disco_outerhand_point", RENDER_GROUP_HIDDEN);
		break;
	case ZombieType::ZOMBIE_BACKUP_DANCER:
		ReanimShowTrack("Zombie_disco_outerarm_lower", RENDER_GROUP_HIDDEN);
		ReanimShowTrack("Zombie_disco_outerhand", RENDER_GROUP_HIDDEN);
		break;
	default:
		ReanimShowPrefix("Zombie_outerarm_lower", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("Zombie_outerarm_hand", RENDER_GROUP_HIDDEN);
		break;
	}

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	float aPosX = mPosX + aDrawPos.mImageOffsetX + 45.0f;
	float aPosY = mPosY + aDrawPos.mImageOffsetY + aDrawPos.mBodyY + 78.0f;
	if (IsWalkingBackwards())
	{
		aPosX += 36.0f;
	}

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim)
	{
		switch (mZombieType)
		{
		case ZombieType::ZOMBIE_FOOTBALL:
			GetTrackPosition("Zombie_football_leftarm_hand", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_football_leftarm_upper", IMAGE_REANIM_ZOMBIE_FOOTBALL_LEFTARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_NEWSPAPER:
			GetTrackPosition("Zombie_paper_leftarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_paper_leftarm_upper", IMAGE_REANIM_ZOMBIE_PAPER_LEFTARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_POLEVAULTER:
			GetTrackPosition("Zombie_polevaulter_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_polevaulter_outerarm_upper", IMAGE_REANIM_ZOMBIE_POLEVAULTER_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_BALLOON:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_outerarm_upper", IMAGE_REANIM_ZOMBIE_BALLOON_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_IMP:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_imp_outerarm_upper", IMAGE_REANIM_ZOMBIE_IMP_ARM1_BONE);
			break;
		case ZombieType::ZOMBIE_DIGGER:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_digger_outerarm_upper", IMAGE_REANIM_ZOMBIE_DIGGER_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_BOBSLED:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_dolphinrider_outerarm_upper", IMAGE_REANIM_ZOMBIE_BOBSLED_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_JACK_IN_THE_BOX:
			GetTrackPosition("Zombie_jackbox_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_jackbox_outerarm_lower", IMAGE_REANIM_ZOMBIE_JACKBOX_OUTERARM_LOWER2);
			break;
		case ZombieType::ZOMBIE_SNORKEL:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_snorkle_outerarm_upper", IMAGE_REANIM_ZOMBIE_SNORKLE_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_DOLPHIN_RIDER:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_dolphinrider_outerarm_upper", IMAGE_REANIM_ZOMBIE_DOLPHINRIDER_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_POGO:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_outerarm_upper", IMAGE_REANIM_ZOMBIE_POGO_OUTERARM_UPPER2);
			aBodyReanim->SetImageOverride("Zombie_pogo_stickhands", IMAGE_REANIM_ZOMBIE_POGO_STICKHANDS2);
			aBodyReanim->SetImageOverride("Zombie_pogo_stick", IMAGE_REANIM_ZOMBIE_POGO_STICKDAMAGE2);
			aBodyReanim->SetImageOverride("Zombie_pogo_stick2", IMAGE_REANIM_ZOMBIE_POGO_STICK2DAMAGE2);
			break;
		case ZombieType::ZOMBIE_FLAG:
		{
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_outerarm_upper", IMAGE_REANIM_ZOMBIE_OUTERARM_UPPER2);

			Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
			if (aHeadReanim)
			{
				aHeadReanim->SetImageOverride("Zombie_flag", IMAGE_REANIM_ZOMBIE_FLAG3);
			}
			break;
		}
		case ZombieType::ZOMBIE_DANCER:
			GetTrackPosition("Zombie_disco_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_disco_outerarm_upper", IMAGE_REANIM_ZOMBIE_DISCO_OUTERARM_UPPER2); // GOTY assets use a different name
			break;
		case ZombieType::ZOMBIE_BACKUP_DANCER:
			GetTrackPosition("Zombie_disco_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_disco_outerarm_upper", IMAGE_REANIM_ZOMBIE_BACKUP_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_LADDER:
			GetTrackPosition("Zombie_outerarm_hand", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_ladder_outerarm_upper", IMAGE_REANIM_ZOMBIE_LADDER_OUTERARM_UPPER2);
			break;
		case ZombieType::ZOMBIE_YETI:
			GetTrackPosition("Zombie_outerarm_hand", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_yeti_outerarm_upper", IMAGE_REANIM_ZOMBIE_YETI_OUTERARM_UPPER2);
			break;
		default:
			GetTrackPosition("Zombie_outerarm_lower", aPosX, aPosY);
			aBodyReanim->SetImageOverride("Zombie_outerarm_upper", IMAGE_REANIM_ZOMBIE_OUTERARM_UPPER2);
			break;
		}
	}

	if (!mInPool && !TestBit(theDamageFlags, DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
	{
		ParticleEffect aEffect = ParticleEffect::PARTICLE_ZOMBIE_ARM;
		if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
		{
			aEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM;
		}

		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 1, aEffect);
		OverrideParticleColor(aParticle);
		OverrideParticleScale(aParticle);

		if (aParticle)
		{
			switch (mZombieType)
			{
			case ZombieType::ZOMBIE_FOOTBALL:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_FOOTBALL_LEFTARM_HAND);
				break;
			case ZombieType::ZOMBIE_NEWSPAPER:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_PAPER_LEFTARM_LOWER);
				break;
			case ZombieType::ZOMBIE_DANCER:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_DISCO_OUTERARM_HAND);
				break;
			case ZombieType::ZOMBIE_BACKUP_DANCER:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_BACKUP_INNERARM_HAND);
				break;
			case ZombieType::ZOMBIE_BOBSLED:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_BOBSLED_OUTERARM_HAND);
				break;
			case ZombieType::ZOMBIE_IMP:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_IMP_ARM2);
				break;
			case ZombieType::ZOMBIE_YETI:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_YETI_OUTERARM_HAND);
				break;
			case ZombieType::ZOMBIE_JACK_IN_THE_BOX:
				aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEJACKBOXARM);
				break;
			case ZombieType::ZOMBIE_DIGGER:
				aParticle->OverrideImage(nullptr, IMAGE_ZOMBIEDIGGERARM);
				break;
			case ZombieType::ZOMBIE_POLEVAULTER:
			case ZombieType::ZOMBIE_BALLOON:
			case ZombieType::ZOMBIE_DOLPHIN_RIDER:
			case ZombieType::ZOMBIE_POGO:
			case ZombieType::ZOMBIE_LADDER:
				aParticle->OverrideImage(nullptr, IMAGE_REANIM_ZOMBIE_OUTERARM_HAND);
				break;
			default:
				break;
			}
		}
	}
}

void Zombie::DropArm(unsigned int theDamageFlags)
{
	if (!CanLoseBodyParts())
	{
		return;
	}
	if (mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
	{
		return;
	}
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
		mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP || mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_READING)
	{
		return;
	}
	if (!mHasArm)
	{
		return;
	}

	mHasArm = false;
	SetupReanimForLostArm(theDamageFlags);
	mApp->PlayFoley(FoleyType::FOLEY_LIMBS_POP);
}

void Zombie::UpdateDamageStates(unsigned int theDamageFlags)
{
	if (!CanLoseBodyParts())
		return;

	if (mHasArm && mBodyHealth < 2 * mBodyMaxHealth / 3 && mBodyHealth > 0)
	{
		DropArm(theDamageFlags);
	}

	if (mHasHead && mBodyHealth < mBodyMaxHealth / 3)
	{
		DropHead(theDamageFlags);
		DropLoot();
		StopZombieSound();

		if (mBoard->HasLevelAwardDropped())
		{
			PlayDeathAnim(theDamageFlags);
		}

		if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
		{
			DieNoLoot();
		}
	}
}

void Zombie::ZamboniDeath(unsigned int theDamageFlags)
{
	if (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE))
	{
		mFlatTires = true;
		mApp->PlayFoley(FoleyType::FOLEY_TIRE_POP);
		mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
		mApp->AddPvzpParticle(mPosX + 29.0f, mPosY + 114.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_TIRE);
		mVelX = 0.0f;

		if (Rand(4) == 0 && mPosX < 600.0f)
		{
			PlayZombieReanim("anim_wheelie2", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 10.0f);
			mPhaseCounter = 280;
		}
		else
		{
			Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
			PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
			if (aParticle)
			{
				aBodyReanim->AttachParticleToTrack("zombie_zamboni_1", aParticle, 35.0f, 85.0f);
			}

			mPhaseCounter = 280;
			PlayZombieReanim("anim_wheelie1", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
		}
	}
	else
	{
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION);
		DieWithLoot();
		mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
	}
}

void Zombie::CatapultDeath(unsigned int theDamageFlags)
{
	if (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE))
	{
		mApp->PlayFoley(FoleyType::FOLEY_TIRE_POP);
		mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
		mApp->AddPvzpParticle(mPosX + 29.0f, mPosY + 114.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_TIRE);
		mVelX = 0.0f;

		AddAttachedParticle(47, 77, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
		mPhaseCounter = 280;
		PlayZombieReanim("anim_bounce", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
	}
	else
	{
		mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION);
		DieWithLoot();
		mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
	}
}

void Zombie::CheckSquish(ZombieAttackType theAttackType)
{
	Rect aAttackRect = GetZombieAttackRect();

	for (Plant* aPlant : mBoard->mPlants)
	{
		if (aPlant->mDead)
			continue;
		if (aPlant->mRow == mRow)
		{
			Rect aPlantRect = aPlant->GetPlantRect();
			if (GetRectOverlap(aAttackRect, aPlantRect) >= 20 && CanTargetPlant(aPlant, theAttackType) && !aPlant->IsSpiky())
			{
				SquishAllInSquare(aPlant->mPlantCol, aPlant->mRow, theAttackType);
				break;
			}
		}
	}

	if (mApp->IsIZombieLevel())
	{
		GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
		if (aBrain)
		{
			mBoard->mChallenge->IZombieSquishBrain(aBrain);
		}
	}
}

void Zombie::DropShield(unsigned int theDamageFlags)
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE)
		return;

	//ZombieDrawPosition aDrawPos;
	//GetDrawPos(aDrawPos);
	if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
	{
		DetachShield();
		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
		{
			float aPosX, aPosY;
			GetTrackPosition("anim_screendoor", aPosX, aPosY);
			PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_DOOR);
			OverrideParticleScale(aParticle);
		}
	}
	else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
	{
		StopEating();
		if (mYuckyFace)
		{
			ShowYuckyFace(false);
			mYuckyFace = false;
			mYuckyFaceCounter = 0;
		}

		mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_MADDENING;
		if (mBoard != nullptr && mBoard->mZombieTierSunMoney >= TWO_MILLION_SUN_THRESHOLD)
		{
			mBodyHealth = static_cast<int32_t>(std::min<int64_t>(
				static_cast<int64_t>(mBodyHealth) * 2, std::numeric_limits<int32_t>::max()));
			mBodyMaxHealth = static_cast<int32_t>(std::min<int64_t>(
				static_cast<int64_t>(mBodyMaxHealth) * 2, std::numeric_limits<int32_t>::max()));
		}
		PlayZombieReanim("anim_gasp", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 8.0f);
		DetachShield();

		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
		{
			float aPosX, aPosY;
			GetTrackPosition("Zombie_paper_paper", aPosX, aPosY);
			PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER);
			OverrideParticleScale(aParticle);
		}

		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)) && !TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_BYPASSES_SHIELD)))
		{
			mApp->PlayFoley(FoleyType::FOLEY_NEWSPAPER_RIP);
			AddAttachedReanim(-11, 0, ReanimationType::REANIM_ZOMBIE_SURPRISE);
		}
	}
	else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
	{
		DetachShield();
		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
		{
			float aPosX = mPosX + 31.0f;
			float aPosY = mPosY + 80.0f;
			PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_LADDER);
			OverrideParticleScale(aParticle);
		}
	}

	mShieldType = ShieldType::SHIELDTYPE_NONE;
}

int Zombie::TakeShieldDamage(int theDamage, unsigned int theDamageFlags)
{
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
	{
		mShieldJustGotShotCounter = 25;
		mJustGotShotCounter = std::max(mJustGotShotCounter, 0);
	}

	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)) && !TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY)))
	{
		mShieldRecoilCounter = 12;
		if (mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_LADDER)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
		}
	}

	int aDamageIndexBeforeDamage = GetShieldDamageIndex();
	int aDamageActual = std::min(mShieldHealth, theDamage);
	int aDamageRemaining = theDamage - aDamageActual;
	mShieldHealth -= aDamageActual;
	if (mShieldHealth == 0)
	{
		DropShield(theDamageFlags);
		return aDamageRemaining;
	}

	int aDamageIndexAfterDamage = GetShieldDamageIndex();
	if (aDamageIndexAfterDamage != aDamageIndexBeforeDamage)
	{
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (mShieldType == ShieldType::SHIELDTYPE_DOOR && aDamageIndexAfterDamage == 1)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("anim_screendoor", IMAGE_REANIM_ZOMBIE_SCREENDOOR2);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_DOOR && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("anim_screendoor", IMAGE_REANIM_ZOMBIE_SCREENDOOR3);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER && aDamageIndexAfterDamage == 1)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_paper_paper", IMAGE_REANIM_ZOMBIE_PAPER_PAPER2);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_paper_paper", IMAGE_REANIM_ZOMBIE_PAPER_PAPER3);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_LADDER && aDamageIndexAfterDamage == 1)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_ladder_1", IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE1);
		}
		else if (mShieldType == ShieldType::SHIELDTYPE_LADDER && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_ladder_1", IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE2);
		}
	}

	return aDamageRemaining;
}

void Zombie::DropHelm(unsigned int theDamageFlags)
{
	if (mHelmType == HelmType::HELMTYPE_NONE)
		return;

	ZombieDrawPosition aDrawPos;
	GetDrawPos(aDrawPos);
	float aPosX = mPosX + aDrawPos.mImageOffsetX + aDrawPos.mHeadX + 14.0f;
	float aPosY = mPosY + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY + 18.0f;
	ParticleEffect aEffect = ParticleEffect::PARTICLE_NONE;
	if (mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE)
	{
		GetTrackPosition("anim_cone", aPosX, aPosY);
		ReanimShowPrefix("anim_cone", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_TRAFFIC_CONE;
	}
	else if (mHelmType == HelmType::HELMTYPE_PAIL)
	{
		if (mZombieType != ZombieType::ZOMBIE_IMP)
		{
			GetTrackPosition("anim_bucket", aPosX, aPosY);
			ReanimShowPrefix("anim_bucket", RENDER_GROUP_HIDDEN);
			ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);
		}
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_PAIL;
	}
	else if (mHelmType == HelmType::HELMTYPE_FOOTBALL)
	{
		GetTrackPosition("zombie_football_helmet", aPosX, aPosY);
		ReanimShowPrefix("zombie_football_helmet", RENDER_GROUP_HIDDEN);
		ReanimShowPrefix("anim_hair", RENDER_GROUP_NORMAL);
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_HELMET;
	}
	else if (mHelmType == HelmType::HELMTYPE_DIGGER)
	{
		GetTrackPosition("Zombie_digger_hardhat", aPosX, aPosY);
		ReanimShowTrack("Zombie_digger_hardhat", RENDER_GROUP_HIDDEN);
		aEffect = ParticleEffect::PARTICLE_ZOMBIE_HEADLIGHT;
	}
	else if (mHelmType == HelmType::HELMTYPE_BOBSLED && !TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
	{
		BobsledCrash();
	}

	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)) && aEffect != ParticleEffect::PARTICLE_NONE)
	{
		PvzpParticleSystem* aParticle = mApp->AddPvzpParticle(aPosX, aPosY, mRenderOrder + 1, aEffect);
		OverrideParticleScale(aParticle);
	}

	mHelmType = HelmType::HELMTYPE_NONE;
}

int Zombie::TakeHelmDamage(int theDamage, unsigned int theDamageFlags)
{
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
	{
		mJustGotShotCounter = 25;
	}

	int aDamageIndexBeforeDamage = GetHelmDamageIndex();
	int aDamageActual = std::min(mHelmHealth, theDamage);
	int aDamageRemaining = theDamage - aDamageActual;
	mHelmHealth -= aDamageActual;
	if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_FREEZE)))
	{
		ApplyChill(false);
	}
	if (mHelmHealth == 0)
	{
		DropHelm(theDamageFlags);
		return aDamageRemaining;
	}

	int aDamageIndexAfterDamage = GetHelmDamageIndex();
	if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
	{
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE && aDamageIndexAfterDamage == 1 && aBodyReanim)
		{
			aBodyReanim->SetImageOverride("anim_cone", IMAGE_REANIM_ZOMBIE_CONE2);
		}
		else if (mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE && aDamageIndexAfterDamage == 2 && aBodyReanim)
		{
			aBodyReanim->SetImageOverride("anim_cone", IMAGE_REANIM_ZOMBIE_CONE3);
		}
		else if (mHelmType == HelmType::HELMTYPE_PAIL && mZombieType != ZombieType::ZOMBIE_IMP && aDamageIndexAfterDamage == 1)
		{
			aBodyReanim->SetImageOverride("anim_bucket", IMAGE_REANIM_ZOMBIE_BUCKET2);
		}
		else if (mHelmType == HelmType::HELMTYPE_PAIL && mZombieType != ZombieType::ZOMBIE_IMP && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("anim_bucket", IMAGE_REANIM_ZOMBIE_BUCKET3);
		}
		else if (mHelmType == HelmType::HELMTYPE_DIGGER && aDamageIndexAfterDamage == 1)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_digger_hardhat", IMAGE_REANIM_ZOMBIE_DIGGER_HARDHAT2);
		}
		else if (mHelmType == HelmType::HELMTYPE_DIGGER && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("Zombie_digger_hardhat", IMAGE_REANIM_ZOMBIE_DIGGER_HARDHAT3);
		}
		else if (mHelmType == HelmType::HELMTYPE_FOOTBALL && aDamageIndexAfterDamage == 1)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("zombie_football_helmet", IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET2);
		}
		else if (mHelmType == HelmType::HELMTYPE_FOOTBALL && aDamageIndexAfterDamage == 2)
		{
			PVZP_ASSERT(aBodyReanim);
			aBodyReanim->SetImageOverride("zombie_football_helmet", IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET3);
		}
		else if (mHelmType == HelmType::HELMTYPE_WALLNUT && aDamageIndexAfterDamage == 1)
		{
			Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
			aHeadReanim->SetImageOverride("anim_face", IMAGE_REANIM_WALLNUT_CRACKED1);
		}
		else if (mHelmType == HelmType::HELMTYPE_WALLNUT && aDamageIndexAfterDamage == 2)
		{
			Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
			aHeadReanim->SetImageOverride("anim_face", IMAGE_REANIM_WALLNUT_CRACKED2);
		}
		else if (mHelmType == HelmType::HELMTYPE_TALLNUT && aDamageIndexAfterDamage == 1)
		{
			Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
			aHeadReanim->SetImageOverride("anim_idle", IMAGE_REANIM_TALLNUT_CRACKED1);
		}
		else if (mHelmType == HelmType::HELMTYPE_TALLNUT && aDamageIndexAfterDamage == 2)
		{
			Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
			aHeadReanim->SetImageOverride("anim_idle", IMAGE_REANIM_TALLNUT_CRACKED2);
		}
	}
	return aDamageRemaining;
}

int Zombie::TakeTierBucketArmorDamage(int theDamage, unsigned int theDamageFlags)
{
	if (mTierBucketArmorHealth <= 0 || theDamage <= 0)
		return theDamage;
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
		mJustGotShotCounter = 25;

	int aDamageActual = std::min(mTierBucketArmorHealth, theDamage);
	mTierBucketArmorHealth -= aDamageActual;
	if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_FREEZE)))
		ApplyChill(false);
	if (mTierBucketArmorHealth == 0)
	{
		float aBucketX = mPosX + 35.0f;
		float aBucketY = mPosY + 10.0f;
		Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
		if (aBodyReanim != nullptr && aBodyReanim->TrackExists("anim_head1"))
			GetTrackPosition("anim_head1", aBucketX, aBucketY);
		mApp->AddPvzpParticle(aBucketX, aBucketY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_PAIL);
	}
	return theDamage - aDamageActual;
}

int Zombie::TakeFlyingDamage(int theDamage, unsigned int theDamageFlags)
{
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
	{
		mJustGotShotCounter = 25;
	}

	int aDamageActual = std::min(mFlyingHealth, theDamage);
	int aDamageRemaining = theDamage - aDamageActual;
	mFlyingHealth -= aDamageActual;
	if (mFlyingHealth == 0)
	{
		LandFlyer(theDamageFlags);
	}

	return aDamageRemaining;
}

void Zombie::TakeBodyDamage(int theDamage, unsigned int theDamageFlags)
{
	if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
	{
		mJustGotShotCounter = 25;
	}

	if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_FREEZE)))
	{
		ApplyChill(false);
	}

	int aBodyHealthOrigin = mBodyHealth;
	int aDamageIndexBeforeDamage = GetBodyDamageIndex();
	mBodyHealth -= theDamage;
	int aDamageIndexAfterDamage = GetBodyDamageIndex();
	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
		}

		if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_SPIKE)))
		{
			aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE2);
			aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE2);
			ZamboniDeath(theDamageFlags);
		}
		else if (mBodyHealth <= 0)
		{
			ZamboniDeath(theDamageFlags);
		}
		else if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
		{
			if (aDamageIndexAfterDamage == 1)
			{
				aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE1);
				aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE1);
			}
			else if (aDamageIndexAfterDamage == 2)
			{
				aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE2);
				aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE2);
				AddAttachedParticle(27, 72, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
			}
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_SPIKE)) || mBodyHealth <= 0)
		{
			aBodyReanim->SetImageOverride("Zombie_catapult_siding", IMAGE_REANIM_ZOMBIE_CATAPULT_SIDING_DAMAGE);
			CatapultDeath(theDamageFlags);
		}
		else if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
		{
			if (aDamageIndexAfterDamage == 1)
			{
				aBodyReanim->SetImageOverride("Zombie_catapult_siding", IMAGE_REANIM_ZOMBIE_CATAPULT_SIDING_DAMAGE);
			}
			else if (aDamageIndexAfterDamage == 2)
			{
				AddAttachedParticle(47, 77, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
			}
		}
	}
	else if (ZombieRules::IsGargantuarType(mZombieType))
	{
		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
		{
			if (aDamageIndexAfterDamage == 1)
			{
				aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_2);
				aBodyReanim->SetImageOverride("Zombie_gargantuar_outerarm_lower", IMAGE_REANIM_ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
			}
			else if (aDamageIndexAfterDamage == 2)
			{
				aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_3);
				aBodyReanim->SetImageOverride("Zombie_gargantuar_outerleg_foot", IMAGE_REANIM_ZOMBIE_GARGANTUAR_FOOT2);
				aBodyReanim->SetImageOverride("Zombie_gargantuar_outerarm_lower", IMAGE_REANIM_ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
				if (mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
				{
					aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2_REDEYE);
				}
				else
				{
					aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2);
				}
			}
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		if (!TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH)))
		{
			mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
		}

		Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
		if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
		{
			if (aDamageIndexAfterDamage == 1)
			{
				aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE1);
				aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE1);
				aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE1);
				aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE1);
				aBodyReanim->SetImageOverride("Boss_innerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE1);
			}
			else if (aDamageIndexAfterDamage == 2)
			{
				aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE2);
				aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE2);
				aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE2);
				aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE2);
				aBodyReanim->SetImageOverride("Boss_outerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE2);
				ApplyBossSmokeParticles(true);
			}
		}

		if (aBodyHealthOrigin >= mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION && mBodyHealth < mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
		{
			mApp->AddPvzpParticle(770.0f, 260.0f, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_BOSS_EXPLOSION);
			mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
			ApplyBossSmokeParticles(true);
		}

		if (mBodyHealth <= 0)
		{
			mBodyHealth = 1;
		}
	}
	else
	{
		UpdateDamageStates(theDamageFlags);
	}

	if (mBodyHealth <= 0)
	{
		mBodyHealth = 0;
		PlayDeathAnim(theDamageFlags);
		DropLoot();
	}
}

void Zombie::TakeDamage(int theDamage, unsigned int theDamageFlags)
{
	if (IsSunTierInvulnerable())
		return;
	if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING || IsDeadOrDying())
		return;
	if (!CanBeTargetedByPlants())
		return;

	int aDamageRemaining = theDamage;

	if (IsFlying())
	{
		aDamageRemaining = TakeFlyingDamage(aDamageRemaining, theDamageFlags);
	}
	if (aDamageRemaining > 0 && mShieldType != ShieldType::SHIELDTYPE_NONE && !TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_BYPASSES_SHIELD)))
	{
		aDamageRemaining = TakeShieldDamage(aDamageRemaining, theDamageFlags);
		if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY)))
		{
			aDamageRemaining = theDamage;
		}
	}
	if (aDamageRemaining > 0 && mTierBucketArmorHealth > 0)
		aDamageRemaining = TakeTierBucketArmorDamage(aDamageRemaining, theDamageFlags);
	if (aDamageRemaining > 0 && mHelmType != HelmType::HELMTYPE_NONE)
	{
		aDamageRemaining = TakeHelmDamage(aDamageRemaining, theDamageFlags);
	}
	if (aDamageRemaining > 0)
	{
		TakeBodyDamage(aDamageRemaining, theDamageFlags);
	}
}
