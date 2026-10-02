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

#pragma once

#include <algorithm>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include "../Entities/GameObject.h"

constexpr const int MAX_MAGNET_ITEMS = 5;

enum PlantSubClass : int32_t
{
	SUBCLASS_NORMAL = 0,
	SUBCLASS_SHOOTER = 1
};

enum PlantWeapon : int32_t
{
	WEAPON_PRIMARY,
	WEAPON_SECONDARY
};

enum PlantOnBungeeState : int32_t
{
	NOT_ON_BUNGEE,
	GETTING_GRABBED_BY_BUNGEE,
	RISING_WITH_BUNGEE
};

enum PlantState : int32_t
{
	STATE_NOTREADY,
	STATE_READY,
	STATE_DOINGSPECIAL,
	STATE_SQUASH_LOOK,
	STATE_SQUASH_PRE_LAUNCH,
	STATE_SQUASH_RISING,
	STATE_SQUASH_FALLING,
	STATE_SQUASH_DONE_FALLING,
	STATE_GRAVEBUSTER_LANDING,
	STATE_GRAVEBUSTER_EATING,
	STATE_CHOMPER_BITING,
	STATE_CHOMPER_BITING_GOT_ONE,
	STATE_CHOMPER_BITING_MISSED,
	STATE_CHOMPER_DIGESTING,
	STATE_CHOMPER_SWALLOWING,
	STATE_POTATO_RISING,
	STATE_POTATO_ARMED,
	STATE_POTATO_MASHED,
	STATE_SPIKEWEED_ATTACKING,
	STATE_SPIKEWEED_ATTACKING_2,
	STATE_SCAREDYSHROOM_LOWERING,
	STATE_SCAREDYSHROOM_SCARED,
	STATE_SCAREDYSHROOM_RAISING,
	STATE_SUNSHROOM_SMALL,
	STATE_SUNSHROOM_GROWING,
	STATE_SUNSHROOM_BIG,
	STATE_MAGNETSHROOM_SUCKING,
	STATE_MAGNETSHROOM_CHARGING,
	STATE_BOWLING_UP,
	STATE_BOWLING_DOWN,
	STATE_CACTUS_LOW,
	STATE_CACTUS_RISING,
	STATE_CACTUS_HIGH,
	STATE_CACTUS_LOWERING,
	STATE_TANGLEKELP_GRABBING,
	STATE_COBCANNON_ARMING,
	STATE_COBCANNON_LOADING,
	STATE_COBCANNON_READY,
	STATE_COBCANNON_FIRING,
	STATE_KERNELPULT_BUTTER,
	STATE_UMBRELLA_TRIGGERED,
	STATE_UMBRELLA_REFLECTING,
	STATE_IMITATER_MORPHING,
	STATE_ZEN_GARDEN_WATERED,
	STATE_ZEN_GARDEN_NEEDY,
	STATE_ZEN_GARDEN_HAPPY,
	STATE_MARIGOLD_ENDING,
	STATE_FLOWERPOT_INVULNERABLE,
	STATE_LILYPAD_INVULNERABLE
};

enum PLANT_LAYER : int32_t
{
	PLANT_LAYER_BELOW = -1,
	PLANT_LAYER_MAIN,
	PLANT_LAYER_REANIM,
	PLANT_LAYER_REANIM_HEAD,
	PLANT_LAYER_REANIM_BLINK,
	PLANT_LAYER_ON_TOP,
	NUM_PLANT_LAYERS
};

enum PLANT_ORDER : int32_t
{
	PLANT_ORDER_LILYPAD,
	PLANT_ORDER_NORMAL,
	PLANT_ORDER_PUMPKIN,
	PLANT_ORDER_FLYER,
	PLANT_ORDER_CHERRYBOMB
};

enum MagnetItemType : int32_t
{
	MAGNET_ITEM_NONE,
	MAGNET_ITEM_PAIL_1,
	MAGNET_ITEM_PAIL_2,
	MAGNET_ITEM_PAIL_3,
	MAGNET_ITEM_FOOTBALL_HELMET_1,
	MAGNET_ITEM_FOOTBALL_HELMET_2,
	MAGNET_ITEM_FOOTBALL_HELMET_3,
	MAGNET_ITEM_DOOR_1,
	MAGNET_ITEM_DOOR_2,
	MAGNET_ITEM_DOOR_3,
	//MAGNET_ITEM_PROPELLER,
	MAGNET_ITEM_POGO_1,
	MAGNET_ITEM_POGO_2,
	MAGNET_ITEM_POGO_3,
	MAGNET_ITEM_JACK_IN_THE_BOX,
	MAGNET_ITEM_LADDER_1,
	MAGNET_ITEM_LADDER_2,
	MAGNET_ITEM_LADDER_3,
	MAGNET_ITEM_LADDER_PLACED,
	MAGNET_ITEM_SILVER_COIN,
	MAGNET_ITEM_GOLD_COIN,
	MAGNET_ITEM_DIAMOND,
	MAGNET_ITEM_PICK_AXE,
	MAGNET_ITEM_SUN_15,
	MAGNET_ITEM_SUN_25,
	MAGNET_ITEM_SUN_50,
	MAGNET_ITEM_SUN_150,
	MAGNET_ITEM_SUN_500,
	MAGNET_ITEM_SUN_100,
	MAGNET_ITEM_SUN_600,
	MAGNET_ITEM_SUN_RANDOM_MIN = 1000,
	MAGNET_ITEM_SUN_RANDOM_MAX = 1100,
	MAGNET_ITEM_SUN_DYNAMIC_BASE = 1000000
};

class MagnetItem
{
public:
	float                   mPosX;
	float                   mPosY;
	float                   mDestOffsetX;
	float                   mDestOffsetY;
	MagnetItemType          mItemType;
};

class Coin;
class Zombie;
class Reanimation;
class PvzpParticleSystem;

class Plant : public GameObject
{
public:
	static constexpr int EPHRAIM_IDLE_ROW = 0;
	static constexpr int EPHRAIM_LANCE_ROW = 1;
	static constexpr int EPHRAIM_CRITICAL_LANCE_ROW = 2;
	static constexpr int EPHRAIM_IDLE_FRAME_COUNT = 6;

	// Each variant owns one complete windup, strike and recovery atlas.
	static constexpr std::array<int, 4> EPHRAIM_ATLAS_FRAME_COUNTS = { 21, 21, 7, 11 };
	// Hold preparation/recovery poses; pass quickly through drawn motion smears.
	static constexpr std::array<std::array<int, 21>, 4> EPHRAIM_ATTACK_FRAME_TICKS = {{
		{ 6, 6, 6, 6, 8, 9, 6, 5, 4, 4, 4, 3, 5, 5, 6, 6, 8, 6, 6, 7, 10 },
		{ 6, 9, 12, 14, 12, 11, 6, 3, 2, 2, 2, 2, 5, 5, 6, 6, 8, 6, 6, 7, 10 },
		{ 10, 15, 10, 6, 4, 10, 15 },
		{ 11, 16, 11, 7, 7, 10, 7, 7, 5, 12, 17 }
	}};
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_DURATION_TICKS = []
	{
		std::array<int, 4> aDurations{};
		for (int aSet = 0; aSet < 4; aSet++)
			for (int aFrame = 0; aFrame < EPHRAIM_ATLAS_FRAME_COUNTS[aSet]; aFrame++)
				aDurations[aSet] += EPHRAIM_ATTACK_FRAME_TICKS[aSet][aFrame];
		return aDurations;
	}();
	// Stop on readable poses, leaving the charge/slash smears free to advance.
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_IMPACT_FRAMES = { 10, 10, 4, 8 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_ANTICIPATION_FRAMES = { 5, 1, 3, 3 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_ANTICIPATION_TICKS = { 22, 46, 12, 32 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_HITSTOP_TICKS = { 20, 36, 8, 16 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_MISS_STOP_TICKS = { 5, 9, 2, 4 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_RELEASE_FRAMES = { 10, 10, 5, 9 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_RELEASE_TICKS = { 0, 0, 10, 18 };
	static constexpr std::array<int, 4> EPHRAIM_RANGED_WINDUP_TICKS = { 0, 0, 18, 38 };
	static constexpr std::array<int, 4> EPHRAIM_RANGED_RECOIL_TICKS = { 0, 0, 18, 34 };
	static constexpr std::array<int, 4> EPHRAIM_RANGED_REST_TICKS = { 0, 0, 40, 80 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_RECOIL_FRAMES = { 18, 18, 6, 10 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_RECOIL_TICKS = { 26, 42, 12, 30 };
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_INTERVAL_TICKS = { 55, 95, 28, 65 };
	static constexpr int EPHRAIM_ATLAS_COUNT = static_cast<int>(EPHRAIM_ATLAS_FRAME_COUNTS.size());
	static constexpr int EPHRAIM_ATTACK_VARIANT_COUNT = EPHRAIM_ATLAS_COUNT;
	static constexpr int EphraimAttackAtlasFrame(int theAttackSet, int theElapsedTicks)
	{
		int aFrameEnd = 0;
		for (int aFrame = 0; aFrame < EPHRAIM_ATLAS_FRAME_COUNTS[theAttackSet]; aFrame++)
		{
			aFrameEnd += EPHRAIM_ATTACK_FRAME_TICKS[theAttackSet][aFrame];
			if (theElapsedTicks < aFrameEnd)
				return aFrame;
		}
		return EPHRAIM_ATLAS_FRAME_COUNTS[theAttackSet] - 1;
	}
	static constexpr int EPHRAIM_ATTACK_RANGE_FRONT = 200;
	static constexpr int EPHRAIM_ATTACK_RANGE_BEHIND = 200;
	static constexpr float EPHRAIM_DRAW_SCALE = 2.1875f;
	static constexpr int EPHRAIM_ATTACK_ANIMATION_TICKS = 140;
	static constexpr int EPHRAIM_LANCE_IMPACT_FRAME = 2;
	static constexpr int EPHRAIM_CRITICAL_LANCE_IMPACT_FRAME = 2;
	static constexpr int EPHRAIM_HIT_STOP_TICKS = 46;
	static constexpr int EPHRAIM_ATTACK_DAMAGE = 80;
	static constexpr int EPHRAIM_ATTACK_HEAL_PER_MILLE = 15;
	// Lance, critical lance, javelin, critical javelin: heavier attacks hit harder.
	static constexpr std::array<int, 4> EPHRAIM_ATTACK_DAMAGE_PERCENT = { 250, 350, 100, 175 };
	static constexpr int EPHRAIM_PROJECTILE_DAMAGE_PERCENT = 125;
	static constexpr int EPHRAIM_JAVELIN_WIDTH = 136;
	static constexpr int EPHRAIM_JAVELIN_HEIGHT = 18;
	static constexpr float EPHRAIM_JAVELIN_SPEED = 5.0f;
	static constexpr float EPHRAIM_WINDUP_JAVELIN_SPEED = 8.0f;
	static constexpr int EPHRAIM_JAVELIN_KNOCKBACK_DISTANCE = 24;
	static constexpr int EPHRAIM_JAVELIN_STAGGER_TICKS = 40;
	static constexpr int EPHRAIM_AFTERIMAGE_CHANCE_PERCENT = 35;
	static constexpr int EPHRAIM_AFTERIMAGE_CHANCE_INCREMENT_PERCENT = 15;
	static constexpr int EPHRAIM_MAX_AFTERIMAGES = 5;
	static constexpr int EPHRAIM_MAX_AFTERIMAGE_NESTING = 3;
	static constexpr int EPHRAIM_AFTERIMAGE_TRAIL_OFFSET = 12;
	static constexpr int EPHRAIM_AFTERIMAGE_SPACING = 28;
	static constexpr int EPHRAIM_MAX_AFTERIMAGE_TRAIL_OFFSET = EPHRAIM_AFTERIMAGE_TRAIL_OFFSET +
		(EPHRAIM_MAX_AFTERIMAGES - 1 + EPHRAIM_MAX_AFTERIMAGE_NESTING * EPHRAIM_MAX_AFTERIMAGES) * EPHRAIM_AFTERIMAGE_SPACING;
	static constexpr int EPHRAIM_ATTACK_FLAG_ANTICIPATION_PAUSE = 1 << 0;
	static constexpr int EPHRAIM_ATTACK_FLAG_PRIMARY_IMPACT = 1 << 1;
	static constexpr int EPHRAIM_ATTACK_FLAG_AFTERIMAGE = 1 << 2;
	static constexpr int EPHRAIM_ATTACK_FLAG_AFTERIMAGE_IMPACT = 1 << 3;
	static constexpr int EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_SHIFT = 4;
	static constexpr int EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_MASK = 7 << EPHRAIM_ATTACK_FLAG_AFTERIMAGE_SET_SHIFT;
	static constexpr int EPHRAIM_ATTACK_FLAG_AFTERIMAGE_ANTICIPATION = 1 << 7;
	static constexpr int EPHRAIM_ATTACK_FLAG_RANGED = 1 << 8;
	static constexpr int EPHRAIM_ATTACK_FLAG_RECOIL = 1 << 9;
	static constexpr int EPHRAIM_ATTACK_FLAG_RELEASE = 1 << 10;
	static constexpr int EPHRAIM_ATTACK_FLAG_RECOVERY = 1 << 11;
	static constexpr int EPHRAIM_ATTACK_FLAGS_MASK = (1 << 12) - 1;

	SeedType                mSeedType;
	int32_t                 mPlantCol;
	int32_t                 mAnimCounter;
	int32_t                 mFrame;
	int32_t                 mFrameLength;
	int32_t                 mNumFrames;
	PlantState              mState;
	int32_t                 mPlantHealth;
	int32_t                 mPlantMaxHealth;
	float                   mContinuousHealthRemainder = 0.0f;
	int32_t                 mSubclass;
	int32_t                 mDisappearCountdown;
	int32_t                 mDoSpecialCountdown;
	int32_t                 mStateCountdown;
	int32_t                 mLaunchCounter;
	int32_t                 mLaunchRate;
	Rect                    mPlantRect;
	Rect                    mPlantAttackRect;
	int32_t                 mTargetX;
	int32_t                 mTargetY;
	int32_t                 mStartRow;
	ParticleSystemID        mParticleID;
	int32_t                 mShootingCounter;
	int32_t                 mEphraimAttackSet;
	int32_t                 mEphraimHitStopCounter;
	int32_t                 mEphraimAttackPauseFlags;
	int32_t                 mEphraimAfterimageFrame;
	int32_t                 mEphraimAfterimageChancePercent = EPHRAIM_AFTERIMAGE_CHANCE_PERCENT;
	int32_t                 mEphraimAfterimageFailureCount = 0;
	int32_t                 mEphraimAfterimagesRemaining = 0;
	struct EphraimAfterimage
	{
		int32_t mAttackSet = 0;
		int32_t mElapsedTicks = 0;
		int32_t mDelayTicks = 0;
		int32_t mHitStopTicks = 0;
		int32_t mPauseFlags = 0;
		int32_t mTargetX = 0;
		int32_t mTrailOffset = EPHRAIM_AFTERIMAGE_TRAIL_OFFSET;
		int32_t mNestingDepth = 0;
		int32_t mOriginX = -10000; // Older saves derive the origin from their stored trail.
	};
	std::vector<EphraimAfterimage> mEphraimAfterimages;
	ReanimationID           mBodyReanimID;
	ReanimationID           mHeadReanimID;
	ReanimationID           mHeadReanimID2;
	ReanimationID           mHeadReanimID3;
	ReanimationID           mBlinkReanimID;
	ReanimationID           mLightReanimID;
	ReanimationID           mSleepingReanimID;
	int32_t                 mBlinkCountdown;
	int32_t                 mRecentlyEatenCountdown;
	int32_t                 mEatenFlashCountdown;
	int32_t                 mBeghouledFlashCountdown;
	float                   mShakeOffsetX;
	float                   mShakeOffsetY;
	MagnetItem              mMagnetItems[MAX_MAGNET_ITEMS];
	ZombieID                mTargetZombieID;
	int32_t                 mWakeUpCounter;
	PlantOnBungeeState      mOnBungeeState;
	SeedType                mImitaterType;
	int32_t                 mPottedPlantIndex;
	bool                    mAnimPing;
	bool                    mDead;
	bool                    mSquished;
	bool                    mIsAsleep;
	bool                    mIsOnBoard;
	bool                    mHighlighted;
	ProjectileType          mGatlingPeaVolleyProjectileType;
	bool                    mGatlingPeaMillionSunVolley = false;
	float                   mGatlingPeaVisualBlend = 0.0f;
	ZombieID                mCattailTargetZombieID = ZombieID::ZOMBIEID_NULL;
	int32_t                 mSunMagnetCoffeeTicksRemaining = 0;
	int32_t                 mSunMagnetCoffeeTicksUntilDamage = 0;
	bool                    mSunMagnetHasPendingPickup = false;
	int32_t                 mTwinSunflowerBombCountdown = 225;

public:
	Plant();

	void                    PlantInitialize(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType);
	void                    Update();
	void                    Animate();
	void                    Draw(Graphics* g);
	void                    MouseDown(int x, int y, int theClickCount);
	void                    DoSpecial();
	void                    Fire(Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY,
		int theWintermelonVolleyIndex = -1, bool theFireWintermelonCherryBomb = false, bool theSkipCatTailOverdriveVolley = false,
		int theKernelPultVolleyIndex = -1, int theKernelPultVolleySize = 0, bool theMillionSunCatTailVolley = false,
		bool theTwoMillionSunCatTailVolley = false);
	Zombie*                 FindTargetZombie(int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY,
		const std::vector<Zombie*>* theExcludedZombies = nullptr, const int* theAttackTargetX = nullptr, bool theMeleeOnly = false, const int* theAttackOriginX = nullptr);
	void                    LaunchEphraimJavelin(int theTargetX, int theAttackSet, int theTrailOffset = 0, const int* theAttackOriginX = nullptr);
	int                     GetEphraimAfterimageOriginX(const EphraimAfterimage& theEcho) const;
	void                    ConfigureEphraimAfterimage(EphraimAfterimage& theEcho, int theParentSet, int thePreviousSet);
	void                    SpawnEphraimAfterimages();
	void                    HealEphraimOnAttack();
	int                     RollEphraimAfterimageCount();
	void                    UpdateEphraimAfterimages();
	void                    Die();
	void                    UpdateProductionPlant();
	void                    UpdatePlanternAttack();
	bool                    HasPlanternTarget();
	bool                    PlanternCoffeeBeanVolley(bool theAutoCoffeeBean = false);
	void                    FirePlanternBomb(Zombie* theTarget);
	bool                    FirePlanternCobBomb(Zombie* theTarget, bool theAutoCoffeeBean = false);
	void                    UpdateShooter();
	bool                    FindTargetAndFire(int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
	void                    LaunchThreepeater();
	static Image*           GetImage(SeedType theSeedType);
	static int              GetCost(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	static std::string       GetNameString(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	static std::string       GetToolTip(SeedType theSeedType);
	static int              GetRefreshTime(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	static bool  IsNocturnal(SeedType theSeedtype);
	static bool  IsFungus(SeedType theSeedType);
	static bool  IsAquatic(SeedType theSeedType);
	static bool  IsFlying(SeedType theSeedtype);
	static bool  IsUpgrade(SeedType theSeedtype);
	void                    UpdateAbilities();
	void                    Squish();
	void                    DoRowAreaDamage(int theDamage, unsigned int theDamageFlags);
	int                     GetDamageRangeFlags(PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
	Rect                    GetPlantRect();
	Rect                    GetPlantAttackRect(PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
	Zombie*                 FindSquashTarget();
	void                    UpdateSquash();
	bool         NotOnGround();
	void                    DoSquashDamage();
	void                    BurnRow(int theRow);
	void                    IceZombies();
	void                    BlowAwayFliers();
	void                    UpdateGraveBuster();
	PvzpParticleSystem*      AddAttachedParticle(int thePosX, int thePosY, int theRenderPosition, ParticleEffect theEffect);
	void                    GetPeaHeadOffset(int& theOffsetX, int& theOffsetY);
	bool         MakesSun();
	static void             DrawSeedType(Graphics* g, SeedType theSeedType, SeedType theImitaterType, DrawVariation theDrawVariation, float thePosX, float thePosY);
	void                    KillAllPlantsNearDoom();
	bool                    IsOnHighGround();
	void                    UpdateTorchwood();
	void                    LaunchStarFruit();
	bool                    FindStarFruitTarget();
	void                    UpdateChomper();
	void                    DoBlink();
	void                    UpdateBlink();
	void                    PlayBodyReanim(const char* theTrackName, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate);
	void                    UpdateMagnetShroom();
	MagnetItem*             GetFreeMagnetItem();
	void                    DrawMagnetItems(Graphics* g);
	void                    UpdateDoomShroom();
	void                    UpdateIceShroom();
	void                    UpdatePotato();
	int                     CalcRenderOrder();
	void                    AnimateNuts();
	void                    SetSleeping(bool theIsAsleep);
	void                    UpdateShooting();
	void                    DrawShadow(Graphics* g, float theOffsetX, float theOffsetY);
	void                    UpdateScaredyShroom();
	int                     DistanceToClosestZombie();
	void                    UpdateSpikeweed();
	void                    MagnetShroomAttactItem(Zombie* theZombie);
	void                    UpdateSunShroom();
	void                    UpdateBowling();
	void                    AnimatePumpkin();
	void                    UpdateBlover();
	void                    UpdateCactus();
	void                    StarFruitFire();
	void                    UpdateTanglekelp();
	Reanimation*            AttachBlinkAnim(Reanimation* theReanimBody);
	void                    UpdateReanimColor();
	bool                    IsUpgradableTo(SeedType theUpgradedType);
	bool                    IsPartOfUpgradableTo(SeedType theUpgradedType);
	bool                    IsChomper() const;
	bool                    IsTallNut() const;
	void                    UpdateCobCannon();
	void                    CobCannonFire(int theTargetX, int theTargetY);
	void                    UpdateGoldMagnetShroom();
	void                    StartSunMagnetCoffeeBoost();
	void                    HealPlantsWithSun();
	bool         IsOnBoard();
	void                    RemoveEffects();
	void                    UpdateCoffeeBean();
	void                    UpdateUmbrella();
	void                    EndBlink();
	void                    AnimateGarlic();
	Coin*                   FindGoldMagnetTarget();
	void                    SpikeweedAttack();
	void                    ImitaterMorph();
	void                    UpdateImitater();
	void                    UpdateReanim();
	void                    SpikyTakeDamage();
	void                    GargantuarSmashTakeDamage();
	bool                    IsSpiky();
	static void  PreloadPlantResources(SeedType theSeedType);
	bool         IsInPlay();
	void                    UpdateNeedsFood() { ; }
	void                    PlayIdleAnim(float theRate);
	void                    UpdateFlowerPot();
	void                    UpdateLilypad();
	bool                    GoldMagnetFindTargets();
	bool                    CollectSunMagnetCoin(Coin* theCoin);
	bool                    IsAGoldMagnetAboutToSuck();
	bool                    DrawMagnetItemsOnTop();
};

float                       PlantDrawHeightOffset(Board* theBoard, Plant* thePlant, SeedType theSeedType, int theCol, int theRow);
float                       PlantFlowerPotHeightOffset(SeedType theSeedType, float theFlowerPotScale);

class PlantDefinition
{
public:
	SeedType                mSeedType;
	Image**                 mPlantImage;
	ReanimationType         mReanimationType;
	int                     mPacketIndex;
	int                     mSeedCost;
	int                     mRefreshTime;
	PlantSubClass           mSubClass;
	int                     mLaunchRate;
	const char*         mPlantName;
};
extern const PlantDefinition gPlantDefs[SeedType::NUM_SEED_TYPES];
extern Image* gEphraimPlantImages[1];  // test plant atlas; filled in during Init resource load

const PlantDefinition& GetPlantDefinition(SeedType theSeedType);
