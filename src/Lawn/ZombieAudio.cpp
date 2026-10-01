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

void Zombie::PlayZombieAppearSound()
{
	if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
	{
		mApp->PlayFoley(FoleyType::FOLEY_DOLPHIN_APPEARS);
	}
	else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		mApp->PlayFoley(FoleyType::FOLEY_BALLOONINFLATE);
	}
	else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		mApp->PlayFoley(FoleyType::FOLEY_ZAMBONI);
	}
}

void Zombie::StartZombieSound()
{
	if (mPlayingSong)
		return;

	if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING && mHasHead)
	{
		mApp->PlayFoley(FoleyType::FOLEY_JACKINTHEBOX);
		mPlayingSong = true;
	}
	else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
	{
		mApp->PlayFoley(FoleyType::FOLEY_DIGGER);
		mPlayingSong = true;
	}
}

void Zombie::StopZombieSound()
{
	if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
	{
		bool aStopSound = true;

		if (mBoard)
		{
			for (Zombie* aZombie : mBoard->mZombies)
			{
				if (aZombie->mDead)
					continue;
				if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && aZombie->IsOnBoard() &&
					(aZombie->mZombieType == ZombieType::ZOMBIE_DANCER || aZombie->mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER))
				{
					aStopSound = false;
					break;
				}
			}
		}

		if (aStopSound)
		{
			mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DANCER);
		}
	}

	if (mPlayingSong)
	{
		mPlayingSong = false;

		if (mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
		{
			mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_JACKINTHEBOX);
		}
		else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
		{
			mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DIGGER);
		}
	}
}
