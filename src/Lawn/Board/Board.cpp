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

bool gShownMoreSunTutorial = false;

Board::Board(LawnApp* theApp)
{
	mApp = theApp;
	mApp->mBoard = this;
	mPlanternFlameDamagePercent.fill(100);

	mZombies.DataArrayInitialize(MAX_ACTIVE_ZOMBIES, "zombies");
	mSplashDamageTargets.reserve(MAX_ACTIVE_ZOMBIES);
	mPlants.DataArrayInitialize(1024U, "plants");
	mProjectiles.DataArrayInitialize(8192U, "projectiles");
	mCoins.DataArrayInitialize(4096U, "coins");
	mLawnMowers.DataArrayInitialize(32U, "lawnmowers");
	mGridItems.DataArrayInitialize(128U, "griditems");
#ifdef LOW_MEMORY
	mRenderItems.reserve(INITIAL_RENDER_ITEM_CAPACITY);
#else
	const auto& aParticleHolder = *mApp->mEffectSystem->mParticleHolder;
	size_t aRenderCapacity = static_cast<size_t>(mPlants.mMaxSize) * 3 + mCoins.mMaxSize +
		static_cast<size_t>(mZombies.mMaxSize) * 5 + static_cast<size_t>(mProjectiles.mMaxSize) * 2 +
		mLawnMowers.mMaxSize + aParticleHolder.mParticleSystems.mMaxSize +
		mApp->mEffectSystem->mReanimationHolder->mReanimations.mMaxSize +
		static_cast<size_t>(mGridItems.mMaxSize) * 2 + MAX_GRID_SIZE_Y + 9;
	mRenderItems.reserve(aRenderCapacity);
#endif

	mApp->mEffectSystem->EffectSystemFreeAll();
	mBoardRandSeed = mApp->mAppRandSeed;
	if (mApp->IsSurvivalMode())
	{
		mBoardRandSeed = Rand();
	}
	mCoinBankFadeCount = 0;
	mLevel = 0;
	mCursorObject = std::make_unique<CursorObject>();
	mCursorPreview = std::make_unique<CursorPreview>();
	mSeedBank = std::make_unique<SeedBank>();
	mCutScene = std::make_unique<CutScene>();
	mSpecialGraveStoneX = -1;
	mSpecialGraveStoneY = -1;
	for (int i = 0; i < MAX_GRID_SIZE_X; i++)
	{
		for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
		{
			mGridSquareType[i][j] = GridSquareType::GRIDSQUARE_GRASS;
			mGridCelLook[i][j] = Rand(20);
			mGridCelOffset[i][j][0] = Rand(10) - 5;
			mGridCelOffset[i][j][1] = Rand(10) - 5;
		}

		for (int k = 0; k < MAX_GRID_SIZE_Y + 1; k++)
		{
			mGridCelFog[i][k] = 0;
		}
	}
	mFogOffset = 0.0f;
	mSunCountDown = 0;
	mShakeCounter = 0;
	mShakeAmountX = 0;
	mShakeAmountY = 0;
	mPaused = false;
	mLevelAwardSpawned = false;
	mFlagRaiseCounter = 0;
	mIceTrapCounter = 0;
	mLevelComplete = false;
	mBoardFadeOutCounter = -1;
	mNextSurvivalStageCounter = 0;
	mScoreNextMowerCounter = 0;
	mProgressMeterWidth = 0;
	mPoolSparklyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mPoolSparklyParticleRetryTicks = 0;
	mFogBlownCountDown = 0;
	mFwooshCountDown = 0;
	mTimeStopCounter = 0;
	mCobCannonCursorDelayCounter = 0;
	mCobCannonMouseX = 0;
	mCobCannonMouseY = 0;
	mDroppedFirstCoin = false;
	mBonusLawnMowersRemaining = 0;
	mEnableGraveStones = false;
	mHelpIndex = AdviceType::ADVICE_NONE;
	mEffectCounter = 0;
	mDrawCount = 0;
	mRiseFromGraveCounter = 0;
	mFinalWaveSoundCounter = 0;
	mKilledYeti = false;
	mTriggeredLawnMowers = 0;
	mPlayTimeActiveLevel = 0;
	mPlayTimeInactiveLevel = 0;
	mMaxSunPlants = 0;
	mStartDrawTime = 0;
	mIntervalDrawTime = 0;
	mIntervalDrawCountStart = 0;
	mPreloadTime = 0;
	mGameID = mApp->GetNowTime();
	mMinFPS = 1000.0f;
	mGravesCleared = 0;
	mPlantsEaten = 0;
	mPlantsShoveled = 0;
	mPeaShooterUsed = false;
	mCatapultPlantsUsed = false;
	mMushroomAndCoffeeBeansOnly = true;
	mMushroomsUsed = false;
	mLevelCoinsCollected = 0;
	mGargantuarsKillsByCornCob = 0;
	mCoinsCollected = 0;
	mDiamondsCollected = 0;
	mPottedPlantsCollected = 0;
	mChocolateCollected = 0;
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		for (int x = 0; x < 12; x++)
		{
			mFwooshID[y][x] = ReanimationID::REANIMATIONID_NULL;
		}
	}
	mPrevMouseX = -1;
	mPrevMouseY = -1;
	mFinalBossKilled = false;
	mMustacheMode = mApp->mMustacheMode;
	mSuperMowerMode = mApp->mSuperMowerMode;
	mFutureMode = mApp->mFutureMode;
	mPinataMode = mApp->mPinataMode;
	mDanceMode = mApp->mDanceMode;
	mDaisyMode = mApp->mDaisyMode;
	mSukhbirMode = mApp->mSukhbirMode;
	mShowShovel = false;
	mToolTip = std::make_unique<ToolTipWidget>();
	mAdvice = std::make_unique<MessageWidget>(mApp);
	mBackground = BackgroundType::BACKGROUND_1_DAY;
	mMainCounter = 0;
	mBoardUpdateCounter = 0;
	mTutorialState = TutorialState::TUTORIAL_OFF;
	mTutorialTimer = -1;
	mTutorialParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mChallenge = std::make_unique<Challenge>();
	mClip = false;
	mDebugTextMode = DebugTextMode::DEBUG_TEXT_NONE;
	mMenuButton = std::make_unique<GameButton>(0);
	mMenuButton->mDrawStoneButton = true;
	mStoreButton = nullptr;
	mIgnoreMouseUp = false;

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mMenuButton->SetLabel("[MAIN_MENU_BUTTON]");
		mMenuButton->Resize(628, -10, 163, 46);

		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mButtonImage = IMAGE_ZENSHOPBUTTON;
		mStoreButton->mOverImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mDownImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mParentWidget = this;
		mStoreButton->Resize(678, 33, IMAGE_ZENSHOPBUTTON->mWidth, 40);
	}
	else
	{
		mMenuButton->SetLabel("[MENU_BUTTON]");
		mMenuButton->Resize(mApp->mWidth - 119, -10, 117, 46);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND)
	{
		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mDrawStoneButton = true;
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->mDisabled = true;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		mMenuButton->SetLabel("[MAIN_MENU_BUTTON]");
		mMenuButton->Resize(628, -10, 163, 46);

		mStoreButton = std::make_unique<GameButton>(1);
		mStoreButton->mDrawStoneButton = true;
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->SetLabel("[GET_FULL_VERSION_BUTTON]");
	}
}

Board::~Board() = default;

void BoardInitForPlayer()
{
	gShownMoreSunTutorial = false;
}

void Board::DisposeBoard()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		mApp->mZenGarden->LeaveGarden();
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		mChallenge->TreeOfWisdomLeave();

	mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_RAIN);
	mApp->mZenGarden->mBoard = nullptr;
	mApp->CrazyDaveDie();
	mApp->mEffectSystem->EffectSystemFreeAll();
}

bool Board::AreEnemyZombiesOnScreen()
{
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled)
		{
			return true;
		}
	}
	return false;
}

int Board::CountZombiesOnScreen()
{
	int aCount = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled && aZombie->IsOnBoard())
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::GetLiveGargantuarCount() {
	int aCount = 0;
	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && aZombie->IsOnBoard() &&
			(aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
			 aZombie->mZombieType == ZombieType::ZOMBIE_BULWARK_GARGANTUAR))
		{
			aCount++;
		}
	}
	return aCount;
}

int Board::CountUntriggerLawnMowers()
{
	int aCount = 0;
	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		if (aLawnMower->mMowerState != LawnMowerState::MOWER_TRIGGERED && aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED)
		{
			aCount++;
		}
	}
	return aCount;
}

void Board::ResetFPSStats()
{
	int64_t aTickCount = SDL_GetTicks();
	mStartDrawTime = aTickCount;
	mIntervalDrawTime = aTickCount;
	mDrawCount = 1;
	mIntervalDrawCountStart = 1;
}

int Board::GetLevelRandSeed()
{
	int aRndSeed = mApp->mPlayerInfo->mId + mBoardRandSeed;
	if (mApp->IsAdventureMode())
	{
		aRndSeed += mApp->mPlayerInfo->mFinishedAdventure * 101 + mLevel;
	}
	else
	{
		aRndSeed += mChallenge->mSurvivalStage * 101 + mApp->mGameMode;
	}
	return aRndSeed;
}

void Board::Pause(bool thePause)
{
	if (mPaused == thePause)
		return;

	mPaused = thePause;
	if (thePause && mApp->mPlayerInfo->mCoins > 0)
	{
		ShowCoinBank();
	}

	if (!thePause || mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO)
	{
		mApp->mSoundSystem->GamePause(thePause);
		mApp->mMusic->GameMusicPause(thePause);
	}

	if (thePause && NeedSaveGame())
	{
		TryToSaveGame();
	}
}

void Board::UpdateGameObjects()
{
	UpdatePlantOverdrive();
	UpdateZombieRain();
	UpdatePlantHealGlows();
	UpdatePlanternFlames();

	for (Plant* aPlant : mPlants)
	{
		if (aPlant->mDead)
			continue;
		aPlant->Update();
	}
	UpdateSunMagnetCollection();

	for (Zombie* aZombie : mZombies)
	{
		if (aZombie->mDead)
			continue;
		aZombie->Update();
	}

	for (Projectile* aProjectile : mProjectiles)
	{
		if (aProjectile->mDead)
			continue;
		aProjectile->Update();
	}

	for (Coin* aCoin : mCoins)
	{
		if (aCoin->mDead)
			continue;
		aCoin->Update();
	}

	for (LawnMower* aLawnMower : mLawnMowers)
	{
		if (aLawnMower->mDead)
			continue;
		aLawnMower->Update();
	}

	mCursorPreview->Update();
	mCursorObject->Update();

	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		mSeedBank->mSeedPackets[i].Update();
	}

}

void Board::UpdateGame()
{
	Sexy::FrameProfileScope aProfileScope(Sexy::FrameProfileMetric::BOARD_UPDATE);
	UpdateGameObjects();
	if (StageHasFog() && mFogBlownCountDown > 0)
	{
		float aMaxFogOffset = 1065.0f - LeftFogColumn() * 80.0f;
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			mFogOffset = PvzpAnimateCurveFloat(200, 0, mFogBlownCountDown, aMaxFogOffset, 0, PvzpCurves::CURVE_EASE_OUT);
		}
		else if (mFogBlownCountDown < 2000)
		{
			mFogOffset = PvzpAnimateCurveFloat(2000, 0, mFogBlownCountDown, aMaxFogOffset, 0, PvzpCurves::CURVE_EASE_OUT);
		}
		else if (mFogOffset < aMaxFogOffset)
		{
			mFogOffset = PvzpAnimateCurveFloat(-5, aMaxFogOffset, mFogOffset * 1.1f, 0, aMaxFogOffset, PvzpCurves::CURVE_LINEAR);
		}
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mCutScene->ShouldRunUpsellBoard())
		return;

	mMainCounter++;
	UpdateSunSpawning();
	UpdateZombieSpawning();
	UpdateIce();
	if (mIceTrapCounter > 0)
	{
		mIceTrapCounter--;
		if (mIceTrapCounter == 0)
		{
			PvzpParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mPoolSparklyParticleID);
			if (aPoolSparklyParticle)
			{
				aPoolSparklyParticle->mDontUpdate = false;
			}
		}
	}

	if (mFogBlownCountDown > 0)
	{
		mFogBlownCountDown--;
	}

	if (mMainCounter == 1 && mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 1)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER);
		}
		else if (mLevel == 2)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
			DisplayAdvice("[ADVICE_PLANT_SUNFLOWER1]", MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			mTutorialTimer = 500;
		}
	}

	UpdateProgressMeter();
}

void Board::Update()
{
	PvzpHesitationBracket aHesitation("Board::Update");

	Widget::Update();
	MarkDirty();

	mBoardUpdateCounter++;
	mCutScene->Update();
	UpdateMousePosition();
	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
	{
		mApp->mZenGarden->ZenGardenUpdate();
	}
	if (IsScaryPotterDaveTalking())
	{
		mApp->UpdateCrazyDave();
	}

	if (mPaused)
	{
		mChallenge->Update();
		mCursorPreview->mVisible = false;
		mCursorObject->mVisible = false;
		return;
	}

	bool aDisabled = !CanInteractWithBoardButtons() || mIgnoreMouseUp;
	if (!mMenuButton->mBtnNoDraw)
	{
		mMenuButton->mDisabled = aDisabled;
	}
	mMenuButton->Update();
	if (mStoreButton)
	{
		mStoreButton->mDisabled = aDisabled;
		mStoreButton->Update();
	}

	mApp->mEffectSystem->Update();
	mAdvice->Update();
	UpdateTutorial();

	if (mCobCannonCursorDelayCounter > 0)
	{
		mCobCannonCursorDelayCounter--;
	}
	if (mOutOfMoneyCounter > 0)
	{
		mOutOfMoneyCounter--;
	}
	if (mApp->mScreenShakeMode == LawnApp::ScreenShakeMode::OFF)
	{
		mShakeCounter = 0;
		mShakeAmountX = 0;
		mShakeAmountY = 0;
		mX = 0;
		mY = 0;
	}
	else if (mShakeCounter > 0)
	{
		mShakeCounter--;
		if (mShakeCounter == 0)
		{
			mX = 0;
			mY = 0;
		}
		else
		{
			if (!Rand(3))
			{
				mShakeAmountX = -mShakeAmountX;
			}
			float aShakeScale = mApp->GetScreenShakeScale();
			mX = static_cast<int>(PvzpAnimateCurve(12, 0, mShakeCounter, 0, mShakeAmountX, PvzpCurves::CURVE_BOUNCE) * aShakeScale);
			mY = static_cast<int>(PvzpAnimateCurve(12, 0, mShakeCounter, 0, mShakeAmountY, PvzpCurves::CURVE_BOUNCE) * aShakeScale);
		}
	}
	if (mCoinBankFadeCount > 0 && mApp->GetDialog(Dialogs::DIALOG_PURCHASE_PACKET_SLOT) == nullptr)
	{
		mCoinBankFadeCount--;
	}
	UpdateLayers();

	if (mTimeStopCounter > 0)
		return;

	mEffectCounter++;
	if (StageHasPool() && !mIceTrapCounter && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON && !mCutScene->IsSurvivalRepick())
	{
		mApp->mPoolEffect->mPoolCounter++;
	}
	if (mBackground == BackgroundType::BACKGROUND_3_POOL && mPoolSparklyParticleID == ParticleSystemID::PARTICLESYSTEMID_NULL)
	{
		if (mPoolSparklyParticleRetryTicks > 0)
		{
			--mPoolSparklyParticleRetryTicks;
		}
		else
		{
			int aRenderPosition = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, 2, 0);
			PvzpParticleSystem* aPoolParticle = mApp->AddPvzpParticle(450, 295, aRenderPosition, ParticleEffect::PARTICLE_POOL_SPARKLY);
			if (aPoolParticle != nullptr)
			{
				mPoolSparklyParticleID = mApp->ParticleGetID(aPoolParticle);
				mPoolSparklyParticleRetryTicks = 0;
			}
			else
			{
				mPoolSparklyParticleRetryTicks = 30;
			}
		}
	}

	UpdateGridItems();
	UpdateFwoosh();
	UpdateGame();
	UpdateFog();
	mChallenge->Update();
	UpdateLevelEndSequence();
	mPrevMouseX = mApp->mWidgetManager->mLastMouseX;
	mPrevMouseY = mApp->mWidgetManager->mLastMouseY;
}

void Board::ProcessDeleteQueue()
{
	{
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead)
			{
				mPlants.DataArrayFree(aPlant);
			}
		}
	}
	{
		for (Zombie* aZombie : mZombies)
		{
			if (aZombie->mDead)
			{
				mZombies.DataArrayFree(aZombie);
			}
		}
	}
	{
		for (Projectile* aProjectile : mProjectiles)
		{
			if (aProjectile->mDead)
			{
				mProjectiles.DataArrayFree(aProjectile);
			}
		}
	}
	{
		for (Coin* aCoin : mCoins)
		{
			if (aCoin->mDead)
			{
				mCoins.DataArrayFree(aCoin);
			}
		}
	}
	{
		for (LawnMower* aLawnMower : mLawnMowers)
		{
			if (aLawnMower->mDead)
			{
				mLawnMowers.DataArrayFree(aLawnMower);
			}
		}
	}
	{
		for (GridItem* aGridItem : mGridItems)
		{
			if (aGridItem->mDead)
			{
				mGridItems.DataArrayFree(aGridItem);
			}
		}
	}
}

void Board::ShakeBoard(int theShakeAmountX, int theShakeAmountY)
{
	if (mApp->mScreenShakeMode == LawnApp::ScreenShakeMode::OFF)
	{
		mShakeCounter = 0;
		mShakeAmountX = 0;
		mShakeAmountY = 0;
		mX = 0;
		mY = 0;
		return;
	}

	mShakeCounter = 12;
	mShakeAmountX = theShakeAmountX;
	mShakeAmountY = theShakeAmountY;
}
