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
#include "ZenGarden.h"
#include "BoardInclude.h"
#include "RenderOrderRules.h"
#include "LawnCommon.h"
#include "System/Music.h"
#include "System/SaveGame.h"
#include "Widget/LawnDialog.h"
#include "System/PlayerInfo.h"
#include "System/PoolEffect.h"
#include "System/TypingCheck.h"
#include "Widget/StoreScreen.h"
#include "Widget/AwardScreen.h"
#include "../PvzpLib/Trail.h"
#include "Widget/ChallengeScreen.h"
#include "../PvzpLib/PvzpDebug.h"
#include "../PvzpLib/PvzpFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "../PvzpLib/Attachment.h"
#include "../PvzpLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "../PvzpLib/PvzpParticle.h"
#include "../PvzpLib/EffectSystem.h"
#include "../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

//#define SEXY_PERF_ENABLED
#include "misc/PerfTimer.h"
#include "misc/FrameProfiler.h"
#include "Widget/AchievementsScreen.h"


#include "BoardPlantingPlan.h"
#include "ZombieStrengthRules.h"
#include "PlantRules.h"


static inline RenderItem& AppendRenderItem(std::vector<RenderItem>& theRenderList, RenderObjectType theType, int theZPos)
{
	theRenderList.emplace_back();
	RenderItem& aRenderItem = theRenderList.back();
	aRenderItem.mRenderObjectType = theType;
	aRenderItem.mZPos = theZPos;
	aRenderItem.mGameObject = nullptr;
	return aRenderItem;
}

void Board::AddBossRenderItem(std::vector<RenderItem>& theRenderList, Zombie* theBossZombie)
{
	int aBackLegRow = 1;
	int aFrontLegRow = 3;
	int aBackArmRow = 4;
	if (theBossZombie->IsDeadOrDying())
	{
		aBackArmRow = 1;
	}
	else if (theBossZombie->mZombiePhase == ZombiePhase::PHASE_BOSS_STOMPING)
	{
		Reanimation* aBossReanim = mApp->ReanimationTryToGet(theBossZombie->mBodyReanimID);
		if (aBossReanim->mAnimTime > 0.25f && aBossReanim->mAnimTime < 0.75f)
		{
			if (theBossZombie->mTargetRow == 1)
			{
				aBackLegRow = 2;
			}
			else if (theBossZombie->mTargetRow == 3)
			{
				aFrontLegRow = 4;
			}
		}
	}

	AppendRenderItem(theRenderList, RenderObjectType::RENDER_ITEM_BOSS_PART,
		MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackLegRow, 2)).mBossPart = BossPart::BOSS_PART_BACK_LEG;
	AppendRenderItem(theRenderList, RenderObjectType::RENDER_ITEM_BOSS_PART,
		MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aFrontLegRow, 2)).mBossPart = BossPart::BOSS_PART_FRONT_LEG;
	AppendRenderItem(theRenderList, RenderObjectType::RENDER_ITEM_BOSS_PART,
		MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, 4, 2)).mBossPart = BossPart::BOSS_PART_MAIN;
	AppendRenderItem(theRenderList, RenderObjectType::RENDER_ITEM_BOSS_PART,
		MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackArmRow, 3)).mBossPart = BossPart::BOSS_PART_BACK_ARM;

	Reanimation* aBallReanim = mApp->ReanimationTryToGet(theBossZombie->mBossFireBallReanimID);
	if (aBallReanim)
	{
		AppendRenderItem(theRenderList, RenderObjectType::RENDER_ITEM_BOSS_PART, aBallReanim->mRenderOrder)
			.mBossPart = BossPart::BOSS_PART_FIREBALL;
	}
}

static inline void AddGameObjectRenderItemCursorPreview(std::vector<RenderItem>& theRenderList,
	RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, theGameObject->mRenderOrder);
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCursorPreview = (CursorPreview*)theGameObject;
}

static inline void AddGameObjectRenderItemPlant(std::vector<RenderItem>& theRenderList,
	RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, theGameObject->mRenderOrder);
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mPlant = (Plant*)theGameObject;
}

static inline void AddGameObjectRenderItemZombie(std::vector<RenderItem>& theRenderList,
	RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, theGameObject->mRenderOrder);
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mZombie = (Zombie*)theGameObject;
}

static inline void AddGameObjectRenderItemProjectile(std::vector<RenderItem>& theRenderList,
	RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, theGameObject->mRenderOrder);
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mProjectile = (Projectile*)theGameObject;
}

static inline void AddGameObjectRenderItemCoin(std::vector<RenderItem>& theRenderList,
	RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, theGameObject->mRenderOrder);
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCoin = (Coin*)theGameObject;
}

static inline void AddUIRenderItem(std::vector<RenderItem>& theRenderList, RenderObjectType theRenderObjectType, int thePosZ)
{
	RenderItem& aRenderItem = AppendRenderItem(theRenderList, theRenderObjectType, thePosZ);
	aRenderItem.mGameObject = nullptr;
}

void Board::DrawGameObjects(Graphics* g)
{
	Sexy::FrameProfileScope aGatherScope(Sexy::FrameProfileMetric::RENDER_GATHER);
	mRenderItems.clear();

	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
				continue;
			if (aPlant->mOnBungeeState == PlantOnBungeeState::NOT_ON_BUNGEE)
			{
				AddGameObjectRenderItemPlant(mRenderItems, RenderObjectType::RENDER_ITEM_PLANT, aPlant);

				if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && aPlant->mPottedPlantIndex != -1)
				{
					RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_PLANT_OVERLAY,
						MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, mY));
					aRenderItem.mPlant = aPlant;
				}

				if ((aPlant->mSeedType == SeedType::SEED_MAGNETSHROOM || aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET ||
					aPlant->mSeedType == SeedType::SEED_SUN_MAGNET) && aPlant->DrawMagnetItemsOnTop())
				{
					RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS,
						MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, -1));
					aRenderItem.mPlant = aPlant;
				}
			}
		}
	}
	{
		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
				continue;
			AddGameObjectRenderItemCoin(mRenderItems, RenderObjectType::RENDER_ITEM_COIN, aCoin);
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
				continue;
			if (aZombie->mZombieType == ZombieType::ZOMBIE_BOSS)
			{
				AddBossRenderItem(mRenderItems, aZombie);
			}
			else
			{
				AddGameObjectRenderItemZombie(mRenderItems, RenderObjectType::RENDER_ITEM_ZOMBIE, aZombie);

				if (aZombie->HasShadow())
				{
					RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW,
						MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aZombie->mRow, 3));
					aRenderItem.mZombie = aZombie;
				}

				if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
				{
					RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET,
						MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, aZombie->mRow, 1));
					aRenderItem.mZombie = aZombie;
				}
			}
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
				continue;
			AddGameObjectRenderItemProjectile(mRenderItems, RenderObjectType::RENDER_ITEM_PROJECTILE, aProjectile);

			RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW,
				MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aProjectile->mRow, 3));
			aRenderItem.mProjectile = aProjectile;
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
				continue;
			RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_MOWER, aLawnMower->mRenderOrder);
			aRenderItem.mMower = aLawnMower;
		}
	}
	{
		for (PvzpParticleSystem* aParticle : mApp->mEffectSystem->mParticleHolder->mParticleSystems)
		{
			if (aParticle->mDead)
				continue;
			if (!aParticle->mIsAttachment)
			{
				RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_PARTICLE, aParticle->mRenderOrder);
				aRenderItem.mParticleSytem = aParticle;
			}
		}
	}
	{
		for (Reanimation* aReanimation : mApp->mEffectSystem->mReanimationHolder->mReanimations)
		{
			if (aReanimation->mDead)
				continue;
			if (!aReanimation->mIsAttachment)
			{
				RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_REANIMATION, aReanimation->mRenderOrder);
				aRenderItem.mReanimation = aReanimation;
			}
		}
	}
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
				continue;
			RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_GRID_ITEM, aGridItem->mRenderOrder);
			aRenderItem.mGridItem = aGridItem;

			if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
			{
				RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY,
					MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, aGridItem->mPosY - 30.0f));
				aRenderItem.mGridItem = aGridItem;
			}
		}
	}
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		if (mIceTimer[i])
		{
			RenderItem& aRenderItem = AppendRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_ICE, GetIceZPos(i));
			aRenderItem.mBoardGridY = i;
		}
	}
	{
		int aZPos;
		if (mTimeStopCounter > 0)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else if (mCutScene->IsAfterSeedChooser() || mCutScene->IsInShovelTutorial() || mHelpIndex == AdviceType::ADVICE_CLICK_TO_CONTINUE)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}

		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_BACKDROP, MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 0));
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_BOTTOM_UI, aZPos);
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_COIN_BANK, MakeRenderOrder(RenderLayer::RENDER_LAYER_COIN_BANK, 0, 0));
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_TOP_UI, MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_TOP, 0, 0));
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_SCREEN_FADE, MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0));
	}
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		int aZPos;
		if (StageHasRoof())
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 0, 4);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 3, 2);
		}
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_DOOR_MASK, aZPos);
	}
	if (StageHasFog())
	{
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_FOG, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 0));
	}
	if (mApp->IsStormyNightLevel() || mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS)
	{
		AddUIRenderItem(mRenderItems, RenderObjectType::RENDER_ITEM_STORM, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 3));
	}
	AddGameObjectRenderItemCursorPreview(mRenderItems, RenderObjectType::RENDER_ITEM_CURSOR_PREVIEW, mCursorPreview.get());

	if (mRenderItems.size() > mRenderItemHighWater)
	{
		mRenderItemHighWater = mRenderItems.size();
		while (mRenderItemHighWater >= mNextRenderItemWarning)
		{
			std::array<size_t, 8> aCategoryCounts{};
			for (const RenderItem& aRenderItem : mRenderItems)
			{
				switch (aRenderItem.mRenderObjectType)
				{
				case RenderObjectType::RENDER_ITEM_PLANT:
				case RenderObjectType::RENDER_ITEM_PLANT_OVERLAY:
				case RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS: ++aCategoryCounts[0]; break;
				case RenderObjectType::RENDER_ITEM_COIN: ++aCategoryCounts[1]; break;
				case RenderObjectType::RENDER_ITEM_ZOMBIE:
				case RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW:
				case RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET:
				case RenderObjectType::RENDER_ITEM_BOSS_PART: ++aCategoryCounts[2]; break;
				case RenderObjectType::RENDER_ITEM_PROJECTILE:
				case RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW: ++aCategoryCounts[3]; break;
				case RenderObjectType::RENDER_ITEM_MOWER: ++aCategoryCounts[4]; break;
				case RenderObjectType::RENDER_ITEM_PARTICLE: ++aCategoryCounts[5]; break;
				case RenderObjectType::RENDER_ITEM_REANIMATION: ++aCategoryCounts[6]; break;
				case RenderObjectType::RENDER_ITEM_GRID_ITEM:
				case RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY: ++aCategoryCounts[7]; break;
				default: break;
				}
			}
			PvzpLogLn("Render list high-water: {} items, capacity {}; plants {}, coins {}, zombies {}, projectiles {}, mowers {}, particles {}, reanimations {}, grid items {}, other {}",
				mRenderItemHighWater, mRenderItems.capacity(), aCategoryCounts[0], aCategoryCounts[1], aCategoryCounts[2],
				aCategoryCounts[3], aCategoryCounts[4], aCategoryCounts[5], aCategoryCounts[6], aCategoryCounts[7],
				mRenderItemHighWater - std::accumulate(aCategoryCounts.begin(), aCategoryCounts.end(), size_t{ 0 }));
			mNextRenderItemWarning *= 2;
		}
	}

	aGatherScope.Stop();
	Sexy::FrameProfileBoardCounts aCounts;
	aCounts.mPlants = { mPlants.mSize, mPlants.mMaxUsedCount, mPlants.mMaxSize };
	aCounts.mZombies = { mZombies.mSize, mZombies.mMaxUsedCount, mZombies.mMaxSize };
	aCounts.mProjectiles = { mProjectiles.mSize, mProjectiles.mMaxUsedCount, mProjectiles.mMaxSize };
	aCounts.mCoins = { mCoins.mSize, mCoins.mMaxUsedCount, mCoins.mMaxSize };
	aCounts.mGridItems = { mGridItems.mSize, mGridItems.mMaxUsedCount, mGridItems.mMaxSize };
	aCounts.mMowers = { mLawnMowers.mSize, mLawnMowers.mMaxUsedCount, mLawnMowers.mMaxSize };
	aCounts.mRenderItems = static_cast<uint32_t>(mRenderItems.size());
	aCounts.mRenderCapacity = static_cast<uint32_t>(mRenderItems.capacity());
	if (mApp->mEffectSystem != nullptr)
	{
		const auto& aParticles = mApp->mEffectSystem->mParticleHolder;
		const auto& aReanimations = mApp->mEffectSystem->mReanimationHolder;
		const auto& anAttachments = mApp->mEffectSystem->mAttachmentHolder;
		if (aParticles)
		{
			aCounts.mParticles = { aParticles->mParticles.mSize, aParticles->mParticles.mMaxUsedCount, aParticles->mParticles.mMaxSize };
			aCounts.mEmitters = { aParticles->mEmitters.mSize, aParticles->mEmitters.mMaxUsedCount, aParticles->mEmitters.mMaxSize };
		}
		if (aReanimations)
			aCounts.mReanimations = { aReanimations->mReanimations.mSize, aReanimations->mReanimations.mMaxUsedCount, aReanimations->mReanimations.mMaxSize };
		if (anAttachments)
			aCounts.mAttachments = { anAttachments->mAttachments.mSize, anAttachments->mAttachments.mMaxUsedCount, anAttachments->mAttachments.mMaxSize };
	}
	Sexy::FrameProfiler::Get().SetBoardCounts(aCounts);
	Sexy::FrameProfileScope aSortScope(Sexy::FrameProfileMetric::RENDER_SORT);
	RenderOrderRules::SortByLayer(mRenderItems.begin(), mRenderItems.end());
	aSortScope.Stop();
	Sexy::FrameProfileScope aRenderItemScope(Sexy::FrameProfileMetric::RENDER_ITEMS);

	for (size_t i = 0; i < mRenderItems.size(); i++)
	{
		RenderItem& aRenderItem = mRenderItems[i];
		switch (aRenderItem.mRenderObjectType)
		{
		case RenderObjectType::RENDER_ITEM_PLANT:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				aPlant->Draw(g);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PLANT_OVERLAY:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				mApp->mZenGarden->DrawPlantOverlay(g, aPlant);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS:
		{
			Plant* aPlant = aRenderItem.mPlant;
			if (aPlant->BeginDraw(g))
			{
				aPlant->DrawMagnetItems(g);
				aPlant->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_MOWER:
		{
			LawnMower* aLawnMower = aRenderItem.mMower;
			aLawnMower->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			if (aZombie->BeginDraw(g))
			{
				aZombie->Draw(g);
				aZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			if (aZombie->BeginDraw(g))
			{
				aZombie->DrawShadow(g);
				aZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET:
		{
			Zombie* aZombie = aRenderItem.mZombie;
			aZombie->DrawBungeeTarget(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_BOSS_PART:
		{
			Zombie* aBossZombie = GetBossZombie();
			if (aBossZombie && aBossZombie->BeginDraw(g))
			{
				aBossZombie->DrawBossPart(g, aRenderItem.mBossPart);
				aBossZombie->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_COIN:
		{
			Coin* aCoin = aRenderItem.mCoin;
			if (aCoin->BeginDraw(g))
			{
				aCoin->Draw(g);
				aCoin->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PROJECTILE:
		{
			Projectile* aProjectile = aRenderItem.mProjectile;
			if (aProjectile->BeginDraw(g))
			{
				aProjectile->Draw(g);
				aProjectile->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW:
		{
			Projectile* aProjectile = aRenderItem.mProjectile;
			if (aProjectile->BeginDraw(g))
			{
				aProjectile->DrawShadow(g);
				aProjectile->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_CURSOR_PREVIEW:
		{
			CursorPreview* aCursorPreview = aRenderItem.mCursorPreview;
			if (aCursorPreview->BeginDraw(g))
			{
				aCursorPreview->Draw(g);
				aCursorPreview->EndDraw(g);
			}
			break;
		}

		case RenderObjectType::RENDER_ITEM_GRID_ITEM:
		{
			GridItem* aGridItem = aRenderItem.mGridItem;
			aGridItem->DrawGridItem(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY:
		{
			GridItem* aGridItem = aRenderItem.mGridItem;
			aGridItem->DrawGridItemOverlay(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_ICE:
			DrawIce(g, aRenderItem.mBoardGridY);
			break;

		case RenderObjectType::RENDER_ITEM_PARTICLE:
		{
			PvzpParticleSystem* aParticle = aRenderItem.mParticleSytem;
			aParticle->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_REANIMATION:
		{
			Reanimation* aReanimation = aRenderItem.mReanimation;
			aReanimation->Draw(g);
			break;
		}

		case RenderObjectType::RENDER_ITEM_COIN_BANK:
			DrawUICoinBank(g);
			break;

		case RenderObjectType::RENDER_ITEM_BACKDROP:
			DrawBackdrop(g);
			break;

		case RenderObjectType::RENDER_ITEM_DOOR_MASK:
			DrawHouseDoorTop(g);
			break;

		case RenderObjectType::RENDER_ITEM_BOTTOM_UI:
			DrawUIBottom(g);
			break;

		case RenderObjectType::RENDER_ITEM_TOP_UI:
			DrawUITop(g);
			break;

		case RenderObjectType::RENDER_ITEM_FOG:
			DrawFog(g);
			break;

		case RenderObjectType::RENDER_ITEM_STORM:
			mChallenge->DrawWeather(g);
			break;

		case RenderObjectType::RENDER_ITEM_SCREEN_FADE:
			DrawFadeOut(g);
			break;

		default:
			PVZP_ASSERT(false);
			break;
		}
	}

}
