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

#include <cstdint>
#include <time.h>
#include "../Entities/Coin.h"
#include "../Board/Board.h"
#include "../Plant/Plant.h"
#include "../Entities/LawnCommon.h"
#include "LawnDialog.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../Modes/ZenGarden.h"
#include "SeedPacket.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../System/Music.h"
#include "SeedChooserScreen.h"
#include "../../GameConstants.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "../../PvzpLib/Reanimator.h"
#include "misc/Debug.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "graphics/ImageFont.h"
#include "widget/WidgetManager.h"
#include "AchievementsScreen.h"
#include <algorithm>


#include "StoreCatalog.h"

StoreScreen::StoreScreen(LawnApp* theApp) : Dialog(nullptr, nullptr, DIALOG_STORE, true, theApp->GetString("STORE", "Store"), "", "", BUTTONS_NONE)
{
	mApp = theApp;
	mClip = false;
	mStoreTime = 0;
	mBubbleCountDown = 0;
	mBubbleClickToContinue = false;
	mAmbientSpeechCountDown = 200;
	mPreviousAmbientSpeechIndex = -1;
	mPage = STORE_PAGE_SLOT_UPGRADES;
	mMouseOverItem = STORE_ITEM_INVALID;
	mHatchTimer = 0;
	mShakeX = 0;
	mShakeY = 0;
	mStartDialog = -1;
	mHatchOpen = true;
	mEasyBuyingCheat = false;
	mWaitForDialog = false;
	mCoins.DataArrayInitialize(1024U, "coins");
	mLoadedResourceNames.push_back("DelayLoad_Store");
	for (std::string& resource : mLoadedResourceNames)
		PvzpLoadResources(resource.c_str());
	Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	mPottedPlantSpecs.InitializePottedPlant(SEED_MARIGOLD);
	mPottedPlantSpecs.mDrawVariation = (DrawVariation)RandRangeInt(VARIATION_MARIGOLD_WHITE, VARIATION_MARIGOLD_LIGHT_GREEN);

	mBackButton = std::make_unique<NewLawnButton>(nullptr, StoreScreen::StoreScreen_Back, this);
	mBackButton->mDoFinger = true;
	mBackButton->SetLabel("[STORE_MAIN_MENU_BUTTON]");
	Image* aMenuImage = Sexy::IMAGE_STORE_MAINMENUBUTTON;
	mBackButton->mButtonImage = aMenuImage;
	mBackButton->mOverImage = Sexy::IMAGE_STORE_MAINMENUBUTTONHIGHLIGHT;
	mBackButton->mDownImage = Sexy::IMAGE_STORE_MAINMENUBUTTONDOWN;
	mBackButton->SetFont(Sexy::FONT_HOUSEOFTERROR20);
	mBackButton->SetLabelColor(Color(98, 153, 235));
	mBackButton->SetLabelHiliteColor(Color(167, 192, 235));
	mBackButton->Resize(366, 512, aMenuImage->mWidth, aMenuImage->mHeight);
	mBackButton->mTextOffsetX = -7;
	mBackButton->mTextOffsetY = 1;
	mBackButton->mTextDownOffsetX = 2;
	mBackButton->mTextDownOffsetY = 1;

	mPrevButton = std::make_unique<NewLawnButton>(nullptr, StoreScreen::StoreScreen_Prev, this);
	mPrevButton->mDoFinger = true;
	mPrevButton->SetLabel("");
	Image* aPrevImage = Sexy::IMAGE_STORE_PREVBUTTON;
	mPrevButton->mButtonImage = aPrevImage;
	mPrevButton->mOverImage = Sexy::IMAGE_STORE_PREVBUTTONHIGHLIGHT;
	mPrevButton->mDownImage = Sexy::IMAGE_STORE_PREVBUTTONHIGHLIGHT;
	mPrevButton->SetLabelColor(Color(255, 240, 0));
	mPrevButton->SetLabelHiliteColor(Color(200, 200, 255));
	mPrevButton->Resize(252, 402, aPrevImage->mWidth, aPrevImage->mHeight);

	mNextButton = std::make_unique<NewLawnButton>(nullptr, StoreScreen::StoreScreen_Next, this);
	mNextButton->mDoFinger = true;
	mNextButton->SetLabel("");
	Image* aNextImage = Sexy::IMAGE_STORE_NEXTBUTTON;
	mNextButton->mButtonImage = aNextImage;
	mNextButton->mOverImage = Sexy::IMAGE_STORE_NEXTBUTTONHIGHLIGHT;
	mNextButton->mDownImage = Sexy::IMAGE_STORE_NEXTBUTTONHIGHLIGHT;
	mNextButton->SetLabelColor(Color(255, 240, 0));
	mNextButton->SetLabelHiliteColor(Color(200, 200, 255));
	mNextButton->Resize(596, 402, aNextImage->mWidth, aNextImage->mHeight);

	mOverlayWidget = std::make_unique<StoreScreenOverlay>(this);
	mOverlayWidget->Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);

	if (!IsPageShown(STORE_PAGE_PLANT_UPGRADES))
	{
		mPrevButton->mDisabledImage = Sexy::IMAGE_STORE_PREVBUTTONDISABLED;
		mPrevButton->SetDisabled(true);
		mNextButton->mDisabledImage = Sexy::IMAGE_STORE_NEXTBUTTONDISABLED;
		mNextButton->SetDisabled(true);
	}
	mDrawnOnce = false;
	mAddedAtUpdateCount = mApp->mUpdateCount;
	mGoToTreeNow = false;
	mPurchasedFullVersion = false;
	mTrialLockedWhenStoreOpened = mApp->IsTrialStageLocked();
}

void StoreScreen::SetBubbleText(int theCrazyDaveMessage, int theTime, bool theClickToContinue)
{
	mApp->CrazyDaveTalkIndex(theCrazyDaveMessage);
	mBubbleCountDown = theTime;
	mBubbleClickToContinue = theClickToContinue;
}

void StoreScreen::StorePreload()
{
	ReanimatorEnsureDefinitionLoaded(REANIM_CRAZY_DAVE, true);
	ReanimatorEnsureDefinitionLoaded(REANIM_ZENGARDEN_FERTILIZER, true);
	mApp->CrazyDaveEnter();

	Plant::PreloadPlantResources(SeedType::SEED_GARLIC);
	Plant::PreloadPlantResources(SeedType::SEED_TWINSUNFLOWER);

	if (mApp->HasFinishedAdventure())
	{
		Plant::PreloadPlantResources(SeedType::SEED_GLOOMSHROOM);
		Plant::PreloadPlantResources(SeedType::SEED_CATTAIL);
		Plant::PreloadPlantResources(SeedType::SEED_WINTERMELON);
		Plant::PreloadPlantResources(SeedType::SEED_GOLD_MAGNET);
		Plant::PreloadPlantResources(SeedType::SEED_SPIKEROCK);
		Plant::PreloadPlantResources(SeedType::SEED_COBCANNON);
		Plant::PreloadPlantResources(SeedType::SEED_IMITATER);
	}
}

bool StoreScreen::CanInteractWithButtons()
{
	return mStoreTime >= 120 && !mBubbleClickToContinue && mHatchTimer <= 0 && !mWaitForDialog;
}

void StoreScreen::Update()
{
	mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);
	mApp->UpdateCrazyDave();

	for (Coin* aCoin : mCoins)
	{
		if (!aCoin->mDead)
		{
			aCoin->Update();
		}
	}

	if (mWaitForDialog)
		return;

	if (mApp->mCrazyDaveState == CRAZY_DAVE_OFF)
	{
		// demo sessions preload by update tick instead of the frame-scheduled mDrawnOnce
		bool aShouldPreload = mApp->IsInDemoMode() ? (mApp->mUpdateCount - mAddedAtUpdateCount >= 2U) : mDrawnOnce;
		if (aShouldPreload)
		{
			StorePreload();
		}
		return;
	}

	mStoreTime++;
	if (mApp->mCrazyDaveState != CRAZY_DAVE_OFF && mApp->mCrazyDaveState != CRAZY_DAVE_ENTERING)
	{
		if (mHatchTimer > 0)
		{
			mHatchTimer--;
			mBackButton->mX -= mShakeX;
			mBackButton->mY -= mShakeY;
			mPrevButton->mX -= mShakeX;
			mPrevButton->mY -= mShakeY;
			mNextButton->mX -= mShakeX;
			mNextButton->mY -= mShakeY;

			/*
            if (mHatchTimer <= 35)
            {
                if (mHatchTimer == 0)
                {
                    EnableButtons(true);
                }
                mShakeY = 0;
            }
            else
            {
                mShakeY = RandRangeInt(1, 3);
            }
            mShakeX = 0;
            */

			if (mHatchTimer == 0)
			{
				EnableButtons(true);
				mShakeX = 0;
				mShakeY = 0;
			}
			else
			{
				mShakeX = 0;
				if (mHatchTimer > 35)
				{
					mShakeY = static_cast<int>(RandRangeInt(1, 3) * mApp->GetScreenShakeScale());
				}
				else
				{
					mShakeY = 0;
				}
			}

			mBackButton->mX += mShakeX;
			mBackButton->mY += mShakeY;
			mPrevButton->mX += mShakeX;
			mPrevButton->mY += mShakeY;
			mNextButton->mX += mShakeX;
			mNextButton->mY += mShakeY;
		}
		else if (mStartDialog != -1)
		{
			SetBubbleText(mStartDialog, 0, true);
			mStartDialog = -1;
		}
		else if (!mBubbleClickToContinue)
		{
			if (mBubbleCountDown > 0)
			{
				mBubbleCountDown--;
				if (mBubbleCountDown == 0)
				{
					if (mApp->mSoundSystem->IsFoleyPlaying(FOLEY_CRAZY_DAVE_SHORT) ||
						mApp->mSoundSystem->IsFoleyPlaying(FOLEY_CRAZY_DAVE_LONG) ||
						mApp->mSoundSystem->IsFoleyPlaying(FOLEY_CRAZY_DAVE_EXTRA_LONG))
					{
						mBubbleCountDown = 1;
					}
					else
					{
						mApp->CrazyDaveStopTalking();
					}
				}
			}
			else
			{
				mAmbientSpeechCountDown--;
				if (mAmbientSpeechCountDown <= 0)
				{
					PvzpWeightedArray aPickArray[4];
					for (int i = 0; i < 4; i++)
					{
						int aMessage = 2015 + i;
						aPickArray[i].mItem = aMessage;
						if (mPreviousAmbientSpeechIndex == aMessage)
						{
							aPickArray[i].mWeight = 0;
						}
						else if (i == 3)
						{
							aPickArray[i].mWeight = mApp->HasFinishedAdventure() ? 20 : 0;
						}
						else
						{
							aPickArray[i].mWeight = 100;
						}
					}

					int aDaveMessage = PvzpPickFromWeightedArray(aPickArray, 4);
					mPreviousAmbientSpeechIndex = aDaveMessage;
					SetBubbleText(aDaveMessage, 800, false);
					mAmbientSpeechCountDown = RandRangeInt(500, 1000);
				}
			}
		}
	}

	UpdateMouse();
	// store opened in trial mode and now unlocked: the player just purchased the full version
	if (CanInteractWithButtons() && mTrialLockedWhenStoreOpened && !mApp->IsTrialStageLocked())
	{
		mPurchasedFullVersion = true;
		mResult = Dialog::ID_OK;
	}
	else
	{
		Widget::Update();
		MarkDirty();
	}
}

void StoreScreen::AddedToManager(WidgetManager* theWidgetManager)
{
	WidgetContainer::AddedToManager(theWidgetManager);
	AddWidget(mBackButton.get());
	AddWidget(mPrevButton.get());
	AddWidget(mNextButton.get());
	AddWidget(mOverlayWidget.get());
}

void StoreScreen::RemovedFromManager(WidgetManager* theWidgetManager)
{
	WidgetContainer::RemovedFromManager(theWidgetManager);
	RemoveWidget(mBackButton.get());
	RemoveWidget(mPrevButton.get());
	RemoveWidget(mNextButton.get());
	RemoveWidget(mOverlayWidget.get());
	mApp->CrazyDaveDie();
}

bool StoreScreen::IsPageShown(StorePages thePage)
{
	// trial mode only shows the default page
	if (mApp->IsTrialStageLocked()) return thePage == STORE_PAGE_SLOT_UPGRADES;
	// finishing adventure unlocks all pages
	if (mApp->HasFinishedAdventure()) return true;
	// the plant upgrades page requires reaching adventure 5-2 (level 42)
	if (thePage == STORE_PAGE_PLANT_UPGRADES) return mApp->mPlayerInfo->mLevel >= 42;
	// the zen garden page requires reaching adventure 5-5 (level 45)
	if (thePage == STORE_PAGE_ZEN1) return mApp->mPlayerInfo->mLevel >= 45;
	// hide the Tree of Wisdom page until adventure is finished
	return thePage != STORE_PAGE_ZEN2;
}

void StoreScreen::AdvanceCrazyDaveDialog()
{
	if (!mBubbleClickToContinue)
		return;

	// "Hey neighbor! I've got some new things to sell!"
	if (mApp->mCrazyDaveMessageIndex == 3100)
	{
		mHatchTimer = 150;
		mHatchOpen = true;
		mApp->PlaySample(Sexy::SOUND_HATCHBACK_OPEN);
	}
	if (!mApp->AdvanceCrazyDaveText())
	{
		mApp->CrazyDaveStopTalking();
		mBubbleClickToContinue = false;
		mBubbleCountDown = 500;
		if (mHatchTimer == 0)
		{
			EnableButtons(true);
		}
	}
	else
	{
		SetBubbleText(mApp->mCrazyDaveMessageIndex, 0, true);
	}

	int aMessage = mApp->mCrazyDaveMessageIndex;
	if (aMessage == 303 || aMessage == 606 || aMessage == 2601)
	{
		mHatchTimer = 150;
		mHatchOpen = true;
		mApp->PlaySample(Sexy::SOUND_HATCHBACK_OPEN);
	}
	else if (aMessage == 603)
	{
		mApp->mPlayerInfo->mNeedsMagicTacoReward = false;
		mApp->WriteCurrentUserConfig();
		mApp->PlaySample(Sexy::SOUND_DIAMOND);
		Coin* aCoin = mCoins.DataArrayAlloc();
		aCoin->CoinInitialize(80, 520, COIN_DIAMOND, COIN_MOTION_FROM_PRESENT);
		aCoin->mVelX = 0;
		aCoin->mVelY = -5;
	}
	else if (aMessage == 902 || aMessage == 1002)
	{
		mApp->mPlayerInfo->AddCoins(100);
	}
}

void StoreScreen::SetupForIntro(int theDialogIndex)
{
	mStartDialog = theDialogIndex;
	mHatchOpen = false;
	mBackButton->mLabel = PvzpStringTranslate("[STORE_NEXT_LEVEL_BUTTON]");
	EnableButtons(false);
}
