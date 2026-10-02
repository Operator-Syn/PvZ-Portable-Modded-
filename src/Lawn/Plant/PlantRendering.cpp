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

#include "../Entities/Coin.h"
#include "Plant.h"
#include "../Board/Board.h"
#include "../Zombie/Zombie.h"
#include "../Modes/Cutscene.h"
#include "../Entities/GridItem.h"
#include "../Modes/ZenGarden.h"
#include "../Modes/Challenge.h"
#include "../Projectile/Projectile.h"
#include "../Widget/SeedPacket.h"
#include "../../LawnApp.h"
#include "../Entities/CursorObject.h"
#include "../../GameConstants.h"
#include <array>
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "../../PvzpLib/PvzpDebug.h"
#include "misc/FrameProfiler.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "../Widget/AchievementsScreen.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>







#include "PlantRules.h"
#include "../Rules/TargetingRules.h"

bool Plant::DrawMagnetItemsOnTop()
{
	if (mSeedType == SeedType::SEED_GOLD_MAGNET || mSeedType == SeedType::SEED_SUN_MAGNET)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			if (mMagnetItems[i].mItemType != MagnetItemType::MAGNET_ITEM_NONE)
			{
				return true;
			}
		}
		if (mSeedType == SeedType::SEED_SUN_MAGNET)
		{
			MagnetItem* anExtraItems = mBoard->GetSunMagnetExtraItems(
				static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(this)), false);
			if (anExtraItems != nullptr)
			{
				for (int i = 0; i < SUN_MAGNET_HIGH_OVERDRIVE_EXTRA_ITEMS; i++)
					if (anExtraItems[i].mItemType != MagnetItemType::MAGNET_ITEM_NONE)
						return true;
			}
		}

		return false;
	}

	if (mSeedType == SeedType::SEED_MAGNETSHROOM)
	{
		for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
		{
			MagnetItem* aMagnetItem = &mMagnetItems[i];
			if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
			{
				SexyVector2 aVectorToPlant(mX + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, mY + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
				if (aVectorToPlant.Magnitude() > 20.0f)
				{
					return true;
				}
			}
		}

		return false;
	}

	return false;
}

float PlantFlowerPotHeightOffset(SeedType theSeedType, float theFlowerPotScale)
{
	float aHeightOffset = -5.0f * theFlowerPotScale;
	float aScaleOffsetFix = 0.0f;

	switch (theSeedType)
	{
	case SeedType::SEED_CHOMPER:
	case SeedType::SEED_CHOMPERNUT:
	case SeedType::SEED_PLANTERN:
		aHeightOffset -= 5.0f;
		break;
	case SeedType::SEED_SCAREDYSHROOM:
		aHeightOffset += 5.0f;
		aScaleOffsetFix -= 8.0f;
		break;
	case SeedType::SEED_SUNSHROOM:
	case SeedType::SEED_PUFFSHROOM:
		aScaleOffsetFix -= 4.0f;
		break;
	case SeedType::SEED_HYPNOSHROOM:
	case SeedType::SEED_MAGNETSHROOM:
	case SeedType::SEED_PEASHOOTER:
	case SeedType::SEED_REPEATER:
	case SeedType::SEED_LEFTPEATER:
	case SeedType::SEED_SNOWPEA:
	case SeedType::SEED_THREEPEATER:
	case SeedType::SEED_SUNFLOWER:
	case SeedType::SEED_MARIGOLD:
	case SeedType::SEED_CABBAGEPULT:
	case SeedType::SEED_MELONPULT:
	case SeedType::SEED_TANGLEKELP:
	case SeedType::SEED_BLOVER:
	case SeedType::SEED_SPIKEWEED:
		aScaleOffsetFix -= 8.0f;
		break;
	case SeedType::SEED_SEASHROOM:
	case SeedType::SEED_POTATOMINE:
		aScaleOffsetFix -= 4.0f;
		break;
	case SeedType::SEED_LILYPAD:
		aScaleOffsetFix -= 16.0f;
		break;
	case SeedType::SEED_INSTANT_COFFEE:
		aScaleOffsetFix -= 20.0f;
		break;
	default:
		break;
	}

	return aHeightOffset + (theFlowerPotScale * aScaleOffsetFix - aScaleOffsetFix);
}

float PlantDrawHeightOffset(Board* theBoard, Plant* thePlant, SeedType theSeedType, int theCol, int theRow)
{
	float aHeightOffset = 0.0f;
	Plant* aFlowerPot = theBoard ? theBoard->GetFlowerPotAt(theCol, theRow) : nullptr;

	bool doFloating = false;
	if (Plant::IsFlying(theSeedType))
	{
		doFloating = false;
	}
	else if (theBoard == nullptr)
	{
		if (Plant::IsAquatic(theSeedType))
		{
			doFloating = true;
		}
	}
	else if (theBoard->IsPoolSquare(theCol, theRow))
	{
		doFloating = true;
	}
	else if (thePlant != nullptr && theBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		doFloating = true;
	}

	if (doFloating)
	{
		uint32_t aCounter = theBoard ? theBoard->mMainCounter : gLawnApp->mAppCounter;

		float aPos = theRow * PI + theCol * 0.25f * PI;
		float aTime = static_cast<float>(aCounter % 200) * (2.0f * PI / 200.0f);
		float aFloatingHeight = sin(aPos + aTime) * 2.0f;
		aHeightOffset += aFloatingHeight;
	}

	if (theBoard && (thePlant == nullptr || !thePlant->mSquished))
	{
		Plant* aPot = aFlowerPot;
		if (aPot && !aPot->mSquished && theSeedType != SeedType::SEED_FLOWERPOT)
		{
			aHeightOffset += PlantFlowerPotHeightOffset(theSeedType, 1.0f);
		}
	}

	if (theSeedType == SeedType::SEED_FLOWERPOT)
	{
		aHeightOffset += 26.0f;
	}
	else if (theSeedType == SeedType::SEED_LILYPAD)
	{
		aHeightOffset += 25.0f;
	}
	else if (theSeedType == SeedType::SEED_STARFRUIT)
	{
		aHeightOffset += 10.0f;
	}
	else if (theSeedType == SeedType::SEED_TANGLEKELP)
	{
		aHeightOffset += 24.0f;
	}
	else if (theSeedType == SeedType::SEED_SEASHROOM)
	{
		aHeightOffset += 28.0f;
	}
	else if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aHeightOffset -= 20.0f;
	}
	//else if (Plant::IsFlying(theSeedType))
	//{
	//    aHeightOffset -= 30.0f;
	//}
	else if (theSeedType == SeedType::SEED_CACTUS)
	{
		return aHeightOffset;
	}
	else if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aHeightOffset += 15.0f;
	}
	else if (theSeedType == SeedType::SEED_PUFFSHROOM)
	{
		aHeightOffset += 5.0f;
	}
	else if (theSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		aHeightOffset -= 14.0f;
	}
	else if (theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		aHeightOffset -= 40.0f;
	}
	else if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		int aBottomRow = 4;
		if (theBoard && theBoard->StageHas6Rows())
		{
			aBottomRow = theBoard->GetNumPlayableRows() - 1;
		}

		if (theSeedType == SeedType::SEED_SPIKEROCK)
		{
			aHeightOffset += 6.0f;
		}

		if (aFlowerPot && gLawnApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		{
			aHeightOffset += 5.0f;
		}
		else if (theBoard && theBoard->StageHasRoof())
		{
			aHeightOffset += 15.0f;
		}
		else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
		{
			aHeightOffset += 0.0f;
		}
		else if (theRow == aBottomRow && theCol >= 7 && theBoard && theBoard->StageHas6Rows())
		{
			aHeightOffset += 1.0f;
		}
		else if (theRow == aBottomRow && theCol < 7)
		{
			aHeightOffset += 12.0f;
		}
		else
		{
			aHeightOffset += 15.0f;
		}
	}

	return aHeightOffset;
}

void Plant::GetPeaHeadOffset(int& theOffsetX, int& theOffsetY)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

	int aTrackIndex = 0;
	if (aBodyReanim->TrackExists("anim_stem"))
	{
		aTrackIndex = aBodyReanim->FindTrackIndex("anim_stem");
	}
	else if(aBodyReanim->TrackExists("anim_idle"))
	{
		aTrackIndex = aBodyReanim->FindTrackIndex("anim_idle");
	}

	ReanimatorTransform aTransform;
	aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
	theOffsetX = aTransform.mTransX;
	theOffsetY = aTransform.mTransY;
}

void Plant::DrawMagnetItems(Graphics* g)
{
	float aOffsetX = 0.0f;
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);

	MagnetItem* anExtraItems = mSeedType == SeedType::SEED_SUN_MAGNET
		? mBoard->GetSunMagnetExtraItems(static_cast<PlantID>(mBoard->mPlants.DataArrayGetID(this)), false)
		: nullptr;
	int aMagnetItemCount = MAX_MAGNET_ITEMS + (anExtraItems != nullptr ? SUN_MAGNET_HIGH_OVERDRIVE_EXTRA_ITEMS : 0);
	for (int i = 0; i < aMagnetItemCount; i++)
	{
		MagnetItem* aMagnetItem = i < MAX_MAGNET_ITEMS ? &mMagnetItems[i] : &anExtraItems[i - MAX_MAGNET_ITEMS];
		if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
		{
			int aCelRow = 0, aCelCol = 0;
			Image* aImage = nullptr;
			float aScale = 0.8f;

			if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PAIL_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_BUCKET3;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET3;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DOOR_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_SCREENDOOR3;
			}
			else if (aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_POGO_1 && aMagnetItem->mItemType <= MagnetItemType::MAGNET_ITEM_POGO_3)
			{
				aCelCol = static_cast<int>(aMagnetItem->mItemType) - static_cast<int>(MagnetItemType::MAGNET_ITEM_POGO_1);
				aImage = IMAGE_ZOMBIEPOGO;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_1)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_2)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE1;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_3)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE2;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_LADDER_PLACED)
			{
				aImage = IMAGE_REANIM_ZOMBIE_LADDER_5;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_JACK_IN_THE_BOX)
			{
				aImage = IMAGE_REANIM_ZOMBIE_JACKBOX_BOX;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_PICK_AXE)
			{
				aImage = IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_SILVER_COIN)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_COIN_SILVER_DOLLAR;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_GOLD_COIN)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_COIN_GOLD_DOLLAR;
			}
			else if (aMagnetItem->mItemType == MagnetItemType::MAGNET_ITEM_DIAMOND)
			{
				aScale = 1.0f;
				aImage = IMAGE_REANIM_DIAMOND;
			}
			else if (aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_SUN_15 &&
				aMagnetItem->mItemType <= MagnetItemType::MAGNET_ITEM_SUN_600 ||
				(aMagnetItem->mItemType >= MagnetItemType::MAGNET_ITEM_SUN_RANDOM_MIN &&
					aMagnetItem->mItemType <= MagnetItemType::MAGNET_ITEM_SUN_RANDOM_MAX) ||
				aMagnetItem->mItemType > MagnetItemType::MAGNET_ITEM_SUN_DYNAMIC_BASE)
			{
				aScale = 0.5f;
				aImage = IMAGE_SUNBANK;
			}
			else
			{
				PVZP_ASSERT(false);
			}

			if (aScale == 1.0f)
			{
				g->DrawImageCel(aImage, aMagnetItem->mPosX - mX + aOffsetX, aMagnetItem->mPosY - mY + aOffsetY, aCelCol, aCelRow);
			}
			else
			{
				PvzpDrawImageCelScaledF(g, aImage, aMagnetItem->mPosX - mX + aOffsetX, aMagnetItem->mPosY - mY + aOffsetY, aCelCol, aCelRow, aScale, aScale);
			}
		}
	}
}

Image* Plant::GetImage(SeedType theSeedType)
{
	Image** aImages = GetPlantDefinition(theSeedType).mPlantImage;
	return aImages ? aImages[0] : nullptr;
}

void Plant::DrawShadow(Sexy::Graphics* g, float theOffsetX, float theOffsetY)
{
	if (mSeedType == SeedType::SEED_LILYPAD || mSeedType == SeedType::SEED_STARFRUIT || mSeedType == SeedType::SEED_TANGLEKELP ||
		mSeedType == SeedType::SEED_SEASHROOM || mSeedType == SeedType::SEED_COBCANNON || mSeedType == SeedType::SEED_SPIKEWEED ||
		mSeedType == SeedType::SEED_SPIKEROCK || mSeedType == SeedType::SEED_GRAVEBUSTER || mSeedType == SeedType::SEED_CATTAIL ||
		mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE)
		return;

	if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && mApp->mZenGarden->mGardenType == GardenType::GARDEN_MAIN)
		return;

	int aShadowType = 0;
	float aShadowOffsetX = -3.0f;
	float aShadowOffsetY = 51.0f;
	float aScale = 1.0f;
	if (mBoard && mBoard->StageIsNight())
	{
		aShadowType = 1;
	}

	if (mSeedType == SeedType::SEED_SQUASH)
	{
		if (mBoard)
		{
			aShadowOffsetY += mBoard->GridToPixelY(mPlantCol, mRow) - mY;
		}
		aShadowOffsetY += 5.0f;
	}
	else if (mSeedType == SeedType::SEED_PUFFSHROOM)
	{
		aScale = 0.5f;
		aShadowOffsetY = 42.0f;
	}
	else if (mSeedType == SeedType::SEED_SUNSHROOM)
	{
		aShadowOffsetY = 42.0f;
		if (mState == PlantState::STATE_SUNSHROOM_SMALL)
		{
			aScale = 0.5f;
		}
		else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
		{
			Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
			aScale = 0.5f + 0.5f * aBodyReanim->mAnimTime;
		}
	}
	else if (mSeedType == SeedType::SEED_UMBRELLA)
	{
		aScale = 0.5f;
		aShadowOffsetX = -7.0f;
		aShadowOffsetY = 52.0f;
	}
	else if (mSeedType == SeedType::SEED_FUMESHROOM || mSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		aScale = 1.3f;
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
	{
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_KERNELPULT)
	{
		aShadowOffsetX = 0.0f;
		aShadowOffsetY = 47.0f;
	}
	else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
	{
		aShadowOffsetX = -9.0f;
		aShadowOffsetY = 55.0f;
	}
	else if (mSeedType == SeedType::SEED_CHOMPER)
	{
		aShadowOffsetX = -21.0f;
		aShadowOffsetY = 57.0f;
	}
	else if (mSeedType == SeedType::SEED_FLOWERPOT)
	{
		aShadowOffsetX = -4.0f;
		aShadowOffsetY = 46.0f;
	}
	else if (IsTallNut())
	{
		aShadowOffsetY = 54.0f;
		aScale = 1.3f;
	}
	else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		aShadowOffsetY = 46.0f;
		aScale = 1.4f;
	}
	else if (mSeedType == SeedType::SEED_CACTUS)
	{
		aShadowOffsetX = -8.0f;
		aShadowOffsetY = 50.0f;
	}
	else if (mSeedType == SeedType::SEED_PLANTERN)
	{
		aShadowOffsetY = 57.0f;
	}
	else if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		aShadowOffsetY = 71.0f;
	}
	else if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		aShadowOffsetX = -33.0f;
		aShadowOffsetY = 56.0f;
		aScale = 1.7f;
	}

	if (Plant::IsFlying(mSeedType))
	{
		aShadowOffsetY += 10.0f;
		if (mBoard && (mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_NORMAL_POSITION) || mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_PUMPKIN)))
			return;
	}

	if (aShadowType == 0)
	{
		PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY, 0, aScale, aScale);
	}
	else
	{
		PvzpDrawImageCelCenterScaledF(g, IMAGE_PLANTSHADOW2, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY, 0, aScale, aScale);
	}
}

void Plant::Draw(Graphics* g)
{
	float aOffsetX = 0.0f;
	float aOffsetY = PlantDrawHeightOffset(mBoard, this, mSeedType, mPlantCol, mRow);
	if (Plant::IsFlying(mSeedType) && mSquished)
	{
		aOffsetY += 30.0f;
	}

	int aImageIndex = mFrame;
	int aImageCol = aImageIndex;
	int aImageRow = 0;
	uint32_t anIdleAnimationCounter = 0;
	Image* aPlantImage = Plant::GetImage(mSeedType);
	if (mSeedType == SeedType::SEED_EPHRAIM)
	{
		const int anAttackSet = std::clamp(mEphraimAttackSet, 0, EPHRAIM_ATTACK_VARIANT_COUNT - 1);
		if (mShootingCounter == 0 &&
			(mEphraimAttackPauseFlags & (EPHRAIM_ATTACK_FLAG_RECOVERY | EPHRAIM_ATTACK_FLAG_RANGED)) ==
			(EPHRAIM_ATTACK_FLAG_RECOVERY | EPHRAIM_ATTACK_FLAG_RANGED))
		{
			// A still resting pose after the throwing sequence finishes its recoil.
			aPlantImage = IMAGE_EPHRAIM_PLANT;
			aImageCol = 0;
			aImageRow = 0;
		}
		else if (mShootingCounter > 0 || (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RECOVERY) != 0)
		{
			const int anAtlasSet = anAttackSet;
			aPlantImage = IMAGE_EPHRAIM_SEQUENCES[anAtlasSet];
			aImageCol = std::clamp(mFrame, 0, EPHRAIM_ATLAS_FRAME_COUNTS[anAtlasSet] - 1);
			aImageRow = 0;
		}
		else
		{
			aImageCol = 0;
			aImageRow = 0;
			anIdleAnimationCounter = IsOnBoard() ? mBoard->mMainCounter : mApp->mAppCounter;
			if (IMAGE_EPHRAIM_PLANT != nullptr && IMAGE_EPHRAIM_PLANT->mNumCols >= 6)
			{
				// Ease through the attack intro and reverse back to its start. At the
				// game's 100 Hz update rate this makes one breathing loop take 3.6s.
				static constexpr int IDLE_INTRO_FRAMES[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1 };
				aPlantImage = IMAGE_EPHRAIM_PLANT;
				aImageCol = IDLE_INTRO_FRAMES[(anIdleAnimationCounter % 360) / 36];
			}
		}
	}
	if (mSeedType == SeedType::SEED_EPHRAIM)
	{
		if (aPlantImage == nullptr || aPlantImage->mNumCols <= 0 || aPlantImage->mNumRows <= 0)
			return;
		if (aImageCol < 0 || aImageCol >= aPlantImage->mNumCols || aImageRow < 0 || aImageRow >= aPlantImage->mNumRows)
		{
			aPlantImage = IMAGE_EPHRAIM_PLANT;
			aImageCol = 0;
			aImageRow = 0;
			if (aPlantImage == nullptr || aPlantImage->mNumCols <= 0 || aPlantImage->mNumRows <= 0)
				return;
		}
	}
	bool anAfterimageDrawn = false;
	auto DrawPlantImageCel = [&](Graphics* theGraphics)
	{
		if (mSeedType != SeedType::SEED_EPHRAIM)
		{
			PvzpDrawImageCelF(theGraphics, aPlantImage, aOffsetX, aOffsetY, aImageCol, aImageRow);
			return;
		}

		const int aCelWidth = aPlantImage->GetCelWidth();
		const int aCelHeight = aPlantImage->GetCelHeight();
		const bool anIdlePose = mShootingCounter == 0 && (mEphraimAttackPauseFlags & EPHRAIM_ATTACK_FLAG_RECOVERY) == 0;
		float aBreathingScale = 1.0f;
		if (anIdlePose)
		{
			aBreathingScale += std::sin(static_cast<float>(anIdleAnimationCounter) * 2.0f * PI / 360.0f) * 0.012f;
		}
		const int aScaledWidth = static_cast<int>(std::round(aCelWidth * EPHRAIM_DRAW_SCALE * aBreathingScale));
		const int aScaledHeight = static_cast<int>(std::round(aCelHeight * EPHRAIM_DRAW_SCALE * aBreathingScale));
		// The source frame has transparent padding and an off-center pose. Center
		// the visible pixels on the plant tile instead of centering the full atlas
		// cell. Attack poses keep the cell centered because their action spans the
		// full tile area while moving through the animation.
		const float aVisibleCenterX = aCelWidth * 0.5f;
		const float aFootAnchorOffset = aCelHeight * EPHRAIM_DRAW_SCALE * aBreathingScale;
		const int aDrawX = static_cast<int>(std::round(aOffsetX + mWidth * 0.5f - aVisibleCenterX * EPHRAIM_DRAW_SCALE * aBreathingScale));
		const int aDrawY = static_cast<int>(std::round(aOffsetY + mHeight - aFootAnchorOffset));
		const Rect aSourceRect(aImageCol * aCelWidth, aImageRow * aCelHeight, aCelWidth, aCelHeight);
		const Rect aDestRect(aDrawX, aDrawY, aScaledWidth, aScaledHeight);
		Graphics aScaledGraphics(*theGraphics);
		aScaledGraphics.SetFastStretch(true);
		// The PNG source faces left. Face right while idle and toward targets on
		// the right; leave the original orientation only for attacks to the left.
		const bool aMirrorAtlas = anIdlePose || mTargetX >= mX + mWidth / 2;
		if (!anAfterimageDrawn)
		{
			for (const EphraimAfterimage& anEcho : mEphraimAfterimages)
			{
				if (anEcho.mDelayTicks > 0)
					continue;
				const int anAfterimageCol = EphraimAttackAtlasFrame(anEcho.mAttackSet, anEcho.mElapsedTicks);
				Image* anAfterimageImage = IMAGE_EPHRAIM_SEQUENCES[anEcho.mAttackSet];
				if (anAfterimageImage == nullptr || anAfterimageCol >= anAfterimageImage->mNumCols)
					continue;
				const int aCellWidth = anAfterimageImage->GetCelWidth();
				const int aCellHeight = anAfterimageImage->GetCelHeight();
				const int anOriginX = GetEphraimAfterimageOriginX(anEcho);
				const bool aMirror = anEcho.mTargetX >= anOriginX;
				const int anOffsetX = anOriginX - (mX + mWidth / 2);
				const Rect aSource(anAfterimageCol * aCellWidth, 0, aCellWidth, aCellHeight);
				const Rect aDest(
					static_cast<int>(std::round(aOffsetX + mWidth * 0.5f - aCellWidth * 0.5f * EPHRAIM_DRAW_SCALE)) + anOffsetX,
					static_cast<int>(std::round(aOffsetY + mHeight - aCellHeight * EPHRAIM_DRAW_SCALE)),
					static_cast<int>(std::round(aCellWidth * EPHRAIM_DRAW_SCALE)),
					static_cast<int>(std::round(aCellHeight * EPHRAIM_DRAW_SCALE)));
				Graphics anEchoG(*theGraphics);
				anEchoG.SetFastStretch(true);
				anEchoG.SetColorizeImages(true);
				anEchoG.SetColor(Color(255, 255, 255, 104));
				anEchoG.DrawImageMirror(anAfterimageImage, aDest, aSource, aMirror);
			}
			anAfterimageDrawn = true;
		}
		aScaledGraphics.DrawImageMirror(aPlantImage, aDestRect, aSourceRect, aMirrorAtlas);
	};

	if (mSquished)
	{
		if (mSeedType == SeedType::SEED_FLOWERPOT)
		{
			aOffsetY -= 15.0f;
		}
		if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
		{
			aOffsetY -= 20.0f;
		}

		g->SetScale(1.0f, 0.25f, 0.0f, 0.0f);
		DrawSeedType(g, mSeedType, mImitaterType, DrawVariation::VARIATION_NORMAL, aOffsetX, 60.0f + aOffsetY);
		g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		bool aDrawPumpkinBack = false;
		Plant* aPumpkin = nullptr;

		if (IsOnBoard())
		{
			aPumpkin = mBoard->GetPumpkinAt(mPlantCol, mRow);
			if (aPumpkin)
			{
				Plant* aPlantInPumpkin = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
				if (aPlantInPumpkin)
				{
					if (aPlantInPumpkin->mRenderOrder > aPumpkin->mRenderOrder || aPlantInPumpkin->mOnBungeeState == GETTING_GRABBED_BY_BUNGEE)
					{
						aPlantInPumpkin = nullptr;
					}
				}

				if (aPlantInPumpkin == this)
				{
					aDrawPumpkinBack = true;
				}
				if (aPlantInPumpkin == nullptr && mSeedType == SeedType::SEED_PUMPKINSHELL)
				{
					aDrawPumpkinBack = true;
				}
			}
			else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				aDrawPumpkinBack = true;
				aPumpkin = this;
			}
		}
		else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aDrawPumpkinBack = true;
			aPumpkin = this;
		}

		DrawShadow(g, aOffsetX, aOffsetY);

		if (Plant::IsFlying(mSeedType))
		{
			uint32_t aCounter = IsOnBoard() ? mBoard->mMainCounter : mApp->mAppCounter;

			float aTime = static_cast<float>(fmod((mRow * 97.0 + mPlantCol * 61.0 + static_cast<double>(aCounter)) * 0.03, 2.0 * PI));
			float aWave = sin(aTime) * 2.0f;
			aOffsetY += aWave;
		}

		if (aDrawPumpkinBack)
		{
			Reanimation* aPumpkinReanim = mApp->ReanimationGet(aPumpkin->mBodyReanimID);
			Graphics aPumpkinGraphics(*g);
			aPumpkinGraphics.mTransX += aPumpkin->mX - mX;
			aPumpkinGraphics.mTransY += aPumpkin->mY - mY;
			aPumpkinReanim->DrawRenderGroup(&aPumpkinGraphics, 1);
		}

		aOffsetX += mShakeOffsetX * mApp->GetScreenShakeScale();
		aOffsetY += mShakeOffsetY * mApp->GetScreenShakeScale();
		if (IsInPlay() && mApp->IsIZombieLevel())
		{
			mBoard->mChallenge->IZombieDrawPlant(g, this);
		}
		else if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
		{
			Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
			if (aBodyReanim)
			{
				if (!mApp->Is3DAccelerated() && mSeedType == SeedType::SEED_FLOWERPOT && IsOnBoard() &&
					aBodyReanim->mAnimRate == 0.0f && aBodyReanim->IsAnimPlaying("anim_idle"))
				{
					mApp->mReanimatorCache->DrawCachedPlant(g, aOffsetX, aOffsetY, mSeedType, DrawVariation::VARIATION_NORMAL);
				}
				else if (mSeedType == SeedType::SEED_SUN_MAGNET && mSunMagnetCoffeeTicksRemaining > 0)
				{
					float aPulse = (std::sin(mBoard->mMainCounter * 0.12f) + 1.0f) * 0.5f;
					float aScale = 1.10f + aPulse * 0.025f;
					Graphics aBoostGraphics(*g);
					aBoostGraphics.SetScale(aScale, aScale, mWidth * 0.5f, mHeight * 0.5f);
					aBodyReanim->Draw(&aBoostGraphics);
				}
				else
				{
					aBodyReanim->Draw(g);
				}
			}
		}
		else
		{
			SeedType aSeedType = SeedType::SEED_NONE;
			if (mBoard)
			{
				aSeedType = mBoard->GetSeedTypeInCursor();
			}

			if (IsPartOfUpgradableTo(aSeedType) && mBoard->CanPlantAt(mPlantCol, mRow, aSeedType) == PlantingReason::PLANTING_OK)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}
			else if (aSeedType == SeedType::SEED_COBCANNON && mBoard->CanPlantAt(mPlantCol - 1, mRow, aSeedType) == PlantingReason::PLANTING_OK)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}
			else if (mBoard && mBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG)
			{
				g->SetColorizeImages(true);
				g->SetColor(GetFlashingColor(mBoard->mMainCounter, 90));
			}

			DrawPlantImageCel(g);
			g->SetColorizeImages(false);
			if (mHighlighted)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				g->SetColorizeImages(true);
				g->SetColor(Color(255, 255, 255, 196));
				DrawPlantImageCel(g);
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				g->SetColorizeImages(false);
			}
			else if (mEatenFlashCountdown > 0)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				g->SetColorizeImages(true);
				g->SetColor(Color(255, 255, 255, std::clamp(mEatenFlashCountdown * 3, 0, 255)));
				DrawPlantImageCel(g);
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				g->SetColorizeImages(false);
			}
		}

		if (mSeedType == SeedType::SEED_SUN_MAGNET && mSunMagnetCoffeeTicksRemaining > 0 && IsOnBoard())
		{
			GraphicsStateGuard aStateGuard(*g);
			float aPulse = (std::sin(mBoard->mMainCounter * 0.12f) + 1.0f) * 0.5f;
			float aRadiusX = 37.0f + aPulse * 4.0f;
			float aRadiusY = 11.0f + aPulse * 2.0f;
			float aCenterX = mWidth * 0.5f;
			float aCenterY = mHeight - 9.0f;
			g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
			g->SetColor(Color(255, 82, 54, 120 + static_cast<int>(aPulse * 95.0f)));
			for (int i = 0; i < 32; i++)
			{
				float aAngle1 = static_cast<float>(i) * 2.0f * PI / 32.0f;
				float aAngle2 = static_cast<float>(i + 1) * 2.0f * PI / 32.0f;
				g->DrawLine(static_cast<int>(aCenterX + std::cos(aAngle1) * aRadiusX),
					static_cast<int>(aCenterY + std::sin(aAngle1) * aRadiusY),
					static_cast<int>(aCenterX + std::cos(aAngle2) * aRadiusX),
					static_cast<int>(aCenterY + std::sin(aAngle2) * aRadiusY));
			}
		}

		if (mSeedType == SeedType::SEED_MAGNETSHROOM && !DrawMagnetItemsOnTop())
		{
			DrawMagnetItems(g);
		}
		bool aIsMagnetStack = mSeedType == SeedType::SEED_MAGNETSHROOM || mSeedType == SeedType::SEED_GOLD_MAGNET ||
			mSeedType == SeedType::SEED_SUN_MAGNET;
		bool aIsFumeGloomStack = mSeedType == SeedType::SEED_FUMESHROOM || mSeedType == SeedType::SEED_GLOOMSHROOM;
		bool aIsCatTailStack = mSeedType == SeedType::SEED_CATTAIL;
		bool aIsMelonPultStack = mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON;
		if ((mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_TWINSUNFLOWER ||
			aIsMagnetStack || aIsFumeGloomStack || aIsCatTailStack || aIsMelonPultStack) && mBoard)
		{
			PlantsOnLawn aPlantsOnTile;
			mBoard->GetPlantsOnLawn(mPlantCol, mRow, &aPlantsOnTile);
			int aStackCount = aIsMagnetStack ? aPlantsOnTile.mMagnetCount :
				aIsFumeGloomStack ? aPlantsOnTile.mFumeGloomCount :
				aIsCatTailStack ? aPlantsOnTile.mCatTailCount :
				aIsMelonPultStack ? aPlantsOnTile.mMelonPultCount : aPlantsOnTile.mSunflowerCount;
			if (aPlantsOnTile.mNormalPlant == this && aStackCount > 1)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				for (int i = 0; i < aStackCount; i++)
				{
					int aWidth = 24 + i * 10;
					int aLeft = 37 - aWidth / 2;
					int aRight = aLeft + aWidth;
					int aTop = mHeight - 10 + i * 3;
					g->SetColor(aIsMagnetStack ? Color(120, 220, 255, 110) :
						aIsFumeGloomStack ? Color(200, 150, 255, 90) :
						aIsCatTailStack ? Color(100, 220, 230, 100) :
						aIsMelonPultStack ? Color(110, 170, 255, 100) : Color(190, 255, 110, 75));
					g->DrawLine(aLeft, aTop, aRight, aTop);
					g->DrawLine(aRight, aTop, aRight, aTop + 4);
					g->DrawLine(aRight, aTop + 4, aLeft, aTop + 4);
					g->DrawLine(aLeft, aTop + 4, aLeft, aTop);
				}
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				g->SetColor(Color::White);
			}
		}
	}
}

void Plant::DrawSeedType(Graphics* g, SeedType theSeedType, SeedType theImitaterType, DrawVariation theDrawVariation, float thePosX, float thePosY)
{
	Graphics aSeedG(*g);
	int aCelRow = 0;
	int aCelCol = 2;
	float aOffsetX = 0.0f;
	float aOffsetY = 0.0f;
	SeedType aSeedType = theSeedType;
	DrawVariation aDrawVariation = theDrawVariation;

	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		aSeedType = theImitaterType;
		aDrawVariation = DrawVariation::VARIATION_IMITATER;
		if (theImitaterType == SeedType::SEED_HYPNOSHROOM || theImitaterType == SeedType::SEED_SQUASH || theImitaterType == SeedType::SEED_POTATOMINE ||
			theImitaterType == SeedType::SEED_GARLIC || theImitaterType == SeedType::SEED_LILYPAD)
			aDrawVariation = DrawVariation::VARIATION_IMITATER_LESS;
	}
	else if (theDrawVariation == DrawVariation::VARIATION_NORMAL && theSeedType == SeedType::SEED_TANGLEKELP)
	{
		aDrawVariation = DrawVariation::VARIATION_AQUARIUM;
	}

	if (((LawnApp*)gSexyAppBase)->mGameMode == GameMode::GAMEMODE_CHALLENGE_BIG_TIME &&
		(aSeedType == SeedType::SEED_WALLNUT || aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_MARIGOLD))
	{
		aSeedG.mScaleX *= 1.5f;
		aSeedG.mScaleY *= 1.5f;
		aOffsetX = -20.0f;
		aOffsetY = -40.0f;
	}
	if (aSeedType == SeedType::SEED_LEFTPEATER)
	{
		aOffsetX += aSeedG.mScaleX * 80.0f;
		aSeedG.mScaleX *= -1.0f;
	}

	if (Challenge::IsZombieSeedType(aSeedType))
	{
		ZombieType aZombieType = Challenge::IZombieSeedTypeToZombieType(aSeedType);
		if (aZombieType == ZombieType::ZOMBIE_DANCER)
		{
			aSeedG.mScaleX *= 0.8f;
			aSeedG.mScaleY *= 0.8f;
			aOffsetX = 20.0f;
			aOffsetY = 42.0f;
		}
		gLawnApp->mReanimatorCache->DrawCachedZombie(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aZombieType);
	}
	else
	{
		const PlantDefinition& aPlantDef = GetPlantDefinition(aSeedType);

		if (aSeedType == SeedType::SEED_GIANT_WALLNUT)
		{
			aSeedG.mScaleX *= 1.4f;
			aSeedG.mScaleY *= 1.4f;
			PvzpDrawImageScaledF(&aSeedG, IMAGE_REANIM_WALLNUT_BODY, thePosX - 53.0f, thePosY - 56.0f, aSeedG.mScaleX, aSeedG.mScaleY);
		}
		else if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
		{
			gLawnApp->mReanimatorCache->DrawCachedPlant(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aSeedType, aDrawVariation);
		}
		else
		{
			if (aSeedType == SeedType::SEED_KERNELPULT)
			{
				aCelRow = 2;
			}
			else if (aSeedType == SeedType::SEED_EPHRAIM)
			{
				aCelCol = 0;
			}
			else if (aSeedType == SeedType::SEED_TWINSUNFLOWER)
			{
				aCelRow = 1;
			}

			Image* aPlantImage = Plant::GetImage(aSeedType);
			if (aSeedType == SeedType::SEED_EPHRAIM)
			{
				if (aPlantImage == nullptr)
					return;
				const int aCelWidth = aPlantImage->GetCelWidth();
				const int aCelHeight = aPlantImage->GetCelHeight();
				const float aScaleX = aSeedG.mScaleX * EPHRAIM_DRAW_SCALE;
				const float aScaleY = aSeedG.mScaleY * EPHRAIM_DRAW_SCALE;
				// Use the planted sprite's cell center and 80-pixel tile baseline.
				const Rect aSourceRect(0, 0, aCelWidth, aCelHeight);
				const Rect aDestRect(
					static_cast<int>(std::round(thePosX + aOffsetX + 40.0f * aSeedG.mScaleX - aCelWidth * 0.5f * aScaleX)),
					static_cast<int>(std::round(thePosY + aOffsetY + 80.0f * aSeedG.mScaleY - aCelHeight * aScaleY)),
					static_cast<int>(std::round(aCelWidth * aScaleX)),
					static_cast<int>(std::round(aCelHeight * aScaleY)));
				aSeedG.SetFastStretch(true);
				aSeedG.DrawImageMirror(aPlantImage, aDestRect, aSourceRect, true);
			}
			else
			{
				if (aPlantImage->mNumCols <= 2)
				{
					aCelCol = aPlantImage->mNumCols - 1;
				}

				PvzpDrawImageCelScaledF(&aSeedG, aPlantImage, thePosX + aOffsetX, thePosY + aOffsetY, aCelCol, aCelRow, aSeedG.mScaleX, aSeedG.mScaleY);
			}
		}
	}
}
