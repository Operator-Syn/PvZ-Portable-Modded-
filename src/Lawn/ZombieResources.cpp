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

void Zombie::PreloadZombieResources(ZombieType theZombieType)
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(theZombieType);
	if (aZombieDef.mReanimationType != ReanimationType::REANIM_NONE)
	{
		ReanimatorEnsureDefinitionLoaded(aZombieDef.mReanimationType, true);
	}

	if (theZombieType == ZombieType::ZOMBIE_DIGGER)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_DIGGER_DIRT, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER, true);
	}
	else if (theZombieType == ZombieType::ZOMBIE_BOSS)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_DRIVER, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_FIREBALL, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_ICEBALL, true);

		for (size_t i = 0; i < LENGTH(gBossZombieList); i++)
		{
			const ZombieDefinition& aDef = GetZombieDefinition(gBossZombieList[i]);
			ReanimatorEnsureDefinitionLoaded(aDef.mReanimationType, true);
		}
	}
	else if (theZombieType == ZombieType::ZOMBIE_DANCER)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BACKUP_DANCER, true);
	}
	else if (ZombieRules::IsGargantuarType(theZombieType))
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_IMP, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED_IMP, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR, true);
	}
	else if (theZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_IMP, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI, true);
	}
	else if (theZombieType == ZombieType::ZOMBIE_CATAPULT)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT, true);
	}

	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
}

void Zombie::ApplyBossSmokeParticles(bool theEnable)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
	ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName("Boss_head");
	AttachmentDetachCrossFadeParticleType(aTrackInstance->mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, nullptr);

	if (theEnable)
	{
		PvzpParticleSystem* aParticle1 = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
		PvzpParticleSystem* aParticle2 = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
		if (aParticle1)
		{
			AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack("Boss_head", aParticle1, 120.0f, 30.0f);
			aAttachEffect->mDontDrawIfParentHidden = true;
			aAttachEffect->mDontPropogateColor = true;
		}
		if (aParticle2)
		{
			AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack("Boss_head", aParticle2, 205.0f, 58.0f);
			aAttachEffect->mDontDrawIfParentHidden = true;
			aAttachEffect->mDontPropogateColor = true;
		}

		if (mBodyHealth < mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
		{
			PvzpParticleSystem* aParticle3 = mApp->AddPvzpParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
			if (aParticle3)
			{
				AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack("Boss_head", aParticle3, 193.0f, 27.0f);
				aAttachEffect->mDontDrawIfParentHidden = true;
				aAttachEffect->mDontPropogateColor = true;
			}
		}
	}
}
