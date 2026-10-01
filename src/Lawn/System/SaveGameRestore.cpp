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

#include "Music.h"
#include "SaveGame.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../MessageWidget.h"
#include "../../PvzpLib/Trail.h"
#include "zlib.h"
#include "../../PvzpLib/Attachment.h"
#include "../../PvzpLib/Reanimator.h"
#include "../../PvzpLib/PvzpParticle.h"
#include "../../PvzpLib/EffectSystem.h"
#include "../../PvzpLib/DataArray.h"
#include "../../PvzpLib/PvzpList.h"
#include "DataSync.h"
#include "../../GameConstants.h"
#include "misc/Buffer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>


#include "SaveGameInternal.h"

namespace SaveGameInternal
{
void FixBoardAfterLoad(Board* theBoard)
{
	{
		for (Plant* aPlant : theBoard->mPlants)
		{
			aPlant->mApp = theBoard->mApp;
			aPlant->mBoard = theBoard;
		}
	}
	{
		for (Zombie* aZombie : theBoard->mZombies)
		{
			aZombie->mApp = theBoard->mApp;
			aZombie->mBoard = theBoard;

			switch (aZombie->mZombieType)
			{
			case ZombieType::ZOMBIE_GARGANTUAR:
			case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
			case ZombieType::ZOMBIE_BULWARK_GARGANTUAR:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_2);
						aBodyReanim->SetImageOverride("Zombie_gargantuar_outerarm_lower", IMAGE_REANIM_ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_3);
						aBodyReanim->SetImageOverride("Zombie_gargantuar_outerleg_foot", IMAGE_REANIM_ZOMBIE_GARGANTUAR_FOOT2);
					}

					if (aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
					{
						if (aDamageIndex >= 2)
							aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2_REDEYE);
						else
							aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD_REDEYE);
					}
					else if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_ZAMBONI:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE1);
						aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE1);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE2);
						aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE2);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_CATAPULT:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_catapult_siding", IMAGE_REANIM_ZOMBIE_CATAPULT_SIDING_DAMAGE);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_BOSS:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_innerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE1);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE2);
					}
				}
				break;
			}

			default:
				break;
			}
		}
	}
	{
		for (Projectile* aProjectile : theBoard->mProjectiles)
		{
			aProjectile->mApp = theBoard->mApp;
			aProjectile->mBoard = theBoard;
		}
	}
	{
		for (Coin* aCoin : theBoard->mCoins)
		{
			aCoin->mApp = theBoard->mApp;
			aCoin->mBoard = theBoard;
		}
	}
	{
		for (LawnMower* aLawnMower : theBoard->mLawnMowers)
		{
			aLawnMower->mApp = theBoard->mApp;
			aLawnMower->mBoard = theBoard;
			if (LawnApp::IsSurvivalEndless(theBoard->mApp->mGameMode) && theBoard->StageHasPool() &&
				aLawnMower->mRow >= 0 && aLawnMower->mRow < MAX_GRID_SIZE_Y &&
				theBoard->mPlantRow[aLawnMower->mRow] == PlantRowType::PLANTROW_POOL)
				aLawnMower->ConvertToPoolCleaner();
		}
	}
	{
		for (GridItem* aGridItem : theBoard->mGridItems)
		{
			aGridItem->mApp = theBoard->mApp;
			aGridItem->mBoard = theBoard;
		}
	}

	theBoard->mAdvice->mApp = theBoard->mApp;
	theBoard->mCursorObject->mApp = theBoard->mApp;
	theBoard->mCursorObject->mBoard = theBoard;
	theBoard->mCursorPreview->mApp = theBoard->mApp;
	theBoard->mCursorPreview->mBoard = theBoard;
	theBoard->mSeedBank->mApp = theBoard->mApp;
	theBoard->mSeedBank->mBoard = theBoard;
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		theBoard->mSeedBank->mSeedPackets[i].mApp = theBoard->mApp;
		theBoard->mSeedBank->mSeedPackets[i].mBoard = theBoard;
	}
	theBoard->mChallenge->mApp = theBoard->mApp;
	theBoard->mChallenge->mBoard = theBoard;
	theBoard->mApp->mMusic->mApp = theBoard->mApp;
	theBoard->mApp->mMusic->mMusicInterface = theBoard->mApp->mMusicInterface.get();
}

}
