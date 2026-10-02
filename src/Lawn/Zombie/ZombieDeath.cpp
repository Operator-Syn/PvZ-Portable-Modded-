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

bool Zombie::TrySpawnLevelAward()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_PLANT_PRACTICE || !IsOnBoard() ||
		mBoard->HasLevelAwardDropped() || mBoard->mLevelComplete || mDroppedLoot)
	{
		return false;
	}

	if (mApp->IsFinalBossLevel())
	{
		if (mZombieType != ZombieType::ZOMBIE_BOSS)
		{
			return false;
		}
	}
	else if (mApp->IsScaryPotterLevel())
	{
		if (!mBoard->mChallenge->ScaryPotterIsCompleted())
		{
			return false;
		}
	}
	else if (mApp->IsContinuousChallenge() || mBoard->mCurrentWave < mBoard->mNumWaves || mBoard->AreEnemyZombiesOnScreen())
	{
		return false;
	}

	if (mApp->IsWhackAZombieLevel() && mBoard->mZombieCountDown > 0)
	{
		return false;
	}

	mBoard->mLevelAwardSpawned = true;
	mApp->mBoardResult = BoardResult::BOARDRESULT_WON;

	Rect aZombieRect = GetZombieRect();
	int aCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
	int aCenterY = aZombieRect.mY + aZombieRect.mHeight / 2;

	if (!mBoard->IsSurvivalStageWithRepick())
	{
		mBoard->RemoveAllZombies();
	}

	CoinType aCoinType;
	if (mApp->IsScaryPotterLevel() && !mBoard->IsFinalScaryPotterStage())
	{
		aCoinType = CoinType::COIN_NONE;
		mBoard->mChallenge->PuzzlePhaseComplete(mBoard->PixelToGridXKeepOnBoard(mPosX + 75, mPosY), mRow);
	}
	else if (mApp->IsAdventureMode() && mBoard->mLevel <= 50)
	{
		if (mBoard->mLevel == 9 || mBoard->mLevel == 19 || mBoard->mLevel == 29 || mBoard->mLevel == 39 || mBoard->mLevel == 49)
		{
			aCoinType = CoinType::COIN_NOTE;
		}
		else if (mBoard->mLevel == 50)
		{
			aCoinType = mApp->HasFinishedAdventure() ? CoinType::COIN_AWARD_MONEY_BAG : CoinType::COIN_AWARD_SILVER_SUNFLOWER;
		}
		else if (mApp->HasFinishedAdventure())
		{
			aCoinType = CoinType::COIN_AWARD_MONEY_BAG;
		}
		else if (mBoard->mLevel == 4)
		{
			aCoinType = CoinType::COIN_SHOVEL;
		}
		else if (mBoard->mLevel == 14)
		{
			aCoinType = CoinType::COIN_ALMANAC;
		}
		else if (mBoard->mLevel == 24)
		{
			aCoinType = CoinType::COIN_CARKEYS;
		}
		else if (mBoard->mLevel == 34)
		{
			aCoinType = CoinType::COIN_TACO;
		}
		else if (mBoard->mLevel == 44)
		{
			aCoinType = CoinType::COIN_WATERING_CAN;
		}
		else
		{
			aCoinType = CoinType::COIN_FINAL_SEED_PACKET;
		}
	}
	else if (mBoard->IsSurvivalStageWithRepick())
	{
		aCoinType = CoinType::COIN_NONE;
		mBoard->FadeOutLevel();
	}
	else if (mBoard->IsLastStandStageWithRepick())
	{
		aCoinType = CoinType::COIN_NONE;

		mBoard->FadeOutLevel();
		mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
		for (int i = 0; i < 10; i++)
		{
			mBoard->AddCoin(aCenterX + i * 5, aCenterY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
		}
	}
	else if (!mApp->IsAdventureMode())
	{
		if (mApp->HasBeatenChallenge(mApp->mGameMode))
		{
			aCoinType = CoinType::COIN_AWARD_MONEY_BAG;
		}
		else if (mApp->TrophiesNeedForGoldSunflower() == 1)
		{
			aCoinType = CoinType::COIN_AWARD_GOLD_SUNFLOWER;
		}
		else
		{
			aCoinType = CoinType::COIN_TROPHY;
		}
	}
	else
	{
		aCoinType = CoinType::COIN_AWARD_MONEY_BAG;
	}

	CoinMotion aCoinMotion = CoinMotion::COIN_MOTION_COIN;
	if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		aCoinMotion = CoinMotion::COIN_MOTION_FROM_BOSS;
	}

	if (aCoinType != CoinType::COIN_NONE)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
		mBoard->AddCoin(aCenterX, aCenterY, aCoinType, aCoinMotion);
	}

	mDroppedLoot = true;
	return true;
}

void Zombie::DropLoot()
{
	if (!IsOnBoard())
		return;

	AlmanacPlayerDefeatedZombie(mZombieType);
	if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		mBoard->mKilledYeti = true;
	}

	TrySpawnLevelAward();
	if (mDroppedLoot || mBoard->HasLevelAwardDropped() || !mBoard->CanDropLoot())
		return;

	mDroppedLoot = true;
	int aZombieValue = GetZombieDefinition(mZombieType).mZombieValue;
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIQUARIUM || mApp->IsIZombieLevel())
	{
		return;
	}
	if (mApp->IsLittleTroubleLevel() && Rand(4) != 0)
	{
		return;
	}

	Rect aZombieRect = GetZombieRect();
	int aCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
	int aCenterY = aZombieRect.mY + aZombieRect.mHeight / 4;
	if (Rand(4) == 0)
	{
		Coin* aSunCoin = mBoard->AddCoin(aCenterX, aCenterY, CoinType::COIN_ZOMBIE_SUN_DROP, CoinMotion::COIN_MOTION_COIN);
		aSunCoin->mTimesDropped = RandRangeInt(100, 200);
		mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
	}
	if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
		mBoard->AddCoin(aCenterX - 20, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
		mBoard->AddCoin(aCenterX - 30, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
		mBoard->AddCoin(aCenterX - 40, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
		mBoard->AddCoin(aCenterX - 50, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
	}
	else
	{
		mBoard->DropLootPiece(aCenterX, aCenterY, aZombieValue);
	}
}

void Zombie::DieWithLoot()
{
	DieNoLoot();
	DropLoot();
}

void Zombie::BobsledDie()
{
	if (!IsBobsledTeamWithSled() || !IsOnBoard())
		return;

	Zombie* aLeaderZombie;
	if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
	{
		aLeaderZombie = this;
	}
	else
	{
		aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
	}

	if (!aLeaderZombie->mDead)
	{
		aLeaderZombie->DieNoLoot();
	}
	for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
	{
		Zombie* aZombie = mBoard->ZombieGet(aLeaderZombie->mFollowerZombieID[i]);
		if (!aZombie->mDead)
		{
			aZombie->DieNoLoot();
		}
	}
}

void Zombie::BobsledBurn()
{
	if (!IsBobsledTeamWithSled())
		return;

	Zombie* aLeaderZombie;
	if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
	{
		aLeaderZombie = this;
	}
	else
	{
		aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
	}

	aLeaderZombie->ApplyBurn();
	for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
	{
		mBoard->ZombieGet(aLeaderZombie->mFollowerZombieID[i])->DieNoLoot();
	}
}

void Zombie::BungeeDropPlant()
{
	if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_GRABBING)
	{
		Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(mTargetPlantID);
		if (aPlant)
		{
			if (aPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				aPlant->mOnBungeeState = PlantOnBungeeState::NOT_ON_BUNGEE;
			}
			else if (aPlant->mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE)
			{
				aPlant->Die();
			}

			mTargetPlantID = PlantID::PLANTID_NULL;
		}
	}
}

void Zombie::BungeeDie()
{
	BungeeDropPlant();

	if (mBoard)  // null check added for safety; DataArrayTryToGet() does not dereference mBoard
	{
		Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(static_cast<unsigned int>(mTargetPlantID));
		if (aPlant)
		{
			mBoard->mPlantsEaten++;
			aPlant->Die();
		}
	}

	Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
	if (aZombie && !aZombie->mDead)
	{
		aZombie->DieNoLoot();
	}
}

void Zombie::DieNoLoot()
{
	StopZombieSound();
	AttachmentDie(mAttachmentID);
	mApp->RemoveReanimation(mBodyReanimID);
	mApp->RemoveReanimation(mMoweredReanimID);
	mApp->RemoveReanimation(mSpecialHeadReanimID);
	mApp->RemoveReanimation(mZombatarHeadReanimID);

	mDead = true;
	TrySpawnLevelAward();
	if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		BobsledDie();
	}
	if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		BungeeDie();
	}
	if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		BossDie();
	}
}

void Zombie::PlayDeathAnim(unsigned int theDamageFlags)
{
	if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr || !aBodyReanim->TrackExists("anim_death"))
	{
		DieNoLoot();
		return;
	}
	if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER && mZombiePhase != ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL)
	{
		DieNoLoot();
		return;
	}
	if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING)
	{
		DieNoLoot();
		return;
	}

	if (mIceTrapCounter > 0)
	{
		AddAttachedParticle(75, 106, ParticleEffect::PARTICLE_ICE_TRAP_RELEASE);
		mIceTrapCounter = 0;
	}
	mButteredCounter = std::min(mButteredCounter, 0);
	if (mYuckyFace)
	{
		ShowYuckyFace(false);
		mYuckyFace = false;
		mYuckyFaceCounter = 0;
	}

	if (TestBit(theDamageFlags, static_cast<int>(DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)))
	{
		if (mZombieType != ZombieType::ZOMBIE_BOSS && !ZombieRules::IsGargantuarType(mZombieType))
		{
			DieNoLoot();
			return;
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		mAltitude = 0.0f;
	}

	AttachmentReanimTypeDie(mAttachmentID, ReanimationType::REANIM_ZOMBIE_SURPRISE);
	StopEating();

	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		DropShield(1U);
	}
	if (mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD && !mHasHead)
	{
		mApp->RemoveReanimation(mSpecialHeadReanimID);
		mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	mVelX = 0.0f;
	mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
	if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
	{
		PlayZombieReanim("anim_aquarium_death", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 14.0f);
		return;
	}
	if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER)
	{
		mZombieHeight = ZombieHeight::HEIGHT_FALLING;
	}

	float aDeathAnimRate;
	if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
	{
		aDeathAnimRate = 24.0f;
	}
	else if (ZombieRules::IsGargantuarType(mZombieType))
	{
		aDeathAnimRate = 14.0f;
		mApp->PlayFoley(FoleyType::FOLEY_GARGANTUDEATH);
	}
	else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
	{
		aDeathAnimRate = 14.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		aDeathAnimRate = 18.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_YETI)
	{
		aDeathAnimRate = 14.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		aDeathAnimRate = 18.0f;

		BossDie();
		Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
		aHeadReanim->PlayReanim("anim_death", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aDeathAnimRate);
	}
	else
	{
		aDeathAnimRate = RandRangeFloat(24.0f, 30.0f);
	}

	const char* aDeathTrackName = "anim_death";
	int aDeathAnimHit = Rand(100);
	bool aCanDoSuperLongDeath = mApp->HasFinishedAdventure() || mBoard->mLevel > 5;
	if (mInPool && aBodyReanim->TrackExists("anim_waterdeath"))
	{
		aDeathTrackName = "anim_waterdeath";
		ReanimIgnoreClipRect("Zombie_duckytube", false);
	}
	else if (aDeathAnimHit == 99 && aBodyReanim->TrackExists("anim_superlongdeath") && aCanDoSuperLongDeath && mChilledCounter == 0 && mBoard->CountZombiesOnScreen() <= 5)
	{
		aDeathAnimRate = 14.0f;
		aDeathTrackName = "anim_superlongdeath";
	}
	else if (aDeathAnimHit > 50 && aBodyReanim->TrackExists("anim_death2"))
	{
		aDeathTrackName = "anim_death2";
	}

	PlayZombieReanim(aDeathTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aDeathAnimRate);
	ReanimShowPrefix("anim_tongue", RENDER_GROUP_HIDDEN);
}

void Zombie::DoDaisies()
{
	if (IsWalkingBackwards())
		return;

	if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
		return;

	if (mZombieType == ZombieType::ZOMBIE_BOBSLED || mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
		return;

	if (mBoard->StageHasRoof())
		return;

	float aOffsetX = 20.0f;
	float aOffsetY = 100.0f;
	if (mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		aOffsetX += 160.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_POGO)
	{
		aOffsetY += 120.0f;
	}
	else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		aOffsetY += 30.0f;
		aOffsetX += 110.0f;
	}
	if (mBoard->StageHasGraveStones())
	{
		aOffsetY += 15.0f;
	}

	int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mRow, 5);
	mApp->AddPvzpParticle(mX + aOffsetX, mY + aOffsetY, aRenderPosition, ParticleEffect::PARTICLE_ZOMBIE_DAISIES);
}

void Zombie::UpdateDeath()
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	if (aBodyReanim == nullptr)
	{
		DieNoLoot();
		return;
	}

	if (mZombieHeight == ZombieHeight::HEIGHT_FALLING)
	{
		UpdateZombieFalling();
	}
	if (ZombieRules::IsGargantuarType(mZombieType))
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.89f))
		{
			mBoard->ShakeBoard(0, 3);
		}
		else if (aBodyReanim->ShouldTriggerTimedEvent(0.98f))
		{
			mBoard->ShakeBoard(0, 1);
		}
	}

	if (!mInPool)
	{
		float aFallTime;
		switch (mZombieType)
		{
		case ZombieType::ZOMBIE_SNORKEL:
		case ZombieType::ZOMBIE_ZAMBONI:
		case ZombieType::ZOMBIE_DOLPHIN_RIDER:
		case ZombieType::ZOMBIE_BUNGEE:
		case ZombieType::ZOMBIE_CATAPULT:
		case ZombieType::ZOMBIE_IMP:
		case ZombieType::ZOMBIE_BOSS:
			aFallTime = -1.0f;
			break;

		case ZombieType::ZOMBIE_NORMAL:
		case ZombieType::ZOMBIE_FLAG:
		case ZombieType::ZOMBIE_TRAFFIC_CONE:
		case ZombieType::ZOMBIE_PAIL:
		case ZombieType::ZOMBIE_BULWARK_BUCKET:
		case ZombieType::ZOMBIE_DOOR:
		case ZombieType::ZOMBIE_PEA_HEAD:
		case ZombieType::ZOMBIE_WALLNUT_HEAD:
		case ZombieType::ZOMBIE_TALLNUT_HEAD:
		case ZombieType::ZOMBIE_JALAPENO_HEAD:
		case ZombieType::ZOMBIE_GATLING_HEAD:
		case ZombieType::ZOMBIE_SQUASH_HEAD:
		case ZombieType::ZOMBIE_DUCKY_TUBE:
			if (aBodyReanim->IsAnimPlaying("anim_superlongdeath"))
			{
				aFallTime = 0.788f;
			}
			else if (aBodyReanim->IsAnimPlaying("anim_death2"))
			{
				aFallTime = 0.71f;
			}
			else
			{
				aFallTime = 0.77f;
			}
			break;

		case ZombieType::ZOMBIE_POLEVAULTER:
			aFallTime = 0.68f;
			break;

		case ZombieType::ZOMBIE_FOOTBALL:
			aFallTime = 0.52f;
			break;

		case ZombieType::ZOMBIE_NEWSPAPER:
			aFallTime = 0.63f;
			break;

		case ZombieType::ZOMBIE_DANCER:
		case ZombieType::ZOMBIE_BACKUP_DANCER:
			aFallTime = 0.83f;
			break;

		case ZombieType::ZOMBIE_BOBSLED:
			aFallTime = 0.81f;
			break;

		case ZombieType::ZOMBIE_JACK_IN_THE_BOX:
			aFallTime = 0.64f;
			break;

		case ZombieType::ZOMBIE_BALLOON:
			aFallTime = 0.68f;
			break;

		case ZombieType::ZOMBIE_DIGGER:
			aFallTime = 0.85f;
			break;

		case ZombieType::ZOMBIE_POGO:
			aFallTime = 0.84f;
			break;

		case ZombieType::ZOMBIE_YETI:
			aFallTime = 0.68f;
			break;

		case ZombieType::ZOMBIE_LADDER:
			aFallTime = 0.62f;
			break;

		case ZombieType::ZOMBIE_GARGANTUAR:
		case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
		case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
			aFallTime = 0.86f;
			break;

		default:
			aFallTime = -1.0f;
			break;
		}

		if (aFallTime > 0 && aBodyReanim->ShouldTriggerTimedEvent(aFallTime))
		{
			mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_FALLING);
			if (ZombieRules::IsGargantuarType(mZombieType))
			{
				mApp->PlayFoley(FoleyType::FOLEY_THUMP);
			}

			if (mBoard->mDaisyMode)
			{
				DoDaisies();
			}
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_BOSS)
	{
		if (aBodyReanim->ShouldTriggerTimedEvent(0.1f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.12f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.15f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.19f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.2f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.26f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.3f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.4f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.42f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.5f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.58f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.61f) ||
			aBodyReanim->ShouldTriggerTimedEvent(0.71f))
		{
			float aExplosionPosX = RandRangeFloat(600.0f, 750.0f);
			float aExplosionPosY = RandRangeFloat(50.0f, 300.0f);
			mApp->AddPvzpParticle(aExplosionPosX, aExplosionPosY, static_cast<int>(RenderLayer::RENDER_LAYER_TOP), ParticleEffect::PARTICLE_BOSS_EXPLOSION);
			mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
		}

		Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
		if (aBodyReanim->ShouldTriggerTimedEvent(0.93f))
		{
			mBoard->ShakeBoard(1, 2);
			mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
			mApp->PlayFoley(FoleyType::FOLEY_THUMP);
		}

		if (aBodyReanim->ShouldTriggerTimedEvent(0.99f))
		{
			aHeadReanim->PlayReanim("anim_flag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 30.0f);
		}

		if (aHeadReanim->IsAnimPlaying("anim_flag") && aHeadReanim->mLoopCount > 0)
		{
			aHeadReanim->PlayReanim("anim_flag_loop", ReanimLoopType::REANIM_LOOP, 20, 17.0f);
		}

		if (aBodyReanim->mLoopCount > 0)
		{
			DropLoot();
		}
	}

	if (mZombieType == ZombieType::ZOMBIE_ZAMBONI && mPhaseCounter > 0)
	{
		mPhaseCounter--;
		if (mPhaseCounter == 0)
		{
			aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
			if (aBodyReanim->IsTrackShowing("anim_wheelie2"))
			{
				mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION2);
			}
			else
			{
				mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION);
			}

			DieWithLoot();
			mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
		}
	}
	else if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		mPhaseCounter--;
		if (mPhaseCounter == 0)
		{
			mApp->AddPvzpParticle(mPosX + 80.0f, mPosY + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION);
			DieWithLoot();
			mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
		}
	}
	else if (mZombieFade == -1 && aBodyReanim->mLoopCount > 0 && mZombieType != ZombieType::ZOMBIE_BOSS)
	{
		mZombieFade = mInPool ? 10 : 100;
	}
}

void Zombie::UpdateMowered()
{
	Reanimation* aMoweredReanim = mApp->ReanimationTryToGet(mMoweredReanimID);
	if (aMoweredReanim == nullptr || aMoweredReanim->mLoopCount > 0)
	{
		DropHead(0U);
		DropArm(0U);
		DieWithLoot();
	}
}
