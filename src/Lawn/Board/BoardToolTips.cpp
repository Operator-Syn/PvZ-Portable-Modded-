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

void Board::UpdateToolTip(const HitResult* theHitResult)
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive || mTimeStopCounter > 0 || mApp->GetDialogCount() > 0 || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mToolTip->mVisible = false;
		return;
	}

	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		if (!mCutScene->mSeedChoosing)
		{
			mToolTip->mVisible = false;
			return;
		}

		if (mSeedBank->ContainsPoint(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY) ||
			mApp->mSeedChooserScreen->mAlmanacButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mStoreButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mImitaterButton->IsMouseOver())
		{
			mToolTip->mVisible = false;
			return;
		}

		Zombie* aZombie = ZombieHitTest(aMouseX, aMouseY);
		if (aZombie == nullptr || aZombie->mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			mToolTip->mVisible = false;
			return;
		}

		std::string aZombieName = std::format("[{}]", GetZombieDefinition(aZombie->mZombieType).mZombieName);
		mToolTip->SetTitle(aZombieName);
		if (mApp->CanShowAlmanac() && aZombie->mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			mToolTip->SetLabel("[CLICK_TO_VIEW]");
		}
		else
		{
			mToolTip->SetLabel("");
		}
		mToolTip->SetWarningText("");

		Rect aRect = aZombie->GetZombieRect();
		mToolTip->mX = aRect.mWidth / 2 + aRect.mX + 5;
		mToolTip->mY = aRect.mHeight + aRect.mY - 10;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
		{
			mToolTip->mY = aZombie->mY;
		}

		mToolTip->mVisible = true;
		mToolTip->mCenter = true;

		mToolTip->mMinLeft = IMAGE_SEEDCHOOSER_BACKGROUND->GetWidth();
		if (mApp->mSeedChooserScreen->mAlmanacButton->mBtnNoDraw && mApp->mSeedChooserScreen->mStoreButton->mBtnNoDraw)
		{
			mToolTip->mMaxBottom = 600;
		}
		else
		{
			mToolTip->mMaxBottom = 570;
		}
		if (!mApp->mSeedChooserScreen->mImitaterButton->mBtnNoDraw)
		{
			mToolTip->CalculateSize();
			if (mX + mToolTip->mX - mToolTip->mWidth / 2 < 524)
			{
				mToolTip->mMaxBottom = 503;
			}
		}

		return;
	}

	if (!CanInteractWithBoardButtons())
	{
		mToolTip->mVisible = false;
		return;
	}

	mToolTip->mMinLeft = 0;
	mToolTip->mMaxBottom = mApp->mHeight;
	mToolTip->SetTitle("");
	mToolTip->SetLabel("");
	mToolTip->SetWarningText("");
	mToolTip->mCenter = false;
	if (mChallenge->UpdateToolTip(aMouseX, aMouseY, theHitResult))
	{
		return;
	}

	HitResult aLocalHitResult;
	if (theHitResult == nullptr)
	{
		MouseHitTest(aMouseX, aMouseY, &aLocalHitResult);
		theHitResult = &aLocalHitResult;
	}

	// Tooltip-only zombie hit testing preserves the existing click/tool targets.
	if (mApp->mGameScene == GameScenes::SCENE_PLAYING &&
		(theHitResult->mObjectType == GameObjectType::OBJECT_TYPE_NONE ||
		 theHitResult->mObjectType == GameObjectType::OBJECT_TYPE_PLANT))
	{
		Zombie* aZombie = ZombieHitTest(aMouseX, aMouseY);
		if (aZombie != nullptr && aZombie->IsOnBoard())
		{
			const int64_t aHealth = static_cast<int64_t>(std::max(0, aZombie->mBodyHealth)) +
				std::max(0, aZombie->mHelmHealth) + std::max(0, aZombie->mShieldHealth) +
				std::max(0, aZombie->mTierBucketArmorHealth) + std::max(0, aZombie->mFlyingHealth);
			const int64_t aMaxHealth = static_cast<int64_t>(aZombie->mBodyMaxHealth) +
				aZombie->mHelmMaxHealth + aZombie->mShieldMaxHealth +
				aZombie->mTierBucketArmorMaxHealth + aZombie->mFlyingMaxHealth;
			const int aHealthPercent = aMaxHealth > 0 ? static_cast<int>(std::clamp<int64_t>(aHealth * 100 / aMaxHealth, 0, 100)) : 0;
			std::string aDetails = std::format("HP: {} / {} ({}%)\nBody: {} / {}",
				aHealth, aMaxHealth, aHealthPercent, std::max(0, aZombie->mBodyHealth), aZombie->mBodyMaxHealth);
			auto aAddArmor = [&](const char* theName, int theHealth, int theMaxHealth)
			{
				if (theMaxHealth > 0)
					aDetails += std::format("\n{}: {} / {}", theName, std::max(0, theHealth), theMaxHealth);
			};
			aAddArmor("Helmet", aZombie->mHelmHealth, aZombie->mHelmMaxHealth);
			aAddArmor("Shield", aZombie->mShieldHealth, aZombie->mShieldMaxHealth);
			aAddArmor("Bucket armor", aZombie->mTierBucketArmorHealth, aZombie->mTierBucketArmorMaxHealth);
			aAddArmor("Flying protection", aZombie->mFlyingHealth, aZombie->mFlyingMaxHealth);
			mToolTip->SetTitle(std::format("[{}]", GetZombieDefinition(aZombie->mZombieType).mZombieName));
			mToolTip->SetLabel(aDetails);
			const Rect aRect = aZombie->GetZombieRect();
			mToolTip->mX = aRect.mX + aRect.mWidth / 2;
			mToolTip->mY = aRect.mY - 8;
			mToolTip->mCenter = true;
			mToolTip->mVisible = true;
			return;
		}
	}

	switch (theHitResult->mObjectType)
	{
	case GameObjectType::OBJECT_TYPE_SHOVEL:
	{
		mToolTip->SetLabel("[SHOVEL_TOOLTIP]");
		Rect aShovelButtonRect = GetShovelButtonRect();
		mToolTip->mX = aShovelButtonRect.mX + 35;
		mToolTip->mY = aShovelButtonRect.mY + 72;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	case GameObjectType::OBJECT_TYPE_NEXT_GARDEN:
	{
		mToolTip->SetLabel("[NEXT_GARDEN_TOOLTIP]");
		Rect aButtonRect = GetShovelButtonRect();
		mToolTip->mX = 599;
		mToolTip->mY = aButtonRect.mY + 52;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	case GameObjectType::OBJECT_TYPE_WATERING_CAN:
		mToolTip->SetLabel("[WATERING_CAN_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_FERTILIZER:
		mToolTip->SetLabel("[FERTILIZER_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_BUG_SPRAY:
		mToolTip->SetLabel("[BUG_SPRAY_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_PHONOGRAPH:
		mToolTip->SetLabel("[PHONOGRAPH_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_CHOCOLATE:
		mToolTip->SetLabel("[CHOCOLATE_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_GLOVE:
		mToolTip->SetLabel("[GLOVE_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_MONEY_SIGN:
		mToolTip->SetLabel("[MONEY_SIGN_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_WHEELBARROW:
		mToolTip->SetLabel("[WHEELBARROW_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_TREE_FOOD:
		mToolTip->SetLabel("[TREE_FERTILIZER_TOOLTIP]");
		break;
	case GameObjectType::OBJECT_TYPE_PLANT:
	{
		Plant* aHoveredPlant = static_cast<Plant*>(theHitResult->mObject);
		if (aHoveredPlant == nullptr || aHoveredPlant->mDead || !aHoveredPlant->IsOnBoard())
		{
			mToolTip->mVisible = false;
			return;
		}

		std::string aPlantDetails;
		for (Plant* aPlant : mPlants)
		{
			if (aPlant->mDead || !aPlant->IsOnBoard() || aPlant->mPlantCol != aHoveredPlant->mPlantCol || aPlant->mRow != aHoveredPlant->mRow)
				continue;

			if (!aPlantDetails.empty())
				aPlantDetails += '\n';

			int aHealthPercent = 0;
			if (aPlant->mPlantMaxHealth > 0)
			{
				int64_t aPercent = static_cast<int64_t>(aPlant->mPlantHealth) * 100 / aPlant->mPlantMaxHealth;
				aHealthPercent = static_cast<int>(std::clamp<int64_t>(aPercent, 0, 100));
			}
			aPlantDetails += std::format("{}: {}/{} HP ({}%)", Plant::GetNameString(aPlant->mSeedType, aPlant->mImitaterType),
				aPlant->mPlantHealth, aPlant->mPlantMaxHealth, aHealthPercent);

			PlantID aPlantID = static_cast<PlantID>(mPlants.DataArrayGetID(aPlant));
			bool aSunMagnetIsHealing = std::any_of(mPlantHealGlows.begin(), mPlantHealGlows.end(),
				[aPlantID](const PlantHealGlow& theGlow){ return theGlow.mPlantID == aPlantID; }) ||
				std::any_of(mSunMagnetHealStacks.begin(), mSunMagnetHealStacks.end(),
				[aPlantID](const SunMagnetHealStack& theStack){ return theStack.mPlantID == aPlantID; });
			bool aChomperIsHealing = std::any_of(mChomperHealAuras.begin(), mChomperHealAuras.end(),
				[aPlantID](const ChomperHealAura& theAura){ return theAura.mPlantID == aPlantID; });
			aPlantDetails += " | Healing: ";
			if (!aSunMagnetIsHealing && !aChomperIsHealing)
			{
				aPlantDetails += "No";
			}
			else
			{
				if (aSunMagnetIsHealing)
					aPlantDetails += Plant::GetNameString(SeedType::SEED_SUN_MAGNET);
				if (aSunMagnetIsHealing && aChomperIsHealing)
					aPlantDetails += ", ";
				if (aChomperIsHealing)
					aPlantDetails += Plant::GetNameString(SeedType::SEED_CHOMPER);
			}

			if (aPlant->IsChomper())
			{
				aPlantDetails += std::format("\nOverdrive: {} (>15,000 sun; digests 2x faster, heals up to 2 forward plants with quadratic group scaling (each gets base heal x recipient count); costs 100 sun/s)",
					mChomperOverdriveActive ? "Active" : "Inactive");
				if (aPlant->IsTallNut())
				{
					aPlantDetails += std::format("\nTall-nut overdrive: {} (>=10,000 sun; restores 50 HP/s, +25% max HP, costs 200 sun/s)",
						mTallNutOverdriveActive ? "Active" : "Inactive");
				}
			}
			else if (aPlant->mSeedType == SeedType::SEED_FUMESHROOM || aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM)
			{
				aPlantDetails += "\nLayering: up to 5 Fume-shrooms and Gloom-shrooms combined per tile at >=1,000,000 sun";
			}
			else if (aPlant->mSeedType == SeedType::SEED_KERNELPULT)
			{
				aPlantDetails += std::format("\nOverdrive: {} (>20,000 sun; 4x attack rate, 50% butter chance, costs 150 sun/s per Kernel Pult. >=500,000: 90% butter chance, 2-5 butter volley, 200-tick butter; Giants immune; costs 175 sun/s)",
					mKernelPultOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_SUNFLOWER)
			{
				aPlantDetails += "\nSun production: sun value compounds by x1.2 per completed flag";
			}
			else if (aPlant->mSeedType == SeedType::SEED_TWINSUNFLOWER)
			{
				aPlantDetails += std::format("\nSun production: sun value compounds by x1.2 per completed flag; stops at >=1,750,000 sun\nOverdrive: Production {} (>1,000 sun; 2x faster); Bomb {} (>=10,000: 10%/event, 300 sun; >=100,000: 30%, 600 sun); Assault {} (>=1,000,000: fires every 112 ticks for 1,200 sun)",
					mTwinSunflowerProductionOverdriveActive ? "Active" : "Inactive",
					mTwinSunflowerBombardmentOverdriveActive ? "Active" : "Inactive",
					mSunMoney >= TWIN_SUNFLOWER_ASSAULT_SUN_THRESHOLD ? "Active" : "Inactive");
			}
			else if (PlantRules::IsMelonPultStackType(aPlant->mSeedType))
			{
				if (aPlant->mSeedType == SeedType::SEED_WINTERMELON)
					aPlantDetails += std::format("\nLayering: up to 5 Melon-pults and Winter Melons combined per tile at >=1,000,000 sun\nOverdrive: {} (>=1,000,000 sun; 2x attack rate, five spread melons per lob at 5 sun each, 10% Cherry Bomb chance or 35% while sacrificing HP; costs 500 sun/s and loses 10 HP/s to 1 HP. Quadratic splash uses ceil(sqrt(targets)); Bungees exempt)",
						mSunMoney >= WINTER_MELON_QUADRATIC_DAMAGE_SUN_THRESHOLD ? "Active" : "Inactive");
				else
					aPlantDetails += "\nLayering: up to 5 Melon-pults and Winter Melons combined per tile at >=1,000,000 sun";
			}
			else if (aPlant->mSeedType == SeedType::SEED_SUN_MAGNET)
			{
				aPlantDetails += std::format("\nSun payout: flag value compounds by x1.2/flag, then magnet multiplies it by 2x or 6x. Overdrive {} (>=5,000 sun; 6x pickup, 15 slots or 35 at 150,000; loses 1 HP/s to 1 HP. Below 1,000,000 each pickup assigns 10-20 heal stacks at 25 sun/stack; at >=1,000,000 healing starts passively each second, prioritizes lowest HP percentage, and costs 50 sun/stack; keeps 5,000 reserve. Coffee Bean: aura/growth, collects 5s, loses 4 HP/s)",
					mSunMagnetOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET)
			{
				aPlantDetails += std::format("\nOverdrive: {} (>=50,000 sun; costs 250 sun/s for board, drains 25 HP/s to 1 HP; 3x gold/diamond odds)",
					mGoldMagnetOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_CATTAIL)
			{
				aPlantDetails += std::format("\nSeed cost: 75 sun\nLayering: up to 5 Cat Tails per tile at >=1,000,000 sun\nOverdrive: {} (>=25,000 sun; about 4x attack rate, 6 shots/cycle, 125 sun/projectile; >=1,000,000: 375 sun/projectile and 5x damage; >=2,000,000: 750 sun/projectile, plus 1.5% of target's current body HP as extra damage per hit (minimum 50), and drains 1% max HP/s; this can kill the Cattail)",
					mCatTailOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_PLANTERN)
			{
				aPlantDetails += std::format("\nSun production: 5x sun value at >=1,000,000 sun; value compounds by x1.2 per completed flag; pauses at 5,000,000 sun and resumes below it\nOverdrive: Production {} (>1,000 sun; about 2x faster); Cherry Bomb cadence speeds up 2.5x at >=1,000, >=50,000, and >=1,000,000 sun, capped at 1.5x Peashooter's 150-tick firing interval; >=1,000,000 costs 300 sun/shot (150 below); >=50,000: impacts trigger a Jalapeno flame across the struck lane. Coffee Bean: Cob/targeted lane (500 sun; 1,000 at >=1,000,000), then 5s flames (300 sun/lane/s); >=2,000,000: if Coffee Bean is in the chosen seed bank, all eligible Planterns auto-fire every 3s for 85% damage at the same costs",
					mSunMoney > 1000 ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_GATLINGPEA)
			{
				aPlantDetails += std::format("\nOverdrive: {} (>=200,000 sun; 15% chance per 4-shot burst for matching smoky Butter, Cherry Bomb, or Melon peas; >=1,000,000: each shot has 50% Cherry Bomb chance for 150 sun, plus 300 sun/s upkeep)",
					mGatlingPeaOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_SPIKEWEED || aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
			{
				aPlantDetails += std::format("\nOverdrive: {} (>=1,000 sun; 7x attack speed; loses {} HP per completed attack)",
					mSpikeweedOverdriveActive ? "Active" : "Inactive",
					aPlant->mSeedType == SeedType::SEED_SPIKEROCK ? 2 : 1);
			}
			else if (aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				aPlantDetails += std::format("\nOverdrive: {} (>=5,000 sun; restores 50 HP/s, gains 25% max HP; nearby Pumpkin healing bonus scales quadratically with adjacent Pumpkin count)",
					mPumpkinOverdriveActive ? "Active" : "Inactive");
			}
			else if (aPlant->mSeedType == SeedType::SEED_GLOOMSHROOM)
			{
				aPlantDetails += "\nRegeneration: restores 25 HP/s for 150 sun/s; Pumpkin healing bonus applies at >=5,000 sun";
			}
			else if (aPlant->IsTallNut())
			{
				aPlantDetails += std::format("\nOverdrive: {} (>=10,000 sun; restores 50 HP/s, +25% max HP, costs 200 sun/s)",
					mTallNutOverdriveActive ? "Active" : "Inactive");
			}
		}

		mToolTip->SetLabel(aPlantDetails);
		mToolTip->mX = aHoveredPlant->mX + 40;
		mToolTip->mY = aHoveredPlant->mY - 8;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}
	case GameObjectType::OBJECT_TYPE_SEEDPACKET:
		break;
	default:
		mToolTip->mVisible = false;
		return;
	}

	if (theHitResult->mObjectType != GameObjectType::OBJECT_TYPE_SEEDPACKET)
	{
		Rect aButtonRect = GetShovelButtonRect();
		GetZenButtonRect(theHitResult->mObjectType, aButtonRect);
		this->mToolTip->mX = aButtonRect.mX + 35;
		this->mToolTip->mY = aButtonRect.mY + 72;
		this->mToolTip->mCenter = true;
		this->mToolTip->mVisible = true;
		return;
	}

	SeedPacket* aSeedPacket = (SeedPacket*)theHitResult->mObject;
	SeedType aUseSeedType = aSeedPacket->mPacketType;
	if (aSeedPacket->mPacketType == SeedType::SEED_IMITATER && aSeedPacket->mImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = aSeedPacket->mImitaterType;
	}

	if (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST)
	{
		if (aUseSeedType == SeedType::SEED_REPEATER)
		{
			mToolTip->SetLabel("[BEGHOULED_REPEATER_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_FUMESHROOM)
		{
			mToolTip->SetLabel("[BEGHOULED_FUMESHROOM_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_TALLNUT)
		{
			mToolTip->SetLabel("[BEGHOULED_TALLNUT_UPGRADE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
		{
			mToolTip->SetLabel("[BEGHOULED_SHUFFLE_TOOLTIP]");
		}
		else if (aUseSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			mToolTip->SetLabel("[BEGHOULED_CRATER_TOOLTIP]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_SLOT_MACHINE_SUN)
	{
		mToolTip->SetLabel("[SLOT_MACHINE_SUN_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_SLOT_MACHINE_DIAMOND)
	{
		mToolTip->SetLabel("[SLOT_MACHINE_DIAMOND_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIQUARIUM_SNORKLE)
	{
		mToolTip->SetLabel("[ZOMBIQUARIUM_SNORKEL_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIQUARIUM_TROPHY)
	{
		mToolTip->SetLabel("[ZOMBIQUARIUM_TROPHY_TOOLTIP]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_NORMAL)
	{
		mToolTip->SetLabel("[ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_TRAFFIC_CONE)
	{
		mToolTip->SetLabel("[CONEHEAD_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_POLEVAULTER)
	{
		mToolTip->SetLabel("[POLE_VAULTING_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_PAIL)
	{
		mToolTip->SetLabel("[BUCKETHEAD_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_LADDER)
	{
		mToolTip->SetLabel("[LADDER_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_DIGGER)
	{
		mToolTip->SetLabel("[DIGGER_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_BUNGEE)
	{
		mToolTip->SetLabel("[BUNGEE_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_FOOTBALL)
	{
		mToolTip->SetLabel("[FOOTBALL_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_BALLOON)
	{
		mToolTip->SetLabel("[BALLOON_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_SCREEN_DOOR)
	{
		mToolTip->SetLabel("[SCREEN_DOOR_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBONI)
	{
		mToolTip->SetLabel("[ZOMBONI]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_POGO)
	{
		mToolTip->SetLabel("[POGO_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_DANCER)
	{
		mToolTip->SetLabel("[DANCING_ZOMBIE]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_GARGANTUAR)
	{
		mToolTip->SetLabel("[GARGANTUAR]");
	}
	else if (aUseSeedType == SeedType::SEED_ZOMBIE_IMP)
	{
		mToolTip->SetLabel("[IMP]");
	}
	else
	{
		mToolTip->SetLabel(Plant::GetNameString(aSeedPacket->mPacketType, aSeedPacket->mImitaterType));
	}

	int aPlantCost = GetCurrentPlantCost(aSeedPacket->mPacketType, aSeedPacket->mImitaterType);
	if (mApp->mEasyPlantingCheat)
	{
		mToolTip->SetWarningText("FREE_PLANTING_CHEAT");
	}
	else if (!aSeedPacket->mActive && (gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST))
	{
		if (aSeedPacket->mPacketType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			mToolTip->SetWarningText("[BEGHOULED_NO_CRATERS]");
		}
		else
		{
			mToolTip->SetWarningText("[BEGHOULED_SEED_ALREADY_PURCHASED]");
		}
	}
	else if (!aSeedPacket->mActive)
	{
		mToolTip->SetWarningText("[WAITING_FOR_SEED]");
	}
	else if (!CanTakeSunMoney(aPlantCost) && !HasConveyorBeltSeedBank() && !mApp->IsSlotMachineLevel())
	{
		mToolTip->SetWarningText("[NOT_ENOUGH_SUN]");
	}
	else if (aUseSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_REPEATER]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_WINTERMELON)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_MELONPULT]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_SUNFLOWER]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_SPIKEWEED]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_COBCANNON)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_KERNELPULTS]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_MAGNETSHROOM]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_FUMESHROOM]");
		}
	}
	else if (aUseSeedType == SeedType::SEED_CATTAIL)
	{
		if (!PlantingRequirementsMet(aUseSeedType))
		{
			mToolTip->SetWarningText("[REQUIRES_LILY_PAD]");
		}
	}

	mToolTip->mX = (SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + mSeedBank->mX + aSeedPacket->mOffsetX + aSeedPacket->mX;
	mToolTip->mY = mSeedBank->mY + aSeedPacket->mY + 70;
	mToolTip->mVisible = true;
}
