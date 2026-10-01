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

#include "PvzpDebug.h"
#include "Definition.h"
#include "PvzpParticle.h"
#include "EffectSystem.h"
#include "misc/FrameProfiler.h"
#include "../LawnApp.h"
#include "../GameConstants.h"
#include "graphics/Graphics.h"
#include "graphics/GLInterface.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <format>
#include <mutex>
#include <thread>


int gParticleDefCount;
std::unique_ptr<PvzpParticleDefinition[]> gParticleDefArray;
int gParticleParamArraySize;
const ParticleParams* gParticleParamArray;

constinit const ParticleParams gLawnParticleArray[ParticleEffect::NUM_PARTICLES] = {
	{ .mParticleEffect = ParticleEffect::PARTICLE_MELONSPLASH, .mParticleFileName = "particles/MelonImpact.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_WINTERMELON, .mParticleFileName = "particles/WinterMelonImpact.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_FUMECLOUD, .mParticleFileName = "particles/FumeCloud.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POPCORNSPLASH, .mParticleFileName = "particles/PopcornSplash.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POWIE, .mParticleFileName = "particles/Powie.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_JACKEXPLODE, .mParticleFileName = "particles/JackExplode.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD, .mParticleFileName = "particles/ZombieHead.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_ARM, .mParticleFileName = "particles/ZombieArm.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_TRAFFIC_CONE, .mParticleFileName = "particles/ZombieTrafficCone.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_PAIL, .mParticleFileName = "particles/ZombiePail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_HELMET, .mParticleFileName = "particles/ZombieHelmet.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_FLAG, .mParticleFileName = "particles/ZombieFlag.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_DOOR, .mParticleFileName = "particles/ZombieDoor.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER, .mParticleFileName = "particles/ZombieNewspaper.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_HEADLIGHT, .mParticleFileName = "particles/ZombieHeadLight.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POW, .mParticleFileName = "particles/Pow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_POGO, .mParticleFileName = "particles/ZombiePogo.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER_HEAD, .mParticleFileName = "particles/ZombieNewspaperHead.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_BALLOON_HEAD, .mParticleFileName = "particles/ZombieBalloonHead.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SOD_ROLL, .mParticleFileName = "particles/SodRoll.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_GRAVE_STONE_RISE, .mParticleFileName = "particles/GraveStoneRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PLANTING, .mParticleFileName = "particles/Planting.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PLANTING_POOL, .mParticleFileName = "particles/PlantingPool.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_RISE, .mParticleFileName = "particles/ZombieRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_GRAVE_BUSTER, .mParticleFileName = "particles/GraveBuster.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE, .mParticleFileName = "particles/GraveBusterDie.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POOL_SPLASH, .mParticleFileName = "particles/PoolSplash.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICE_SPARKLE, .mParticleFileName = "particles/IceSparkle.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SEED_PACKET, .mParticleFileName = "particles/SeedPacket.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_TALL_NUT_BLOCK, .mParticleFileName = "particles/TallNutBlock.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DOOM, .mParticleFileName = "particles/Doom.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DIGGER_RISE, .mParticleFileName = "particles/DiggerRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DIGGER_TUNNEL, .mParticleFileName = "particles/DiggerTunnel.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DANCER_RISE, .mParticleFileName = "particles/DancerRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POOL_SPARKLY, .mParticleFileName = "particles/PoolSparkly.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_WALLNUT_EAT_SMALL, .mParticleFileName = "particles/WallnutEatSmall.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE, .mParticleFileName = "particles/WallnutEatLarge.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PEA_SPLAT, .mParticleFileName = "particles/PeaSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_BUTTER_SPLAT, .mParticleFileName = "particles/ButterSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CABBAGE_SPLAT, .mParticleFileName = "particles/CabbageSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PUFF_SPLAT, .mParticleFileName = "particles/PuffSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_STAR_SPLAT, .mParticleFileName = "particles/StarSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICE_TRAP, .mParticleFileName = "particles/IceTrap.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SNOWPEA_SPLAT, .mParticleFileName = "particles/SnowPeaSplat.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SNOWPEA_PUFF, .mParticleFileName = "particles/SnowPeaPuff.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SNOWPEA_TRAIL, .mParticleFileName = "particles/SnowPeaTrail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_LANTERN_SHINE, .mParticleFileName = "particles/LanternShine.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SEED_PACKET_PICKUP, .mParticleFileName = "particles/Award.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POTATO_MINE, .mParticleFileName = "particles/PotatoMine.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POTATO_MINE_RISE, .mParticleFileName = "particles/PotatoMineRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL, .mParticleFileName = "particles/PuffShroomTrail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE, .mParticleFileName = "particles/PuffShroomMuzzle.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SEED_PACKET_FLASH, .mParticleFileName = "particles/SeedPacketFlash.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_WHACK_A_ZOMBIE_RISE, .mParticleFileName = "particles/WhackAZombieRise.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_LADDER, .mParticleFileName = "particles/ZombieLadder.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_UMBRELLA_REFLECT, .mParticleFileName = "particles/UmbrellaReflect.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SEED_PACKET_PICK, .mParticleFileName = "particles/SeedPacketPick.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICE_TRAP_ZOMBIE, .mParticleFileName = "particles/IceTrapZombie.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICE_TRAP_RELEASE, .mParticleFileName = "particles/IceTrapRelease.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZAMBONI_SMOKE, .mParticleFileName = "particles/ZamboniSmoke.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_GLOOMCLOUD, .mParticleFileName = "particles/GloomCloud.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_POGO_HEAD, .mParticleFileName = "particles/ZombiePogoHead.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZAMBONI_TIRE, .mParticleFileName = "particles/ZamboniTire.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, .mParticleFileName = "particles/ZamboniExplosion.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION2, .mParticleFileName = "particles/ZamboniExplosion2.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, .mParticleFileName = "particles/CatapultExplosion.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_MOWER_CLOUD, .mParticleFileName = "particles/MowerCloud.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_BOSS_ICE_BALL, .mParticleFileName = "particles/BossIceBallTrail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_BLASTMARK, .mParticleFileName = "particles/BlastMark.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_COIN_PICKUP_ARROW, .mParticleFileName = "particles/CoinPickupArrow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PRESENT_PICKUP, .mParticleFileName = "particles/PresentPickup.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_IMITATER_MORPH, .mParticleFileName = "particles/ImitaterMorph.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD, .mParticleFileName = "particles/MoweredZombieHead.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM, .mParticleFileName = "particles/MoweredZombieArm.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL, .mParticleFileName = "particles/ZombieHeadPool.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL, .mParticleFileName = "particles/Zombie_boss_fireball.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_FIREBALL_DEATH, .mParticleFileName = "particles/FireballDeath.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICEBALL_DEATH, .mParticleFileName = "particles/IceballDeath.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ICEBALL_TRAIL, .mParticleFileName = "particles/Iceball_Trail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_FIREBALL_TRAIL, .mParticleFileName = "particles/Fireball_Trail.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_BOSS_EXPLOSION, .mParticleFileName = "particles/BossExplosion.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_SCREEN_FLASH, .mParticleFileName = "particles/ScreenFlash.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_TROPHY_SPARKLE, .mParticleFileName = "particles/TrophySparkle.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PORTAL_CIRCLE, .mParticleFileName = "particles/PortalCircle.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PORTAL_SQUARE, .mParticleFileName = "particles/PortalSquare.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POTTED_PLANT_GLOW, .mParticleFileName = "particles/PottedPlantGlow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW, .mParticleFileName = "particles/PottedWaterPlantGlow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_POTTED_ZEN_GLOW, .mParticleFileName = "particles/PottedZenGlow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_MIND_CONTROL, .mParticleFileName = "particles/MindControl.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_VASE_SHATTER, .mParticleFileName = "particles/VaseShatter.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_VASE_SHATTER_LEAF, .mParticleFileName = "particles/VaseShatterLeaf.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_VASE_SHATTER_ZOMBIE, .mParticleFileName = "particles/VaseShatterZombie.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW, .mParticleFileName = "particles/AwardPickupArrow.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, .mParticleFileName = "particles/Zombie_seaweed.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_MUSTACHE, .mParticleFileName = "particles/ZombieMustache.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS, .mParticleFileName = "particles/ZombieFutureGlasses.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_PINATA, .mParticleFileName = "particles/Pinata.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DUST_SQUASH, .mParticleFileName = "particles/Dust_Squash.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_DUST_FOOT, .mParticleFileName = "particles/Dust_Foot.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_ZOMBIE_DAISIES, .mParticleFileName = "particles/Daisy.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CREDIT_STROBE, .mParticleFileName = "particles/Credits_Strobe.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CREDITS_RAYSWIPE, .mParticleFileName = "particles/Credits_RaysWipe.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CREDITS_ZOMBIEHEADWIPE, .mParticleFileName = "particles/Credits_ZombieHeadWipe.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_STARBURST, .mParticleFileName = "particles/Starburst.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_CREDITS_FOG, .mParticleFileName = "particles/Credits_fog.xml" },
	{ .mParticleEffect = ParticleEffect::PARTICLE_PERSENT_PICK_UP_ARROW, .mParticleFileName = "particles/UpsellArrow.xml" },
};  // 0x6A0FF0

bool PvzpParticleLoadADef(PvzpParticleDefinition* theParticleDef, const char* theParticleFileName)
{
	PvzpHesitationBracket("Load Particle {}", theParticleFileName);
	if (!DefinitionLoadXML(theParticleFileName, &gParticleDefMap, theParticleDef))
	{
		return false;
	}
	else
	{
		for (int i = 0; i < theParticleDef->mEmitterDefCount; i++)
		{
			PvzpEmitterDefinition& aDef = theParticleDef->mEmitterDefs[i];
			FloatTrackSetDefault(aDef.mSystemDuration, 0.0f);
			FloatTrackSetDefault(aDef.mSpawnRate, 0.0f);
			FloatTrackSetDefault(aDef.mSpawnMinActive, -1.0f);
			FloatTrackSetDefault(aDef.mSpawnMaxActive, -1.0f);
			FloatTrackSetDefault(aDef.mSpawnMaxLaunched, -1.0f);
			FloatTrackSetDefault(aDef.mEmitterRadius, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterOffsetX, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterOffsetY, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterBoxX, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterBoxY, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterSkewX, 0.0f);
			FloatTrackSetDefault(aDef.mEmitterSkewY, 0.0f);
			FloatTrackSetDefault(aDef.mParticleDuration, 100.0f);
			FloatTrackSetDefault(aDef.mLaunchSpeed, 0.0f);
			FloatTrackSetDefault(aDef.mSystemRed, 1.0f);
			FloatTrackSetDefault(aDef.mSystemGreen, 1.0f);
			FloatTrackSetDefault(aDef.mSystemBlue, 1.0f);
			FloatTrackSetDefault(aDef.mSystemAlpha, 1.0f);
			FloatTrackSetDefault(aDef.mSystemBrightness, 1.0f);
			FloatTrackSetDefault(aDef.mLaunchAngle, 0.0f);
			FloatTrackSetDefault(aDef.mCrossFadeDuration, 0.0f);
			FloatTrackSetDefault(aDef.mParticleRed, 1.0f);
			FloatTrackSetDefault(aDef.mParticleGreen, 1.0f);
			FloatTrackSetDefault(aDef.mParticleBlue, 1.0f);
			FloatTrackSetDefault(aDef.mParticleAlpha, 1.0f);
			FloatTrackSetDefault(aDef.mParticleBrightness, 1.0f);
			FloatTrackSetDefault(aDef.mParticleSpinAngle, 0.0f);
			FloatTrackSetDefault(aDef.mParticleSpinSpeed, 0.0f);
			FloatTrackSetDefault(aDef.mParticleScale, 1.0f);
			FloatTrackSetDefault(aDef.mParticleStretch, 1.0f);
			FloatTrackSetDefault(aDef.mCollisionReflect, 0.0f);
			FloatTrackSetDefault(aDef.mCollisionSpin, 0.0f);
			FloatTrackSetDefault(aDef.mClipTop, 0.0f);
			FloatTrackSetDefault(aDef.mClipBottom, 0.0f);
			FloatTrackSetDefault(aDef.mClipLeft, 0.0f);
			FloatTrackSetDefault(aDef.mClipRight, 0.0f);
			FloatTrackSetDefault(aDef.mAnimationRate, 0.0f);
			if (aDef.mImage)
				reinterpret_cast<MemoryImage*>(aDef.mImage)->mRenderFlags |= RenderImageFlags::RenderImageFlag_MinimizeNumSubdivisions;
		}
		return true;
	}
}

void PvzpParticleLoadDefinitions(const ParticleParams* theParticleParamArray, int theParticleParamArraySize)
{
	PvzpHesitationBracket aHesitiation("PvzpParticleLoadDefinitions");
	PVZP_ASSERT(!gParticleParamArray && !gParticleDefArray);
	gParticleParamArraySize = theParticleParamArraySize;
	gParticleParamArray = theParticleParamArray;
	gParticleDefCount = theParticleParamArraySize;
	gParticleDefArray = std::make_unique<PvzpParticleDefinition[]>(theParticleParamArraySize);

	for (int i = 0; i < gParticleParamArraySize; i++)
	{
		const ParticleParams& aParticleParams = gParticleParamArray[i];
		PVZP_ASSERT(aParticleParams.mParticleEffect == i);
		if (!PvzpParticleLoadADef(&gParticleDefArray[i], aParticleParams.mParticleFileName))
		{
			PvzpErrorMessageBox(std::format("Failed to load particle '{}'", aParticleParams.mParticleFileName), "Error");
		}
		gSexyAppBase->mCompletedLoadingThreadTasks += 6;
	}
}

void PvzpParticleFreeDefinitions()
{
	for (int i = 0; i < gParticleDefCount; i++)
		DefinitionFreeMap(&gParticleDefMap, &gParticleDefArray[i]);
	gParticleDefArray.reset();
	gParticleDefCount = 0;
	gParticleParamArray = nullptr;
	gParticleParamArraySize = 0;
}

