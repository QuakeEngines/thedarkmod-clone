/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/
// Copyright (C) 2004 Id Software, Inc.
//

#include "../idlib/precompiled.h"
#pragma hdrstop

#pragma warning(disable : 4355) // greebo: Disable warning "'this' used in constructor"

static bool init_version = FileVersionList("$Id$", init_version);

#include "game_local.h"
#include "ai/aas_local.h"
#include "../DarkMod/DarkModGlobals.h"
#include "../DarkMod/PlayerData.h"
#include "../DarkMod/Intersection.h"
#include "../DarkMod/Relations.h"
#include "../DarkMod/DarkmodAASHidingSpotFinder.h"
#include "../DarkMod/StimResponse/StimResponseCollection.h"
#include "../DarkMod/MissionData.h"
#include "../DarkMod/Inventory/Inventory.h"
#include "../DarkMod/Inventory/WeaponItem.h"
#include "../DarkMod/shop.h"

/*
===============================================================================

	Player control of the TDM player thief.
	This object handles all player movement and world interaction.

===============================================================================
*/

bool NextFrame = true;

// amount of health per dose from the health station
const int HEALTH_PER_DOSE = 10;

// time before a weapon dropped to the floor disappears
const int WEAPON_DROP_TIME = 20 * 1000;

// time before a next or prev weapon switch happens
const int WEAPON_SWITCH_DELAY = 150;

// how many units to raise spectator above default view height so it's in the head of someone
const int SPECTATE_RAISE = 25;

const int HEALTHPULSE_TIME = 333;

// minimum speed to bob and play run/walk animations at
const float MIN_BOB_SPEED = 5.0f;

// shouldered body immobilizations
const int SHOULDER_IMMOBILIZATIONS = EIM_CLIMB | EIM_ITEM_SELECT | EIM_WEAPON_SELECT | EIM_ATTACK | EIM_ITEM_USE | EIM_MANTLE | EIM_FROB_COMPLEX;
const float SHOULDER_JUMP_HINDERANCE = 0.25f;

const idEventDef EV_Player_GetButtons( "getButtons", NULL, 'd' );
const idEventDef EV_Player_GetMove( "getMove", NULL, 'v' );
const idEventDef EV_Player_GetViewAngles( "getViewAngles", NULL, 'v' );
const idEventDef EV_Player_GetMouseGesture( "getMouseGesture", NULL, 'd');
const idEventDef EV_Player_MouseGestureFinished( "mouseGestureFinished", NULL, 'd' );
const idEventDef EV_Player_StartMouseGesture( "startMouseGesture", "ddddfd" );
const idEventDef EV_Player_StopMouseGesture( "stopMouseGesture" );
const idEventDef EV_Player_StopFxFov( "stopFxFov" );
const idEventDef EV_Player_EnableWeapon( "enableWeapon" );
const idEventDef EV_Player_DisableWeapon( "disableWeapon" );
const idEventDef EV_Player_GetCurrentWeapon( "getCurrentWeapon", NULL, 's' );
const idEventDef EV_Player_GetPreviousWeapon( "getPreviousWeapon", NULL, 's' );
const idEventDef EV_Player_SelectWeapon( "selectWeapon", "s" );
const idEventDef EV_Player_GetWeaponEntity( "getWeaponEntity", NULL, 'e' );
const idEventDef EV_Player_OpenPDA( "openPDA" );
const idEventDef EV_Player_InPDA( "inPDA", NULL, 'd' );
const idEventDef EV_Player_ExitTeleporter( "exitTeleporter" );
const idEventDef EV_Player_StopAudioLog( "stopAudioLog" );
const idEventDef EV_Player_HideTip( "hideTip" );
const idEventDef EV_Player_LevelTrigger( "levelTrigger" );
const idEventDef EV_SpectatorTouch( "spectatorTouch", "et" );
const idEventDef EV_Player_GetIdealWeapon( "getIdealWeapon", NULL, 's' );

const idEventDef EV_Player_GetEyePos( "getEyePos", NULL, 'v' );
const idEventDef EV_Player_SetImmobilization( "setImmobilization", "sd" );
const idEventDef EV_Player_GetImmobilization( "getImmobilization", "s", 'd' );
const idEventDef EV_Player_GetNextImmobilization( "getNextImmobilization", "ss", 's' );
const idEventDef EV_Player_SetHinderance( "setHinderance", "sff" );
const idEventDef EV_Player_GetHinderance( "getHinderance", "s", 'v' );
const idEventDef EV_Player_GetNextHinderance( "getNextHinderance", "ss", 's' );
const idEventDef EV_Player_SetTurnHinderance( "setTurnHinderance", "sff" );
const idEventDef EV_Player_GetTurnHinderance( "getTurnHinderance", "s", 'v' );
const idEventDef EV_Player_GetNextTurnHinderance( "getNextTurnHinderance", "ss", 's' );

const idEventDef EV_Player_SetGui( "setGui", "ds" );
const idEventDef EV_Player_GetInventoryOverlay( "getInventoryOverlay", NULL, 'd' );

const idEventDef EV_Player_PlayStartSound( "playStartSound", NULL );
const idEventDef EV_Player_MissionFailed("missionFailed", NULL );
const idEventDef EV_Player_CustomDeath("customDeath", NULL );
const idEventDef EV_Player_DeathMenu("deathMenu", NULL );
const idEventDef EV_Player_HoldEntity( "holdEntity", "E", 'f' );
const idEventDef EV_Player_HeldEntity( "heldEntity", NULL, 'E' );

const idEventDef EV_Player_RopeRemovalCleanup( "ropeRemovalCleanup", "e" );
// NOTE: The following all take the "user" objective indices, starting at 1 instead of 0
const idEventDef EV_Player_SetObjectiveState( "setObjectiveState", "dd" );
const idEventDef EV_Player_GetObjectiveState( "getObjectiveState", "d", 'd');
const idEventDef EV_Player_SetObjectiveComp( "setObjectiveComp", "ddd" );
const idEventDef EV_Player_GetObjectiveComp( "getObjectiveComp", "dd", 'd' );
const idEventDef EV_Player_ObjectiveUnlatch( "objectiveUnlatch", "d" );
const idEventDef EV_Player_ObjectiveCompUnlatch( "objectiveCompUnlatch", "dd" );
const idEventDef EV_Player_SetObjectiveVisible( "setObjectiveVisible", "dd" );
const idEventDef EV_Player_SetObjectiveOptional( "setObjectiveOptional", "dd" );
const idEventDef EV_Player_SetObjectiveOngoing( "setObjectiveOngoing", "dd" );
const idEventDef EV_Player_SetObjectiveEnabling( "setObjectiveEnabling", "ds" );
// greebo: This allows scripts to set the "healthpool" for gradual healing
const idEventDef EV_Player_GiveHealthPool("giveHealthPool", "f");
const idEventDef EV_Player_WasDamaged("wasDamaged", NULL, 'd');

const idEventDef EV_Mission_Success("missionSuccess", NULL);
const idEventDef EV_TriggerMissionEnd("triggerMissionEnd", NULL);

const idEventDef EV_GetLocation("getLocation", NULL, 'e');

// greebo: These events are handling the FOV.
const idEventDef EV_Player_StartZoom("startZoom", "fff");
const idEventDef EV_Player_EndZoom("endZoom", "f");
const idEventDef EV_Player_ResetZoom("resetZoom", NULL);
const idEventDef EV_Player_GetFov("getFov", NULL, 'f');

const idEventDef EV_Player_PauseGame("pauseGame", NULL);
const idEventDef EV_Player_UnpauseGame("unpauseGame", NULL);
const idEventDef EV_Player_StartGamePlayTimer("startGamePlayTimer", NULL);

const idEventDef EV_CheckAAS("checkAAS", NULL);

// greebo: Allows scripts to set a named lightgem modifier to a certain value (e.g. "lantern" => 32)
const idEventDef EV_Player_SetLightgemModifier("setLightgemModifier", "sd");
const idEventDef EV_ReadLightgemModifierFromWorldspawn("readLightgemModifierFromWorldspawn", NULL);

// greebo: Changes the projectile entityDef name of the given weapon (e.g. "broadhead").
const idEventDef EV_ChangeWeaponProjectile("changeWeaponProjectile", "ss", NULL);
const idEventDef EV_ResetWeaponProjectile("resetWeaponProjectile", "s", NULL);
const idEventDef EV_ChangeWeaponName("changeWeaponName", "ss", NULL);
const idEventDef EV_GetCurWeaponName("getCurWeaponName", NULL, 's');
const idEventDef EV_SetActiveInventoryMapEnt("setActiveInventoryMapEnt", "e");
const idEventDef EV_ClearActiveInventoryMap("clearActiveInventoryMap", NULL);

CLASS_DECLARATION( idActor, idPlayer )
	EVENT( EV_Player_GetButtons,			idPlayer::Event_GetButtons )
	EVENT( EV_Player_GetMove,				idPlayer::Event_GetMove )
	EVENT( EV_Player_GetViewAngles,			idPlayer::Event_GetViewAngles )
	EVENT( EV_Player_GetMouseGesture,		idPlayer::Event_GetMouseGesture )
	EVENT( EV_Player_MouseGestureFinished,	idPlayer::Event_MouseGestureFinished )
	EVENT( EV_Player_StartMouseGesture,		idPlayer::StartMouseGesture )
	EVENT( EV_Player_StopMouseGesture,		idPlayer::StopMouseGesture )
	EVENT( EV_Player_StopFxFov,				idPlayer::Event_StopFxFov )
	EVENT( EV_Player_EnableWeapon,			idPlayer::Event_EnableWeapon )
	EVENT( EV_Player_DisableWeapon,			idPlayer::Event_DisableWeapon )
	EVENT( EV_Player_GetCurrentWeapon,		idPlayer::Event_GetCurrentWeapon )
	EVENT( EV_Player_GetPreviousWeapon,		idPlayer::Event_GetPreviousWeapon )
	EVENT( EV_Player_SelectWeapon,			idPlayer::Event_SelectWeapon )
	EVENT( EV_Player_GetWeaponEntity,		idPlayer::Event_GetWeaponEntity )
	EVENT( EV_Player_OpenPDA,				idPlayer::Event_OpenPDA )
	EVENT( EV_Player_InPDA,					idPlayer::Event_InPDA )
	EVENT( EV_Player_ExitTeleporter,		idPlayer::Event_ExitTeleporter )
	EVENT( EV_Player_StopAudioLog,			idPlayer::Event_StopAudioLog )
	EVENT( EV_Player_HideTip,				idPlayer::Event_HideTip )
	EVENT( EV_Player_LevelTrigger,			idPlayer::Event_LevelTrigger )
	EVENT( EV_Gibbed,						idPlayer::Event_Gibbed )

	EVENT( EV_Player_GetEyePos,				idPlayer::Event_GetEyePos )
	EVENT( EV_Player_SetImmobilization,		idPlayer::Event_SetImmobilization )
	EVENT( EV_Player_GetImmobilization,		idPlayer::Event_GetImmobilization )
	EVENT( EV_Player_GetNextImmobilization,	idPlayer::Event_GetNextImmobilization )
	EVENT( EV_Player_SetHinderance,			idPlayer::Event_SetHinderance )
	EVENT( EV_Player_GetHinderance,			idPlayer::Event_GetHinderance )
	EVENT( EV_Player_GetNextHinderance,		idPlayer::Event_GetNextHinderance )
	EVENT( EV_Player_SetTurnHinderance,		idPlayer::Event_SetTurnHinderance )
	EVENT( EV_Player_GetTurnHinderance,		idPlayer::Event_GetTurnHinderance )
	EVENT( EV_Player_GetNextTurnHinderance,	idPlayer::Event_GetNextTurnHinderance )

	EVENT( EV_Player_SetGui,				idPlayer::Event_SetGui )
	EVENT( EV_Player_GetInventoryOverlay,	idPlayer::Event_GetInventoryOverlay )

	EVENT( EV_Player_PlayStartSound,		idPlayer::Event_PlayStartSound )
	EVENT( EV_Player_MissionFailed,			idPlayer::Event_MissionFailed )
	EVENT( EV_Player_CustomDeath,			idPlayer::Event_CustomDeath )
	EVENT( EV_Player_DeathMenu,				idPlayer::Event_LoadDeathMenu )
	EVENT( EV_Player_HoldEntity,			idPlayer::Event_HoldEntity )
	EVENT( EV_Player_HeldEntity,			idPlayer::Event_HeldEntity )
	EVENT( EV_Player_RopeRemovalCleanup,	idPlayer::Event_RopeRemovalCleanup )
	EVENT( EV_Player_SetObjectiveState,		idPlayer::Event_SetObjectiveState )
	EVENT( EV_Player_GetObjectiveState,		idPlayer::Event_GetObjectiveState )
	EVENT( EV_Player_SetObjectiveComp,		idPlayer::Event_SetObjectiveComp )
	EVENT( EV_Player_GetObjectiveComp,		idPlayer::Event_GetObjectiveComp )
	EVENT( EV_Player_ObjectiveUnlatch,		idPlayer::Event_ObjectiveUnlatch )
	EVENT( EV_Player_ObjectiveCompUnlatch,	idPlayer::Event_ObjectiveComponentUnlatch )
	EVENT( EV_Player_SetObjectiveVisible,	idPlayer::Event_SetObjectiveVisible )
	EVENT( EV_Player_SetObjectiveOptional,	idPlayer::Event_SetObjectiveOptional )
	EVENT( EV_Player_SetObjectiveOngoing,	idPlayer::Event_SetObjectiveOngoing )
	EVENT( EV_Player_SetObjectiveEnabling,	idPlayer::Event_SetObjectiveEnabling )

	EVENT( EV_Player_GiveHealthPool,		idPlayer::Event_GiveHealthPool )
	EVENT( EV_Player_WasDamaged,			idPlayer::Event_WasDamaged )

	EVENT( EV_Player_SetLightgemModifier,	idPlayer::Event_SetLightgemModifier )
	EVENT( EV_ReadLightgemModifierFromWorldspawn, idPlayer::Event_ReadLightgemModifierFromWorldspawn )

	EVENT( EV_Player_StartZoom,				idPlayer::Event_StartZoom )
	EVENT( EV_Player_EndZoom,				idPlayer::Event_EndZoom )
	EVENT( EV_Player_ResetZoom,				idPlayer::Event_ResetZoom )
	EVENT( EV_Player_GetFov,				idPlayer::Event_GetFov )

	// Events needed for the Objectives GUI (is a blocking GUI - pauses the game)
	EVENT( EV_Player_PauseGame,				idPlayer::Event_Pausegame )
	EVENT( EV_Player_UnpauseGame,			idPlayer::Event_Unpausegame )
	EVENT( EV_Player_StartGamePlayTimer,	idPlayer::Event_StartGamePlayTimer )

	EVENT( EV_Mission_Success,				idPlayer::Event_MissionSuccess)
	EVENT( EV_TriggerMissionEnd,			idPlayer::Event_TriggerMissionEnd )

	EVENT( EV_GetLocation,					idPlayer::Event_GetLocation )

	EVENT( EV_ChangeWeaponProjectile,		idPlayer::Event_ChangeWeaponProjectile )
	EVENT( EV_ResetWeaponProjectile,		idPlayer::Event_ResetWeaponProjectile )
	EVENT( EV_ChangeWeaponName,				idPlayer::Event_ChangeWeaponName )
	EVENT( EV_GetCurWeaponName,				idPlayer::Event_GetCurWeaponName )
	EVENT( EV_SetActiveInventoryMapEnt,		idPlayer::Event_SetActiveInventoryMapEnt )
	EVENT( EV_ClearActiveInventoryMap,		idPlayer::Event_ClearActiveInventoryMap )

	EVENT( EV_CheckAAS,						idPlayer::Event_CheckAAS )

END_CLASS

const int MAX_RESPAWN_TIME = 10000;
const int RAGDOLL_DEATH_TIME = 3000;
const int STEPUP_TIME = 200;

idVec3 idPlayer::colorBarTable[ 5 ] = {
	idVec3( 0.25f, 0.25f, 0.25f ),
	idVec3( 1.00f, 0.00f, 0.00f ),
	idVec3( 0.00f, 0.80f, 0.10f ),
	idVec3( 0.20f, 0.50f, 0.80f ),
	idVec3( 1.00f, 0.80f, 0.10f )
};

/*
==============
idPlayer::idPlayer
==============
*/
idPlayer::idPlayer() :
	m_ButtonStateTracker(this)
{
	memset( &usercmd, 0, sizeof( usercmd ) );

	noclip					= false;
	godmode					= false;

	spawnAnglesSet			= false;
	spawnAngles				= ang_zero;
	viewAngles				= ang_zero;
	cmdAngles				= ang_zero;

	oldButtons				= 0;
	buttonMask				= 0;
	oldFlags				= 0;

	lastHitTime				= 0;
	lastSndHitTime			= 0;
	lastSavingThrowTime		= 0;

	lockpickHUD				= 0;

	hasLanded				= false;

	weapon					= NULL;

	hud						= NULL;
	inventoryHUDNeedsUpdate = true;

	objectiveSystem			= NULL;
	objectiveSystemOpen		= false;

	heartRate				= BASE_HEARTRATE;
	heartInfo.Init( 0, 0, 0, 0 );
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	lastDmgTime				= 0;
	deathClearContentsTime	= 0;
	lastArmorPulse			= -10000;
	stamina					= 0.0f;
	healthPool				= 0.0f;
	nextHealthPulse			= 0;
	healthPulse				= false;
	nextHealthTake			= 0;
	healthTake				= false;
	healthPoolTimeInterval	= HEALTHPULSE_TIME;
	healthPoolTimeIntervalFactor = 1.0f;
	healthPoolStepAmount	= 5;

	scoreBoardOpen			= false;
	forceScoreBoard			= false;
	forceRespawn			= false;
	spectating				= false;
	spectator				= 0;
	colorBar				= vec3_zero;
	colorBarIndex			= 0;
	forcedReady				= false;
	wantSpectate			= false;

	lastHitToggle			= false;

	minRespawnTime			= 0;
	maxRespawnTime			= 0;

	firstPersonViewOrigin	= vec3_zero;
	firstPersonViewAxis		= mat3_identity;

	hipJoint				= INVALID_JOINT;
	chestJoint				= INVALID_JOINT;
	headJoint				= INVALID_JOINT;

	bobFoot					= 0;
	bobFrac					= 0.0f;
	bobfracsin				= 0.0f;
	bobCycle				= 0;
	xyspeed					= 0.0f;
	stepUpTime				= 0;
	stepUpDelta				= 0.0f;
	idealLegsYaw			= 0.0f;
	legsYaw					= 0.0f;
	legsForward				= true;
	oldViewYaw				= 0.0f;
	viewBobAngles			= ang_zero;
	viewBob					= vec3_zero;
	landChange				= 0;
	landTime				= 0;
	lastFootstepPlaytime	= -1;

	currentWeapon			= -1;
	idealWeapon				= -1;
	previousWeapon			= -1;
	weaponSwitchTime		=  0;
	weaponEnabled			= true;
	weapon_soulcube			= -1;
	weapon_pda				= -1;
	weapon_fists			= -1;
	showWeaponViewModel		= true;

	skin					= NULL;
	powerUpSkin				= NULL;
	baseSkinName			= "";

	numProjectilesFired		= 0;
	numProjectileHits		= 0;

	airless					= false;
	airTics					= 0;
	lastAirDamage			= 0;
	underWaterEffectsActive	= false;
	underWaterGUIHandle		= -1;

	gibDeath				= false;
	gibsLaunched			= false;
	gibsDir					= vec3_zero;

	zoomFov.Init( 0, 0, 0, 0 );
	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	privateCameraView		= NULL;

	m_ListenerLoc			= vec3_zero;
	m_DoorListenLoc			= vec3_zero;

	// m_immobilization.Clear();
	m_immobilizationCache	= 0;

	// m_hinderance.Clear();
	m_hinderanceCache	= 1.0f;
	m_TurnHinderanceCache = 1.0f;
	m_JumpHinderanceCache = 1.0f;

	memset( loggedViewAngles, 0, sizeof( loggedViewAngles ) );
	memset( loggedAccel, 0, sizeof( loggedAccel ) );
	currentLoggedAccel	= 0;

#if 0
	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusCharacter			= NULL;
	talkCursor				= 0;
	focusVehicle			= NULL;
#endif
	cursor					= NULL;
	
	oldMouseX				= 0;
	oldMouseY				= 0;

	pdaAudio				= "";
	pdaVideo				= "";
	pdaVideoWave			= "";

	lastDamageDef			= 0;
	lastDamageDir			= vec3_zero;
	lastDamageLocation		= 0;
	m_bDamagedThisFrame		= false;
	smoothedFrame			= 0;
	smoothedOriginUpdated	= false;
	smoothedOrigin			= vec3_zero;
	smoothedAngles			= ang_zero;

	fl.networkSync			= true;

	latchedTeam				= -1;
	doingDeathSkin			= false;
	weaponGone				= false;
	useInitialSpawns		= false;
	tourneyRank				= 0;
	lastSpectateTeleport	= 0;
	tourneyLine				= 0;
	hiddenWeapon			= false;
	tipUp					= false;
	objectiveUp				= false;
	teleportEntity			= NULL;
	teleportKiller			= -1;
	respawning				= false;
	ready					= false;
	leader					= false;
	lastSpectateChange		= 0;
	lastTeleFX				= -9999;
	weaponCatchup			= false;
	lastSnapshotSequence	= 0;

	MPAim					= -1;
	lastMPAim				= -1;
	lastMPAimTime			= 0;
	MPAimFadeTime			= 0;
	MPAimHighlight			= false;

	spawnedTime				= 0;
	lastManOver				= false;
	lastManPlayAgain		= false;
	lastManPresent			= false;

	isTelefragged			= false;

	isLagged				= false;
	isChatting				= false;
	selfSmooth				= false;

	m_bGrabberActive		= false;
	m_bDraggingBody			= false;
	m_bShoulderingBody		= false;

	m_IdealCrouchState		= false;
	m_CrouchIntent			= false;

	m_LeanButtonTimeStamp	= 0;
	m_InventoryOverlay		= -1;
	m_WeaponCursor			= CInventoryCursorPtr();
	m_MapCursor				= CInventoryCursorPtr();

	m_LightgemModifier		= 0;
}

/*
==============
idPlayer::LinkScriptVariables

set up conditions for animation
==============
*/
void idPlayer::LinkScriptVariables()
{
	// Call the base class first
	idActor::LinkScriptVariables();

	AI_FORWARD.LinkTo(			scriptObject, "AI_FORWARD" );
	AI_BACKWARD.LinkTo(			scriptObject, "AI_BACKWARD" );
	AI_STRAFE_LEFT.LinkTo(		scriptObject, "AI_STRAFE_LEFT" );
	AI_STRAFE_RIGHT.LinkTo(		scriptObject, "AI_STRAFE_RIGHT" );
	AI_ATTACK_HELD.LinkTo(		scriptObject, "AI_ATTACK_HELD" );
	AI_BLOCK_HELD.LinkTo(		scriptObject, "AI_BLOCK_HELD" );
	AI_WEAPON_FIRED.LinkTo(		scriptObject, "AI_WEAPON_FIRED" );
	AI_WEAPON_BLOCKED.LinkTo(	scriptObject, "AI_WEAPON_BLOCKED" );
	AI_JUMP.LinkTo(				scriptObject, "AI_JUMP" );
	AI_CROUCH.LinkTo(			scriptObject, "AI_CROUCH" );
	AI_ONGROUND.LinkTo(			scriptObject, "AI_ONGROUND" );
	AI_ONLADDER.LinkTo(			scriptObject, "AI_ONLADDER" );
	AI_HARDLANDING.LinkTo(		scriptObject, "AI_HARDLANDING" );
	AI_SOFTLANDING.LinkTo(		scriptObject, "AI_SOFTLANDING" );
	AI_RUN.LinkTo(				scriptObject, "AI_RUN" );
	AI_PAIN.LinkTo(				scriptObject, "AI_PAIN" );
	AI_RELOAD.LinkTo(			scriptObject, "AI_RELOAD" );
	AI_TELEPORT.LinkTo(			scriptObject, "AI_TELEPORT" );
	AI_TURN_LEFT.LinkTo(		scriptObject, "AI_TURN_LEFT" );
	AI_TURN_RIGHT.LinkTo(		scriptObject, "AI_TURN_RIGHT" );
	AI_LEAN_LEFT.LinkTo(		scriptObject, "AI_LEAN_LEFT" );
	AI_LEAN_RIGHT.LinkTo(		scriptObject, "AI_LEAN_RIGHT" );
	AI_LEAN_FORWARD.LinkTo(		scriptObject, "AI_LEAN_FORWARD" );
	AI_CREEP.LinkTo(			scriptObject, "AI_CREEP" );
}

/*
==============
idPlayer::SetupWeaponEntity
==============
*/
void idPlayer::SetupWeaponEntity()
{
	if ( weapon.GetEntity() ) {
		// get rid of old weapon
		weapon.GetEntity()->Clear();
		currentWeapon = -1;
	}
	else if ( !gameLocal.isClient ) {
		weapon = static_cast<idWeapon *>( gameLocal.SpawnEntityType( idWeapon::Type, NULL ) );
		weapon.GetEntity()->SetOwner( this );
		currentWeapon = -1;
	}

	for (const idKeyValue* kv = spawnArgs.MatchPrefix("def_weapon"); kv != NULL; kv = spawnArgs.MatchPrefix("def_weapon", kv))
	{
		if (kv->GetValue().IsEmpty()) continue; // skip empty spawnargs

		idWeapon::CacheWeapon(kv->GetValue());
	}
}

/*
==============
idPlayer::Init
==============
*/
void idPlayer::Init( void ) {
	const char			*value;
	const idKeyValue	*kv;

	noclip					= false;
	godmode					= false;

	oldButtons				= 0;
	oldFlags				= 0;

	currentWeapon			= -1;
	idealWeapon				= -1;
	previousWeapon			= -1;
	weaponSwitchTime		= 0;
	weaponEnabled			= true;
	weapon_soulcube			= -1;//SlotForWeapon( "weapon_soulcube" );
	weapon_pda				= -1;//SlotForWeapon( "weapon_pda" );
	weapon_fists			= 0;//SlotForWeapon( "weapon_fists" );
	showWeaponViewModel		= GetUserInfo()->GetBool( "ui_showGun" );

	lastDmgTime				= 0;
	lastArmorPulse			= -10000;
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	heartInfo.Init( 0, 0, 0, 0 );

	bobCycle				= 0;
	bobFrac					= 0.0f;
	landChange				= 0;
	landTime				= 0;
	lastFootstepPlaytime	= -1;
	isPushing				= false;
	zoomFov.Init( 0, 0, 0, 0 );

	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	currentLoggedAccel		= 0;

#if 0
	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusCharacter			= NULL;
	talkCursor				= 0;
	focusVehicle			= NULL;
#endif

	// remove any damage effects
	playerView.ClearEffects();

	// damage values
	fl.takedamage			= true;
	ClearPain();

	// restore persistent data
	RestorePersistantInfo();

	bobCycle		= 0;
	stamina			= 0.0f;
	healthPool		= 0.0f;
	nextHealthPulse = 0;
	healthPulse		= false;
	nextHealthTake	= 0;
	healthTake		= false;

	SetupWeaponEntity();
	currentWeapon = -1;
	previousWeapon = -1;

	heartRate = BASE_HEARTRATE;
	AdjustHeartRate( BASE_HEARTRATE, 0.0f, 0.0f, true );

	idealLegsYaw = 0.0f;
	legsYaw = 0.0f;
	legsForward	= true;
	oldViewYaw = 0.0f;

	// set the pm_ cvars
	if ( !gameLocal.isMultiplayer || gameLocal.isServer ) {
		kv = spawnArgs.MatchPrefix( "pm_", NULL );
		while( kv ) {
			cvarSystem->SetCVarString( kv->GetKey(), kv->GetValue() );
			kv = spawnArgs.MatchPrefix( "pm_", kv );
		}
	}

	// Commented out by Dram. TDM does not use stamina
/*
	// disable stamina on hell levels
	if ( gameLocal.world && gameLocal.world->spawnArgs.GetBool( "no_stamina" ) ) {
		pm_stamina.SetFloat( 0.0f );
	}

	// stamina always initialized to maximum
	stamina = pm_stamina.GetFloat();
*/
	stamina = 0.0f; // Set stamina to 0 - Dram

	// air always initialized to maximum too
	airTics = pm_airTics.GetInteger();
	airless = false;

	gibDeath = false;
	gibsLaunched = false;
	gibsDir.Zero();

	// set the gravity
	physicsObj.SetGravity( gameLocal.GetGravity() );

	// start out standing
	SetEyeHeight( pm_normalviewheight.GetFloat() );

	stepUpTime = 0;
	stepUpDelta = 0.0f;
	viewBobAngles.Zero();
	viewBob.Zero();

	value = spawnArgs.GetString( "model" );
	if ( value && ( *value != 0 ) ) {
		SetModel( value );
	}

	if ( cursor ) {
		cursor->SetStateInt( "talkcursor", 0 );
		cursor->SetStateString( "combatcursor", "1" );
		cursor->SetStateString( "itemcursor", "0" );
		cursor->SetStateString( "guicursor", "0" );
	}

	if ( ( gameLocal.isMultiplayer || g_testDeath.GetBool() ) && skin ) {
		SetSkin( skin );
		renderEntity.shaderParms[6] = 0.0f;
	} else if ( spawnArgs.GetString( "spawn_skin", NULL, &value ) ) {
		skin = declManager->FindSkin( value );
		SetSkin( skin );
		renderEntity.shaderParms[6] = 0.0f;
	}

	value = spawnArgs.GetString( "bone_hips", "" );
	hipJoint = animator.GetJointHandle( value );
	if ( hipJoint == INVALID_JOINT ) {
		gameLocal.Error( "Joint '%s' not found for 'bone_hips' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "bone_chest", "" );
	chestJoint = animator.GetJointHandle( value );
	if ( chestJoint == INVALID_JOINT ) {
		gameLocal.Error( "Joint '%s' not found for 'bone_chest' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "bone_head", "" );
	headJoint = animator.GetJointHandle( value );
	if ( headJoint == INVALID_JOINT ) {
		gameLocal.Error( "Joint '%s' not found for 'bone_head' on '%s'", value, name.c_str() );
	}

	// initialize the script variables
	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_ATTACK_HELD	= false;
	AI_BLOCK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_WEAPON_BLOCKED = false;
	AI_JUMP			= false;
	AI_DEAD			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;

	AI_LEAN_LEFT	= false;
	AI_LEAN_RIGHT	= false;
	AI_LEAN_FORWARD	= false;

	AI_CREEP		= false;



	// reset the script object
	ConstructScriptObject();

	// execute the script so the script object's constructor takes effect immediately
	scriptThread->Execute();
	
	forceScoreBoard		= false;
	forcedReady			= false;

	privateCameraView	= NULL;

	lastSpectateChange	= 0;
	lastTeleFX			= -9999;

	hiddenWeapon		= false;
	tipUp				= false;
	objectiveUp			= false;
	teleportEntity		= NULL;
	teleportKiller		= -1;
	leader				= false;

	SetPrivateCameraView( NULL );

	lastSnapshotSequence	= 0;

	MPAim				= -1;
	lastMPAim			= -1;
	lastMPAimTime		= 0;
	MPAimFadeTime		= 0;
	MPAimHighlight		= false;

	if ( hud ) {
		hud->HandleNamedEvent( "aim_clear" );
	}

	cvarSystem->SetCVarBool( "ui_chat", false );
}

/*
==============
idPlayer::Spawn

Prepare any resources used by the player.
==============
*/
void idPlayer::Spawn( void ) 
{
	int			i;
	idStr		temp;
	idBounds	bounds;

	if ( entityNumber >= MAX_CLIENTS ) {
		gameLocal.Error( "entityNum > MAX_CLIENTS for player.  Player may only be spawned with a client." );
	}

	// allow thinking during cinematics
	cinematic = true;

	if ( gameLocal.isMultiplayer ) {
		// always start in spectating state waiting to be spawned in
		// do this before SetClipModel to get the right bounding box
		spectating = true;
	}

	maxHealth = spawnArgs.GetInt("maxhealth", "100");

	m_immobilization.Clear();
	m_immobilizationCache = 0;

	m_hinderance.Clear();
	m_hinderanceCache = 1.0f;

	m_TurnHinderance.Clear();
	m_TurnHinderanceCache = 1.0f;

	// set our collision model
	physicsObj.SetSelf( this );
	SetClipModel();
	physicsObj.SetMass( spawnArgs.GetFloat( "mass", "100" ) );
	physicsObj.SetContents( CONTENTS_BODY );
	// SR CONTENTS_RESPONSE FIX
	if( m_StimResponseColl->HasResponse() )
		physicsObj.SetContents( physicsObj.GetContents() | CONTENTS_RESPONSE );
	
	physicsObj.SetClipMask( MASK_PLAYERSOLID );
	SetPhysics( &physicsObj );
	InitAASLocation();

	skin = renderEntity.customSkin;

	// only the local player needs guis
	if ( !gameLocal.isMultiplayer || entityNumber == gameLocal.localClientNum ) {

		// load HUD
		if ( gameLocal.isMultiplayer ) {
			// I need the HUD uniqued for the inventory code to interact with it.
			//hud = uiManager->FindGui( "guis/mphud.gui", true, false, true );
			hud = uiManager->FindGui( "guis/mphud.gui", true, true );
		} else if ( spawnArgs.GetString( "hud", "", temp ) ) {
			// I need the HUD uniqued for the inventory code to interact with it.
			//hud = uiManager->FindGui( temp, true, false, true );
			hud = uiManager->FindGui( temp, true, true );
		}
		if ( hud ) {
			hud->Activate( true, gameLocal.time );
		}

		// load cursor
		if ( spawnArgs.GetString( "cursor", "", temp ) ) {
			cursor = uiManager->FindGui( temp, true, gameLocal.isMultiplayer, gameLocal.isMultiplayer );
		}
		if ( cursor ) {
			cursor->Activate( true, gameLocal.time );
		}

		objectiveSystem = uiManager->FindGui( "guis/pda.gui", true, false, true );
		objectiveSystemOpen = false;
	}

	// Remove entity GUIs from our overlay system.
	for ( i = 0; i < MAX_RENDERENTITY_GUI; i++ )
		m_overlays.destroyOverlay( OVERLAYS_MIN_HANDLE + i );
	// Add the HUD.
	if ( m_overlays.createOverlay( 0, LAYER_MAIN_HUD ) >= OVERLAYS_MIN_HANDLE )
		m_overlays.setGui( OVERLAYS_MIN_HANDLE , hud );
	else
		gameLocal.Warning( "Unable to create overlay for HUD.\n" );

	SetLastHitTime( 0 );

	// load the armor sound feedback
	declManager->FindSound( "player_sounds_hitArmor" );

	// set up conditions for animation
	LinkScriptVariables();

	animator.RemoveOriginOffset( true );

	// initialize user info related settings
	// on server, we wait for the userinfo broadcast, as this controls when the player is initially spawned in game
	if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
		UserInfoChanged(false);
	}

	// create combat collision hull for exact collision detection
	SetCombatModel();

	// init the damage effects
	playerView.SetPlayerEntity( this );

	// suppress model in non-player views, but allow it in mirrors and remote views
	renderEntity.suppressSurfaceInViewID = entityNumber+1;

	// don't project shadow on self or weapon
	renderEntity.noSelfShadow = true;

	idAFAttachment *headEnt = head.GetEntity();
	if ( headEnt ) {
		headEnt->GetRenderEntity()->suppressSurfaceInViewID = entityNumber+1;
		headEnt->GetRenderEntity()->noSelfShadow = true;
	}

	if ( gameLocal.isMultiplayer )
	{
		Init();
		Hide();	// properly hidden if starting as a spectator
		if ( !gameLocal.isClient )
		{
			// set yourself ready to spawn. idMultiplayerGame will decide when/if appropriate and call SpawnFromSpawnSpot
			SetupWeaponEntity();
			SpawnFromSpawnSpot();
			forceRespawn = true;
			assert( spectating );
		}
	}
	else
	{
		SetupWeaponEntity();
		SpawnFromSpawnSpot();
	}

	// trigger playtesting item gives, if we didn't get here from a previous level
	// the devmap key will be set on the first devmap, but cleared on any level
	// transitions
	if ( !gameLocal.isMultiplayer && gameLocal.serverInfo.FindKey( "devmap" ) ) {
		// fire a trigger with the name "devmap"
		idEntity *ent = gameLocal.FindEntity( "devmap" );
		if ( ent ) {
			ent->ActivateTargets( this );
		}
	}

	if ( gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
	{
		hiddenWeapon = true;
		if ( weapon.GetEntity() )
			weapon.GetEntity()->LowerWeapon();
		idealWeapon = 0;
	}
	else
		hiddenWeapon = false;
	
	if ( hud )
	{
		UpdateHudWeapon();
		hud->StateChanged( gameLocal.time );
		// greebo: Initialise the Message code on the HUD
		hud->HandleNamedEvent("SetupMessageSystem");
	}

	tipUp = false;
	objectiveUp = false;

	// See if any levelTriggers are pending. Trigger them now, if this is the case
	if ( levelTriggers.Num() )
	{
		PostEventMS( &EV_Player_LevelTrigger, 0 );
	}

	// copy step volumes over from cvars
	UpdateMoveVolumes();

	if ( !gameLocal.isMultiplayer )
	{
		if ( g_skill.GetInteger() < 2 )
		{
			if ( health < 25 )
			{
				health = 25;
			}
			if ( g_useDynamicProtection.GetBool() )
			{
				g_damageScale.SetFloat( 1.0f );
			}
		}
		else
		{
			g_damageScale.SetFloat( 1.0f );
			g_armorProtection.SetFloat( ( g_skill.GetInteger() < 2 ) ? 0.4f : 0.2f );
#ifndef ID_DEMO_BUILD
			if ( g_skill.GetInteger() == 3 )
			{
				healthTake = true;
				nextHealthTake = gameLocal.time + g_healthTakeTime.GetInteger() * 1000;
			}
#endif
		}
	}

	lockpickHUD = CreateOverlay("guis/tdm_lockpick.gui", 20);

	// Clear the lightgem modifiers
	m_LightgemModifierList.clear();

	//FIX: Set the walkspeed back to the stored value.
	pm_walkspeed.SetFloat( gameLocal.m_walkSpeed );
	SetupInventory();

	// greebo: Set the player variable on the grabber
	gameLocal.m_Grabber->SetPlayer(this);

	// greebo: Initialise the default fov.
	zoomFov.Init(gameLocal.time, 0, g_fov.GetFloat(), g_fov.GetFloat());

	// Post an event to read the LG modifier from the worldspawn entity
	PostEventMS(&EV_ReadLightgemModifierFromWorldspawn, 0);

	// Start the gameplay timer half a second after spawn
	PostEventMS(&EV_Player_StartGamePlayTimer, 500);

	// Check the AAS status
	PostEventMS(&EV_CheckAAS, 0);
}

CInventoryWeaponItemPtr idPlayer::GetCurrentWeaponItem()
{
	if (m_WeaponCursor == NULL)
	{
		return CInventoryWeaponItemPtr();
	}

	return boost::dynamic_pointer_cast<CInventoryWeaponItem>(m_WeaponCursor->GetCurrentItem());
}

CInventoryWeaponItemPtr idPlayer::GetWeaponItem(const idStr& weaponName)
{
	if (m_WeaponCursor == NULL)
	{
		return CInventoryWeaponItemPtr();
	}

	CInventoryCategoryPtr weaponCategory = m_WeaponCursor->GetCurrentCategory();

	if (weaponCategory == NULL)
	{
		return CInventoryWeaponItemPtr();
	}

	// Cycle through all available weapons and find the one with the given name
	for (int i = 0; i < weaponCategory->GetNumItems(); ++i)
	{
		CInventoryWeaponItemPtr weapon = boost::dynamic_pointer_cast<CInventoryWeaponItem>(weaponCategory->GetItem(i));

		if (weapon != NULL && weaponName == weapon->GetWeaponName())
		{
			return weapon; // Found!
		}
	}

	return CInventoryWeaponItemPtr();
}

void idPlayer::AddWeaponsToInventory()
{
	for (const idKeyValue* kv = spawnArgs.MatchPrefix("def_weapon"); kv != NULL; kv = spawnArgs.MatchPrefix("def_weapon", kv))
	{
		if (kv->GetValue().IsEmpty()) continue; // skip empty spawnargs

		// Get the weapon index from the key by removing the prefix
		idStr weaponNumStr = kv->GetKey();
		weaponNumStr.StripLeading("def_weapon");

		if (weaponNumStr.IsEmpty() || !weaponNumStr.IsNumeric()) continue; // not a numeric weapon

		int weaponNum = atoi(weaponNumStr);

		DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Trying to add weapon as defined by key %s to inventory (index %d)\r", kv->GetKey().c_str(), weaponNum);

		idStr weaponDef(kv->GetValue());

		if (gameLocal.FindEntityDefDict(weaponDef) == NULL)
		{
			DM_LOG(LC_INVENTORY, LT_ERROR)LOGSTRING("Weapon entityDef not found: %s\r", weaponDef.c_str());
			continue;
		}

		DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Adding weapon to inventory: %s\r", weaponDef.c_str());

		// Allocate a new weapon item using the found entityDef
		CInventoryWeaponItemPtr item(new CInventoryWeaponItem(weaponDef, this));
			
		item->SetWeaponIndex(weaponNum);

		// Add it to the weapon category
		m_WeaponCursor->GetCurrentCategory()->PutItem(item);
	}

	DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Total number of weapons found: %d\r", m_WeaponCursor->GetCurrentCategory()->GetNumItems());
}

void idPlayer::NextInventoryMap()
{
	if (GetImmobilization() & EIM_ITEM_SELECT) return;

	if (m_MapCursor == NULL)
	{
		return; // We have no cursor!
	}

	CInventoryItemPtr prevMapItem = m_MapCursor->GetCurrentItem();

	if (prevMapItem != NULL)
	{
		// We already have a map selected, toggle it off
		UseInventoryItem(EPressed, prevMapItem, 0, false); 
	}

	// Clear any previously active maps
	ClearActiveInventoryMap();

	if (m_MapCursor->IsLastItemInCategory())
	{
		// Reached last map, return without cycling to the next map
		// Clear the item, so that we start afresh next time
		m_MapCursor->ClearItem();
		return;
	}

	// Advance the cursor to the next item
	CInventoryItemPtr nextMapItem = m_MapCursor->GetNextItem();

	if (nextMapItem != NULL && nextMapItem != prevMapItem)
	{
		// Use this new item
		UseInventoryItem(EPressed, nextMapItem, 0, false);
	}
}

void idPlayer::SetupInventory()
{
	m_InventoryOverlay = CreateOverlay(cv_tdm_inv_gui_file.GetString(), LAYER_INVENTORY);
	
	idUserInterface* invGUI = m_overlays.getGui(m_InventoryOverlay);
	
	if (invGUI == NULL)
	{
		gameLocal.Error("Could not set up inventory GUI: %s", cv_tdm_inv_gui_file.GetString());
		return;
	}

	// Initialise the pickup message system
	invGUI->HandleNamedEvent("SetupInventoryPickUpMessageSystem");

	const CInventoryPtr& inv = Inventory();
	int idx = 0;

	// We create a cursor and a category for the weapons, which is then locked
	// to this category, so we can only cycle within that one group.
	m_WeaponCursor = inv->CreateCursor();
	inv->CreateCategory(TDM_PLAYER_WEAPON_CATEGORY, &idx);
	m_WeaponCursor->SetCurrentCategory(idx);
	m_WeaponCursor->SetCategoryLock(true);

	// greebo: Parse the spawnargs and add the weapon items to the inventory.
	AddWeaponsToInventory();

	// Disable all non-ammo weapons except unarmed, the items will be given by the shop
	CInventoryCategoryPtr category = m_WeaponCursor->GetCurrentCategory();
	for (int i = 0; i < category->GetNumItems(); i++)
	{
		CInventoryWeaponItemPtr item = 
			boost::dynamic_pointer_cast<CInventoryWeaponItem>(category->GetItem(i));

		if (item->GetWeaponIndex() != 0 && item->IsAllowedEmpty())
		{
			// Doesn't need ammo, disable this weapon for now
			item->SetEnabled(false);
		}
	}

	// greebo: Create the cursor for map/floorplan inventory items.
	m_MapCursor = inv->CreateCursor();
	inv->CreateCategory(TDM_PLAYER_MAPS_CATEGORY, &idx);
	m_MapCursor->SetCurrentCategory(idx);
	m_MapCursor->SetCategoryLock(true);
	m_MapCursor->SetWrapAround(true);
	m_MapCursor->ClearItem(); // invalidate the cursor

	// give the player weapon ammo based on shop purchases
	category = m_WeaponCursor->GetCurrentCategory();
	ShopItemList startingItems = gameLocal.m_Shop->GetPlayerItems();

	for (int si = 0; si < startingItems.Num(); si++)
	{
		const CShopItemPtr& shopItem = startingItems[si];
		idStr weaponName = shopItem->GetID();

		// greebo: Append "atdm:" if it's missing, all weapon entityDefs are atdm:* now
		if (idStr::Cmpn(weaponName, "atdm:", 5) != 0)
		{
			weaponName = "atdm:" + weaponName;
		}

		if (idStr::Cmpn(weaponName, "atdm:weapon_", 12) == 0)
		{
			weaponName.Strip("atdm:weapon_");

			for (int i = 0; i < category->GetNumItems(); i++)
			{
				CInventoryWeaponItemPtr item = 
					boost::dynamic_pointer_cast<CInventoryWeaponItem>(category->GetItem(i));

				if (item->GetWeaponName() == weaponName)
				{
					// Enable non-ammo weapons, if the count is > 0
					if (item->IsAllowedEmpty())
					{
						item->SetEnabled(shopItem->GetCount() > 0);
					}

					// Set the persistent flag and the ammo
					item->SetPersistent(shopItem->GetPersistent());
					item->SetAmmo(shopItem->GetCount());
					break;
				}
			}
		}
	}

	// Now create the standard cursor for all the other inventory items (excl. weapons)
	CInventoryCursorPtr crsr = InventoryCursor();

	// We set the filter to ignore the weapon category, since this will be
	// handled by the weapon cursor. We don't want the weapons to show up
	// in the weapon slot AND in the inventory at the same time.
	crsr->AddCategoryIgnored(TDM_PLAYER_WEAPON_CATEGORY);

	// The player always gets a dumyyentry (so the player can have an empty space if he 
	// chooses to not see the inventory all the time.
	CInventoryItemPtr it(new CInventoryItem(this));
	it->SetName(TDM_DUMMY_ITEM);
	it->SetType(CInventoryItem::IT_DUMMY);
	it->SetCount(0);
	it->SetStackable(false);
	crsr->Inventory()->PutItem(it, TDM_INVENTORY_DEFAULT_GROUP);

	// Focus on the empty dummy inventory item
	crsr->SetCurrentItem(TDM_DUMMY_ITEM);

	// greebo: Set up the loot inventory item
	const idDeclEntityDef* lootItemDef = static_cast<const idDeclEntityDef*>(
		declManager->FindType(DECL_ENTITYDEF, cv_tdm_inv_loot_item_def.GetString())
	);

	if (lootItemDef == NULL)
	{
		gameLocal.Error("Could not find loot inventory entityDef.\n");
	}
	
	idEntity* lootItemEnt;
	gameLocal.SpawnEntityDef(lootItemDef->dict, &lootItemEnt);

	assert(lootItemEnt != NULL); // must succeed

	CInventoryItemPtr lootItem = Inventory()->PutItem(lootItemEnt, this);
	assert(lootItem != NULL); // must succeed as well

	// Flag this item as loot info item
	lootItem->SetType(CInventoryItem::IT_LOOT_INFO);
	
	// Give player non-weapon items obtained from the Shop
	for (int si = 0; si < startingItems.Num(); si++)
	{
		const CShopItemPtr& item = startingItems[si];

		idStr itemName = item->GetID();
		int count = item->GetCount();

		// greebo: Append "atdm:" if it's missing, all weapon entityDefs are atdm:* now
		if (idStr::Cmpn(itemName, "atdm:", 5) != 0)
		{
			itemName = "atdm:" + itemName;
		}

		if (idStr::Cmpn(itemName, "atdm:weapon_", 12) != 0 && count > 0)
		{
			const idStringList& classNames = item->GetClassnames();

			for (int j = 0; j < classNames.Num(); ++j)
			{
				// Spawn this entitydef
				const idDict* itemDict = gameLocal.FindEntityDefDict(classNames[j], true);

				idEntity* entity = NULL;
				gameLocal.SpawnEntityDef( *itemDict, &entity );

				// add it to the inventory
				CInventoryItemPtr invItem = crsr->Inventory()->PutItem(entity, this);
				invItem->SetCount(count);
				invItem->SetPersistent(item->GetPersistent());
			}
		}
	}
}


/*
==============
idPlayer::~idPlayer()

Release any resources used by the player.
==============
*/
idPlayer::~idPlayer()
{
	delete weapon.GetEntity();
	weapon = NULL;
}

/*
===========
idPlayer::Save
===========
*/
void idPlayer::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteUsercmd( usercmd );
	playerView.Save( savefile );

	savefile->WriteBool( noclip );
	savefile->WriteBool( godmode );

	// don't save spawnAnglesSet, since we'll have to reset them after loading the savegame
	savefile->WriteAngles( spawnAngles );
	savefile->WriteAngles( viewAngles );
	savefile->WriteAngles( cmdAngles );

	// Mouse gesture
	m_MouseGesture.Save( savefile );

	savefile->WriteInt( buttonMask );
	savefile->WriteInt( oldButtons );
	savefile->WriteInt( oldFlags );

	savefile->WriteInt( lastHitTime );
	savefile->WriteInt( lastSndHitTime );
	savefile->WriteInt( lastSavingThrowTime );

	savefile->WriteInt( lockpickHUD );
	savefile->WriteBool( hasLanded );

	// idBoolFields don't need to be saved, just re-linked in Restore

	savefile->WriteInt( levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		savefile->WriteString( levelTriggers[i].levelName );
		savefile->WriteString( levelTriggers[i].triggerName );
	}

	weapon.Save( savefile );

	savefile->WriteUserInterface( hud, false );
	savefile->WriteBool(inventoryHUDNeedsUpdate);
	savefile->WriteUserInterface( objectiveSystem, false );
	savefile->WriteBool( objectiveSystemOpen );

	savefile->WriteInt(hudMessages.Num());
	for (int i = 0; i < hudMessages.Num(); i++) 
	{
		savefile->WriteString(hudMessages[i]);
	}

	savefile->WriteInt(inventoryPickedUpMessages.Num());
	for (int i = 0; i < inventoryPickedUpMessages.Num(); i++) 
	{
		savefile->WriteString(inventoryPickedUpMessages[i]);
	}

	savefile->WriteInt( weapon_soulcube );
	savefile->WriteInt( weapon_pda );
	savefile->WriteInt( weapon_fists );

	savefile->WriteInt( heartRate );
	savefile->WriteBool(m_HeartBeatAllow);

	savefile->WriteFloat( heartInfo.GetStartTime() );
	savefile->WriteFloat( heartInfo.GetDuration() );
	savefile->WriteFloat( heartInfo.GetStartValue() );
	savefile->WriteFloat( heartInfo.GetEndValue() );

	savefile->WriteInt( lastHeartAdjust );
	savefile->WriteInt( lastHeartBeat );
	savefile->WriteInt( lastDmgTime );
	savefile->WriteInt( deathClearContentsTime );
	savefile->WriteBool( doingDeathSkin );
	savefile->WriteInt( lastArmorPulse );
	savefile->WriteFloat( stamina );
	savefile->WriteFloat( healthPool );
	savefile->WriteInt( nextHealthPulse );
	savefile->WriteBool( healthPulse );
	savefile->WriteInt( nextHealthTake );
	savefile->WriteBool( healthTake );

	savefile->WriteInt(healthPoolStepAmount);
	savefile->WriteInt(healthPoolTimeInterval);
	savefile->WriteFloat(healthPoolTimeIntervalFactor);

	savefile->WriteBool( hiddenWeapon );
	soulCubeProjectile.Save( savefile );

	savefile->WriteInt( spectator );
	savefile->WriteVec3( colorBar );
	savefile->WriteInt( colorBarIndex );
	savefile->WriteBool( scoreBoardOpen );
	savefile->WriteBool( forceScoreBoard );
	savefile->WriteBool( forceRespawn );
	savefile->WriteBool( spectating );
	savefile->WriteInt( lastSpectateTeleport );
	savefile->WriteBool( lastHitToggle );
	savefile->WriteBool( forcedReady );
	savefile->WriteBool( wantSpectate );
	savefile->WriteBool( weaponGone );
	savefile->WriteBool( useInitialSpawns );
	savefile->WriteInt( latchedTeam );
	savefile->WriteInt( tourneyRank );
	savefile->WriteInt( tourneyLine );

	teleportEntity.Save( savefile );
	savefile->WriteInt( teleportKiller );

	savefile->WriteInt( minRespawnTime );
	savefile->WriteInt( maxRespawnTime );

	savefile->WriteVec3( firstPersonViewOrigin );
	savefile->WriteMat3( firstPersonViewAxis );

	// don't bother saving dragEntity since it's a dev tool

	savefile->WriteJoint( hipJoint );
	savefile->WriteJoint( chestJoint );
	savefile->WriteJoint( headJoint );

	savefile->WriteStaticObject( physicsObj );

	savefile->WriteInt( aasLocation.Num() );
	for( i = 0; i < aasLocation.Num(); i++ ) {
		savefile->WriteInt( aasLocation[ i ].areaNum );
		savefile->WriteVec3( aasLocation[ i ].pos );
	}

	savefile->WriteInt( bobFoot );
	savefile->WriteFloat( bobFrac );
	savefile->WriteFloat( bobfracsin );
	savefile->WriteInt( bobCycle );
	savefile->WriteFloat( xyspeed );
	savefile->WriteInt( stepUpTime );
	savefile->WriteFloat( stepUpDelta );
	savefile->WriteFloat( idealLegsYaw );
	savefile->WriteFloat( legsYaw );
	savefile->WriteBool( legsForward );
	savefile->WriteFloat( oldViewYaw );
	savefile->WriteAngles( viewBobAngles );
	savefile->WriteVec3( viewBob );
	savefile->WriteInt( landChange );
	savefile->WriteInt( landTime );
	savefile->WriteInt( lastFootstepPlaytime );
	savefile->WriteBool(isPushing);

	savefile->WriteInt( currentWeapon );
	savefile->WriteInt( idealWeapon );
	savefile->WriteInt( previousWeapon );
	savefile->WriteInt( weaponSwitchTime );
	savefile->WriteBool( weaponEnabled );
	savefile->WriteBool( showWeaponViewModel );

	savefile->WriteSkin( skin );
	savefile->WriteSkin( powerUpSkin );
	savefile->WriteString( baseSkinName );

	savefile->WriteInt( numProjectilesFired );
	savefile->WriteInt( numProjectileHits );

	savefile->WriteBool( airless );
	savefile->WriteInt( airTics );
	savefile->WriteInt( lastAirDamage );

	savefile->WriteBool(underWaterEffectsActive);
	savefile->WriteInt(underWaterGUIHandle);

	savefile->WriteBool( gibDeath );
	savefile->WriteBool( gibsLaunched );
	savefile->WriteVec3( gibsDir );

	savefile->WriteFloat( zoomFov.GetStartTime() );
	savefile->WriteFloat( zoomFov.GetDuration() );
	savefile->WriteFloat( zoomFov.GetStartValue() );
	savefile->WriteFloat( zoomFov.GetEndValue() );

	savefile->WriteFloat( centerView.GetStartTime() );
	savefile->WriteFloat( centerView.GetDuration() );
	savefile->WriteFloat( centerView.GetStartValue() );
	savefile->WriteFloat( centerView.GetEndValue() );

	savefile->WriteBool( fxFov );

	savefile->WriteFloat( influenceFov );
	savefile->WriteInt( influenceActive );
	savefile->WriteFloat( influenceRadius );
	savefile->WriteObject( influenceEntity );
	savefile->WriteMaterial( influenceMaterial );
	savefile->WriteSkin( influenceSkin );

	savefile->WriteObject( privateCameraView );
	savefile->WriteVec3( m_ListenerLoc );
	savefile->WriteVec3( m_DoorListenLoc );

	savefile->WriteDict( &m_immobilization );
	savefile->WriteInt( m_immobilizationCache );

	savefile->WriteDict( &m_hinderance );
	savefile->WriteFloat( m_hinderanceCache );
	savefile->WriteDict( &m_TurnHinderance );
	savefile->WriteFloat( m_TurnHinderanceCache );

	for( i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++ ) {
		savefile->WriteAngles( loggedViewAngles[ i ] );
	}
	for( i = 0; i < NUM_LOGGED_ACCELS; i++ ) {
		savefile->WriteInt( loggedAccel[ i ].time );
		savefile->WriteVec3( loggedAccel[ i ].dir );
	}
	savefile->WriteInt( currentLoggedAccel );

#if 0
	savefile->WriteObject( focusGUIent );
	// can't save focusUI
	savefile->WriteObject( focusCharacter );
	savefile->WriteInt( talkCursor );
	savefile->WriteInt( focusTime );
	savefile->WriteObject( focusVehicle );
#endif
	savefile->WriteUserInterface( cursor, false );

	savefile->WriteInt( oldMouseX );
	savefile->WriteInt( oldMouseY );

	savefile->WriteString( pdaAudio );
	savefile->WriteString( pdaVideo );
	savefile->WriteString( pdaVideoWave );

	savefile->WriteBool( tipUp );
	savefile->WriteBool( objectiveUp );

	savefile->WriteInt( lastDamageDef );
	savefile->WriteVec3( lastDamageDir );
	savefile->WriteInt( lastDamageLocation );
	savefile->WriteBool( m_bDamagedThisFrame );
	savefile->WriteInt( smoothedFrame );
	savefile->WriteBool( smoothedOriginUpdated );
	savefile->WriteVec3( smoothedOrigin );
	savefile->WriteAngles( smoothedAngles );

	savefile->WriteBool( ready );
	savefile->WriteBool( respawning );
	savefile->WriteBool( leader );
	savefile->WriteInt( lastSpectateChange );
	savefile->WriteInt( lastTeleFX );

	// Commented out by Dram. TDM does not use stamina
	//savefile->WriteFloat( pm_stamina.GetFloat() );

	savefile->WriteBool( m_bGrabberActive );
	savefile->WriteBool( m_bDraggingBody );
	savefile->WriteBool( m_bShoulderingBody );

	savefile->WriteBool( m_IdealCrouchState );
	savefile->WriteBool( m_CrouchIntent );

	savefile->WriteInt(m_InventoryOverlay);

	savefile->WriteBool(m_WeaponCursor != NULL);
	if (m_WeaponCursor != NULL) {
		savefile->WriteInt(m_WeaponCursor->GetId());
	}

	savefile->WriteBool(m_MapCursor != NULL);
	if (m_MapCursor != NULL) {
		savefile->WriteInt(m_MapCursor->GetId());
	}

	m_ActiveInventoryMapEnt.Save(savefile);

	savefile->WriteInt(m_LightgemModifier);

	savefile->WriteInt(static_cast<int>(m_LightgemModifierList.size()));
	for (std::map<std::string, int>::const_iterator i = m_LightgemModifierList.begin(); i != m_LightgemModifierList.end(); ++i)
	{
		savefile->WriteString(i->first.c_str());
		savefile->WriteInt(i->second);
	}

	if(hud)
	{
		hud->SetStateString( "message", common->GetLanguageDict()->GetString( "#str_02916" ) );
		hud->HandleNamedEvent( "Message" );
	}
}

/*
===========
idPlayer::Restore
===========
*/
void idPlayer::Restore( idRestoreGame *savefile ) {
	int	  i;
	int	  num;
	float set;

	savefile->ReadUsercmd( usercmd );
	playerView.Restore( savefile );

	savefile->ReadBool( noclip );
	savefile->ReadBool( godmode );

	savefile->ReadAngles( spawnAngles );
	savefile->ReadAngles( viewAngles );
	savefile->ReadAngles( cmdAngles );

	memset( usercmd.angles, 0, sizeof( usercmd.angles ) );
	SetViewAngles( viewAngles );
	spawnAnglesSet = true;

	m_MouseGesture.Restore( savefile );

	savefile->ReadInt( buttonMask );
	savefile->ReadInt( oldButtons );
	savefile->ReadInt( oldFlags );

	usercmd.flags = 0;
	oldFlags = 0;

	savefile->ReadInt( lastHitTime );
	savefile->ReadInt( lastSndHitTime );
	savefile->ReadInt( lastSavingThrowTime );

	savefile->ReadInt( lockpickHUD );
	savefile->ReadBool( hasLanded );

	// Re-link idBoolFields to the scriptObject, values will be restored in scriptObject's restore
	LinkScriptVariables();

	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idLevelTriggerInfo lti;
		savefile->ReadString( lti.levelName );
		savefile->ReadString( lti.triggerName );
		levelTriggers.Append( lti );
	}

	weapon.Restore( savefile );

	savefile->ReadUserInterface( hud );
	savefile->ReadBool(inventoryHUDNeedsUpdate);
	savefile->ReadUserInterface( objectiveSystem );
	savefile->ReadBool( objectiveSystemOpen );

	savefile->ReadInt(num);
	hudMessages.Clear();
	hudMessages.SetNum(num);
	for (i = 0; i < num; i++) 
	{
		savefile->ReadString(hudMessages[i]);
	}

	savefile->ReadInt(num);
	inventoryPickedUpMessages.Clear();
	inventoryPickedUpMessages.SetNum(num);
	for (i = 0; i < num; i++) 
	{
		savefile->ReadString(inventoryPickedUpMessages[i]);
	}

	savefile->ReadInt( weapon_soulcube );
	savefile->ReadInt( weapon_pda );
	savefile->ReadInt( weapon_fists );

	savefile->ReadInt( heartRate );
	savefile->ReadBool(m_HeartBeatAllow);

	savefile->ReadFloat( set );
	heartInfo.SetStartTime( set );
	savefile->ReadFloat( set );
	heartInfo.SetDuration( set );
	savefile->ReadFloat( set );
	heartInfo.SetStartValue( set );
	savefile->ReadFloat( set );
	heartInfo.SetEndValue( set );

	savefile->ReadInt( lastHeartAdjust );
	savefile->ReadInt( lastHeartBeat );
	savefile->ReadInt( lastDmgTime );
	savefile->ReadInt( deathClearContentsTime );
	savefile->ReadBool( doingDeathSkin );
	savefile->ReadInt( lastArmorPulse );
	savefile->ReadFloat( stamina );
	savefile->ReadFloat( healthPool );
	savefile->ReadInt( nextHealthPulse );
	savefile->ReadBool( healthPulse );
	savefile->ReadInt( nextHealthTake );
	savefile->ReadBool( healthTake );

	savefile->ReadInt(healthPoolStepAmount);
	savefile->ReadInt(healthPoolTimeInterval);
	savefile->ReadFloat(healthPoolTimeIntervalFactor);

	savefile->ReadBool( hiddenWeapon );
	soulCubeProjectile.Restore( savefile );

	savefile->ReadInt( spectator );
	savefile->ReadVec3( colorBar );
	savefile->ReadInt( colorBarIndex );
	savefile->ReadBool( scoreBoardOpen );
	savefile->ReadBool( forceScoreBoard );
	savefile->ReadBool( forceRespawn );
	savefile->ReadBool( spectating );
	savefile->ReadInt( lastSpectateTeleport );
	savefile->ReadBool( lastHitToggle );
	savefile->ReadBool( forcedReady );
	savefile->ReadBool( wantSpectate );
	savefile->ReadBool( weaponGone );
	savefile->ReadBool( useInitialSpawns );
	savefile->ReadInt( latchedTeam );
	savefile->ReadInt( tourneyRank );
	savefile->ReadInt( tourneyLine );

	teleportEntity.Restore( savefile );
	savefile->ReadInt( teleportKiller );

	savefile->ReadInt( minRespawnTime );
	savefile->ReadInt( maxRespawnTime );

	savefile->ReadVec3( firstPersonViewOrigin );
	savefile->ReadMat3( firstPersonViewAxis );

	// don't bother saving dragEntity since it's a dev tool
	dragEntity.Clear();

	savefile->ReadJoint( hipJoint );
	savefile->ReadJoint( chestJoint );
	savefile->ReadJoint( headJoint );

	savefile->ReadStaticObject( physicsObj );
	RestorePhysics( &physicsObj );

	savefile->ReadInt( num );
	aasLocation.SetGranularity( 1 );
	aasLocation.SetNum( num );
	for( i = 0; i < num; i++ ) {
		savefile->ReadInt( aasLocation[ i ].areaNum );
		savefile->ReadVec3( aasLocation[ i ].pos );
	}

	savefile->ReadInt( bobFoot );
	savefile->ReadFloat( bobFrac );
	savefile->ReadFloat( bobfracsin );
	savefile->ReadInt( bobCycle );
	savefile->ReadFloat( xyspeed );
	savefile->ReadInt( stepUpTime );
	savefile->ReadFloat( stepUpDelta );
	savefile->ReadFloat( idealLegsYaw );
	savefile->ReadFloat( legsYaw );
	savefile->ReadBool( legsForward );
	savefile->ReadFloat( oldViewYaw );
	savefile->ReadAngles( viewBobAngles );
	savefile->ReadVec3( viewBob );
	savefile->ReadInt( landChange );
	savefile->ReadInt( landTime );
	savefile->ReadInt( lastFootstepPlaytime );
	savefile->ReadBool(isPushing);

	savefile->ReadInt( currentWeapon );
	savefile->ReadInt( idealWeapon );
	savefile->ReadInt( previousWeapon );
	savefile->ReadInt( weaponSwitchTime );
	savefile->ReadBool( weaponEnabled );
	savefile->ReadBool( showWeaponViewModel );

	savefile->ReadSkin( skin );
	savefile->ReadSkin( powerUpSkin );
	savefile->ReadString( baseSkinName );

	savefile->ReadInt( numProjectilesFired );
	savefile->ReadInt( numProjectileHits );

	savefile->ReadBool( airless );
	savefile->ReadInt( airTics );
	savefile->ReadInt( lastAirDamage );

	savefile->ReadBool(underWaterEffectsActive);
	savefile->ReadInt(underWaterGUIHandle);

	savefile->ReadBool( gibDeath );
	savefile->ReadBool( gibsLaunched );
	savefile->ReadVec3( gibsDir );

	savefile->ReadFloat( set );
	zoomFov.SetStartTime( set );
	savefile->ReadFloat( set );
	zoomFov.SetDuration( set );
	savefile->ReadFloat( set );
	zoomFov.SetStartValue( set );
	savefile->ReadFloat( set );
	zoomFov.SetEndValue( set );

	savefile->ReadFloat( set );
	centerView.SetStartTime( set );
	savefile->ReadFloat( set );
	centerView.SetDuration( set );
	savefile->ReadFloat( set );
	centerView.SetStartValue( set );
	savefile->ReadFloat( set );
	centerView.SetEndValue( set );

	savefile->ReadBool( fxFov );

	savefile->ReadFloat( influenceFov );
	savefile->ReadInt( influenceActive );
	savefile->ReadFloat( influenceRadius );
	savefile->ReadObject( reinterpret_cast<idClass *&>( influenceEntity ) );
	savefile->ReadMaterial( influenceMaterial );
	savefile->ReadSkin( influenceSkin );

	savefile->ReadObject( reinterpret_cast<idClass *&>( privateCameraView ) );
	savefile->ReadVec3( m_ListenerLoc );
	savefile->ReadVec3( m_DoorListenLoc );

	savefile->ReadDict( &m_immobilization );
	savefile->ReadInt( m_immobilizationCache );

	savefile->ReadDict( &m_hinderance );
	savefile->ReadFloat( m_hinderanceCache );
	savefile->ReadDict( &m_TurnHinderance );
	savefile->ReadFloat( m_TurnHinderanceCache );

	for( i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++ ) {
		savefile->ReadAngles( loggedViewAngles[ i ] );
	}
	for( i = 0; i < NUM_LOGGED_ACCELS; i++ ) {
		savefile->ReadInt( loggedAccel[ i ].time );
		savefile->ReadVec3( loggedAccel[ i ].dir );
	}
	savefile->ReadInt( currentLoggedAccel );

#if 0
	savefile->ReadObject( reinterpret_cast<idClass *&>( focusGUIent ) );
	// can't save focusUI
	focusUI = NULL;
	savefile->ReadObject( reinterpret_cast<idClass *&>( focusCharacter ) );
	savefile->ReadInt( talkCursor );
	savefile->ReadInt( focusTime );
	savefile->ReadObject( reinterpret_cast<idClass *&>( focusVehicle ) );
#endif
	savefile->ReadUserInterface( cursor );

	savefile->ReadInt( oldMouseX );
	savefile->ReadInt( oldMouseY );

	savefile->ReadString( pdaAudio );
	savefile->ReadString( pdaVideo );
	savefile->ReadString( pdaVideoWave );

	savefile->ReadBool( tipUp );
	savefile->ReadBool( objectiveUp );

	savefile->ReadInt( lastDamageDef );
	savefile->ReadVec3( lastDamageDir );
	savefile->ReadInt( lastDamageLocation );
	savefile->ReadBool( m_bDamagedThisFrame );
	savefile->ReadInt( smoothedFrame );
	savefile->ReadBool( smoothedOriginUpdated );
	savefile->ReadVec3( smoothedOrigin );
	savefile->ReadAngles( smoothedAngles );

	savefile->ReadBool( ready );
	savefile->ReadBool( respawning );
	savefile->ReadBool( leader );
	savefile->ReadInt( lastSpectateChange );
	savefile->ReadInt( lastTeleFX );

	// set the pm_ cvars
	const idKeyValue	*kv;
	kv = spawnArgs.MatchPrefix( "pm_", NULL );
	while( kv ) {
		cvarSystem->SetCVarString( kv->GetKey(), kv->GetValue() );
		kv = spawnArgs.MatchPrefix( "pm_", kv );
	}

	// savefile->ReadFloat( set );
	// Commented out by Dram. TDM does not use stamina
	//pm_stamina.SetFloat( set );

	savefile->ReadBool( m_bGrabberActive );
	savefile->ReadBool( m_bDraggingBody );
	savefile->ReadBool( m_bShoulderingBody );

	savefile->ReadBool( m_IdealCrouchState );
	savefile->ReadBool( m_CrouchIntent );

	savefile->ReadInt(m_InventoryOverlay);

	bool hasWeaponCursor;
	savefile->ReadBool(hasWeaponCursor);
	if (hasWeaponCursor) {
		int cursorId;
		savefile->ReadInt(cursorId);
		m_WeaponCursor = Inventory()->GetCursor(cursorId);
	}

	bool hasMapCursor;
	savefile->ReadBool(hasMapCursor);
	if (hasMapCursor) {
		int cursorId;
		savefile->ReadInt(cursorId);
		m_MapCursor = Inventory()->GetCursor(cursorId);
	}

	m_ActiveInventoryMapEnt.Restore(savefile);

	savefile->ReadInt(m_LightgemModifier);

	savefile->ReadInt(num);
	for (int i = 0; i < num; i++)
	{
		idStr name;
		int value;
		savefile->ReadString(name);
		savefile->ReadInt(value);
		// Store the pair into the map
		m_LightgemModifierList[std::string(name.c_str())] = value;
	}

	// create combat collision hull for exact collision detection
	SetCombatModel();

	// Necessary, since the overlay system can't save external GUI pointers.
	if ( m_overlays.isExternal( OVERLAYS_MIN_HANDLE  ) )
		m_overlays.setGui( OVERLAYS_MIN_HANDLE, hud );
	else
		gameLocal.Warning( "Unable to relink HUD to overlay system.\n" );
}

/*
===============
idPlayer::PrepareForRestart
================
*/
void idPlayer::PrepareForRestart( void ) {
	Spectate( true );
	forceRespawn = true;
	
	// we will be restarting program, clear the client entities from program-related things first
	ShutdownThreads();

	// the sound world is going to be cleared, don't keep references to emitters
	FreeSoundEmitter( false );
}

/*
===============
idPlayer::Restart
================
*/
void idPlayer::Restart( void ) {
	idActor::Restart();
	
	// client needs to setup the animation script object again
	if ( gameLocal.isClient ) {
		Init();
	} else {
		// choose a random spot and prepare the point of view in case player is left spectating
		assert( spectating );
		SpawnFromSpawnSpot();
	}

	useInitialSpawns = true;
	UpdateSkinSetup( true );
}

/*
===============
idPlayer::ServerSpectate
================
*/
void idPlayer::ServerSpectate( bool spectate ) {
	assert( !gameLocal.isClient );

	if ( spectating != spectate ) {
		Spectate( spectate );
		if ( spectate ) {
			SetSpectateOrigin();
		} else {
			if ( gameLocal.gameType == GAME_DM ) {
				// make sure the scores are reset so you can't exploit by spectating and entering the game back
				// other game types don't matter, as you either can't join back, or it's team scores
				gameLocal.mpGame.ClearFrags( entityNumber );
			}
		}
	}
	if ( !spectate ) {
		SpawnFromSpawnSpot();
	}
}

/*
===========
idPlayer::SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
void idPlayer::SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles ) {
	idEntity *spot;
	idStr skin;

	spot = gameLocal.SelectInitialSpawnPoint( this );

	// set the player skin from the spawn location
	if ( spot->spawnArgs.GetString( "skin", NULL, skin ) ) {
		spawnArgs.Set( "spawn_skin", skin );
	}

	// activate the spawn locations targets
	spot->PostEventMS( &EV_ActivateTargets, 0, this );

	origin = spot->GetPhysics()->GetOrigin();
	origin[2] += 4.0f + CM_BOX_EPSILON;		// move up to make sure the player is at least an epsilon above the floor
	angles = spot->GetPhysics()->GetAxis().ToAngles();
}

/*
===========
idPlayer::SpawnFromSpawnSpot

Chooses a spawn location and spawns the player
============
*/
void idPlayer::SpawnFromSpawnSpot( void ) {
	idVec3		spawn_origin;
	idAngles	spawn_angles;
	
	SelectInitialSpawnPoint( spawn_origin, spawn_angles );
	SpawnToPoint( spawn_origin, spawn_angles );
}

/*
===========
idPlayer::SpawnToPoint

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState

when called here with spectating set to true, just place yourself and init
============
*/
void idPlayer::SpawnToPoint( const idVec3 &spawn_origin, const idAngles &spawn_angles ) {
	idVec3 spec_origin;

	assert( !gameLocal.isClient );

	respawning = true;

	Init();

	fl.noknockback = false;

	// stop any ragdolls being used
	StopRagdoll();

	// set back the player physics
	SetPhysics( &physicsObj );

	physicsObj.SetClipModelAxis();
	physicsObj.EnableClip();

	if ( !spectating ) {
		SetCombatContents( true );
	}

	physicsObj.SetLinearVelocity( vec3_origin );

	// setup our initial view
	if ( !spectating ) {
		SetOrigin( spawn_origin );
	} else {
		spec_origin = spawn_origin;
		spec_origin[ 2 ] += pm_normalheight.GetFloat();
		spec_origin[ 2 ] += SPECTATE_RAISE;
		SetOrigin( spec_origin );
	}

	// if this is the first spawn of the map, we don't have a usercmd yet,
	// so the delta angles won't be correct.  This will be fixed on the first think.
	viewAngles = ang_zero;
	SetDeltaViewAngles( ang_zero );
	SetViewAngles( spawn_angles );
	spawnAngles = spawn_angles;
	spawnAnglesSet = false;

	legsForward = true;
	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	if ( spectating ) {
		Hide();
	} else {
		Show();
	}

	if ( gameLocal.isMultiplayer ) {
		if ( !spectating ) {
			// we may be called twice in a row in some situations. avoid a double fx and 'fly to the roof'
			if ( lastTeleFX < gameLocal.time - 1000 ) {
				idEntityFx::StartFx( spawnArgs.GetString( "fx_spawn" ), &spawn_origin, NULL, this, true );
				lastTeleFX = gameLocal.time;
			}
		}
		AI_TELEPORT = true;
	} else {
		AI_TELEPORT = false;
	}

	// kill anything at the new position
	if ( !spectating ) {
		physicsObj.SetClipMask( MASK_PLAYERSOLID ); // the clip mask is usually maintained in Move(), but KillBox requires it
		gameLocal.KillBox( this );
	}

	// don't allow full run speed for a bit
	physicsObj.SetKnockBack( 100 );

	// set our respawn time and buttons so that if we're killed we don't respawn immediately
	minRespawnTime = gameLocal.time;
	maxRespawnTime = gameLocal.time;
	if ( !spectating ) {
		forceRespawn = false;
	}

	privateCameraView = NULL;

	BecomeActive( TH_THINK );

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	Think();

	respawning			= false;
	lastManOver			= false;
	lastManPlayAgain	= false;
	isTelefragged		= false;
}

/*
===============
idPlayer::SavePersistantInfo

Saves any inventory and player stats when changing levels.
===============
*/
void idPlayer::SavePersistantInfo( void ) {
	idDict &playerInfo = gameLocal.persistentPlayerInfo[entityNumber];

	playerInfo.Clear();
	playerInfo.SetInt( "health", health );
	playerInfo.SetInt( "current_weapon", currentWeapon );
}

/*
===============
idPlayer::RestorePersistantInfo

Restores any inventory and player stats when changing levels.
===============
*/
void idPlayer::RestorePersistantInfo( void ) {
	if ( gameLocal.isMultiplayer ) {
		gameLocal.persistentPlayerInfo[entityNumber].Clear();
	}

	spawnArgs.Copy( gameLocal.persistentPlayerInfo[entityNumber] );

	health = spawnArgs.GetInt( "health", "100" );
	if ( !gameLocal.isClient ) {
		idealWeapon = spawnArgs.GetInt( "current_weapon", "0" );
	}
}

/*
================
idPlayer::GetUserInfo
================
*/
idDict *idPlayer::GetUserInfo( void ) {
	return &gameLocal.userInfo[ entityNumber ];
}

/*
==============
idPlayer::UpdateSkinSetup
==============
*/
void idPlayer::UpdateSkinSetup( bool restart ) {
	if ( restart ) {
		team = ( idStr::Icmp( GetUserInfo()->GetString( "ui_team" ), "Blue" ) == 0 );
	}
	if ( gameLocal.gameType == GAME_TDM ) {
		if ( team ) {
			baseSkinName = "skins/characters/player/marine_mp_blue";
		} else {
			baseSkinName = "skins/characters/player/marine_mp_red";
		}
		if ( !gameLocal.isClient && team != latchedTeam ) {
			gameLocal.mpGame.SwitchToTeam( entityNumber, latchedTeam, team );
		}
		latchedTeam = team;
	} else {
		baseSkinName = GetUserInfo()->GetString( "ui_skin" );
	}
	if ( !baseSkinName.Length() ) {
		baseSkinName = "skins/characters/player/marine_mp";
	}
	skin = declManager->FindSkin( baseSkinName, false );
	assert( skin );
	// match the skin to a color band for scoreboard
	if ( baseSkinName.Find( "red" ) != -1 ) {
		colorBarIndex = 1;
	} else if ( baseSkinName.Find( "green" ) != -1 ) {
		colorBarIndex = 2;
	} else if ( baseSkinName.Find( "blue" ) != -1 ) {
		colorBarIndex = 3;
	} else if ( baseSkinName.Find( "yellow" ) != -1 ) {
		colorBarIndex = 4;
	} else {
		colorBarIndex = 0;
	}
	colorBar = colorBarTable[ colorBarIndex ];
}

/*
==============
idPlayer::BalanceTDM
==============
*/
bool idPlayer::BalanceTDM( void ) {
	int			i, balanceTeam, teamCount[2];
	idEntity	*ent;

	teamCount[ 0 ] = teamCount[ 1 ] = 0;
	for( i = 0; i < gameLocal.numClients; i++ ) {
		ent = gameLocal.entities[ i ];
		if ( ent && ent->IsType( idPlayer::Type ) ) {
			teamCount[ static_cast< idPlayer * >( ent )->team ]++;
		}
	}
	balanceTeam = -1;
	if ( teamCount[ 0 ] < teamCount[ 1 ] ) {
		balanceTeam = 0;
	} else if ( teamCount[ 0 ] > teamCount[ 1 ] ) {
		balanceTeam = 1;
	}
	if ( balanceTeam != -1 && team != balanceTeam ) {
		common->DPrintf( "team balance: forcing player %d to %s team\n", entityNumber, balanceTeam ? "blue" : "red" );
		team = balanceTeam;
		GetUserInfo()->Set( "ui_team", team ? "Blue" : "Red" );
		return true;
	}
	return false;
}

/*
==============
idPlayer::UserInfoChanged
==============
*/
bool idPlayer::UserInfoChanged( bool canModify ) {
	idDict	*userInfo;
	bool	modifiedInfo;
	bool	spec;
	bool	newready;

	userInfo = GetUserInfo();
	showWeaponViewModel = userInfo->GetBool( "ui_showGun" );

	if ( !gameLocal.isMultiplayer ) {
		return false;
	}

	modifiedInfo = false;

	spec = ( idStr::Icmp( userInfo->GetString( "ui_spectate" ), "Spectate" ) == 0 );
	if ( gameLocal.serverInfo.GetBool( "si_spectators" ) ) {
		// never let spectators go back to game while sudden death is on
		if ( canModify && gameLocal.mpGame.GetGameState() == idMultiplayerGame::SUDDENDEATH && !spec && wantSpectate == true ) {

			userInfo->Set( "ui_spectate", "Spectate" );
			modifiedInfo |= true;
		} else {
			if ( spec != wantSpectate && !spec ) {
				// returning from spectate, set forceRespawn so we don't get stuck in spectate forever
				forceRespawn = true;
			}
			wantSpectate = spec;
		}
	} else {
		if ( canModify && spec ) {

			userInfo->Set( "ui_spectate", "Play" );
			modifiedInfo |= true;
		} else if ( spectating ) {  
			// allow player to leaving spectator mode if they were in it when si_spectators got turned off

			forceRespawn = true;
		}
		wantSpectate = false;
	}
	newready = ( idStr::Icmp( userInfo->GetString( "ui_ready" ), "Ready" ) == 0 );
	if ( ready != newready && gameLocal.mpGame.GetGameState() == idMultiplayerGame::WARMUP && !wantSpectate ) {
		gameLocal.mpGame.AddChatLine( common->GetLanguageDict()->GetString( "#str_07180" ), userInfo->GetString( "ui_name" ), newready ? common->GetLanguageDict()->GetString( "#str_04300" ) : common->GetLanguageDict()->GetString( "#str_04301" ) );
	}
	ready = newready;
	team = ( idStr::Icmp( userInfo->GetString( "ui_team" ), "Blue" ) == 0 );
	// server maintains TDM balance
	if ( canModify && gameLocal.gameType == GAME_TDM && !gameLocal.mpGame.IsInGame( entityNumber ) && g_balanceTDM.GetBool() ) {

		modifiedInfo |= BalanceTDM( );
	}
	UpdateSkinSetup( false );
	
	isChatting = userInfo->GetBool( "ui_chat", "0" );
	if ( canModify && isChatting && AI_DEAD ) {

		// if dead, always force chat icon off.
		isChatting = false;
		userInfo->SetBool( "ui_chat", false );
		modifiedInfo |= true;
	}

	return modifiedInfo;
}

/*
===============
idPlayer::UpdateHudAmmo
===============
*/
void idPlayer::UpdateHudAmmo()
{
	CInventoryWeaponItemPtr curWeapon = GetCurrentWeaponItem();

	// If no weapon item there, or the first one is selected, switch off the HUD
	bool weaponSelected = (curWeapon != NULL && curWeapon->GetWeaponIndex() > 0);

	hud->SetStateBool("WeaponAmmoVisible", weaponSelected && !curWeapon->IsAllowedEmpty());
	
	if (!weaponSelected) return; // done here

	hud->SetStateString("WeaponAmmoAmount", va("%d", curWeapon->GetAmmo()));
}

/*
===============
idPlayer::UpdateHudStats
===============
*/
void idPlayer::UpdateHudStats( idUserInterface *_hud )
{
	// Commented out by Dram. TDM does not use stamina
	//int staminapercentage;
	//float max_stamina;

	assert( _hud );

	// Commented out by Dram. TDM does not use stamina
	/*max_stamina = pm_stamina.GetFloat();
	if ( !max_stamina ) {
		// stamina disabled, so show full stamina bar
		staminapercentage = 100;
	} else {
		staminapercentage = idMath::FtoiFast( 100.0f * stamina / max_stamina );
	}*/

	_hud->SetStateInt( "player_health", health );
	// Commented out by Dram. TDM does not use stamina
	//_hud->SetStateInt( "player_stamina", staminapercentage );
	_hud->SetStateInt( "player_shadow", 1 );

	_hud->SetStateInt( "player_hr", heartRate );
	// Commented out by Dram. TDM does not use stamina
	//_hud->SetStateInt( "player_nostamina", ( max_stamina == 0 ) ? 1 : 0 );

	_hud->HandleNamedEvent( "updateArmorHealthAir" );

	if ( healthPulse ) {
		_hud->HandleNamedEvent( "healthPulse" );
		StartSound( "snd_healthpulse", SND_CHANNEL_ITEM, 0, false, NULL );
		healthPulse = false;
	}

	if ( healthTake ) {
		_hud->HandleNamedEvent( "healthPulse" );
		StartSound( "snd_healthtake", SND_CHANNEL_ITEM, 0, false, NULL );
		healthTake = false;
	}

	UpdateHudAmmo();
}

/*
===============
idPlayer::UpdateHudWeapon
===============
*/
void idPlayer::UpdateHudWeapon( bool flashWeapon )
{
	// if updating the hud of a followed client
	if ( gameLocal.localClientNum >= 0 && gameLocal.entities[ gameLocal.localClientNum ] && gameLocal.entities[ gameLocal.localClientNum ]->IsType( idPlayer::Type ) ) {
		idPlayer *p = static_cast< idPlayer * >( gameLocal.entities[ gameLocal.localClientNum ] );
		if ( p->spectating && p->spectator == entityNumber ) {
			assert( p->hud );
			hud = p->hud;
		}
	}

	if (hud == NULL || m_WeaponCursor == NULL) {
		return;
	}

	CInventoryWeaponItemPtr curWeapon = GetCurrentWeaponItem();

	// If no weapon item there, or the first one is selected, switch off the HUD
	bool weaponSelected = (curWeapon != NULL && curWeapon->GetWeaponIndex() > 0);

	// Update the visibility of the various GUI elements
	hud->SetStateBool("WeaponIconVisible", weaponSelected);
	hud->SetStateBool("WeaponNameVisible", weaponSelected);
		
	if (!weaponSelected) return; // done here

	// Set the icon and name strings
	hud->SetStateString("WeaponName", curWeapon->GetName().c_str());
	hud->SetStateString("WeaponIcon", curWeapon->GetIcon().c_str());
	
	hud->HandleNamedEvent("OnWeaponChange");

	UpdateHudAmmo();
}

void idPlayer::PrintDebugHUD(void)
{
	idStr strText;
	idVec3 a, b, c;
	int y;

	// TODO: Remove this when no longer needed.
	y = 100;
	sprintf(strText, "ViewOrg:    x: %f   y: %f   z: %f", renderView->vieworg.x, renderView->vieworg.y, renderView->vieworg.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	renderView->viewaxis.GetMat3Params(a, b, c);
	sprintf(strText, "ViewMatrix:", renderView->vieworg.x, renderView->vieworg.y, renderView->vieworg.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "x: %f   y: %f   z: %f", a.x, a.y, a.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "x: %f   y: %f   z: %f", b.x, b.y, b.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "x: %f   y: %f   z: %f", c.x, c.y, c.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "FOV x: %f   y: %f", renderView->fov_x, renderView->fov_y);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "ViewAngles Pitch: %f   Yaw: %f   Roll: %f", viewAngles.pitch, viewAngles.yaw, viewAngles.roll);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	sprintf(strText, "ViewBobAngles Pitch: %f   Yaw: %f   Roll: %f", viewBobAngles.pitch, viewBobAngles.yaw, viewBobAngles.roll);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
	y += 12;
	a = GetEyePosition();
	sprintf(strText, "EyePosition x: %f   y: %f   z: %f", a.x, a.y, a.z);
	renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
}

/*
===============
idPlayer::DrawHUD
===============
*/
void idPlayer::DrawHUD(idUserInterface *_hud)
{
//	if(cv_lg_debug.GetInteger() != 0)
//		PrintDebugHUD();

	/*renderSystem->DrawSmallStringExt(1, 30, 
		va("Player velocity: %f", physicsObj.GetLinearVelocity().Length()), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));*/

	const char *name;
	if((name = cv_dm_distance.GetString()) != NULL)
	{
		idEntity *e;

		if((e = gameLocal.FindEntity(name)) != NULL)
		{
			idStr strText;
			float d;
			int y;

			d = (GetPhysics()->GetOrigin() - e->GetPhysics()->GetOrigin()).Length();

			y = 100;
			sprintf(strText, "Entity [%s]   distance: %f", name, d);
			renderSystem->DrawSmallStringExt(1, y, strText.c_str( ), idVec4( 1, 1, 1, 1 ), false, declManager->FindMaterial( "textures/bigchars" ));
		}
	}

	if(_hud)
		DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("PlayerHUD: [%s]\r", (_hud->Name() == NULL)?"null":_hud->Name());
	else
		DM_LOG(LC_SYSTEM, LT_INFO)LOGSTRING("PlayerHUD: NULL\r");

	if ( !weapon.GetEntity() || influenceActive != INFLUENCE_NONE || privateCameraView || gameLocal.GetCamera() || !_hud || !g_showHud.GetBool() ) {
		return;
	}

	UpdateHudStats( _hud );

	//_hud->SetStateString( "weapicon", weapon.GetEntity()->Icon() );

	// FIXME: this is temp to allow the sound meter to show up in the hud
	// it should be commented out before shipping but the code can remain
	// for mod developers to enable for the same functionality
	_hud->SetStateInt( "s_debug", cvarSystem->GetCVarInteger( "s_showLevelMeter" ) );

	weapon.GetEntity()->UpdateGUI();

	//_hud->Redraw( gameLocal.realClientTime );
	m_overlays.drawOverlays();

	// weapon targeting crosshair

#if 0 // greebo: disabled cursor calls entirely
	if ( !GuiActive() ) {
		if ( cursor && weapon.GetEntity()->ShowCrosshair() ) {
			cursor->Redraw( gameLocal.realClientTime );
		}
	}
#endif

	// Only use this if the old lightgem is selected. This may be usefull for
	// slower machines.
	if(cv_lg_weak.GetBool() == true)
		AdjustLightgem();

	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Setting Lightgemvalue: %u on hud: %08lX\r\r", g_Global.m_DarkModPlayer->m_LightgemValue, hud);
	hud->SetStateInt("lightgem_val", g_Global.m_DarkModPlayer->m_LightgemValue);
}

/*
===============
idPlayer::EnterCinematic
===============
*/
void idPlayer::EnterCinematic( void ) {
	Hide();
	StopAudioLog();
	StopSound( SND_CHANNEL_PDA, false );
	if ( hud ) {
		hud->HandleNamedEvent( "radioChatterDown" );
	}
	
	physicsObj.SetLinearVelocity( vec3_origin );
	
	SetState( "EnterCinematic" );
	UpdateScript();

	if ( weaponEnabled && weapon.GetEntity() ) {
		weapon.GetEntity()->EnterCinematic();
	}

	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_RUN			= false;
	AI_ATTACK_HELD	= false;
	AI_BLOCK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_WEAPON_BLOCKED = false;
	AI_JUMP			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_DEAD			= ( health <= 0 );
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;

	AI_LEAN_LEFT	= false;
	AI_LEAN_RIGHT	= false;
	AI_LEAN_FORWARD	= false;

	AI_CREEP		= false;
}

/*
===============
idPlayer::ExitCinematic
===============
*/
void idPlayer::ExitCinematic( void ) {
	Show();

	if ( weaponEnabled && weapon.GetEntity() ) {
		weapon.GetEntity()->ExitCinematic();
	}

	SetState( "ExitCinematic" );
	UpdateScript();
}

/*
=====================
idPlayer::UpdateConditions
=====================
*/
void idPlayer::UpdateConditions( void )
{
	idVec3	velocity;
	float	fallspeed;
	float	forwardspeed;
	float	sidespeed;

	// minus the push velocity to avoid playing the walking animation and sounds when riding a mover
	velocity = physicsObj.GetLinearVelocity() - physicsObj.GetPushedLinearVelocity();
	fallspeed = velocity * physicsObj.GetGravityNormal();

	if ( influenceActive ) {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	} else if ( gameLocal.time - lastDmgTime < 500 ) {
		forwardspeed = velocity * viewAxis[ 0 ];
		sidespeed = velocity * viewAxis[ 1 ];
		AI_FORWARD		= AI_ONGROUND && ( forwardspeed > 20.01f );
		AI_BACKWARD		= AI_ONGROUND && ( forwardspeed < -20.01f );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( sidespeed > 20.01f );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( sidespeed < -20.01f );
	} else if ( xyspeed > MIN_BOB_SPEED ) {
		AI_FORWARD		= AI_ONGROUND && ( usercmd.forwardmove > 0 );
		AI_BACKWARD		= AI_ONGROUND && ( usercmd.forwardmove < 0 );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( usercmd.rightmove < 0 );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( usercmd.rightmove > 0 );
	} else {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	}

	// stamina disabled, always run regardless of stamina
	//AI_RUN			= ( usercmd.buttons & BUTTON_RUN ) && ( ( !pm_stamina.GetFloat() ) || ( stamina > pm_staminathreshold.GetFloat() ) );
	AI_RUN = ( usercmd.buttons & BUTTON_RUN ) && true ;
	AI_DEAD			= ( health <= 0 );
	
	// DarkMod: Catch the creep modifier
	AI_CREEP		=( usercmd.buttons & BUTTON_5 ) && true;
}

/*
==================
WeaponFireFeedback

Called when a weapon fires, generates head twitches, etc
==================
*/
void idPlayer::WeaponFireFeedback( const idDict *weaponDef ) {
	// force a blink
	blink_time = 0;

	// play the fire animation
	AI_WEAPON_FIRED = true;

	// update view feedback
	playerView.WeaponFireFeedback( weaponDef );
}

/*
===============
idPlayer::StopFiring
===============
*/
void idPlayer::StopFiring( void ) 
{
	AI_ATTACK_HELD	= false;
	AI_BLOCK_HELD	= false;
	AI_WEAPON_BLOCKED = false;
	AI_WEAPON_FIRED = false;
	AI_RELOAD		= false;
	if ( weapon.GetEntity() ) {
		weapon.GetEntity()->EndAttack();
	}
}

/*
===============
idPlayer::FireWeapon
===============
*/
void idPlayer::FireWeapon( void ) 
{
	idMat3 axis;
	idVec3 muzzle;

	if ( privateCameraView ) {
		return;
	}

	if ( g_editEntityMode.GetInteger() ) {
		GetViewPos( muzzle, axis );
		if ( gameLocal.editEntities->SelectEntity( muzzle, axis[0], this ) ) {
			return;
		}
	}

	if ( !hiddenWeapon && weapon.GetEntity()->IsReady() ) 
	{
		if ( weapon.GetEntity()->AmmoInClip() || weapon.GetEntity()->AmmoAvailable() ) 
		{
			AI_ATTACK_HELD = true;
			weapon.GetEntity()->BeginAttack();
			if ( ( weapon_soulcube >= 0 ) && ( currentWeapon == weapon_soulcube ) ) 
			{
				if ( hud ) 
				{
					hud->HandleNamedEvent( "soulCubeNotReady" );
				}
				SelectWeapon( previousWeapon, false );
			}
		} else if( cv_weapon_next_on_empty.GetBool() )
		{
			NextBestWeapon();
		}
	}

	if ( hud ) {
		if ( tipUp ) {
			HideTip();
		}
		// may want to track with with a bool as well
		// keep from looking up named events so often
		if ( objectiveUp ) {
			HideObjective();
		}
	}
}

/*
===============
idPlayer::BlockWeapon
===============
*/
void idPlayer::BlockWeapon( void ) 
{
	if ( privateCameraView ) 
	{
		return;
	}

	if ( !hiddenWeapon && weapon.GetEntity()->IsReady() ) 
	{
		AI_BLOCK_HELD = true;
		weapon.GetEntity()->BeginBlock();
	}
}

/*
===============
idPlayer::CacheWeapons
===============
*/
void idPlayer::CacheWeapons()
{
	// greebo: Cache all weapons, regardless if we have them or not
	for (const idKeyValue* kv = spawnArgs.MatchPrefix("def_weapon"); kv != NULL; kv = spawnArgs.MatchPrefix("def_weapon", kv))
	{
		if (kv->GetValue().IsEmpty()) continue; // skip empty spawnargs

		idWeapon::CacheWeapon(kv->GetValue());
	}
}

/*
===============
idPlayer::Give
===============
*/
bool idPlayer::Give( const char *statname, const char *value ) {
	int amount;

	if ( AI_DEAD ) {
		return false;
	}

	if ( !idStr::Icmp( statname, "health" ) ) {
		if ( health >= maxHealth ) {
			return false;
		}
		amount = atoi( value );
		if ( amount ) {
			health += amount;
			if ( health > maxHealth ) {
				health = maxHealth;
			}
			if ( hud ) {
				hud->HandleNamedEvent( "healthPulse" );
			}
		}

	} else if ( !idStr::Icmp( statname, "stamina" ) ) {
		if ( stamina >= 100 ) {
			return false;
		}
		stamina += atof( value );
		if ( stamina > 100 ) {
			stamina = 100;
		}

	} else if ( !idStr::Icmp( statname, "heartRate" ) ) {
		heartRate += atoi( value );
		if ( heartRate > MAX_HEARTRATE ) {
			heartRate = MAX_HEARTRATE;
		}

	} else if ( !idStr::Icmp( statname, "air" ) ) {
		if ( airTics >= pm_airTics.GetInteger() ) {
			return false;
		}
		airTics += static_cast<int>(atof( value ) / 100.0 * pm_airTics.GetInteger());
		if ( airTics > pm_airTics.GetInteger() ) {
			airTics = pm_airTics.GetInteger();
		}
	} else {
		return false;//inventory.Give( this, spawnArgs, statname, value, &idealWeapon, true );
	}
	return true;
}


/*
===============
idPlayer::GiveHealthPool

adds health to the player health pool
===============
*/
void idPlayer::GiveHealthPool( float amt ) {
	
	if ( AI_DEAD ) {
		return;
	}

	if ( health > 0 ) {
		healthPool += amt;
		if ( healthPool > maxHealth - health ) {
			healthPool = maxHealth - health;
		}
		nextHealthPulse = gameLocal.time;

		// Reset the values to default
		healthPoolTimeInterval = HEALTHPULSE_TIME;
		healthPoolTimeIntervalFactor = 1.0f;
		healthPoolStepAmount = 5;
	}
}

/*
===============
idPlayer::PowerUpModifier
===============
*/
float idPlayer::PowerUpModifier( int type ) {
	// greebo: Unused at the moment, maybe in the future
	return 1.0f;
}

/*
===============
idPlayer::GivePowerUp
===============
*/
bool idPlayer::GivePowerUp( int powerup, int time ) {
	const char *sound;
	const char *skin;

	if ( powerup >= 0 && powerup < MAX_POWERUPS ) {

		if ( gameLocal.isServer ) {
			idBitMsg	msg;
			byte		msgBuf[MAX_EVENT_PARAM_SIZE];

			msg.Init( msgBuf, sizeof( msgBuf ) );
			msg.WriteShort( powerup );
			msg.WriteBits( 1, 1 );
			ServerSendEvent( EVENT_POWERUP, &msg, false, -1 );
		}

		/*if ( powerup != MEGAHEALTH ) {
			inventory.GivePowerUp( this, powerup, time );
		}*/

		const idDeclEntityDef *def = NULL;

		switch( powerup ) {
			case BERSERK: {
				if ( spawnArgs.GetString( "snd_berserk_third", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_DEMONIC, 0, false, NULL );
				}
				if ( baseSkinName.Length() ) {
					powerUpSkin = declManager->FindSkin( baseSkinName + "_berserk" );
				}
				if ( !gameLocal.isClient ) {
					idealWeapon = 0;
				}
				break;
			}
			case INVISIBILITY: {
				spawnArgs.GetString( "skin_invisibility", "", &skin );
				powerUpSkin = declManager->FindSkin( skin );
				// remove any decals from the model
				if ( modelDefHandle != -1 ) {
					gameRenderWorld->RemoveDecals( modelDefHandle );
				}
				if ( weapon.GetEntity() ) {
					weapon.GetEntity()->UpdateSkin();
				}
				if ( spawnArgs.GetString( "snd_invisibility", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_ANY, 0, false, NULL );
				}
				break;
			}
			case ADRENALINE: {
				stamina = 100.0f;
				break;
			 }
			case MEGAHEALTH: {
				if ( spawnArgs.GetString( "snd_megahealth", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_ANY, 0, false, NULL );
				}
				def = gameLocal.FindEntityDef( "powerup_megahealth", false );
				if ( def ) {
					health = def->dict.GetInt( "inv_health" );
				}
				break;
			 }
		}

		if ( hud ) {
			hud->HandleNamedEvent( "itemPickup" );
		}

		return true;
	} else {
		gameLocal.Warning( "Player given power up %i\n which is out of range", powerup );
	}
	return false;
}

/*
==============
idPlayer::UpdatePowerUps
==============
*/
void idPlayer::UpdatePowerUps( void ) {
	if ( health > 0 ) {
		if ( powerUpSkin ) {
			renderEntity.customSkin = powerUpSkin;
		} else {
			renderEntity.customSkin = skin;
		}
	}

	if ( healthPool && gameLocal.time > nextHealthPulse && !AI_DEAD && health > 0 ) {
		assert( !gameLocal.isClient );	// healthPool never be set on client

		//int amt = ( healthPool > 5 ) ? 5 : healthPool; // old code
		// greebo: Changed step amount to be a variable that can be set from the "outside"
		int amt = ( healthPool > healthPoolStepAmount ) ? healthPoolStepAmount : static_cast<int>(healthPool);

		int oldHealth = health;

		health += amt;
		if ( health > maxHealth ) {
			health = maxHealth;
			healthPool = 0;
		} else {
			healthPool -= amt;
		}

		// greebo: Check how much health we actually took
		int healthTaken = health - oldHealth;
		// Update the mission statistics
		gameLocal.m_MissionData->HealthReceivedByPlayer(healthTaken);

		nextHealthPulse = gameLocal.time + healthPoolTimeInterval;

		// Check whether we have a valid interval factor and if yes: apply it
		if (healthPoolTimeIntervalFactor > 0) {
			healthPoolTimeInterval = static_cast<int>(healthPoolTimeInterval*healthPoolTimeIntervalFactor);
		}

		healthPulse = true;
	}
#ifndef ID_DEMO_BUILD
	if ( !gameLocal.inCinematic && influenceActive == 0 && g_skill.GetInteger() == 3 && gameLocal.time > nextHealthTake && !AI_DEAD && health > g_healthTakeLimit.GetInteger() ) {
		assert( !gameLocal.isClient );	// healthPool never be set on client
		health -= g_healthTakeAmt.GetInteger();
		if ( health < g_healthTakeLimit.GetInteger() ) {
			health = g_healthTakeLimit.GetInteger();
		}
		nextHealthTake = gameLocal.time + g_healthTakeTime.GetInteger() * 1000;
		healthTake = true;
	}
#endif
}

/*
===============
idPlayer::GivePDA
===============
*/
void idPlayer::GivePDA( const char *pdaName, idDict *item )
{
}

/*
===============
idPlayer::GiveItem
===============
*/
void idPlayer::GiveItem( const char *itemname ) {
	idDict args;

	args.Set( "classname", itemname );
	args.Set( "owner", name.c_str() );
	gameLocal.SpawnEntityDef( args );
	if ( hud ) {
		hud->HandleNamedEvent( "itemPickup" );
	}
}

/*
==================
idPlayer::SlotForWeapon
==================
*/
int idPlayer::SlotForWeapon( const char *weaponName )
{	
	// Find the weapon category
	CInventoryCategoryPtr weaponCategory = m_WeaponCursor->GetCurrentCategory();

	if (weaponCategory == NULL)
	{
		DM_LOG(LC_INVENTORY, LT_ERROR)LOGSTRING("Could not find weapon category in inventory.\r");
		return -1;
	}
	
	// Look for the weapon with the given name
	for (int i = 0; i < weaponCategory->GetNumItems(); i++)
	{
		CInventoryWeaponItemPtr weaponItem = 
			boost::dynamic_pointer_cast<CInventoryWeaponItem>(weaponCategory->GetItem(i));

		// Is this the right weapon?
		if (weaponItem != NULL && weaponItem->GetWeaponName() == weaponName)
		{
			// We're done
			return i;
		}
	}

	// not found
	return -1;
}

/*
===============
idPlayer::Reload
===============
*/
void idPlayer::Reload( void ) {
	if ( gameLocal.isClient ) {
		return;
	}

	if ( spectating || gameLocal.inCinematic || influenceActive ) {
		return;
	}

	if ( weapon.GetEntity() && weapon.GetEntity()->IsLinked() ) {
		weapon.GetEntity()->Reload();
	}
}

/*
===============
idPlayer::NextBestWeapon
===============
*/
void idPlayer::NextBestWeapon( void ) {
	// greebo: No "best" weapons in TDM, route the call to NextWeapon()
	NextWeapon();
}

/*
===============
idPlayer::NextWeapon
===============
*/
void idPlayer::NextWeapon() {
	if ( !weaponEnabled || spectating || hiddenWeapon || gameLocal.inCinematic || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || health < 0 ) {
		return;
	}

	if ( gameLocal.isClient ) {
		return;
	}

	if (m_WeaponCursor == NULL || m_WeaponCursor->GetCurrentCategory() == NULL) {
		return;
	}

	CInventoryCategoryPtr weaponCategory = m_WeaponCursor->GetCurrentCategory();

	int numWeapons = weaponCategory->GetNumItems();

	if (numWeapons == 0) {
		return; // no weapons...
	}
	
	// Get the current weaponItem
	CInventoryWeaponItemPtr curItem = GetCurrentWeaponItem();

	if (curItem == NULL) {
		return;
	}

	int curWeaponIndex = curItem->GetWeaponIndex();

	CInventoryWeaponItemPtr lastWeapon =
		boost::dynamic_pointer_cast<CInventoryWeaponItem>(weaponCategory->GetItem(weaponCategory->GetNumItems() - 1));

	if (lastWeapon == NULL) return;

	int highestIndex = lastWeapon->GetWeaponIndex();

	int nextWeaponIndex = curWeaponIndex;

	do {
		// Try to select the next weapon item
		nextWeaponIndex++;

		if (nextWeaponIndex > highestIndex) {
			nextWeaponIndex = 0;
		}
	} while (!SelectWeapon(nextWeaponIndex, false) && nextWeaponIndex != curWeaponIndex);
}

/*
===============
idPlayer::PrevWeapon
===============
*/
void idPlayer::PrevWeapon( void ) {
	if ( !weaponEnabled || spectating || hiddenWeapon || gameLocal.inCinematic || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || health < 0 ) {
		return;
	}

	if ( gameLocal.isClient ) {
		return;
	}

	if (m_WeaponCursor == NULL || m_WeaponCursor->GetCurrentCategory() == NULL) {
		return;
	}

	CInventoryCategoryPtr weaponCategory = m_WeaponCursor->GetCurrentCategory();

	int numWeapons = weaponCategory->GetNumItems();

	if (numWeapons == 0) {
		return; // no weapons...
	}

	// Get the current weaponItem
	CInventoryWeaponItemPtr curItem = GetCurrentWeaponItem();

	if (curItem == NULL) {
		return;
	}

	int curWeaponIndex = curItem->GetWeaponIndex();

	CInventoryWeaponItemPtr lastWeapon =
		boost::dynamic_pointer_cast<CInventoryWeaponItem>(weaponCategory->GetItem(weaponCategory->GetNumItems() - 1));

	if (lastWeapon == NULL) return;

	int highestIndex = lastWeapon->GetWeaponIndex();
	
	int prevWeaponIndex = curWeaponIndex;

	do {
		// Try to select the previous weapon item
		prevWeaponIndex--;

		if (prevWeaponIndex < 0) {
			prevWeaponIndex = highestIndex;
		}
	} while (!SelectWeapon(prevWeaponIndex, false) && prevWeaponIndex != curWeaponIndex);
}

/*
===============
idPlayer::SelectWeapon
===============
*/
bool idPlayer::SelectWeapon( int num, bool force )
{
	if ( !weaponEnabled || spectating || gameLocal.inCinematic || health < 0 ) {
		return false;
	}

	if ( gameLocal.isClient ) {
		return false;
	}

	if ( ( num != weapon_pda ) && gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) {
		num = weapon_fists;
		hiddenWeapon ^= 1;
		if ( hiddenWeapon && weapon.GetEntity() ) {
			weapon.GetEntity()->LowerWeapon();
		} else {
			weapon.GetEntity()->RaiseWeapon();
		}
	}	

	if (m_WeaponCursor == NULL) {
		return false;
	}

	// Check if we want to toggle the current weapon item (requested index == current index)
	CInventoryWeaponItemPtr item = GetCurrentWeaponItem();

	if (item != NULL && item->GetWeaponIndex() == num && item->IsToggleable()) {
		// Requested toggleable weapon is already active, hide it (switch to unarmed)
		num = 0;
	}

	CInventoryCategoryPtr category = m_WeaponCursor->GetCurrentCategory();
	if (category == NULL) {
		return false;
	}

	// Cycle through the weapons and find the one with the given weaponIndex
	for (int i = 0; i < category->GetNumItems(); i++)
	{
		// Try to retrieve a weapon item from the given category
		CInventoryWeaponItemPtr item = 
			boost::dynamic_pointer_cast<CInventoryWeaponItem>(category->GetItem(i));
		
		if (item != NULL && item->GetWeaponIndex() == num)
		{
			if (item->GetAmmo() <= 0 && !item->IsAllowedEmpty())
			{
				DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Weapon requires ammo. Cannot select: %d\r", num);
				break;
			}

			if (!item->IsEnabled())
			{
				DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Weapon not enabled, cannot select: %d\r", num);
				break;
			}

			DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Selecting weapon #%d\r", num);

			// Set the cursor onto this item
			m_WeaponCursor->SetCurrentItem(item);

			weaponSwitchTime = gameLocal.time + WEAPON_SWITCH_DELAY;
			idealWeapon = num;

			UpdateHudWeapon();
			return true;
		}
	}

	return false;
}

/*
=================
idPlayer::DropWeapon
=================
*/
void idPlayer::DropWeapon( bool died ) {
	idVec3 forward, up;
	int inclip, ammoavailable;

	assert( !gameLocal.isClient );
	
	if ( spectating || weaponGone || weapon.GetEntity() == NULL ) {
		return;
	}
	
	if ( ( !died && !weapon.GetEntity()->IsReady() ) || weapon.GetEntity()->IsReloading() ) {
		return;
	}
	// ammoavailable is how many shots we can fire
	// inclip is which amount is in clip right now
	ammoavailable = weapon.GetEntity()->AmmoAvailable();
	inclip = weapon.GetEntity()->AmmoInClip();
	
	// don't drop a grenade if we have none left
	/*if ( !idStr::Icmp( idWeapon::GetAmmoNameForNum( weapon.GetEntity()->GetAmmoType() ), "ammo_grenades" ) && ( ammoavailable - inclip <= 0 ) ) {
		return;
	}*/

	// expect an ammo setup that makes sense before doing any dropping
	// ammoavailable is -1 for infinite ammo, and weapons like chainsaw
	// a bad ammo config usually indicates a bad weapon state, so we should not drop
	// used to be an assertion check, but it still happens in edge cases
	if ( ( ammoavailable != -1 ) && ( ammoavailable - inclip < 0 ) ) {
		common->DPrintf( "idPlayer::DropWeapon: bad ammo setup\n" );
		return;
	}
	idEntity *item = NULL;
	if ( died ) {
		// ain't gonna throw you no weapon if I'm dead
		item = weapon.GetEntity()->DropItem( vec3_origin, 0, WEAPON_DROP_TIME, died );
	} else {
		viewAngles.ToVectors( &forward, NULL, &up );
		item = weapon.GetEntity()->DropItem( 250.0f * forward + 150.0f * up, 500, WEAPON_DROP_TIME, died );
	}
	if ( !item ) {
		return;
	}
	// set the appropriate ammo in the dropped object
	const idKeyValue * keyval = item->spawnArgs.MatchPrefix( "inv_ammo_" );
	if ( keyval ) {
		item->spawnArgs.SetInt( keyval->GetKey(), ammoavailable );
		idStr inclipKey = keyval->GetKey();
		inclipKey.Insert( "inclip_", 4 );
		item->spawnArgs.SetInt( inclipKey, inclip );
	}
	if ( !died ) {
		// remove from our local inventory completely
		weapon.GetEntity()->ResetAmmoClip();
		NextWeapon();
		weapon.GetEntity()->WeaponStolen();
		weaponGone = true;
	}
}

/*
=================
idPlayer::StealWeapon
steal the target player's current weapon
=================
*/
void idPlayer::StealWeapon( idPlayer *player )
{
	assert( !gameLocal.isClient );

	// make sure there's something to steal
	idWeapon *player_weapon = static_cast< idWeapon * >( player->weapon.GetEntity() );
	if ( !player_weapon || !player_weapon->CanDrop() || weaponGone ) {
		return;
	}
	// steal - we need to effectively force the other player to abandon his weapon
	int newweap = player->currentWeapon;
	if ( newweap == -1 ) {
		return;
	}

	const char *weapon_classname = spawnArgs.GetString( va( "def_weapon%d", newweap ) );
	assert( weapon_classname );
	int ammoavailable = player->weapon.GetEntity()->AmmoAvailable();
	int inclip = player->weapon.GetEntity()->AmmoInClip();
	if ( ( ammoavailable != -1 ) && ( ammoavailable - inclip < 0 ) ) {
		// see DropWeapon
		common->DPrintf( "idPlayer::StealWeapon: bad ammo setup\n" );
		// we still steal the weapon, so let's use the default ammo levels
		inclip = -1;
		const idDeclEntityDef *decl = gameLocal.FindEntityDef( weapon_classname );
		assert( decl );
		const idKeyValue *keypair = decl->dict.MatchPrefix( "inv_ammo_" );
		assert( keypair );
		ammoavailable = atoi( keypair->GetValue() );
	}

	player->weapon.GetEntity()->WeaponStolen();
	//player->inventory.Drop( player->spawnArgs, NULL, newweap );
	player->SelectWeapon( weapon_fists, false );
	// in case the robbed player is firing rounds with a continuous fire weapon like the chaingun/plasma etc.
	// this will ensure the firing actually stops
	player->weaponGone = true;

	// give weapon, setup the ammo count
	Give( "weapon", weapon_classname );
	idealWeapon = newweap;
}

/*
===============
idPlayer::ActiveGui
===============
*/
idUserInterface *idPlayer::ActiveGui( void ) {
	if ( objectiveSystemOpen )
		return objectiveSystem;

	if ( m_overlays.findInteractive() )
		return m_overlays.findInteractive();

	return NULL; // greebo: No focusUI anymore
	//return focusUI;
}

/*
===============
idPlayer::Weapon_Combat
===============
*/
void idPlayer::Weapon_Combat( void ) {
	if ( influenceActive || !weaponEnabled || gameLocal.inCinematic || privateCameraView ) {
		return;
	}

	weapon.GetEntity()->RaiseWeapon();
	if ( weapon.GetEntity()->IsReloading() ) {
		if ( !AI_RELOAD ) {
			AI_RELOAD = true;
			SetState( "ReloadWeapon" );
			UpdateScript();
		}
	} else {
		AI_RELOAD = false;
	}

	if ( idealWeapon == weapon_soulcube && soulCubeProjectile.GetEntity() != NULL ) {
		idealWeapon = currentWeapon;
	}

	if ( idealWeapon != currentWeapon ) {
		if ( weaponCatchup ) {
			assert( gameLocal.isClient );

			currentWeapon = idealWeapon;
			weaponGone = false;
			animPrefix = spawnArgs.GetString( va( "def_weapon%d", currentWeapon ) );
			weapon.GetEntity()->GetWeaponDef( animPrefix, 0 /*inventory.clip[ currentWeapon ]*/ );
			animPrefix.Strip( "weapon_" );

			weapon.GetEntity()->NetCatchup();
			const function_t *newstate = GetScriptFunction( "NetCatchup" );
			if ( newstate ) {
				SetState( newstate );
				UpdateScript();
			}
			weaponCatchup = false;			
		} else {
			if ( weapon.GetEntity()->IsReady() ) {
				weapon.GetEntity()->PutAway();
			}

			if ( weapon.GetEntity()->IsHolstered() ) {
				assert( idealWeapon >= 0 );
				
				if ( currentWeapon != weapon_pda && !spawnArgs.GetBool( va( "weapon%d_toggle", currentWeapon ) ) ) {
					previousWeapon = currentWeapon;
				}
				currentWeapon = idealWeapon;
				weaponGone = false;
				animPrefix = spawnArgs.GetString( va( "def_weapon%d", currentWeapon ) );
				weapon.GetEntity()->GetWeaponDef( animPrefix, 0/*inventory.clip[ currentWeapon ]*/ );
				animPrefix.Strip( "weapon_" );

				weapon.GetEntity()->Raise();
			}
		}
	} else {
		weaponGone = false;	// if you drop and re-get weap, you may miss the = false above 
		if ( weapon.GetEntity()->IsHolstered() ) {
			if ( !weapon.GetEntity()->AmmoAvailable() )
			{
				// weapons can switch automatically if they have no more ammo
				// ishtvan: Only if the cvar is set
				if (cv_weapon_next_on_empty.GetBool())
				{
					NextBestWeapon();
				}
				else
				{
					// Switch to unarmed if no more ammo available
					SelectWeapon(weapon_fists, false);
				}
			}
			else
			{
				weapon.GetEntity()->Raise();
				state = GetScriptFunction( "RaiseWeapon" );
				if ( state ) {
					SetState( state );
				}
			}
		}
	}

	// check for attack
	AI_WEAPON_FIRED = false;
	if ( !influenceActive ) 
	{
		if ( ( usercmd.buttons & BUTTON_ATTACK ) && !weaponGone ) {
			FireWeapon();
		} else if ( oldButtons & BUTTON_ATTACK ) {
			AI_ATTACK_HELD = false;
			weapon.GetEntity()->EndAttack();
		}
	}

	// check for block
	AI_WEAPON_BLOCKED = false;
	if ( !influenceActive ) 
	{
		if ( ( usercmd.buttons & BUTTON_ZOOM ) && !weaponGone )
			BlockWeapon();
		else if ( oldButtons & BUTTON_ZOOM ) 
		{
			AI_BLOCK_HELD = false;
			weapon.GetEntity()->EndBlock();
		}
	}

	// update our ammo clip in our inventory
	if ( currentWeapon == idealWeapon ) {
		UpdateHudAmmo();
	}
}

/*
===============
idPlayer::Weapon_NPC
===============
*/
#if 0
void idPlayer::Weapon_NPC( void ) {
	if ( idealWeapon != currentWeapon ) {
		Weapon_Combat();
	}
	StopFiring();
	weapon.GetEntity()->LowerWeapon();

	if ( ( usercmd.buttons & BUTTON_ATTACK ) && !( oldButtons & BUTTON_ATTACK ) ) {
		buttonMask |= BUTTON_ATTACK;
		focusCharacter->TalkTo( this );
	}
}
#endif

/*
===============
idPlayer::LowerWeapon
===============
*/
void idPlayer::LowerWeapon( void ) {
	if ( weapon.GetEntity() && !weapon.GetEntity()->IsHidden() ) {
		weapon.GetEntity()->LowerWeapon();
	}
}

/*
===============
idPlayer::RaiseWeapon
===============
*/
void idPlayer::RaiseWeapon( void ) 
{
	if (weapon.GetEntity() && 
		weapon.GetEntity()->IsHidden() && 
		!(GetImmobilization() & EIM_ATTACK)) 
	{
		weapon.GetEntity()->RaiseWeapon();
	}
}

/*
===============
idPlayer::WeaponLoweringCallback
===============
*/
void idPlayer::WeaponLoweringCallback( void ) {
	SetState( "LowerWeapon" );
	UpdateScript();
	UpdateWeaponEncumbrance();
}

/*
===============
idPlayer::WeaponRisingCallback
===============
*/
void idPlayer::WeaponRisingCallback( void ) {
	SetState( "RaiseWeapon" );
	UpdateScript();
	UpdateWeaponEncumbrance();
}

/*
===============
idPlayer::Weapon_GUI
===============
*/
void idPlayer::Weapon_GUI( void ) {

	if ( !objectiveSystemOpen ) {
		if ( idealWeapon != currentWeapon ) {
			Weapon_Combat();
		}
		StopFiring();
		weapon.GetEntity()->LowerWeapon();
	}

	// disable click prediction for the GUIs. handy to check the state sync does the right thing
	if ( gameLocal.isClient && !net_clientPredictGUI.GetBool() ) {
		return;
	}

	if ( ( oldButtons ^ usercmd.buttons ) & BUTTON_ATTACK ) {
		sysEvent_t ev;
		const char *command = NULL;
		bool updateVisuals = false;

		idUserInterface *ui = ActiveGui();
		if ( ui ) {
			ev = sys->GenerateMouseButtonEvent( 1, ( usercmd.buttons & BUTTON_ATTACK ) != 0 );
			command = ui->HandleEvent( &ev, gameLocal.time, &updateVisuals );
#if 0
			if ( updateVisuals && focusGUIent && ui == focusUI ) {
				focusGUIent->UpdateVisuals();
			}
#endif
		}
		if ( gameLocal.isClient ) {
			// we predict enough, but don't want to execute commands
			return;
		}

#if 0
		if ( focusGUIent ) {
			HandleGuiCommands( focusGUIent, command );
		} else {
			HandleGuiCommands( this, command );
		}
#else // greebo: Replaced the above with this, no focusGUIEnt anymore
		HandleGuiCommands( this, command );
#endif
	}
}

/*
===============
idPlayer::UpdateWeapon
===============
*/
void idPlayer::UpdateWeapon( void ) {
	if ( health <= 0 ) {
		return;
	}

	assert( !spectating );

	if ( gameLocal.isClient ) {
		// clients need to wait till the weapon and it's world model entity
		// are present and synchronized ( weapon.worldModel idEntityPtr to idAnimatedEntity )
		if ( !weapon.GetEntity()->IsWorldModelReady() ) {
			return;
		}
	}

	// always make sure the weapon is correctly setup before accessing it
	if ( !weapon.GetEntity()->IsLinked() ) {
		if ( idealWeapon != -1 ) {
			animPrefix = spawnArgs.GetString( va( "def_weapon%d", idealWeapon ) );
			weapon.GetEntity()->GetWeaponDef( animPrefix, 0/*inventory.clip[ idealWeapon ]*/ );
			assert( weapon.GetEntity()->IsLinked() );
		} else {
			return;
		}
	}

	if ( hiddenWeapon && tipUp && usercmd.buttons & BUTTON_ATTACK ) {
		HideTip();
	}

	if ( g_dragEntity.GetBool() ) {
		StopFiring();
		weapon.GetEntity()->LowerWeapon();
		dragEntity.Update( this );
	} else if ( ActiveGui() ) {
		// gui handling overrides weapon use
		Weapon_GUI();
#if 0
	} else 	if ( focusCharacter && ( focusCharacter->health > 0 ) ) {
		Weapon_NPC();
#endif
	} else if( gameLocal.m_Grabber->GetSelected() ) {
		gameLocal.m_Grabber->Update( this, true );
	} else {
		Weapon_Combat();
	}

	if( GetImmobilization() & EIM_ATTACK )
	{
		StopFiring();
		weapon.GetEntity()->LowerWeapon();
	}
	
	if ( hiddenWeapon ) {
		weapon.GetEntity()->LowerWeapon();
	}

	// update weapon state, particles, dlights, etc
	weapon.GetEntity()->PresentWeapon( showWeaponViewModel );
}

void idPlayer::ChangeWeaponProjectile(const idStr& weaponName, const idStr& projectileDefName)
{
	CInventoryWeaponItemPtr weaponItem = GetWeaponItem(weaponName);
	if (weaponItem == NULL) return;

	weaponItem->SetProjectileDefName(projectileDefName);
}

void idPlayer::ResetWeaponProjectile(const idStr& weaponName)
{
	CInventoryWeaponItemPtr weaponItem = GetWeaponItem(weaponName);
	if (weaponItem == NULL) return;

	weaponItem->ResetProjectileDefName();
}

void idPlayer::ChangeWeaponName(const idStr& weaponName, const idStr& displayName)
{
	CInventoryWeaponItemPtr weaponItem = GetWeaponItem(weaponName);
	if (weaponItem == NULL) return;

	if (!displayName.IsEmpty())
	{
		weaponItem->SetName(displayName);
	}
	else
	{
		const idDeclEntityDef* def = gameLocal.FindEntityDef(weaponItem->GetWeaponDefName());

		if (def != NULL)
		{
			// Empty name passed, reset to definition
			weaponItem->SetName(def->dict.GetString("inv_name"));
		}
	}

	UpdateHudWeapon();
}

void idPlayer::SetIsPushing(bool isPushing)
{
	this->isPushing = isPushing;

	// Raise/lower the weapons according to our push state
	SetImmobilization("pushing", isPushing ? EIM_ATTACK : 0);
}

bool idPlayer::IsPushing()
{
	return isPushing;
}

void idPlayer::OnStartShoulderingBody(idEntity* body)
{
	// Check if the other body is at least 30 kg heavier than us
	bool isHeavy = body->GetPhysics()->GetMass() > GetPhysics()->GetMass() + 30;

	// play the sound on the player, not the body (that was creating inconsistent volume)
	StartSound( isHeavy ? "snd_shoulder_body_heavy" : "snd_shoulder_body", SND_CHANNEL_ITEM, 0, false, NULL );

	// set immobilizations
	int immob = SHOULDER_IMMOBILIZATIONS;

	// TODO: Also make sure you can't grab anything else (hands are full)
	// requires a new EIM flag?
	SetImmobilization( "ShoulderedBody", SHOULDER_IMMOBILIZATIONS );
	
	// set hinderance
	float maxSpeed = body->spawnArgs.GetFloat("shouldered_maxspeed","1.0f");
	SetHinderance( "ShoulderedBody", 1.0f, maxSpeed );
	SetJumpHinderance( "ShoulderedBody", 1.0f, SHOULDER_JUMP_HINDERANCE );

	// greebo: Determine which icon to display on the HUD
	idStr iconName;

	if( body->health > 0 )
	{
		iconName = body->spawnArgs.GetString("shouldered_name", "Body");
	}
	else
	{
		iconName = body->spawnArgs.GetString("shouldered_name_dead", "Corpse");
	}

	// Send the name to the inventory HUD
	SetGuiString(m_InventoryOverlay, "GrabbedItemName", iconName);

	// Notify all GUIs about the event
	m_overlays.broadcastNamedEvent("OnStartShoulderingBody");

	// Clear the inventory cursor
	SelectInventoryItem("");

	m_bShoulderingBody = true;
}

void idPlayer::OnStopShoulderingBody(idEntity* body)
{
	// clear immobilizations
	SetImmobilization( "ShoulderedBody", 0 );
	SetHinderance( "ShoulderedBody", 1.0f, 1.0f );
	SetJumpHinderance( "ShoulderedBody", 1.0f, 1.0f );

	// same sound for unshouldering as shouldering
	StartSound( "snd_shoulder_body", SND_CHANNEL_ITEM, 0, false, NULL );

	m_overlays.broadcastNamedEvent("OnStopShoulderingBody");

	// Send the name to the inventory HUD
	SetGuiString(m_InventoryOverlay, "GrabbedItemName", "");

	m_bShoulderingBody = false;
}

/*
===============
idPlayer::SpectateFreeFly
===============
*/
void idPlayer::SpectateFreeFly( bool force ) {
	idPlayer	*player;
	idVec3		newOrig;
	idVec3		spawn_origin;
	idAngles	spawn_angles;

	player = gameLocal.GetClientByNum( spectator );
	if ( force || gameLocal.time > lastSpectateChange ) {
		spectator = entityNumber;
		if ( player && player != this && !player->spectating && !player->IsInTeleport() ) {
			newOrig = player->GetPhysics()->GetOrigin();
			if ( player->physicsObj.IsCrouching() ) {
				newOrig[ 2 ] += pm_crouchviewheight.GetFloat();
			} else {
				newOrig[ 2 ] += pm_normalviewheight.GetFloat();
			}
			newOrig[ 2 ] += SPECTATE_RAISE;
			idBounds b = idBounds( vec3_origin ).Expand( pm_spectatebbox.GetFloat() * 0.5f );
			idVec3 start = player->GetPhysics()->GetOrigin();
			start[2] += pm_spectatebbox.GetFloat() * 0.5f;
			trace_t t;
			// assuming spectate bbox is inside stand or crouch box
			gameLocal.clip.TraceBounds( t, start, newOrig, b, MASK_PLAYERSOLID, player );
			newOrig.Lerp( start, newOrig, t.fraction );
			SetOrigin( newOrig );
			idAngles angle = player->viewAngles;
			angle[ 2 ] = 0;
			SetViewAngles( angle );
		} else {	
			SelectInitialSpawnPoint( spawn_origin, spawn_angles );
			spawn_origin[ 2 ] += pm_normalviewheight.GetFloat();
			spawn_origin[ 2 ] += SPECTATE_RAISE;
			SetOrigin( spawn_origin );
			SetViewAngles( spawn_angles );
		}
		lastSpectateChange = gameLocal.time + 500;
	}
}

/*
===============
idPlayer::SpectateCycle
===============
*/
void idPlayer::SpectateCycle( void ) {
	idPlayer *player;

	if ( gameLocal.time > lastSpectateChange ) {
		int latchedSpectator = spectator;
		spectator = gameLocal.GetNextClientNum( spectator );
		player = gameLocal.GetClientByNum( spectator );
		assert( player ); // never call here when the current spectator is wrong
		// ignore other spectators
		while ( latchedSpectator != spectator && player->spectating ) {
			spectator = gameLocal.GetNextClientNum( spectator );
			player = gameLocal.GetClientByNum( spectator );
		}
		lastSpectateChange = gameLocal.time + 500;
	}
}

/*
===============
idPlayer::UpdateSpectating
===============
*/
void idPlayer::UpdateSpectating( void ) {
	assert( spectating );
	assert( !gameLocal.isClient );
	assert( IsHidden() );
	idPlayer *player;
	if ( !gameLocal.isMultiplayer ) {
		return;
	}
	player = gameLocal.GetClientByNum( spectator );
	if ( !player || ( player->spectating && player != this ) ) {
		SpectateFreeFly( true );
	} else if ( usercmd.upmove > 0 ) {
		SpectateFreeFly( false );
	} else if ( usercmd.buttons & BUTTON_ATTACK ) {
		SpectateCycle();
	}
}

/*
===============
idPlayer::HandleSingleGuiCommand
===============
*/
bool idPlayer::HandleSingleGuiCommand( idEntity *entityGui, idLexer *src ) {
	idToken token;

	if ( !src->ReadToken( &token ) ) {
		return false;
	}

	if ( token == ";" ) {
		return false;
	}

	if ( token.Icmp( "addhealth" ) == 0 ) {
		if ( entityGui && health < 100 ) {
			int _health = entityGui->spawnArgs.GetInt( "gui_parm1" );
			int amt = ( _health >= HEALTH_PER_DOSE ) ? HEALTH_PER_DOSE : _health;
			_health -= amt;
			entityGui->spawnArgs.SetInt( "gui_parm1", _health );
			if ( entityGui->GetRenderEntity() && entityGui->GetRenderEntity()->gui[ 0 ] ) {
				entityGui->GetRenderEntity()->gui[ 0 ]->SetStateInt( "gui_parm1", _health );
			}
			health += amt;
			if ( health > 100 ) {
				health = 100;
			}
		}
		return true;
	}

	if ( token.Icmp( "ready" ) == 0 ) {
		PerformImpulse( IMPULSE_17 );
		return true;
	}

	if ( token.Icmp( "updatepda" ) == 0 ) {
		UpdatePDAInfo( true );
		return true;
	}

	if ( token.Icmp( "updatepda2" ) == 0 ) {
		UpdatePDAInfo( false );
		return true;
	}

	if ( token.Icmp( "stoppdavideo" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaVideoWave.Length() > 0 ) {
			StopSound( SND_CHANNEL_PDA, false );
		}
		return true;
	}

	if ( token.Icmp( "close" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen ) {
			TogglePDA();
		}
	}

	if ( token.Icmp( "playpdavideo" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaVideo.Length() > 0 ) {
			const idMaterial *mat = declManager->FindMaterial( pdaVideo );
			if ( mat ) {
				int c = mat->GetNumStages();
				for ( int i = 0; i < c; i++ ) {
					const shaderStage_t *stage = mat->GetStage(i);
					if ( stage && stage->texture.cinematic ) {
						stage->texture.cinematic->ResetTime( gameLocal.time );
					}
				}
				if ( pdaVideoWave.Length() ) {
					const idSoundShader *shader = declManager->FindSound( pdaVideoWave );
					StartSoundShader( shader, SND_CHANNEL_PDA, 0, false, NULL );
				}
			}
		}
	}

	if ( token.Icmp( "playpdaaudio" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaAudio.Length() > 0 ) {
			const idSoundShader *shader = declManager->FindSound( pdaAudio );
			int ms;
			StartSoundShader( shader, SND_CHANNEL_PDA, 0, false, &ms );
			StartAudioLog();
			CancelEvents( &EV_Player_StopAudioLog );
			PostEventMS( &EV_Player_StopAudioLog, ms + 150 );
		}
		return true;
	}

	if ( token.Icmp( "stoppdaaudio" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaAudio.Length() > 0 ) {
			// idSoundShader *shader = declManager->FindSound( pdaAudio );
			StopAudioLog();
			StopSound( SND_CHANNEL_PDA, false );
		}
		return true;
	}

	src->UnreadToken( &token );
	return false;
}

/*
==============
idPlayer::Collide
==============
*/

bool idPlayer::Collide( const trace_t &collision, const idVec3 &velocity ) {
	if( collision.fraction == 1.0f || collision.c.type == CONTACT_NONE // didnt hit anything
		|| gameLocal.isClient // server computes
		)
	{
		return false;
	}
	idEntity *other = gameLocal.entities[ collision.c.entityNum ];
	// don't let player collide with grabber entity
	if ( other && other != gameLocal.m_Grabber->GetSelected() ) 
	{
		ProcCollisionStims( other, collision.c.id );

		other->Signal( SIG_TOUCH );
		if ( !spectating ) {
			if ( other->RespondsTo( EV_Touch ) ) {
				other->ProcessEvent( &EV_Touch, this, &collision );
			}
		} else {
			if ( other->RespondsTo( EV_SpectatorTouch ) ) {
				other->ProcessEvent( &EV_SpectatorTouch, this, &collision );
			}
		}
	}
	return false;
}

/*
================
idPlayer::UpdateLocation

Searches nearby locations 
================
*/
void idPlayer::UpdateLocation( void ) {
	if ( hud ) {
		idLocationEntity *locationEntity = gameLocal.LocationForPoint( GetEyePosition() );
		if ( locationEntity ) {
			hud->SetStateString( "location", locationEntity->GetLocation() );
		} else {
			hud->SetStateString( "location", common->GetLanguageDict()->GetString( "#str_02911" ) );
		}
	}
}

/*
================
idPlayer::GetLocation
================
*/
idLocationEntity *idPlayer::GetLocation( void )
{
	return gameLocal.LocationForPoint( GetEyePosition() );
}

/*
================
idPlayer::ClearFocus

Clears the focus cursor
================
*/
#if 0
void idPlayer::ClearFocus( void ) {
	focusCharacter	= NULL;
	focusGUIent		= NULL;
	focusUI			= NULL;
	focusVehicle	= NULL;
	talkCursor		= 0;
}
#endif

/*
================
idPlayer::UpdateFocus

Searches nearby entities for interactive guis, possibly making one of them
the focus and sending it a mouse move event
================
*/
#if 0
void idPlayer::UpdateFocus( void ) {
	idClipModel *clipModelList[ MAX_GENTITIES ];
	idClipModel *clip;
	int			listedClipModels;
	idEntity	*oldFocus;
	idEntity	*ent;
	idUserInterface *oldUI;
	idAI		*oldChar;
	int			oldTalkCursor;
	idAFEntity_Vehicle *oldVehicle;
	int			i;
	idVec3		start, end;
	bool		allowFocus;
	const char *command;
	trace_t		trace;
	guiPoint_t	pt;
	sysEvent_t	ev;
	idUserInterface *ui;

	if ( gameLocal.inCinematic ) {
		return;
	}

	// only update the focus character when attack button isn't pressed so players
	// can still chainsaw NPC's
	if ( gameLocal.isMultiplayer || ( !focusCharacter && ( usercmd.buttons & BUTTON_ATTACK ) ) ) {
		allowFocus = false;
	} else {
		allowFocus = true;
	}

	oldFocus		= focusGUIent;
	oldUI			= focusUI;
	oldChar			= focusCharacter;
	oldTalkCursor	= talkCursor;
	oldVehicle		= focusVehicle;

	if ( focusTime <= gameLocal.time ) {
		ClearFocus();
	}

	// don't let spectators interact with GUIs
	if ( spectating ) {
		return;
	}

	start = GetEyePosition();
	end = start + viewAngles.ToForward() * 80.0f;

	// player identification -> names to the hud
	if ( gameLocal.isMultiplayer && entityNumber == gameLocal.localClientNum ) {
		idVec3 end = start + viewAngles.ToForward() * 768.0f;
		gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_BOUNDINGBOX, this );
		int iclient = -1;
		if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum < MAX_CLIENTS ) ) {
			iclient = trace.c.entityNum;
		}
		if ( MPAim != iclient ) {
			lastMPAim = MPAim;
			MPAim = iclient;
			lastMPAimTime = gameLocal.realClientTime;
		}
	}

	idBounds bounds( start );
	bounds.AddPoint( end );

	listedClipModels = gameLocal.clip.ClipModelsTouchingBounds( bounds, -1, clipModelList, MAX_GENTITIES );

	// no pretense at sorting here, just assume that there will only be one active
	// gui within range along the trace
	for ( i = 0; i < listedClipModels; i++ ) {
		clip = clipModelList[ i ];
		ent = clip->GetEntity();

		if ( ent->IsHidden() ) {
			continue;
		}

		if ( allowFocus ) {
			if ( ent->IsType( idAFAttachment::Type ) ) {
				idEntity *body = static_cast<idAFAttachment *>( ent )->GetBody();
				if ( body && body->IsType( idAI::Type ) && ( static_cast<idAI *>( body )->GetTalkState() >= TALK_OK ) ) {
					gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
					if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
						ClearFocus();
						focusCharacter = static_cast<idAI *>( body );
						talkCursor = 1;
						focusTime = gameLocal.time + FOCUS_TIME;
						break;
					}
				}
				continue;
			}

			if ( ent->IsType( idAI::Type ) ) {
				if ( static_cast<idAI *>( ent )->GetTalkState() >= TALK_OK ) {
					gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
					if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
						ClearFocus();
						focusCharacter = static_cast<idAI *>( ent );
						talkCursor = 1;
						focusTime = gameLocal.time + FOCUS_TIME;
						break;
					}
				}
				continue;
			}

			if ( ent->IsType( idAFEntity_Vehicle::Type ) ) {
				gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
				if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
					ClearFocus();
					focusVehicle = static_cast<idAFEntity_Vehicle *>( ent );
					focusTime = gameLocal.time + FOCUS_TIME;
					break;
				}
				continue;
			}
		}

		if ( !ent->GetRenderEntity() || !ent->GetRenderEntity()->gui[ 0 ] || !ent->GetRenderEntity()->gui[ 0 ]->IsInteractive() ) {
			continue;
		}

		if ( ent->spawnArgs.GetBool( "inv_item" ) ) {
			// don't allow guis on pickup items focus
			continue;
		}

		pt = gameRenderWorld->GuiTrace( ent->GetModelDefHandle(), start, end );
		if ( pt.x != -1 ) {
			// we have a hit
			renderEntity_t *focusGUIrenderEntity = ent->GetRenderEntity();
			if ( !focusGUIrenderEntity ) {
				continue;
			}

			if ( pt.guiId == 1 ) {
				ui = focusGUIrenderEntity->gui[ 0 ];
			} else if ( pt.guiId == 2 ) {
				ui = focusGUIrenderEntity->gui[ 1 ];
			} else {
				ui = focusGUIrenderEntity->gui[ 2 ];
			}
			
			if ( ui == NULL ) {
				continue;
			}

			ClearFocus();
			focusGUIent = ent;
			focusUI = ui;

			// clamp the mouse to the corner
			ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
			command = focusUI->HandleEvent( &ev, gameLocal.time );
 			HandleGuiCommands( focusGUIent, command );

			// move to an absolute position
			ev = sys->GenerateMouseMoveEvent( static_cast<int>(pt.x) * SCREEN_WIDTH, static_cast<int>(pt.y) * SCREEN_HEIGHT );
			command = focusUI->HandleEvent( &ev, gameLocal.time );
			HandleGuiCommands( focusGUIent, command );
			focusTime = gameLocal.time + FOCUS_GUI_TIME;
			break;
		}
	}

	if ( focusGUIent && focusUI ) {
		if ( !oldFocus || oldFocus != focusGUIent ) {
			command = focusUI->Activate( true, gameLocal.time );
			HandleGuiCommands( focusGUIent, command );
			StartSound( "snd_guienter", SND_CHANNEL_ANY, 0, false, NULL );
			// HideTip();
			// HideObjective();
		}
	} else if ( oldFocus && oldUI ) {
		command = oldUI->Activate( false, gameLocal.time );
		HandleGuiCommands( oldFocus, command );
		StartSound( "snd_guiexit", SND_CHANNEL_ANY, 0, false, NULL );
	}

	if ( cursor && ( oldTalkCursor != talkCursor ) ) {
		cursor->SetStateInt( "talkcursor", talkCursor );
	}

	if ( oldChar != focusCharacter && hud ) {
		if ( focusCharacter ) {
			hud->SetStateString( "npc", focusCharacter->spawnArgs.GetString( "npc_name", "Joe" ) );
			hud->HandleNamedEvent( "showNPC" );
			// HideTip();
			// HideObjective();
		} else {
			hud->SetStateString( "npc", "" );
			hud->HandleNamedEvent( "hideNPC" );
		}
	}
}
#endif

/*
=================
idPlayer::CrashLand

Check for hard landings that generate sound events
=================
*/

void idPlayer::CrashLand( const idVec3 &savedOrigin, const idVec3 &savedVelocity ) {
	
	AI_SOFTLANDING = false;
	AI_HARDLANDING = false;

	CrashLandResult result = idActor::CrashLand( physicsObj, savedOrigin, savedVelocity );

	if (result.hasLanded && 
		( (!AI_CROUCH && savedVelocity.z < -300) || savedVelocity.z < -600) )
	{
		hasLanded = true;

		PlayFootStepSound();
	}
	else
	{
		hasLanded = false;
	}
	
	
	if (health < 0)
	{
		// This was a deadly fall
		AI_HARDLANDING = true;
		landChange = -32;
		landTime = gameLocal.time;
	}
	else if (result.damageDealt >= m_damage_thresh_hard)
	{
		AI_HARDLANDING = true;
		landChange = -24;
		landTime = gameLocal.time;
	}
	else if (result.hasLanded || result.damageDealt >= m_damage_thresh_min)
	{
		AI_SOFTLANDING = true;
		landChange	= -8;
		landTime = gameLocal.time;
	}

	// otherwise, just walk on
}

/*
===============
idPlayer::BobCycle
===============
*/
void idPlayer::BobCycle( const idVec3 &pushVelocity ) {
	float		bobmove;
	int			old, deltaTime;
	idVec3		vel, gravityDir, velocity;
	idMat3		viewaxis;
	float		bob;
	float		delta;
	float		speed;
	float		f;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	velocity = physicsObj.GetLinearVelocity() - pushVelocity;

	gravityDir = physicsObj.GetGravityNormal();
	vel = velocity - ( velocity * gravityDir ) * gravityDir;
	xyspeed = vel.LengthFast();

	// do not evaluate the bob for other clients
	// when doing a spectate follow, don't do any weapon bobbing
	if ( gameLocal.isClient && entityNumber != gameLocal.localClientNum ) {
		viewBobAngles.Zero();
		viewBob.Zero();
		return;
	}

	if ( !physicsObj.HasGroundContacts() || influenceActive == INFLUENCE_LEVEL2 || ( gameLocal.isMultiplayer && spectating ) ) {
		// airborne
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} 
	else if (physicsObj.GetWaterLevel() >= WATERLEVEL_HEAD || noclip)
	{
		// No viewbob when fully underwater or in noclip mode, start at beginning of cycle again
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	}
	else if ( ( !usercmd.forwardmove && !usercmd.rightmove ) || ( xyspeed <= MIN_BOB_SPEED ) ) {
		// Play a footstep sound when we stop walking (the foot is lowered to the ground;
		// also this prevents exploits)
		if (bobCycle != 0) {
			PlayFootStepSound();
		}
		// start at beginning of cycle again
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} 
	else {
		if ( physicsObj.IsCrouching() ) {
			// greebo: Double the crouchbob speed when fully running
			bobmove = pm_crouchbob.GetFloat() * (1 + bobFrac);
		} 
		else 
		{
			// vary the bobbing based on the speed of the player
			bobmove = pm_walkbob.GetFloat() * ( 1.0f - bobFrac ) + pm_runbob.GetFloat() * bobFrac;
		}

		// greebo: is the player creeping? (Only kicks in when not running, run key cancels out creep key)
		if (usercmd.buttons & BUTTON_5 && !(usercmd.buttons & BUTTON_RUN)) 
		{
			bobmove *= 0.5f * (1 - bobFrac);
		}
		
		// additional explanatory comments added by Crispy
		
		old = bobCycle;
		
		// bobCycle is effectively an 8-bit integer, which increases at a speed determined by bobmove
		// and wraps around when it exceeds 8 bits.
		bobCycle = (int)( old + bobmove * gameLocal.msec ) & 255;
		
		// bobFoot = most significant bit of bobCycle, so it will be equal to 1 for half the time,
		// and 0 for the other half. This represents which foot we're placing our weight on right now.
		bobFoot = ( bobCycle & 128 ) >> 7;
		
		// Take the other 7 bits of bobCycle, scale them to range from 0 to PI, and take the sine.
		// The result produces positive values only, from within the first "hump" of the function.
		// (Look at the graph of sin(x) with x = 0...PI)
		bobfracsin = idMath::Fabs( sin( ( bobCycle & 127 ) / 127.0 * idMath::PI ) );
		
		// Crispy: Play footstep sounds when we hit the bottom of the cycle (i.e. when bobFoot changes)
		if ((old&128) != (bobCycle&128)) {
			// We've changed feet, so play a footstep
			PlayFootStepSound();
		}
	}

	// calculate angles for view bobbing
	viewBobAngles.Zero();

	viewaxis = viewAngles.ToMat3() * physicsObj.GetGravityAxis();

	// add angles based on velocity
	delta = velocity * viewaxis[0];
	viewBobAngles.pitch += delta * pm_runpitch.GetFloat();
	
	delta = velocity * viewaxis[1];
	viewBobAngles.roll -= delta * pm_runroll.GetFloat();

	// add angles based on bob
	// make sure the bob is visible even at low speeds
	speed = xyspeed > 200 ? xyspeed : 200;

	delta = bobfracsin * pm_bobpitch.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching
	}
	viewBobAngles.pitch += delta;
	delta = bobfracsin * pm_bobroll.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching accentuates roll
	}
	if ( bobFoot & 1 ) {
		delta = -delta;
	}
	viewBobAngles.roll += delta;

	// calculate position for view bobbing
	viewBob.Zero();

	if ( physicsObj.HasSteppedUp() ) {

		// check for stepping up before a previous step is completed
		deltaTime = gameLocal.time - stepUpTime;
		if ( deltaTime < STEPUP_TIME ) {
			stepUpDelta = stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME + physicsObj.GetStepUp();
		} else {
			stepUpDelta = physicsObj.GetStepUp();
		}
		if ( stepUpDelta > 2.0f * pm_stepsize.GetFloat() ) {
			stepUpDelta = 2.0f * pm_stepsize.GetFloat();
		}
		stepUpTime = gameLocal.time;
	}

	idVec3 gravity = physicsObj.GetGravityNormal();

	// if the player stepped up recently
	deltaTime = gameLocal.time - stepUpTime;
	if ( deltaTime < STEPUP_TIME ) {
		viewBob += gravity * ( stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME );
	}

	// add bob height after any movement smoothing
	bob = bobfracsin * xyspeed * pm_bobup.GetFloat();
	if ( bob > 6 ) {
		bob = 6;
	}
	viewBob[2] += bob;

	// add fall height
	delta = gameLocal.time - landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		f = delta / LAND_DEFLECT_TIME;
		viewBob -= gravity * ( landChange * f );
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		delta -= LAND_DEFLECT_TIME;
		f = 1.0 - ( delta / LAND_RETURN_TIME );
		viewBob -= gravity * ( landChange * f );
	}
}

/*
================
idPlayer::UpdateDeltaViewAngles
================
*/
void idPlayer::UpdateDeltaViewAngles( const idAngles &angles ) {
	// set the delta angle
	idAngles delta;
	for( int i = 0; i < 3; i++ ) {
		delta[ i ] = angles[ i ] - SHORT2ANGLE( usercmd.angles[ i ] );
	}

	SetDeltaViewAngles( delta );
}

/*
================
idPlayer::SetViewAngles
================
*/
void idPlayer::SetViewAngles( const idAngles &angles ) {
	UpdateDeltaViewAngles( angles );
	viewAngles = angles;
}

/*
================
idPlayer::UpdateViewAngles
================
*/
void idPlayer::UpdateViewAngles( void ) 
{
	int i;
	idAngles delta;
	idAngles TestAngles = viewAngles;

	if ( !noclip && ( gameLocal.inCinematic || privateCameraView || gameLocal.GetCamera() || influenceActive == INFLUENCE_LEVEL2 || objectiveSystemOpen || GetImmobilization() & EIM_VIEW_ANGLE ) ) 
	{
		// no view changes at all, but we still want to update the deltas or else when
		// we get out of this mode, our view will snap to a kind of random angle
		UpdateDeltaViewAngles( viewAngles );
		goto Quit;
	}

	// if dead
	if ( health <= 0 ) {
		if ( pm_thirdPersonDeath.GetBool() ) {
			viewAngles.roll = 0.0f;
			viewAngles.pitch = 30.0f;
		} else {
			viewAngles.roll = 40.0f;
			viewAngles.pitch = -15.0f;
		}
		goto Quit;
	}

	// circularly clamp the angles with deltas
	for ( i = 0; i < 3; i++ ) 
	{
		cmdAngles[i] = SHORT2ANGLE( usercmd.angles[i] );
		if ( influenceActive == INFLUENCE_LEVEL3 ) 
		{
			TestAngles[i] += idMath::ClampFloat( -1.0f, 1.0f, idMath::AngleDelta( idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] ) , viewAngles[i] ) );
		} 
		else if( GetTurnHinderance() != 1.0f )
		{
			TestAngles[i] += GetTurnHinderance() * idMath::AngleDelta( idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] ) , viewAngles[i] );
		}
		else
		{
			TestAngles[i] = idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] );
		}
	}
	if ( !centerView.IsDone( gameLocal.time ) ) {
		TestAngles.pitch = centerView.GetCurrentValue(gameLocal.time);
	}

	// clamp the pitch
	if ( noclip ) {
		if ( TestAngles.pitch > 89.0f ) {
			// don't let the player look down more than 89 degrees while noclipping
			TestAngles.pitch = 89.0f;
		} else if ( TestAngles.pitch < -89.0f ) {
			// don't let the player look up more than 89 degrees while noclipping
			TestAngles.pitch = -89.0f;
		}
	} else {
		if ( TestAngles.pitch > pm_maxviewpitch.GetFloat() ) {
			// don't let the player look down enough to see the shadow of his (non-existant) feet
			TestAngles.pitch = pm_maxviewpitch.GetFloat();
		} else if ( TestAngles.pitch < pm_minviewpitch.GetFloat() ) {
			// don't let the player look up more than 89 degrees
			TestAngles.pitch = pm_minviewpitch.GetFloat();
		}
	}

	// TDM: Check for collisions due to delta yaw when leaning, overwrite test angles to avoid
	physicsObj.UpdateLeanedInputYaw( TestAngles );
	viewAngles = TestAngles;

	UpdateDeltaViewAngles( viewAngles );

	// orient the model towards the direction we're looking
	// LeanMod: SophisticatedZombie: Added roll to this
	SetAngles( idAngles( 0.0f, viewAngles.yaw, viewAngles.roll ) );

	// save in the log for analyzing weapon angle offsets
	loggedViewAngles[ gameLocal.framenum & (NUM_LOGGED_VIEW_ANGLES-1) ] = viewAngles;

Quit:
	return;
}

/*
==============
idPlayer::AdjustHeartRate

Player heartrate works as follows

DEF_HEARTRATE is resting heartrate

Taking damage when health is above 75 adjusts heart rate by 1 beat per second
Taking damage when health is below 75 adjusts heart rate by 5 beats per second
Maximum heartrate from damage is MAX_HEARTRATE

Firing a weapon adds 1 beat per second up to a maximum of COMBAT_HEARTRATE

Being at less than 25% stamina adds 5 beats per second up to ZEROSTAMINA_HEARTRATE

All heartrates are target rates.. the heart rate will start falling as soon as there have been no adjustments for 5 seconds
Once it starts falling it always tries to get to DEF_HEARTRATE

The exception to the above rule is upon death at which point the rate is set to DYING_HEARTRATE and starts falling 
immediately to zero

Heart rate volumes go from zero ( -40 db for DEF_HEARTRATE to 5 db for MAX_HEARTRATE ) the volume is 
scaled linearly based on the actual rate

Exception to the above rule is once the player is dead, the dying heart rate starts at either the current volume if
it is audible or -10db and scales to 8db on the last few beats
==============
*/
void idPlayer::AdjustHeartRate( int target, float timeInSecs, float delay, bool force ) {

	if ( heartInfo.GetEndValue() == target ) {
		return;
	}

	if ( AI_DEAD && !force ) {
		return;
	}

    lastHeartAdjust = gameLocal.time;

	heartInfo.Init( gameLocal.time + delay * 1000, timeInSecs * 1000, heartRate, target );
}

/*
==============
idPlayer::GetBaseHeartRate
==============
*/
int idPlayer::GetBaseHeartRate( void ) {
	/*
	int base = idMath::FtoiFast( ( BASE_HEARTRATE + LOWHEALTH_HEARTRATE_ADJ ) - ( (float)health / 100.0f ) * LOWHEALTH_HEARTRATE_ADJ );
	int rate = idMath::FtoiFast( base + ( ZEROSTAMINA_HEARTRATE - base ) * ( 1.0f - stamina / pm_stamina.GetFloat() ) );
	int diff = ( lastDmgTime ) ? gameLocal.time - lastDmgTime : 99999;
	rate += ( diff < 5000 ) ? ( diff < 2500 ) ? ( diff < 1000 ) ? 15 : 10 : 5 : 0;
	return rate;
	*/
	return BASE_HEARTRATE;
}

/*
==============
idPlayer::SetCurrentHeartRate
==============
*/
void idPlayer::SetCurrentHeartRate( void ) {
	/// reasons why we should exit
	if( false == airless && health > 0 )
    {
		if( true == m_HeartBeatAllow )
		{
			AdjustHeartRate( BASE_HEARTRATE, 5.5f, 0.0f, false );/// We were allowing so fade it
		}
		else /// We were NOT allowing it so set to default
		{
			heartRate = BASE_HEARTRATE;
		}
		m_HeartBeatAllow = false;
        return;
    }
	/*
	if( false == m_HeartBeatAllow )/// we did not want heartbeat heard so make sure
    {
		heartInfo.Init( gameLocal.time, 0, BASE_HEARTRATE, BASE_HEARTRATE );
		heartRate = BASE_HEARTRATE;
		StopSound( SND_CHANNEL_HEART, false );
    }
	*/
	m_HeartBeatAllow = true;


	int base = idMath::FtoiFast( ( BASE_HEARTRATE + LOWHEALTH_HEARTRATE_ADJ ) - ( (float) health / 100.0f ) * LOWHEALTH_HEARTRATE_ADJ );

	/// removed adrenaline affect - Rich
	heartRate = idMath::FtoiFast( heartInfo.GetCurrentValue( gameLocal.time ) );
	int currentRate = GetBaseHeartRate();
	if ( health >= 0 && gameLocal.time > lastHeartAdjust + 2500 ) {
		AdjustHeartRate( currentRate, 2.5f, 0.0f, false );
	}
	int bps = idMath::FtoiFast( 60.0f / heartRate * 1000.0f );
	if ( gameLocal.time - lastHeartBeat > bps ) {
		int dmgVol = DMG_VOLUME;
		int deathVol = DEATH_VOLUME;
		int zeroVol = ZERO_VOLUME;
		float pct = 0.0;
		if ( heartRate > BASE_HEARTRATE && health > 0 ) {
			pct = (float)(heartRate - base) / (MAX_HEARTRATE - base);
			pct *= ((float)dmgVol - (float)zeroVol);
		} else if ( health <= 0 ) {
			pct = (float)(heartRate - DYING_HEARTRATE) / (BASE_HEARTRATE - DYING_HEARTRATE);
			if ( pct > 1.0f ) {
				pct = 1.0f;
			} else if (pct < 0.0f) {
				pct = 0.0f;
			}
			pct *= ((float)deathVol - (float)zeroVol);
		} 

		pct += (float)zeroVol;

		if ( pct != zeroVol ) {
			StartSound( "snd_heartbeat", SND_CHANNEL_HEART, SSF_PRIVATE_SOUND, false, NULL );
			// modify just this channel to a custom volume
			soundShaderParms_t	parms;
			memset( &parms, 0, sizeof( parms ) );
			parms.volume = pct;
			refSound.referenceSound->ModifySound( SND_CHANNEL_HEART, &parms );
		}

		lastHeartBeat = gameLocal.time;
	}
}

/*
==============
idPlayer::UpdateAir
==============
*/
void idPlayer::UpdateAir( void ) {	
	if ( health <= 0 ) {
		return;
	}

	// see if the player is connected to the info_vacuum
	bool	newAirless = false;

	if ( gameLocal.vacuumAreaNum != -1 ) {
		int	num = GetNumPVSAreas();
		if ( num > 0 ) {
			int		areaNum;

			// if the player box spans multiple areas, get the area from the origin point instead,
			// otherwise a rotating player box may poke into an outside area
			if ( num == 1 ) {
				const int	*pvsAreas = GetPVSAreas();
				areaNum = pvsAreas[0];
			} else {
				areaNum = gameRenderWorld->PointInArea( this->GetPhysics()->GetOrigin() );
			}
			newAirless = gameRenderWorld->AreasAreConnected( gameLocal.vacuumAreaNum, areaNum, PS_BLOCK_AIR );
		}
	}

#ifdef MOD_WATERPHYSICS // check if the player is in water

	idPhysics* phys = GetPhysics();
	
	if (phys != NULL && phys->IsType(idPhysics_Actor::Type) && 
		static_cast<idPhysics_Actor*>(phys)->GetWaterLevel() >= WATERLEVEL_HEAD )
	{
		newAirless = true;	// MOD_WATERPHYSICS
	}

#endif		// MOD_WATERPHYSICS


	if ( newAirless ) {
		if ( !airless ) {
			StartSound( "snd_decompress", SND_CHANNEL_ANY, SSF_GLOBAL, false, NULL );
			StartSound( "snd_noAir", SND_CHANNEL_BODY2, 0, false, NULL );
			if ( hud ) {
				hud->HandleNamedEvent( "noAir" );
			}
		}
		airTics--;
		if ( airTics < 0 ) {
			airTics = 0;
			// check for damage
			const idDict *damageDef = gameLocal.FindEntityDefDict( "damage_noair", false );
			int dmgTiming = 1000 * (damageDef ? static_cast<int>(damageDef->GetFloat( "delay", "3.0" )) : 3 );
			if ( gameLocal.time > lastAirDamage + dmgTiming ) {
				Damage( NULL, NULL, vec3_origin, "damage_noair", 1.0f, 0 );
				lastAirDamage = gameLocal.time;
			}
		}
		
	} else {
		if ( airless ) {
			StartSound( "snd_recompress", SND_CHANNEL_ANY, SSF_GLOBAL, false, NULL );
			StopSound( SND_CHANNEL_BODY2, false );
			if ( hud ) {
				hud->HandleNamedEvent( "Air" );
			}
		}
		airTics+=2;	// regain twice as fast as lose
		if ( airTics > pm_airTics.GetInteger() ) {
			airTics = pm_airTics.GetInteger();
		}
	}

	airless = newAirless;

	if ( hud ) {
		hud->SetStateInt( "player_air", 100 * airTics / pm_airTics.GetInteger() );
	}
}

int	idPlayer::getAirTicks() const {
	return airTics;
}

void idPlayer::setAirTicks(int airTicks) {
	airTics = airTicks;
	// Clamp to maximum value
	if( airTics > pm_airTics.GetInteger() ) {
		airTics = pm_airTics.GetInteger();
	}
}

/*
==============
idPlayer::AddGuiPDAData
==============
 */
int idPlayer::AddGuiPDAData( const declType_t dataType, const char *listName, const idDeclPDA *src, idUserInterface *gui ) {
#if 0
	int c, i;
	idStr work;
	if ( dataType == DECL_EMAIL ) {
		c = src->GetNumEmails();
		for ( i = 0; i < c; i++ ) {
			const idDeclEmail *email = src->GetEmailByIndex( i );
			if ( email == NULL ) {
				work = va( "-\tEmail %d not found\t-", i );
			} else {
				work = email->GetFrom();
				work += "\t";
				work += email->GetSubject();
				work += "\t";
				work += email->GetDate();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	} else if ( dataType == DECL_AUDIO ) {
		c = src->GetNumAudios();
		for ( i = 0; i < c; i++ ) {
			const idDeclAudio *audio = src->GetAudioByIndex( i );
			if ( audio == NULL ) {
				work = va( "Audio Log %d not found", i );
			} else {
				work = audio->GetAudioName();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	} else if ( dataType == DECL_VIDEO ) {
		c = inventory.videos.Num();
		for ( i = 0; i < c; i++ ) {
			const idDeclVideo *video = GetVideo( i );
			if ( video == NULL ) {
				work = va( "Video CD %s not found", inventory.videos[i].c_str() );
			} else {
				work = video->GetVideoName();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	}
#endif
	return 0;
}

/*
==============
idPlayer::GetPDA
==============
 */
const idDeclPDA *idPlayer::GetPDA( void ) const {
	/*if ( inventory.pdas.Num() ) {
		return static_cast< const idDeclPDA* >( declManager->FindType( DECL_PDA, inventory.pdas[ 0 ] ) );
	} else {*/
		return NULL;
	//}
}

/*
==============
idPlayer::UpdatePDAInfo
==============
*/
void idPlayer::UpdatePDAInfo( bool updatePDASel ) {
	// greebo: Commented out this whole block, no PDAs in TDM
#if 0
	int j, sel;

	if ( objectiveSystem == NULL ) {
		return;
	}

	assert( hud );

	int currentPDA = objectiveSystem->State().GetInt( "listPDA_sel_0", "0" );
	if ( currentPDA == -1 ) {
		currentPDA = 0;
	}

	if ( updatePDASel ) {
		objectiveSystem->SetStateInt( "listPDAVideo_sel_0", 0 );
		objectiveSystem->SetStateInt( "listPDAEmail_sel_0", 0 );
		objectiveSystem->SetStateInt( "listPDAAudio_sel_0", 0 );
	}

	if ( currentPDA > 0 ) {
		currentPDA = inventory.pdas.Num() - currentPDA;
	}

	// Mark in the bit array that this pda has been read
	if ( currentPDA < 128 ) {
		inventory.pdasViewed[currentPDA >> 5] |= 1 << (currentPDA & 31);
	}

	pdaAudio = "";
	pdaVideo = "";
	pdaVideoWave = "";
	idStr name, data, preview, info, wave;
	for ( j = 0; j < MAX_PDAS; j++ ) {
		objectiveSystem->SetStateString( va( "listPDA_item_%i", j ), "" );
	}
	for ( j = 0; j < MAX_PDA_ITEMS; j++ ) {
		objectiveSystem->SetStateString( va( "listPDAVideo_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDAAudio_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDAEmail_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDASecurity_item_%i", j ), "" );
	}
	for ( j = 0; j < inventory.pdas.Num(); j++ ) {

		const idDeclPDA *pda = static_cast< const idDeclPDA* >( declManager->FindType( DECL_PDA, inventory.pdas[j], false ) );

		if ( pda == NULL ) {
			continue;
		}

		int index = inventory.pdas.Num() - j;
		if ( j == 0 ) {
			// Special case for the first PDA
			index = 0;
		}

		if ( j != currentPDA && j < 128 && inventory.pdasViewed[j >> 5] & (1 << (j & 31)) ) {
			// This pda has been read already, mark in gray
			objectiveSystem->SetStateString( va( "listPDA_item_%i", index), va(S_COLOR_GRAY "%s", pda->GetPdaName()) );
		} else {
			// This pda has not been read yet
		objectiveSystem->SetStateString( va( "listPDA_item_%i", index), pda->GetPdaName() );
		}

		const char *security = pda->GetSecurity();
		if ( j == currentPDA || (currentPDA == 0 && security && *security ) ) {
			if ( *security == NULL ) {
				security = common->GetLanguageDict()->GetString( "#str_00066" );
			}
			objectiveSystem->SetStateString( "PDASecurityClearance", security );
		}

		if ( j == currentPDA ) {

			objectiveSystem->SetStateString( "pda_icon", pda->GetIcon() );
			objectiveSystem->SetStateString( "pda_id", pda->GetID() );
			objectiveSystem->SetStateString( "pda_title", pda->GetTitle() );

			if ( j == 0 ) {
				// Selected, personal pda
				// Add videos
				if ( updatePDASel || !inventory.pdaOpened ) {
				objectiveSystem->HandleNamedEvent( "playerPDAActive" );
				objectiveSystem->SetStateString( "pda_personal", "1" );
					inventory.pdaOpened = true;
				}
				objectiveSystem->SetStateString( "pda_location", hud->State().GetString("location") );
				objectiveSystem->SetStateString( "pda_name", cvarSystem->GetCVarString( "ui_name") );
				AddGuiPDAData( DECL_VIDEO, "listPDAVideo", pda, objectiveSystem );
				sel = objectiveSystem->State().GetInt( "listPDAVideo_sel_0", "0" );
				const idDeclVideo *vid = NULL;
				if ( sel >= 0 && sel < inventory.videos.Num() ) {
					vid = static_cast< const idDeclVideo * >( declManager->FindType( DECL_VIDEO, inventory.videos[ sel ], false ) );
				}
				if ( vid ) {
					pdaVideo = vid->GetRoq();
					pdaVideoWave = vid->GetWave();
					objectiveSystem->SetStateString( "PDAVideoTitle", vid->GetVideoName() );
					objectiveSystem->SetStateString( "PDAVideoVid", vid->GetRoq() );
					objectiveSystem->SetStateString( "PDAVideoIcon", vid->GetPreview() );
					objectiveSystem->SetStateString( "PDAVideoInfo", vid->GetInfo() );
				} else {
					//FIXME: need to precache these in the player def
					objectiveSystem->SetStateString( "PDAVideoVid", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAVideoIcon", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAVideoTitle", "" );
					objectiveSystem->SetStateString( "PDAVideoInfo", "" );
				}
			} else {
				// Selected, non-personal pda
				// Add audio logs
				if ( updatePDASel ) {
				objectiveSystem->HandleNamedEvent( "playerPDANotActive" );
				objectiveSystem->SetStateString( "pda_personal", "0" );
					//inventory.pdaOpened = true;
				}
				objectiveSystem->SetStateString( "pda_location", pda->GetPost() );
				objectiveSystem->SetStateString( "pda_name", pda->GetFullName() );
				int audioCount = AddGuiPDAData( DECL_AUDIO, "listPDAAudio", pda, objectiveSystem );
				objectiveSystem->SetStateInt( "audioLogCount", audioCount );
				l = objectiveSystem->State().GetInt( "listPDAAudio_sel_0", "0" );
				const idDeclAudio *aud = NULL;
				if ( sel >= 0 ) {
					aud = pda->GetAudioByIndex( sel );
				}
				if ( aud ) {
					pdaAudio = aud->GetWave();
					objectiveSystem->SetStateString( "PDAAudioTitle", aud->GetAudioName() );
					objectiveSystem->SetStateString( "PDAAudioIcon", aud->GetPreview() );
					objectiveSystem->SetStateString( "PDAAudioInfo", aud->GetInfo() );
				} else {
					objectiveSystem->SetStateString( "PDAAudioIcon", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAAutioTitle", "" );
					objectiveSystem->SetStateString( "PDAAudioInfo", "" );
				}
			}
			// add emails
			name = "";
			data = "";
			int numEmails = pda->GetNumEmails();
			if ( numEmails > 0 ) {
				AddGuiPDAData( DECL_EMAIL, "listPDAEmail", pda, objectiveSystem );
				sel = objectiveSystem->State().GetInt( "listPDAEmail_sel_0", "-1" );
				if ( sel >= 0 && sel < numEmails ) {
					const idDeclEmail *email = pda->GetEmailByIndex( sel );
					name = email->GetSubject();
					data = email->GetBody();
				}
			}
			objectiveSystem->SetStateString( "PDAEmailTitle", name );
			objectiveSystem->SetStateString( "PDAEmailText", data );
		}
	}
	if ( objectiveSystem->State().GetInt( "listPDA_sel_0", "-1" ) == -1 ) {
		objectiveSystem->SetStateInt( "listPDA_sel_0", 0 );
	}
	objectiveSystem->StateChanged( gameLocal.time );
#endif
}

/*
==============
idPlayer::TogglePDA
==============
*/
void idPlayer::TogglePDA( void ) {
	// greebo: Commented this out, no PDAs in TDM.
#if 0	
	if ( objectiveSystem == NULL ) {
		return;
	}

	if ( inventory.pdas.Num() == 0 ) {
		ShowTip( spawnArgs.GetString( "text_infoTitle" ), spawnArgs.GetString( "text_noPDA" ), true );
		return;
	}

	assert( hud );

	if ( !objectiveSystemOpen ) {
		int j, c = inventory.items.Num();
		objectiveSystem->SetStateInt( "inv_count", c );
		for ( j = 0; j < MAX_INVENTORY_ITEMS; j++ ) {
			objectiveSystem->SetStateString( va( "inv_name_%i", j ), "" );
			objectiveSystem->SetStateString( va( "inv_icon_%i", j ), "" );
			objectiveSystem->SetStateString( va( "inv_text_%i", j ), "" );
		}
		for ( j = 0; j < c; j++ ) {
			idDict *item = inventory.items[j];
			if ( !item->GetBool( "inv_pda" ) ) {
				const char *iname = item->GetString( "inv_name" );
				const char *iicon = item->GetString( "inv_icon" );
				const char *itext = item->GetString( "inv_text" );
				objectiveSystem->SetStateString( va( "inv_name_%i", j ), iname );
				objectiveSystem->SetStateString( va( "inv_icon_%i", j ), iicon );
				objectiveSystem->SetStateString( va( "inv_text_%i", j ), itext );
				const idKeyValue *kv = item->MatchPrefix( "inv_id", NULL );
				if ( kv ) {
					objectiveSystem->SetStateString( va( "inv_id_%i", j ), kv->GetValue() );
				}
			}
		}

		for ( j = 0; j < MAX_WEAPONS; j++ ) {
			const char *weapnum = va( "def_weapon%d", j );
			const char *hudWeap = va( "weapon%d", j );
			int weapstate = 0;
			if ( inventory.weapons & ( 1 << j ) ) {
				const char *weap = spawnArgs.GetString( weapnum );
				if ( weap && *weap ) {
					weapstate++;
				}
			}
			objectiveSystem->SetStateInt( hudWeap, weapstate );
		}

		//objectiveSystem->SetStateInt( "listPDA_sel_0", inventory.selPDA );
		UpdatePDAInfo( false );
		objectiveSystem->Activate( true, gameLocal.time );
		hud->HandleNamedEvent( "pdaPickupHide" );
		hud->HandleNamedEvent( "videoPickupHide" );
	} else {
		//inventory.selPDA = objectiveSystem->State().GetInt( "listPDA_sel_0" );
		objectiveSystem->Activate( false, gameLocal.time );
	}
	objectiveSystemOpen ^= 1;
#endif
}

/*
==============
idPlayer::ToggleScoreboard
==============
*/
void idPlayer::ToggleScoreboard( void ) {
	scoreBoardOpen ^= 1;
}

/*
==============
idPlayer::Spectate
==============
*/
void idPlayer::Spectate( bool spectate ) {
	idBitMsg	msg;
	byte		msgBuf[MAX_EVENT_PARAM_SIZE];

	// track invisible player bug
	// all hiding and showing should be performed through Spectate calls
	// except for the private camera view, which is used for teleports
	assert( ( teleportEntity.GetEntity() != NULL ) || ( IsHidden() == spectating ) );

	if ( spectating == spectate ) {
		return;
	}

	spectating = spectate;

	if ( gameLocal.isServer ) {
		msg.Init( msgBuf, sizeof( msgBuf ) );
		msg.WriteBits( spectating, 1 );
		ServerSendEvent( EVENT_SPECTATE, &msg, false, -1 );
	}

	if ( spectating ) {
		// join the spectators
		spectator = this->entityNumber;
		Init();
		StopRagdoll();
		SetPhysics( &physicsObj );
		physicsObj.DisableClip();
		Hide();
		Event_DisableWeapon();
		if ( hud ) {
			hud->HandleNamedEvent( "aim_clear" );
			MPAimFadeTime = 0;
		}
	} else {
		// put everything back together again
		currentWeapon = -1;	// to make sure the def will be loaded if necessary
		Show();
		Event_EnableWeapon();
	}
	SetClipModel();
}

/*
==============
idPlayer::SetClipModel
==============
*/
void idPlayer::SetClipModel( void ) {
	idBounds bounds;

	if ( spectating ) {
		bounds = idBounds( vec3_origin ).Expand( pm_spectatebbox.GetFloat() * 0.5f );
	} else {
		bounds[0].Set( -pm_bboxwidth.GetFloat() * 0.5f, -pm_bboxwidth.GetFloat() * 0.5f, 0 );
		bounds[1].Set( pm_bboxwidth.GetFloat() * 0.5f, pm_bboxwidth.GetFloat() * 0.5f, pm_normalheight.GetFloat() );
	}
	// the origin of the clip model needs to be set before calling SetClipModel
	// otherwise our physics object's current origin value gets reset to 0
	idClipModel *newClip;
	if ( pm_usecylinder.GetBool() ) {
		newClip = new idClipModel( idTraceModel( bounds, 8 ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	} else {
		newClip = new idClipModel( idTraceModel( bounds ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	}
}

/*
==============
idPlayer::UseVehicle
==============
*/
void idPlayer::UseVehicle( void ) {
	trace_t	trace;
	idVec3 start, end;
	idEntity *ent;

	if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
		Show();
		static_cast<idAFEntity_Vehicle*>(GetBindMaster())->Use( this );
	} else {
		start = GetEyePosition();
		end = start + viewAngles.ToForward() * 80.0f;
		gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
		if ( trace.fraction < 1.0f ) {
			ent = gameLocal.entities[ trace.c.entityNum ];
			if ( ent && ent->IsType( idAFEntity_Vehicle::Type ) ) {
				Hide();
				static_cast<idAFEntity_Vehicle*>(ent)->Use( this );
			}
		}
	}
}

/*
==============
idPlayer::PerformImpulse
==============
*/
void idPlayer::PerformImpulse( int impulse ) {

	if ( gameLocal.isClient ) {
		idBitMsg	msg;
		byte		msgBuf[MAX_EVENT_PARAM_SIZE];

		assert( entityNumber == gameLocal.localClientNum );
		msg.Init( msgBuf, sizeof( msgBuf ) );
		msg.BeginWriting();
		msg.WriteBits( impulse, 6 );
		ClientSendEvent( EVENT_IMPULSE, &msg );
	}

	if ( impulse >= IMPULSE_0 && impulse <= IMPULSE_12 ) 
	{
		// Prevent the player from choosing to switch weapons.
		if ( GetImmobilization() & EIM_WEAPON_SELECT ) 
		{
			return;
		}

		SelectWeapon( impulse, false );
		return;
	}

	switch( impulse )
	{
		case IMPULSE_13:
		{
			Reload();
			break;
		}

		case IMPULSE_14:		// Next weapon
		{
			// If the grabber is active, next weapon modifies the distance based on the CVAR setting
			if (m_bGrabberActive)
			{
				gameLocal.m_Grabber->IncrementDistance( cv_reverse_grab_control.GetBool() );
			}

			// Pass the "next weapon" event to the GUIs
			m_overlays.broadcastNamedEvent("nextWeapon");

			// Prevent the player from choosing to switch weapons.
			if ( GetImmobilization() & EIM_WEAPON_SELECT ) 
			{
				return;
			}

			NextWeapon();
			break;
		}
		case IMPULSE_15:		// Previous Weapon
		{
			// If the grabber is active, previous weapon  modifies the distance based on the CVAR setting
			if (m_bGrabberActive)
			{
				gameLocal.m_Grabber->IncrementDistance( !cv_reverse_grab_control.GetBool() );
			}

			// Pass the "previous weapon" event to the GUIs
			m_overlays.broadcastNamedEvent("prevWeapon");

			// Prevent the player from choosing to switch weapons.
			if ( GetImmobilization() & EIM_WEAPON_SELECT ) 
			{
				return;
			}

			PrevWeapon();
			break;
		}
		case IMPULSE_17:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum )
			{
				gameLocal.mpGame.ToggleReady();
			}
			break;
		}

		case IMPULSE_18:
		{
			centerView.Init(gameLocal.time, 200, viewAngles.pitch, 0);
			break;
		}
		case IMPULSE_19: // Toggle Objectives GUI (was previously assigned to the PDA)
		{
			// TODO: Find a way to toggle the main menu from here
			break;
		}
		case IMPULSE_20:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum )
			{
				gameLocal.mpGame.ToggleTeam();
			}
			break;
		}
		case IMPULSE_22:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum )
			{
				gameLocal.mpGame.ToggleSpectate();
			}
			break;
		}

		case IMPULSE_23:		// Crouch
		{
			if (!cv_tdm_crouch_toggle.GetBool())
			{
				m_CrouchIntent = true;
			}
			
			m_ButtonStateTracker.StartTracking(impulse);
		}
		break;

		case IMPULSE_24:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) 
			{
					physicsObj.PerformMantle();
			}
			break;
		}

		// DarkMod: Sophisticated Zombie: For testing purposes only
	    // This tests the hiding spot search function relative to the player
		case IMPULSE_25:		// Test hiding spots.
		{
			DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Attempting hiding spot test");
			idVec3 searchOrigin = GetEyePosition();
			idBounds searchBounds (searchOrigin);
			
			idVec3 forward;
			idVec3 right;
			viewAngles.ToVectors (&forward, &right);
			right.Normalize();
			forward.Normalize();

			idVec3 widthPoint = searchOrigin + (right * 300.0f);
			searchBounds.AddPoint (widthPoint);
			idVec3 widthPoint2 = searchOrigin - (right * 300.0f);
			searchBounds.AddPoint (widthPoint2);  
			idVec3 endPoint = searchOrigin + (forward * 1000.0f);
			searchBounds.AddPoint (endPoint);

			// Get AAS
			idAAS* p_aas = NULL;
			for (int i = 0; i < gameLocal.NumAAS(); i++) {
				p_aas = gameLocal.GetAAS(i);
				if (p_aas != NULL) {
					CDarkmodAASHidingSpotFinder::testFindHidingSpots(
						searchOrigin,
						0.35f,
						searchBounds,
						this, // Ignore self as a hiding screen
						p_aas
					);
					DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Done hiding spot test");
				}
			}

			if (p_aas == NULL)
			{
				DM_LOG(LC_AI, LT_WARNING)LOGSTRING("No default AAS is present for map, number of AAS: %d\n", gameLocal.NumAAS());
			}
			
		}
		break;

		case IMPULSE_27:
			{
				//LAS.pvsToAASMappingTable.DebugShowMappings(10000);
				idAASLocal* aas = dynamic_cast<idAASLocal*>(gameLocal.GetAAS(cv_debug_aastype.GetString()));
					
				if (aas != NULL)
				{
					aas->DrawAreas(GetEyePosition());
				}
			}
			break;

		case IMPULSE_28:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum )
			{
				gameLocal.mpGame.CastVote( gameLocal.localClientNum, true );
			}
		}
		break;

		case IMPULSE_29:
		{
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum )
			{
				gameLocal.mpGame.CastVote( gameLocal.localClientNum, false );
			}
		}
		break;

		case IMPULSE_40:		// TDM: grab item with grabber
		{
			CGrabber *grabber = gameLocal.m_Grabber;
			idEntity *useEnt = grabber->GetSelected();
			if(useEnt == NULL)
			{
				idEntity *frob = g_Global.m_DarkModPlayer->m_FrobEntity.GetEntity();
				if(frob != NULL)
					grabber->PutInHands(frob, frob->GetPhysics()->GetAxis() );
			}
		}
		break;

		case IMPULSE_41:		// TDM Use/Frob
		{
			// Register the button for tracking
			m_ButtonStateTracker.StartTracking(impulse);
			// Perform the frob
			PerformFrob();
		}
		break;

		/*!
		* Lean forward is impulse 44
		*/
		case IMPULSE_44:		// Lean forward
		{
			m_ButtonStateTracker.StartTracking(impulse);
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) 
				physicsObj.ToggleLean(90.0);
		}
		break;

		/*!
		* Lean left is impulse 45
		*/
		case IMPULSE_45:		// Lean left
		{
			m_ButtonStateTracker.StartTracking(impulse);
			DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("Left lean impulse pressed\r");
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) 
			{
				// Do we need to enter the leaning state?
				DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("Left leaning started\r");
				physicsObj.ToggleLean(180.0);
			}
		}
		break;

		/*!
		* Lean right is impulse 46
		*/
		case IMPULSE_46:		// Lean right
		{
			m_ButtonStateTracker.StartTracking(impulse);
			DM_LOG(LC_SYSTEM, LT_DEBUG)LOGSTRING("Right lean impulse pressed\r");
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) 
				physicsObj.ToggleLean(0.0);
		}
		break;

		case IMPULSE_47:	// Inventory previous item
		{
			// If the grabber is active, prev item modifies the distance based on the CVAR setting
			if (m_bGrabberActive)
			{
				gameLocal.m_Grabber->IncrementDistance( !cv_reverse_grab_control.GetBool() );
			}

			// Notify the GUIs about the button event
			m_overlays.broadcastNamedEvent("inventoryPrevItem");

			if(GetImmobilization() & EIM_ITEM_SELECT)
				return;

			PrevInventoryItem();
		}
		break;

		case IMPULSE_48:	// Inventory next item
		{
			// If the grabber is active, next item modifies the distance based on the CVAR setting
			if (m_bGrabberActive)
			{
				gameLocal.m_Grabber->IncrementDistance( cv_reverse_grab_control.GetBool() );
			}

			// Notify the GUIs about the button event
			m_overlays.broadcastNamedEvent("inventoryNextItem");

			if(GetImmobilization() & EIM_ITEM_SELECT)
				return;

			NextInventoryItem();
		}
		break;

		case IMPULSE_49:	// Inventory previous group
		{
			// Check for a held grabber entity, which should be put back into the inventory
			if (AddGrabberEntityToInventory())
				return;

			// Notify the GUIs about the button event
			m_overlays.broadcastNamedEvent("inventoryPrevGroup");

			if(GetImmobilization() & EIM_ITEM_SELECT)
				return;

			PrevInventoryGroup();
		}
		break;

		case IMPULSE_50:	// Inventory next group
		{
			// Check for a held grabber entity, which should be put back into the inventory
			if (AddGrabberEntityToInventory())
				return;

			// Notify the GUIs about the button event
			m_overlays.broadcastNamedEvent("inventoryNextGroup");

			if(GetImmobilization() & EIM_ITEM_SELECT)
				return;

			NextInventoryGroup();
		}
		break;

		case IMPULSE_51:	// Inventory use item
		{
			// Use key has "hold down" functions
			m_ButtonStateTracker.StartTracking(impulse);
			// Pass the call
			UseInventoryItem();
		}
		break;

		case IMPULSE_52:	// Inventory drop item
		{
			// Pass the "inventoryDropItem" event to the GUIs
			m_overlays.broadcastNamedEvent("inventoryDropItem");

			if (GetImmobilization() & EIM_ITEM_DROP) {
				return;
			}

			DropInventoryItem();
		}
		break;

	} 
}

void idPlayer::PerformKeyRepeat(int impulse, int holdTime)
{


	switch (impulse)
	{
		case IMPULSE_23:		// TDM Crouch
		{
			if (holdTime > cv_tdm_crouch_toggle_hold_time.GetFloat())
			{
				if (physicsObj.OnRope())
				{
					physicsObj.RopeDetach();
					physicsObj.m_bClimbDetachCrouchHeld = true;
				}
				else if (physicsObj.OnLadder())
				{
					physicsObj.ClimbDetach();
					physicsObj.m_bClimbDetachCrouchHeld = true;
				}
			}

		}
		break;

		case IMPULSE_41:		// TDM Use/Frob
		{
			PerformFrobKeyRepeat();
		}
		break;

		case IMPULSE_51:		// Inventory Use Item
		{
			const CInventoryCursorPtr& crsr = InventoryCursor();
			CInventoryItemPtr it = crsr->GetCurrentItem();

			if (it != NULL && it->GetType() != CInventoryItem::IT_DUMMY)
			{
				UseInventoryItem(ERepeat, it, holdTime, false);
			}
		}
		break;
	}
}

void idPlayer::PerformKeyRelease(int impulse, int holdTime)
{
	// The key associated to <impulse> has been released
	DM_LOG(LC_FROBBING, LT_INFO)LOGSTRING("Button %d has been released, has been held down %d ms.\r", impulse, holdTime);

	switch (impulse)
	{
		case IMPULSE_23:		// TDM crouch
			if (cv_tdm_crouch_toggle.GetBool())
			{
				if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) 
				{
					m_CrouchIntent = !m_CrouchIntent;
				}
			}		
			else
			{
				m_CrouchIntent = false;
			}

			// clear climb detach intent when crouch is released
			physicsObj.m_bClimbDetachCrouchHeld = false;

		break;

		case IMPULSE_41:		// TDM Use/Frob
		{
			PerformFrobKeyRelease();
		}
		break;



		case IMPULSE_44:
			if ( !cv_pm_lean_toggle.GetBool() && physicsObj.IsLeaning() )
				physicsObj.ToggleLean(90.0);
		break;

		case IMPULSE_45:
			if ( !cv_pm_lean_toggle.GetBool() && physicsObj.IsLeaning() )
				physicsObj.ToggleLean(180.0);
		break;

		case IMPULSE_46:
			if ( !cv_pm_lean_toggle.GetBool() && physicsObj.IsLeaning() )
				physicsObj.ToggleLean(0.0);
		break;

		case IMPULSE_51:		// Inventory Use item
		{
			InventoryUseKeyRelease(holdTime);
		}
		break;
	}
}

bool idPlayer::HandleESC( void ) {
	if ( gameLocal.inCinematic ) {
		return SkipCinematic();
	}

	if ( objectiveSystemOpen ) {
		TogglePDA();
		return true;
	}

	if ( m_overlays.findInteractive() ) {
		// Handle the escape?
		// Not yet implemented.
		// return true?
	}

	return false;
}

bool idPlayer::SkipCinematic( void ) {
	StartSound( "snd_skipcinematic", SND_CHANNEL_ANY, 0, false, NULL );
	return gameLocal.SkipCinematic();
}


/*
==============
idPlayer::GetImmobilization
==============
*/
int idPlayer::GetImmobilization()
{
	// Has something changed since the cache was last calculated?
	if ( m_immobilizationCache & EIM_UPDATE )
	{
		// Recalculate the immobilization from scratch.
		m_immobilizationCache = 0;

		for (const idKeyValue* kv = m_immobilization.MatchPrefix(""); kv != NULL; kv = m_immobilization.MatchPrefix("", kv))
		{
			m_immobilizationCache |= atoi(kv->GetValue());
		}
	}

	return m_immobilizationCache;
}

/*
==============
idPlayer::GetHinderance
==============
*/
float idPlayer::GetHinderance( void ) 
{
	// Has something changed since the cache was last calculated?
	if (m_hinderanceCache < 0.0f) 
	{
		// Recalculate the hinderance from scratch.
		float mCap = 1.0f, aCap = 1.0f;

		for (const idKeyValue* kv = m_hinderance.MatchPrefix("", NULL); kv != NULL; kv = m_hinderance.MatchPrefix("", kv))
		{
			idVec3 vec = m_hinderance.GetVector(kv->GetKey());
			mCap *= vec[0];

			if ( aCap > vec[1] ) 
			{
				aCap = vec[1];
			}
		}

		if ( aCap > mCap ) 
		{
			aCap = mCap;
		}

		m_hinderanceCache = aCap;
	}

	return m_hinderanceCache;
}

/*
==============
idPlayer::GetTurnHinderance
==============
*/
float idPlayer::GetTurnHinderance( void ) 
{
	// Has something changed since the cache was last calculated?
	if (m_TurnHinderanceCache < 0.0f) 
	{
		// Recalculate the hinderance from scratch.
		float mCap = 1.0f, aCap = 1.0f;

		for (const idKeyValue* kv = m_TurnHinderance.MatchPrefix( "", NULL ); kv != NULL; kv = m_TurnHinderance.MatchPrefix("", kv))
		{
			idVec3 vec = m_TurnHinderance.GetVector(kv->GetKey());
			mCap *= vec[0];

			if ( aCap > vec[1] ) 
			{
				aCap = vec[1];
			}
		}

		if ( aCap > mCap ) 
		{
			aCap = mCap;
		}

		m_TurnHinderanceCache = aCap;
	}

	return m_TurnHinderanceCache;
}

/*
==============
idPlayer::EvaluateControls
==============
*/
void idPlayer::EvaluateControls( void ) 
{
	// check for respawning
	// TDM: Section removed, click-to-respawn functionality not needed for TDM
	// TDM: Added lean key check

	// in MP, idMultiplayerGame decides spawns
	if ( forceRespawn && !gameLocal.isMultiplayer && !g_testDeath.GetBool() ) {
		// in single player, we let the session handle restarting the level or loading a game
		gameLocal.sessionCommand = "died";
	}

	if( m_MouseGesture.bActive )
		UpdateMouseGesture();

	if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) )
	{
		PerformImpulse( usercmd.impulse );
	}

	scoreBoardOpen = ( ( usercmd.buttons & BUTTON_SCORES ) != 0 || forceScoreBoard );

	oldFlags = usercmd.flags;

	AdjustSpeed();

	// update the viewangles
	UpdateViewAngles();
}


void idPlayer::EvaluateCrouch()
{
	if ( GetImmobilization() & EIM_CROUCH) 
	{
		m_IdealCrouchState = false;
		return;
	} 
	m_IdealCrouchState = m_CrouchIntent;
}



/**
* TDM Mouse Gestures
**/
void idPlayer::StartMouseGesture( int impulse, int thresh, EMouseTest test, bool bInverted, float TurnHinderance, int DecideTime )
{
	m_MouseGesture.bActive = true;
	m_MouseGesture.test = test;
	m_MouseGesture.bInverted = bInverted;
	m_MouseGesture.key = impulse;
	m_MouseGesture.thresh = thresh;
	m_MouseGesture.DecideTime = DecideTime;

	m_MouseGesture.started = gameLocal.time;
	m_MouseGesture.StartPos.x = usercmd.mx;
	m_MouseGesture.StartPos.y = usercmd.my;
	m_MouseGesture.motion = vec2_zero;
	
	// NOTE: Do not clear the last result as we may use it again if we can't decide
	
	SetTurnHinderance( "MouseGesture", 1.0f, TurnHinderance );
}

void idPlayer::UpdateMouseGesture( void )
{
	bool bStop( false );
	float mag(0.0f);
	EMouseDir CurrentDir;

	m_MouseGesture.motion.x += usercmd.mx - m_MouseGesture.StartPos.x;
	m_MouseGesture.motion.y += usercmd.my - m_MouseGesture.StartPos.y;
	if( m_MouseGesture.bInverted )
		m_MouseGesture.motion = -m_MouseGesture.motion;

	EMouseTest test = m_MouseGesture.test;
	idVec2 motion = m_MouseGesture.motion;

	// Get the current dominant direction and magnitude
	// NOTE: Apparently y is positive if we go down, not up?
	if( test == MOUSETEST_UPDOWN )
	{
		if( motion.y < 0 )
			CurrentDir = MOUSEDIR_UP;
		else
			CurrentDir = MOUSEDIR_DOWN;

		mag = idMath::Fabs( motion.y );
	}
	else if ( test == MOUSETEST_LEFTRIGHT )
	{
		if( motion.x > 0 )
			CurrentDir = MOUSEDIR_RIGHT;
		else
			CurrentDir = MOUSEDIR_LEFT;

		mag = idMath::Fabs( motion.x );
	}
	else if( test == MOUSETEST_4DIR )
	{
		// up/down motion dominant
		if( idMath::Fabs(motion.y) > idMath::Fabs(motion.x) )
		{
			if( motion.y < 0 )
				CurrentDir = MOUSEDIR_UP;
			else
				CurrentDir = MOUSEDIR_DOWN;

			mag = idMath::Fabs( motion.y );			
		}
		// side/side dominant (default left for zero input)
		else
		{
			if( motion.x > 0 )
				CurrentDir = MOUSEDIR_RIGHT;
			else
				CurrentDir = MOUSEDIR_LEFT;

			mag = idMath::Fabs( motion.x );
		}
	}
	else
	{
		// Test along 8 directions (NOT TESTED!)

		// Step 1, resolve onto diagonal basis
		idVec2 DiagVec;
		// upper right
		DiagVec.x = motion * idMath::SQRT_1OVER2 * idVec2(1.0f,1.0f);
		// lower right
		DiagVec.y = motion * idMath::SQRT_1OVER2 * idVec2(1.0f,-1.0f);

		// figure out which is dominant
		idList<float> dirs;
		dirs.Append(idMath::Fabs(motion.y));
		dirs.Append(idMath::Fabs(motion.x));
		dirs.Append(idMath::Fabs(DiagVec.x));
		dirs.Append(idMath::Fabs(DiagVec.y));

		mag = 0.0f;
		int MaxAxis = 1; // default to left if we have zero motion

		for( int i=0; i < 4; i++ )
		{
			if( dirs[i] > mag )
			{
				mag = dirs[i];
				MaxAxis = i;
			}
		}

		// up/down
		if( MaxAxis == 0 )
		{
			if( motion.y < 0 )
				CurrentDir = MOUSEDIR_UP;
			else
				CurrentDir = MOUSEDIR_DOWN;
		}
		// left/right
		else if( MaxAxis == 1)
		{
			if( motion.x > 0 )
				CurrentDir = MOUSEDIR_RIGHT;
			else
				CurrentDir = MOUSEDIR_LEFT;
		}
		// lower right/upper left
		else if( MaxAxis == 2)
		{
			if( DiagVec.x < 0 )
				CurrentDir = MOUSEDIR_UP_LEFT;
			else
				CurrentDir = MOUSEDIR_DOWN_RIGHT;
		}
		// upper right/lower left
		else
		{
			if( DiagVec.y < 0 )
				CurrentDir = MOUSEDIR_DOWN_LEFT;
			else
				CurrentDir = MOUSEDIR_UP_RIGHT;
		}
	}


	// If above threshold, decision timer ran out, or button released, we're done
	if( mag > m_MouseGesture.thresh 
		|| (m_MouseGesture.DecideTime >= 0 && (gameLocal.time - m_MouseGesture.started) > m_MouseGesture.DecideTime) 
		|| !common->ButtonState(m_MouseGesture.key) )
	{
		m_MouseGesture.result = CurrentDir;
		StopMouseGesture();
	}
}

void idPlayer::StopMouseGesture( void )
{
	m_MouseGesture.bActive = false;

	// clear the angular hinderance
	SetTurnHinderance( "MouseGesture", 1.0f, 1.0f );
}

EMouseDir idPlayer::GetMouseGesture( void )
{
	return m_MouseGesture.result;
}

void idPlayer::Event_GetMouseGesture( void )
{
	idThread::ReturnInt( (int) GetMouseGesture() );
}

void idPlayer::Event_MouseGestureFinished( void )
{
	idThread::ReturnInt( !m_MouseGesture.bActive );
}

/*
==============
idPlayer::AdjustSpeed

DarkMod Note: Wow... this was pretty badly written
==============
*/
void idPlayer::AdjustSpeed( void ) 
{
	float speed(0.0f);
	float crouchspeed(0.0f);
	float rate(0.0f);
	float MaxSpeed(0.0f);

	MaxSpeed = pm_walkspeed.GetFloat() * cv_pm_runmod.GetFloat() * GetHinderance();

	if ( spectating )
	{
		speed = pm_spectatespeed.GetFloat();
		bobFrac = 0.0f;
	}
	else if ( noclip && !( usercmd.buttons & BUTTON_RUN ))
	{
		// "Walk" noclip
		speed = pm_noclipspeed.GetFloat();
		bobFrac = 0.0f;
	}
	else if ( noclip && ( usercmd.buttons & BUTTON_RUN ))
	{
		// "run" noclip
		speed = pm_noclipspeed.GetFloat() * cv_pm_runmod.GetFloat();
		bobFrac = 0.0f;
	} 
	// running case
	// TDM: removed check for not crouching..
	else if ( !physicsObj.OnLadder() && ( usercmd.buttons & BUTTON_RUN ) && ( usercmd.forwardmove || usercmd.rightmove ) ) 
	{
		// TDM: Removed stamina. TDM does not use stamina
		bobFrac = 1.0f;
		speed = pm_walkspeed.GetFloat() * cv_pm_runmod.GetFloat();
		
		// ishtvan: we'll see if this works to prevent backwards running, depends on order things are set
		// Don't apply backwards run penalty to crouch run.  It's already slow enough.
		crouchspeed = speed * cv_pm_crouchmod.GetFloat();
		if( usercmd.forwardmove < 0 )
			speed *= cv_pm_run_backmod.GetFloat();

		// Clamp to encumbrance limits:
		if( speed > MaxSpeed )
			speed = MaxSpeed;
		if( crouchspeed > MaxSpeed )
			crouchspeed = MaxSpeed;
	}
	// standing still, walking, or creeping case
	else 
	{
		// TDM: Removed stamina

		speed = pm_walkspeed.GetFloat();
		bobFrac = 0.0f;

		// apply creep modifier; creep is on button_5
		if( usercmd.buttons & BUTTON_5 )
			speed *= cv_pm_creepmod.GetFloat();

		// apply movement multipliers to crouch as well
		crouchspeed = speed * cv_pm_crouchmod.GetFloat();

		// Clamp to encumbrance limits:
		if( speed > MaxSpeed )
			speed = MaxSpeed;
		if( crouchspeed > MaxSpeed )
			crouchspeed = MaxSpeed;
	}

	// greebo: Clamp speed if swimming to 1.3 x walkspeed
	if (physicsObj.GetWaterLevel() >= WATERLEVEL_WAIST)
	{
		speed = idMath::ClampFloat(0, pm_walkspeed.GetFloat() * cv_pm_max_swimspeed_mod.GetFloat(), speed);
	}

	// TDM: leave this in for speed potions or something
	speed *= PowerUpModifier(SPEED);

	if ( influenceActive == INFLUENCE_LEVEL3 ) 
	{
		speed *= 0.33f;
	}

	physicsObj.SetSpeed( speed, crouchspeed );
}

/*
==============
idPlayer::AdjustBodyAngles
==============
*/
void idPlayer::AdjustBodyAngles( void ) {
	idMat3	lookAxis;
	idMat3	legsAxis;
	bool	blend;
	float	diff;
	float	frac;
	float	upBlend;
	float	forwardBlend;
	float	downBlend;

	if ( health < 0 ) {
		return;
	}

	blend = true;

	if ( !physicsObj.HasGroundContacts() ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else if ( usercmd.forwardmove < 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( -usercmd.forwardmove, usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = false;
	} else if ( usercmd.forwardmove > 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( usercmd.forwardmove, -usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = true;
	} else if ( ( usercmd.rightmove != 0 ) && physicsObj.IsCrouching() ) {
		if ( !legsForward ) {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), usercmd.rightmove, 0.0f ).ToYaw() );
		} else {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), -usercmd.rightmove, 0.0f ).ToYaw() );
		}
	} else if ( usercmd.rightmove != 0 ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else {
		legsForward = true;
		diff = idMath::Fabs( idealLegsYaw - legsYaw );
		idealLegsYaw = idealLegsYaw - idMath::AngleNormalize180( viewAngles.yaw - oldViewYaw );
		if ( diff < 0.1f ) {
			legsYaw = idealLegsYaw;
			blend = false;
		}
	}

	if ( !physicsObj.IsCrouching() ) {
		legsForward = true;
	}

	oldViewYaw = viewAngles.yaw;

	AI_TURN_LEFT = false;
	AI_TURN_RIGHT = false;
	if ( idealLegsYaw < -45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_RIGHT = true;
		blend = true;
	} else if ( idealLegsYaw > 45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_LEFT = true;
		blend = true;
	}

	if ( blend ) {
		legsYaw = legsYaw * 0.9f + idealLegsYaw * 0.1f;
	}
	legsAxis = idAngles( 0.0f, legsYaw, 0.0f ).ToMat3();
	animator.SetJointAxis( hipJoint, JOINTMOD_WORLD, legsAxis );

	// calculate the blending between down, straight, and up
	frac = viewAngles.pitch / 90.0f;
	if ( frac > 0.0f ) {
		downBlend		= frac;
		forwardBlend	= 1.0f - frac;
		upBlend			= 0.0f;
	} else {
		downBlend		= 0.0f;
		forwardBlend	= 1.0f + frac;
		upBlend			= -frac;
	}

    animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 0, downBlend );
	animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 1, forwardBlend );
	animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 2, upBlend );

	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 0, downBlend );
	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 1, forwardBlend );
	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 2, upBlend );
}

/*
==============
idPlayer::InitAASLocation
==============
*/
void idPlayer::InitAASLocation( void ) {
	int		i;
	int		num;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	GetFloorPos( 64.0f, origin );

	num = gameLocal.NumAAS();
	aasLocation.SetGranularity( 1 );
	aasLocation.SetNum( num );	
	for( i = 0; i < aasLocation.Num(); i++ ) {
		aasLocation[ i ].areaNum = 0;
		aasLocation[ i ].pos = origin;
		aas = gameLocal.GetAAS( i );
		if ( aas && aas->GetSettings() ) {
			size = aas->GetSettings()->boundingBoxes[0][1];
			bounds[0] = -size;
			size.z = 32.0f;
			bounds[1] = size;

			aasLocation[ i ].areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		}
	}
}

/*
==============
idPlayer::SetAASLocation
==============
*/
void idPlayer::SetAASLocation( void ) {
	int		i;
	int		areaNum;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	if ( !GetFloorPos( 64.0f, origin ) ) {
		return;
	}
	
	for( i = 0; i < aasLocation.Num(); i++ ) {
		aas = gameLocal.GetAAS( i );
		if ( !aas ) {
			continue;
		}

		size = aas->GetSettings()->boundingBoxes[0][1];
		bounds[0] = -size;
		size.z = 32.0f;
		bounds[1] = size;

		areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		if ( areaNum ) {
			aasLocation[ i ].pos = origin;
			aasLocation[ i ].areaNum = areaNum;
		}
	}
}

/*
==============
idPlayer::GetAASLocation
==============
*/
void idPlayer::GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const {
	int i;

	if ( aas != NULL ) {
		for( i = 0; i < aasLocation.Num(); i++ ) {
			if ( aas == gameLocal.GetAAS( i ) ) {
				areaNum = aasLocation[ i ].areaNum;
				pos = aasLocation[ i ].pos;
				return;
			}
		}
	}

	areaNum = 0;
	pos = physicsObj.GetOrigin();
}

/*
==============
idPlayer::Move
==============
*/
void idPlayer::Move( void ) 
{
	float newEyeOffset;
	idVec3 savedOrigin;
	idVec3 savedVelocity;
	idVec3 pushVelocity;

	// save old origin and velocity for crashlanding
	savedOrigin = physicsObj.GetOrigin();
	savedVelocity = physicsObj.GetLinearVelocity();

	pushVelocity = physicsObj.GetPushedLinearVelocity();

	// set physics variables
	physicsObj.SetMaxStepHeight( pm_stepsize.GetFloat() );
	physicsObj.SetMaxJumpHeight( pm_jumpheight.GetFloat()*GetJumpHinderance() );

	if ( noclip ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetMovementType( PM_NOCLIP );
	} else if ( spectating ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetMovementType( PM_SPECTATOR );
	} else if ( health <= 0 ) {
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
		physicsObj.SetMovementType( PM_DEAD );
	} else if ( gameLocal.inCinematic || gameLocal.GetCamera() || privateCameraView || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_FREEZE );
	} else {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_NORMAL );
	}
	// SR CONTENTS_RESPONSE FIX
	if( m_StimResponseColl->HasResponse() )
		physicsObj.SetContents( physicsObj.GetContents() | CONTENTS_RESPONSE );

	if ( spectating ) {
		physicsObj.SetClipMask( MASK_DEADSOLID );
	} else if ( health <= 0 ) {
		physicsObj.SetClipMask( MASK_DEADSOLID );
	} else {
		physicsObj.SetClipMask( MASK_PLAYERSOLID );
	}

	physicsObj.SetDebugLevel( g_debugMove.GetBool() );
	physicsObj.SetPlayerInput( usercmd, viewAngles );

	// FIXME: physics gets disabled somehow
	BecomeActive( TH_PHYSICS );
	RunPhysics();

	// update our last valid AAS location for the AI
	SetAASLocation();

	if ( spectating ) {
		newEyeOffset = 0.0f;
	} else if ( health <= 0 ) {
		newEyeOffset = pm_deadviewheight.GetFloat();
	} else if ( physicsObj.IsCrouching() ) {
		newEyeOffset = pm_crouchviewheight.GetFloat();
	} else if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
		newEyeOffset = 0.0f;
	} else {
		newEyeOffset = pm_normalviewheight.GetFloat();
	}

	if ( EyeHeight() != newEyeOffset ) {
		if ( spectating ) {
			SetEyeHeight( newEyeOffset );
		} else {
			// smooth out duck height changes
			SetEyeHeight( EyeHeight() * pm_crouchrate.GetFloat() + newEyeOffset * ( 1.0f - pm_crouchrate.GetFloat() ) );
		}
	}

	if ( noclip || gameLocal.inCinematic || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		AI_CROUCH	= false;
		AI_ONGROUND	= ( influenceActive == INFLUENCE_LEVEL2 );
		AI_ONLADDER	= false;
		AI_JUMP		= false;
	} else {
		AI_CROUCH	= physicsObj.IsCrouching();
		AI_ONGROUND	= physicsObj.HasGroundContacts();
		AI_ONLADDER	= physicsObj.OnLadder();
		AI_JUMP		= physicsObj.HasJumped();

		// check if we're standing on top of a monster and give a push if we are
		idEntity *groundEnt = physicsObj.GetGroundEntity();
		if 
		(	
			groundEnt && groundEnt->IsType( idAI::Type )
			&& groundEnt->health > 0
			&& static_cast<idAI *>(groundEnt)->m_bPushOffPlayer
		)
		{
			idVec3 vel = physicsObj.GetLinearVelocity();
			if ( vel.ToVec2().LengthSqr() < 0.1f ) {
				vel.ToVec2() = physicsObj.GetOrigin().ToVec2() - groundEnt->GetPhysics()->GetAbsBounds().GetCenter().ToVec2();
				vel.ToVec2().NormalizeFast();
				vel.ToVec2() *= pm_walkspeed.GetFloat();
			} else {
				// give em a push in the direction they're going
				vel *= 1.1f;
			}
			physicsObj.SetLinearVelocity( vel );
		}
	}

	if ( AI_JUMP ) 
	{
		// bounce the view weapon
 		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[2] = 200;
		acc->dir[0] = acc->dir[1] = 0;
	}

	// play rope movement sounds
	if( physicsObj.OnRope() )
	{
		int old_vert(0), new_vert(0);
		// Correct for moving reference frame
		int startTime = gameLocal.previousTime;
		int endTime = gameLocal.time;
		float RefZOffset = MS2SEC(endTime - startTime) * physicsObj.GetRefEntVel().z;
		
		old_vert = static_cast<int>(savedOrigin.z / cv_pm_rope_snd_rep_dist.GetInteger());
		new_vert = static_cast<int>((physicsObj.GetOrigin().z - RefZOffset) / cv_pm_rope_snd_rep_dist.GetInteger());
		
		if ( old_vert != new_vert ) 
		{
			StartSound( "snd_rope_climb", SND_CHANNEL_ANY, 0, false, NULL );
		}
	}


	// play climbing movement sounds
	if ( AI_ONLADDER ) 
	{
		idStr TempStr, LocalSound, sound;
		bool bSoundPlayed = false;
		sound.Clear();
		int old_vert(0), new_vert(0), old_horiz(0), new_horiz(0);
		
		// Correct for moving reference frame
		int startTime = gameLocal.previousTime;
		int endTime = gameLocal.time;
		idVec3 RefFrameOffset = MS2SEC(endTime - startTime) * physicsObj.GetRefEntVel();
		idVec3 GravNormal = physicsObj.GetGravityNormal();
		idVec3 RefFrameOffsetXY = RefFrameOffset - (RefFrameOffset * GravNormal ) * GravNormal;

		old_vert = static_cast<int>(savedOrigin.z / physicsObj.GetClimbSndRepDistVert());
		new_vert = static_cast<int>((physicsObj.GetOrigin().z - RefFrameOffset.z) / physicsObj.GetClimbSndRepDistVert());

		old_horiz = static_cast<int>(physicsObj.GetClimbLateralCoord( savedOrigin ) / physicsObj.GetClimbSndRepDistHoriz());
		new_horiz = static_cast<int>(physicsObj.GetClimbLateralCoord( physicsObj.GetOrigin() - RefFrameOffsetXY ) / physicsObj.GetClimbSndRepDistHoriz());

		if ( old_vert != new_vert ) 
		{
			LocalSound = "snd_climb_vert_";
			bSoundPlayed = true;
		}

		else if( old_horiz != new_horiz )
		{
			LocalSound = "snd_climb_horiz_";
			bSoundPlayed = true;
		}

		if( bSoundPlayed )
		{
			idStr SurfName = physicsObj.GetClimbSurfaceType();
			TempStr = LocalSound + SurfName;
			sound = spawnArgs.GetString( TempStr.c_str() );
			if( sound.IsEmpty() )
			{
				TempStr = LocalSound + "default";
				sound = spawnArgs.GetString( TempStr.c_str() );
			}

			// DM_LOG(LC_MOVEMENT, LT_DEBUG)LOGSTRING("Climb sound: %s\r", TempStr.c_str() );
			if ( !sound.IsEmpty() )
				StartSound( TempStr.c_str(), SND_CHANNEL_ANY, 0, false, NULL );
		}
	}

	if ( health > 0 )
	{
		CrashLand( savedOrigin, savedVelocity );
	}

	BobCycle( pushVelocity );

}

/*
==============
idPlayer::UpdateHUD
==============
*/
void idPlayer::UpdateHUD()
{
	if (hud == NULL) return;
	
	if ( entityNumber != gameLocal.localClientNum ) {
		return;
	}

	// Broadcast the HUD opacity value
	m_overlays.setGlobalStateFloat("HUD_Opacity", cv_tdm_hud_opacity.GetFloat());

	// Propagate the CVAR to the HUD
	hud->SetStateBool("HUD_LightgemVisible", !cv_tdm_hud_hide_lightgem.GetBool());

	// Update the inventory HUD
	UpdateInventoryHUD();

	// Check if any HUD messages are pending
	UpdateHUDMessages();
	UpdateInventoryPickedUpMessages();

	if (cv_show_gameplay_time.GetBool())
	{
		// Update the playing time in the HUD, if desired
		hud->SetStateString("PlayingTime", gameLocal.m_GamePlayTimer.GetTime().c_str());
	}
}

void idPlayer::UpdateInventoryHUD()
{
	if (!inventoryHUDNeedsUpdate) return; // nothing to do

	// Clear the flag right before taking any action
	inventoryHUDNeedsUpdate = false;

	CInventoryItemPtr curItem = InventoryCursor()->GetCurrentItem();

	if (curItem == NULL) return; // no current item, nothing to do

	idEntity* curItemEnt = curItem->GetItemEntity();

	if (curItemEnt != NULL) 
	{
		// Call the item's update routine for the update
		idThread* thread = curItemEnt->CallScriptFunctionArgs(
			"inventory_item_update", true, 0, 
			"eef", curItemEnt, curItem->GetOwner(), static_cast<float>(curItem->GetOverlay())
		);

		if (thread != NULL)
		{
			thread->Execute();
		}
	}

	// Now take the correct action based on the selected type
	switch (curItem->GetType())
	{
		case CInventoryItem::IT_ITEM:
		{
			// We display the default hud if the item doesn't have its own. Each item
			// with its own gui is responsible for switching it on and off
			// appropriately with their respective events.
			if (curItem->HasHUD() == false)
			{
				SetGuiInt(m_InventoryOverlay, "Inventory_GroupVisible", 1);
				SetGuiInt(m_InventoryOverlay, "Inventory_ItemVisible", 1);

				SetGuiFloat(m_InventoryOverlay, "Inventory_ItemStackable", curItem->IsStackable() ? 1 : 0);
				SetGuiString(m_InventoryOverlay, "Inventory_ItemGroup", curItem->Category()->GetName().c_str());
				SetGuiString(m_InventoryOverlay, "Inventory_ItemName", curItem->GetName());
				SetGuiInt(m_InventoryOverlay, "Inventory_ItemCount", curItem->GetCount());
				SetGuiString(m_InventoryOverlay, "Inventory_ItemIcon", curItem->GetIcon().c_str());
			}
		}
		break;

		case CInventoryItem::IT_DUMMY:
		{
			// All objects are set to empty, so we have an empty entry in the inventory.
			SetGuiInt(m_InventoryOverlay, "Inventory_ItemVisible", 0);
			SetGuiInt(m_InventoryOverlay, "Inventory_LootVisible", 0);
			SetGuiInt(m_InventoryOverlay, "Inventory_GroupVisible", 0);
		}
		break;
		
		default: break;
	}

	idUserInterface* invGUI = m_overlays.getGui(m_InventoryOverlay);
	if (invGUI != NULL)
	{
		invGUI->StateChanged(gameLocal.time);
	}
}

void idPlayer::UpdateHUDMessages()
{
	if (hudMessages.Num() == 0) {
		return; // nothing to do
	}

	// We have a message to display, check if the HUD is ready for it
	if (hud == NULL) {
		return; // no HUD?
	}

	int hasMessage = hud->GetStateInt("HasMessage");

	if (hasMessage == 1) {
		// The HUD is still busy, leave it for the moment being
		return; 
	}

	// greebo: We have a message and the HUD is ready, shove it into the GUI.
	hud->SetStateString("MessageText", hudMessages[0]);
	hud->HandleNamedEvent("DisplayMessage");

	// Remove the first string, now that it's been moved into the GUI
	hudMessages.RemoveIndex(0);
}

void idPlayer::UpdateInventoryPickedUpMessages()
{
	if (inventoryPickedUpMessages.Num() == 0) return; // nothing to do

	// We have a message to display, check if the HUD is ready for it

	idUserInterface* invGUI = m_overlays.getGui(m_InventoryOverlay);

	if (invGUI == NULL) 
	{
		DM_LOG(LC_INVENTORY, LT_ERROR)LOGSTRING("Could not find inventory HUD, cannot update pickup messages.\r");
		return; // no overlay?
	}

	/* 
	greebo: Commented this out to let new pickup messages overwrite the old ones.
	Put this back in to let the GUI display one message after the other.

	int hasMessage = invGUI->GetStateInt("HasInventoryPickUpMessage");

	if (hasMessage == 1)
	{
		// The HUD is still busy, leave it for the moment being
		return; 
	}*/

	// greebo: We have a message and the HUD is ready, shove it into the GUI.
	invGUI->SetStateString("InventoryPickUpMessageText", inventoryPickedUpMessages[0]);
	invGUI->HandleNamedEvent("DisplayInventoryPickUpMessage");

	// Remove the first string, now that it's been moved into the GUI
	inventoryPickedUpMessages.RemoveIndex(0);
}

/*
==============
idPlayer::UpdateDeathSkin
==============
*/
void idPlayer::UpdateDeathSkin( bool state_hitch ) {
	if ( !( gameLocal.isMultiplayer || g_testDeath.GetBool() ) ) {
		return;
	}
	if ( health <= 0 ) {
		if ( !doingDeathSkin ) {
			deathClearContentsTime = spawnArgs.GetInt( "deathSkinTime" );
			doingDeathSkin = true;
			renderEntity.noShadow = true;
			if ( state_hitch ) {
				renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = gameLocal.time * 0.001f - 2.0f;
			} else {
				renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = gameLocal.time * 0.001f;
			}
			UpdateVisuals();
		}

		// wait a bit before switching off the content
		if ( deathClearContentsTime && gameLocal.time > deathClearContentsTime ) {
			SetCombatContents( false );
			deathClearContentsTime = 0;
		}
	} else {
		renderEntity.noShadow = false;
		renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = 0.0f;
		UpdateVisuals();
		doingDeathSkin = false;
	}
}

/*
==============
idPlayer::StartFxOnBone
==============
*/
void idPlayer::StartFxOnBone( const char *fx, const char *bone ) {
	idVec3 offset;
	idMat3 axis;
	jointHandle_t jointHandle = GetAnimator()->GetJointHandle( bone );

	if ( jointHandle == INVALID_JOINT ) {
		gameLocal.Printf( "Cannot find bone %s\n", bone );
		return;
	}

	if ( GetAnimator()->GetJointTransform( jointHandle, gameLocal.time, offset, axis ) ) {
		offset = GetPhysics()->GetOrigin() + offset * GetPhysics()->GetAxis();
		axis = axis * GetPhysics()->GetAxis();
	}

	idEntityFx::StartFx( fx, &offset, &axis, this, true );
}

void idPlayer::UpdateUnderWaterEffects() {
	if ( physicsObj.GetWaterLevel() >= WATERLEVEL_HEAD ) {
		if (!underWaterEffectsActive) {
			StartSound( "snd_airless", SND_CHANNEL_DEMONIC, 0, false, NULL );

			// Underwater GUI section, allows custom water guis for each water entity etc - Dram
			idStr overlay;
			idPhysics_Liquid* CurWaterEnt = GetPlayerPhysics()->GetWater();
			if (CurWaterEnt != NULL) { // Get the GUI from the current water entity
				idEntity* CurEnt = CurWaterEnt->GetSelf();
				if (CurEnt != NULL) {
					overlay = CurEnt->spawnArgs.GetString("underwater_gui");
				}
				gameLocal.Printf( "UNDERWATER: After water check overlay is %s\n", overlay.c_str() );
			}
			if (overlay.IsEmpty()) { // If the overlay string is empty it has failed to find the GUI on the entity, so give warning
				gameLocal.Warning( "UNDERWATER: water check overlay failed, check key/val pairs\n" );
			}
			else if (!overlay.IsEmpty()) {
				underWaterGUIHandle = CreateOverlay(overlay.c_str(), LAYER_UNDERWATER);
			}
			underWaterEffectsActive = true;
		}
	}
	else {
		if (underWaterEffectsActive)
		{
			StopSound( SND_CHANNEL_DEMONIC, false );
			if (underWaterGUIHandle != -1) {
				DestroyOverlay(underWaterGUIHandle);
				underWaterGUIHandle = -1;
			}

			// If we were underwater for more than 4 seconds, play the "take breath" sound
			if (gameLocal.time > physicsObj.GetSubmerseTime() + 4000)
			{
				StartSound( "snd_resurface", SND_CHANNEL_VOICE, 0, false, NULL );
			}

			underWaterEffectsActive = false;
		}
	}
}

/*
==============
idPlayer::Think

Called every tic for each player
==============
*/
void idPlayer::Think( void )
{
	renderEntity_t *headRenderEnt;
	UpdatePlayerIcons();

	// latch button actions
	oldButtons = usercmd.buttons;

	// grab out usercmd
	usercmd_t oldCmd = usercmd;
	usercmd = gameLocal.usercmds[ entityNumber ];
	buttonMask &= usercmd.buttons;
	usercmd.buttons &= ~buttonMask;

	// angua: disable doom3 crouching
	if (usercmd.upmove < 0)
	{
		usercmd.upmove = 0;
	}

	if( AI_PAIN )
		m_bDamagedThisFrame = true;
	else
		m_bDamagedThisFrame = false;

	if ( gameLocal.inCinematic && gameLocal.skipCinematic ) {
		return;
	}

	// clear the ik before we do anything else so the skeleton doesn't get updated twice
	walkIK.ClearJointMods();
	
	// if this is the very first frame of the map, set the delta view angles
	// based on the usercmd angles
	if ( !spawnAnglesSet && ( gameLocal.GameState() != GAMESTATE_STARTUP ) ) {
		spawnAnglesSet = true;
		SetViewAngles( spawnAngles );
		oldFlags = usercmd.flags;
	}

	if ( objectiveSystemOpen || gameLocal.inCinematic || influenceActive ) {
		if ( objectiveSystemOpen && AI_PAIN ) {
			TogglePDA();
		}
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	// I'm not sure if this is the best way to do things, since it doesn't yet
	// take into account whether the player is swimming/climbing, etc. But it
	// should be good enough for now. I'll improve it later.
	// -Gildoran
	if ( GetImmobilization() & EIM_MOVEMENT ) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
	}
	// Note to self: I should probably be thinking about having some sort of
	// flag where the player is kept crouching if that's how they started out.

	// angua: crouch immobilization is handled in EvaluateCrouch now
	if ( GetImmobilization() & EIM_JUMP && usercmd.upmove > 0 ) {
		usercmd.upmove = 0;
	}

	// log movement changes for weapon bobbing effects
	if ( usercmd.forwardmove != oldCmd.forwardmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[0] = usercmd.forwardmove - oldCmd.forwardmove;
		acc->dir[1] = acc->dir[2] = 0;
	}

	if ( usercmd.rightmove != oldCmd.rightmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[1] = usercmd.rightmove - oldCmd.rightmove;
		acc->dir[0] = acc->dir[2] = 0;
	}

	// freelook centering
	if ( ( usercmd.buttons ^ oldCmd.buttons ) & BUTTON_MLOOK ) {
		centerView.Init( gameLocal.time, 200, viewAngles.pitch, 0 );
	}

	// zooming (greebo: Disabled this due to interference with the spyglass, isn't needed anymore)
	/*if ( ( usercmd.buttons ^ oldCmd.buttons ) & BUTTON_ZOOM ) {
		if ( ( usercmd.buttons & BUTTON_ZOOM ) && weapon.GetEntity() ) {
			zoomFov.Init( gameLocal.time, 200.0f, CalcFov( false ), weapon.GetEntity()->GetZoomFov() );
		} else {
			zoomFov.Init( gameLocal.time, 200.0f, zoomFov.GetCurrentValue( gameLocal.time ), DefaultFov() );
		}
	}*/

	// if we have an active gui, we will unrotate the view angles as
	// we turn the mouse movements into gui events
	idUserInterface *gui = ActiveGui();
	if ( gui /*&& gui != focusUI*/ ) {
		RouteGuiMouse( gui );
	}

	// set the push velocity on the weapon before running the physics
	if ( weapon.GetEntity() ) {
		weapon.GetEntity()->SetPushVelocity( physicsObj.GetPushedLinearVelocity() );
	}

	EvaluateControls();

	EvaluateCrouch();


	if ( !af.IsActive() ) {
		AdjustBodyAngles();
		CopyJointsFromBodyToHead();
	}

	Move();

	if ( !g_stopTime.GetBool() ) {

		if ( !noclip && !spectating && ( health > 0 ) && !IsHidden() ) {
			TouchTriggers();
		}

		// not done on clients for various reasons. don't do it on server and save the sound channel for other things
		/*if ( !gameLocal.isMultiplayer ) {
			SetCurrentHeartRate();
			float scale = g_damageScale.GetFloat();
			if ( g_useDynamicProtection.GetBool() && scale < 1.0f && gameLocal.time - lastDmgTime > 500 ) {
				if ( scale < 1.0f ) {
					scale += 0.05f;
				}
				if ( scale > 1.0f ) {
					scale = 1.0f;
				}
				g_damageScale.SetFloat( scale );
			}
		}*/

#if 0
		// update GUIs, Items, and character interactions
		UpdateFocus();
#endif
		
		UpdateLocation();

		// update player script
		UpdateScript();

		// service animations
		if ( !spectating && !af.IsActive() && !gameLocal.inCinematic ) {
			// Update the button state, this calls PerformKeyRelease() if a button has been released
			m_ButtonStateTracker.Update();

			UpdateConditions();

			UpdateAnimState();
			CheckBlink();
		}

		// clear out our pain flag so we can tell if we recieve any damage between now and the next time we think
		AI_PAIN = false;
	}

	// calculate the exact bobbed view position, which is used to
	// position the view weapon, among other things
	CalculateFirstPersonView();

	// this may use firstPersonView, or a thirdPeroson / camera view
	CalculateRenderView();

	FrobCheck();

	// Check if we just hit the attack button
	idEntity* frobbedEnt = g_Global.m_DarkModPlayer->m_FrobEntity.GetEntity();

	if (frobbedEnt != NULL && usercmd.buttons & BUTTON_ATTACK && !(oldButtons & BUTTON_ATTACK))
	{
		frobbedEnt->AttackAction(this);
	}

/*
	// TODO: remove this because it is just to determine how to fill out the renderstructure.
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("RenderViewId: %u\r", renderView->viewID);
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("x: %u   y: %u   w: %u   h: %u\r", renderView->x, renderView->y, renderView->width, renderView->height);
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("FovX: %f   FovY: %f\r", renderView->fov_x, renderView->fov_y);
	DM_LOGVECTOR3(LC_LIGHT, LT_DEBUG, "vieworg", renderView->vieworg);
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("cramZNear: %u   forceUpdate: %u\r", renderView->cramZNear, renderView->forceUpdate);
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("time: %u\r", renderView->time);
	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("time: %u\r", renderView->globalMaterial);
	for(i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++)
		DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Param[%u]: %f\r", i, renderView->shaderParms[i]);
*/
	if ( spectating ) {
		UpdateSpectating();
	} else if ( health > 0 ) {
		UpdateWeapon();
	}

	UpdateAir();

	// greebo: update underwater overlay and sounds
	UpdateUnderWaterEffects();
	
	UpdateHUD();

	UpdatePowerUps();

	UpdateDeathSkin( false );

	if ( gameLocal.isMultiplayer ) {
		DrawPlayerIcons();
	}

	if ( head.GetEntity() ) {
		headRenderEnt = head.GetEntity()->GetRenderEntity();
	} else {
		headRenderEnt = NULL;
	}

	if ( headRenderEnt ) {
		if ( influenceSkin ) {
			headRenderEnt->customSkin = influenceSkin;
		} else {
			headRenderEnt->customSkin = NULL;
		}
	}

	if ( gameLocal.isMultiplayer || g_showPlayerShadow.GetBool() ) {
		renderEntity.suppressShadowInViewID	= 0;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = 0;
		}
	} else {
		renderEntity.suppressShadowInViewID	= entityNumber+1;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = entityNumber+1;
		}
	}
	// never cast shadows from our first-person muzzle flashes
	renderEntity.suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	if ( headRenderEnt ) {
		headRenderEnt->suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	}

	if ( !g_stopTime.GetBool() ) {
		UpdateAnimation();

        Present();

		UpdateDamageEffects();

		LinkCombat();

		playerView.CalculateShake();
	}
	
	if ( !( thinkFlags & TH_THINK ) ) {
		gameLocal.Printf( "player %d not thinking?\n", entityNumber );
	}

	if ( g_showEnemies.GetBool() ) {
		idActor *ent;
		int num = 0;
		for( ent = enemyList.Next(); ent != NULL; ent = ent->enemyNode.Next() ) {
			gameLocal.Printf( "enemy (%d)'%s'\n", ent->entityNumber, ent->name.c_str() );
			gameRenderWorld->DebugBounds( colorRed, ent->GetPhysics()->GetBounds().Expand( 2 ), ent->GetPhysics()->GetOrigin() );
			num++;
		}
		gameLocal.Printf( "%d: enemies\n", num );
	}

	if (cv_pm_show_waterlevel.GetBool())
	{
		idStr waterStr;
		switch (physicsObj.GetWaterLevel())
		{
			case WATERLEVEL_NONE: waterStr = "WATERLEVEL: NONE"; break;
			case WATERLEVEL_FEET: waterStr = "WATERLEVEL: FEET"; break;
			case WATERLEVEL_WAIST: waterStr = "WATERLEVEL: WAIST"; break;
			case WATERLEVEL_HEAD: waterStr = "WATERLEVEL: HEAD"; break;
			default: waterStr = "WATERLEVEL: ???"; break;
		};
		gameRenderWorld->DrawText(waterStr.c_str(), GetEyePosition() + viewAxis.ToAngles().ToForward()*200, 0.7f, colorWhite, viewAxis, 1, 16);
	}

	// determine if portal sky is in pvs
	gameLocal.portalSkyActive = gameLocal.pvs.CheckAreasForPortalSky( gameLocal.GetPlayerPVS(), GetPhysics()->GetOrigin() );
}

/*
=================
idPlayer::RouteGuiMouse
=================
*/
void idPlayer::RouteGuiMouse( idUserInterface *gui ) {
	sysEvent_t ev;
	const char *command;

	/*if ( gui == m_overlays.findInteractive() ) {
		// Ensure that gui overlays are always able to execute commands,
		// even if the player isn't moving the mouse.
		ev = sys->GenerateMouseMoveEvent( usercmd.mx - oldMouseX, usercmd.my - oldMouseY );
		command = gui->HandleEvent( &ev, gameLocal.time );
		HandleGuiCommands( this, command );
		oldMouseX = usercmd.mx;
		oldMouseY = usercmd.my;
	}*/
		
	if ( usercmd.mx != oldMouseX || usercmd.my != oldMouseY ) {
		ev = sys->GenerateMouseMoveEvent( usercmd.mx - oldMouseX, usercmd.my - oldMouseY );
		command = gui->HandleEvent( &ev, gameLocal.time );
		oldMouseX = usercmd.mx;
		oldMouseY = usercmd.my;
	}
}

/*
==================
idPlayer::LookAtKiller
==================
*/
void idPlayer::LookAtKiller( idEntity *inflictor, idEntity *attacker ) {
	idVec3 dir;
	
	if ( attacker && attacker != this ) {
		dir = attacker->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else if ( inflictor && inflictor != this ) {
		dir = inflictor->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else {
		dir = viewAxis[ 0 ];
	}

	idAngles ang( 0, dir.ToYaw(), 0 );
	SetViewAngles( ang );
}

/*
==============
idPlayer::Kill
==============
*/
void idPlayer::Kill( bool delayRespawn, bool nodamage ) {
	if ( spectating ) {
		SpectateFreeFly( false );
	} else if ( health > 0 ) {
		godmode = false;
		if ( nodamage ) {
			ServerSpectate( true );
			forceRespawn = true;
		} else {
			Damage( this, this, vec3_origin, "damage_suicide", 1.0f, INVALID_JOINT );
			if ( delayRespawn ) {
				forceRespawn = false;
				int delay = spawnArgs.GetInt( "respawn_delay" );
				minRespawnTime = gameLocal.time + SEC2MS( delay );
				maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
			}
		}
	}
}

/*
==================
idPlayer::Killed
==================
*/
void idPlayer::Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location ) {
	float delay;

	assert( !gameLocal.isClient );

	// stop taking knockback once dead
	fl.noknockback = true;
	if ( health < -999 ) {
		health = -999;
	}

	if ( AI_DEAD ) {
		AI_PAIN = true;
		return;
	}

	heartInfo.Init( 0, 0, 0, BASE_HEARTRATE );
	AdjustHeartRate( DEAD_HEARTRATE, 10.0f, 0.0f, true );

	AI_DEAD = true;

	// greebo: Before posting mission failed and going to ragdoll, check for custom death script
	if (gameLocal.world != NULL && gameLocal.world->spawnArgs.GetInt("custom_death_delay") > 0)
	{
		// Run the death event in a few seconds
		PostEventMS(&EV_Player_CustomDeath, SEC2MS(gameLocal.world->spawnArgs.GetInt("custom_death_delay")));
		return; // stop death processing here
	}

	this->PostEventMS( &EV_Player_MissionFailed, spawnArgs.GetInt("death_transit_time", "2000") );

	SetAnimState( ANIMCHANNEL_LEGS, "Legs_Death", 4 );
	SetAnimState( ANIMCHANNEL_TORSO, "Torso_Death", 4 );
	SetWaitState( "" );

	animator.ClearAllJoints();

// NOTE: RespawnTime not used anymore in TDM except maybe for third person death later?
	if ( StartRagdoll() ) {
		pm_modelView.SetInteger( 0 );
		minRespawnTime = gameLocal.time + RAGDOLL_DEATH_TIME;
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	} else {
		// don't allow respawn until the death anim is done
		// g_forcerespawn may force spawning at some later time
		delay = spawnArgs.GetFloat( "respawn_delay" );
		minRespawnTime = gameLocal.time + SEC2MS( delay );
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	}

	physicsObj.SetMovementType( PM_DEAD );

	// greebo: Prevent all movement, weapon and item usage when dead
	SetImmobilization("death", EIM_ALL);

	// Play the death sound on the voice channel
	idStr deathSound = (physicsObj.GetWaterLevel() >= WATERLEVEL_HEAD) ? "snd_death_liquid" : "snd_death";
	StartSound( deathSound, SND_CHANNEL_VOICE, 0, false, NULL );
	
	StopSound( SND_CHANNEL_BODY2, false );

	fl.takedamage = true;		// can still be gibbed

	// get rid of weapon
	weapon.GetEntity()->OwnerDied();

	// drop the weapon as an item
	DropWeapon( true );

	if ( !g_testDeath.GetBool() ) {
		LookAtKiller( inflictor, attacker );
	}

	if ( gameLocal.isMultiplayer || g_testDeath.GetBool() ) {
		idPlayer *killer = NULL;
		// no gibbing in MP. Event_Gib will early out in MP
		if ( attacker->IsType( idPlayer::Type ) ) {
			killer = static_cast<idPlayer*>(attacker);
			if ( health < -20 ) {
				gibDeath = true;
				gibsDir = dir;
				gibsLaunched = false;
			}
		}
		gameLocal.mpGame.PlayerDeath( this, killer, isTelefragged );
	} else
	{
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
		// SR CONTENTS_RESPONSE FIX
		if( m_StimResponseColl->HasResponse() )
		{
			physicsObj.SetContents( physicsObj.GetContents() | CONTENTS_RESPONSE );
		}
	}

	UpdateVisuals();

	isChatting = false;
}

/*
=====================
idPlayer::GetAIAimTargets

Returns positions for the AI to aim at.
=====================
*/
void idPlayer::GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos ) {
	idVec3 offset;
	idMat3 axis;
	idVec3 origin;
	
	origin = lastSightPos - physicsObj.GetOrigin();

	GetJointWorldTransform( chestJoint, gameLocal.time, offset, axis );
	headPos = offset + origin;

	GetJointWorldTransform( headJoint, gameLocal.time, offset, axis );
	chestPos = offset + origin;
}

/*
================
idPlayer::DamageFeedback

callback function for when another entity recieved damage from this entity.  damage can be adjusted and returned to the caller.
================
*/
void idPlayer::DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage ) {
	assert( !gameLocal.isClient );
	damage = static_cast<int>(damage * PowerUpModifier( BERSERK ));
	if ( damage && ( victim != this ) && victim->IsType( idActor::Type ) ) {
		SetLastHitTime( gameLocal.time );
	}

	// greebo: Update the mission statistics, we've damaged another actor
	gameLocal.m_MissionData->AIDamagedByPlayer(damage);
}

/*
=================
idPlayer::CalcDamagePoints

Calculates how many health and armor points will be inflicted, but
doesn't actually do anything with them.  This is used to tell when an attack
would have killed the player, possibly allowing a "saving throw"
=================
*/
void idPlayer::CalcDamagePoints( idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor ) {
	int		damage;
	int		armorSave;

	damageDef->GetInt( "damage", "20", damage );
	damage = GetDamageForLocation( damage, location );

	idPlayer *player = attacker->IsType( idPlayer::Type ) ? static_cast<idPlayer*>(attacker) : NULL;
	if ( !gameLocal.isMultiplayer ) {
		if ( inflictor != gameLocal.world ) {
			switch ( g_skill.GetInteger() ) {
				case 0: 
					damage *= 0.80f;
					if ( damage < 1 ) {
						damage = 1;
					}
					break;
				case 2:
					damage *= 1.70f;
					break;
				case 3:
					damage *= 3.5f;
					break;
				default:
					break;
			}
		}
	}

	damage *= damageScale;

	// always give half damage if hurting self
	if ( attacker == this ) {
		if ( gameLocal.isMultiplayer ) {
			// only do this in mp so single player plasma and rocket splash is very dangerous in close quarters
			damage *= damageDef->GetFloat( "selfDamageScale", "0.5" );
		} else {
			damage *= damageDef->GetFloat( "selfDamageScale", "1" );
		}
	}

	// check for completely getting out of the damage
	if ( !damageDef->GetBool( "noGod" ) ) {
		// check for godmode
		if ( godmode ) {
			damage = 0;
		}
	}

	// inform the attacker that they hit someone
	attacker->DamageFeedback( this, inflictor, damage );

	// save some from armor
	if ( !damageDef->GetBool( "noArmor" ) ) {
		/*float armor_protection;

		armor_protection = ( gameLocal.isMultiplayer ) ? g_armorProtectionMP.GetFloat() : g_armorProtection.GetFloat();

		armorSave = ceil( damage * armor_protection );
		if ( armorSave >= inventory.armor ) {
			armorSave = inventory.armor;
		}*/
		armorSave = 0;

		if ( !damage ) {
			armorSave = 0;
		} else if ( armorSave >= damage ) {
			armorSave = damage - 1;
			damage = 1;
		} else {
			damage -= armorSave;
		}
	} else {
		armorSave = 0;
	}

	// check for team damage
	if ( gameLocal.gameType == GAME_TDM
		&& !gameLocal.serverInfo.GetBool( "si_teamDamage" )
		&& !damageDef->GetBool( "noTeam" )
		&& player
		&& player != this		// you get self damage no matter what
		&& player->team == team ) {
			damage = 0;
	}

	*health = damage;
	*armor = armorSave;
}

/*
============
Damage

this		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: this=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback in global space

damageDef	an idDict with all the options for damage effects

inflictor, attacker, dir, and point can be NULL for environmental effects
============
*/
void idPlayer::Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir,
					   const char *damageDefName, const float damageScale, const int location, 
					   trace_t *collision ) 
{
	idVec3		kick;
	int			damage;
	int			armorSave;
	int			knockback;
	idVec3		damage_from;
	idVec3		localDamageVector;	
	float		attackerPushScale;

	// damage is only processed on server
	if ( gameLocal.isClient ) {
		return;
	}
	
	if ( !fl.takedamage || noclip || spectating || gameLocal.inCinematic ) {
		return;
	}

	if ( !inflictor ) {
		inflictor = gameLocal.world;
	}
	if ( !attacker ) {
		attacker = gameLocal.world;
	}

	if ( attacker->IsType( idAI::Type ) ) {
		// don't take damage from monsters during influences
		if ( influenceActive != 0 ) {
			return;
		}
	}

	const idDeclEntityDef *damageDef = gameLocal.FindEntityDef( damageDefName, false );
	if ( !damageDef ) {
		gameLocal.Warning( "Unknown damageDef '%s'", damageDefName );
		return;
	}

	if ( damageDef->dict.GetBool( "ignore_player" ) ) {
		return;
	}

	CalcDamagePoints( inflictor, attacker, &damageDef->dict, damageScale, location, &damage, &armorSave );

	// determine knockback
	damageDef->dict.GetInt( "knockback", "20", knockback );

	if ( knockback != 0 && !fl.noknockback ) {
		if ( attacker == this ) {
			damageDef->dict.GetFloat( "attackerPushScale", "0", attackerPushScale );
		} else {
			attackerPushScale = 1.0f;
		}

		kick = dir;
		kick.Normalize();
		kick *= g_knockback.GetFloat() * knockback * attackerPushScale / 200.0f;
		physicsObj.SetLinearVelocity( physicsObj.GetLinearVelocity() + kick );

		// set the timer so that the player can't cancel out the movement immediately
		physicsObj.SetKnockBack( idMath::ClampInt( 50, 200, knockback * 2 ) );
	}

	// give feedback on the player view and audibly when armor is helping
	/*if ( armorSave ) {
		inventory.armor -= armorSave;

		if ( gameLocal.time > lastArmorPulse + 200 ) {
			StartSound( "snd_hitArmor", SND_CHANNEL_ITEM, 0, false, NULL );
		}
		lastArmorPulse = gameLocal.time;
	}*/
	
	if ( damageDef->dict.GetBool( "burn" ) ) 
	{
		StartSound( "snd_burn", SND_CHANNEL_BODY3, 0, false, NULL );
	}

	if ( g_debugDamage.GetInteger() ) {
		gameLocal.Printf( "client:%i health:%i damage:%i armor:%i\n", 
			entityNumber, health, damage, armorSave );
	}

	// move the world direction vector to local coordinates
	damage_from = dir;
	damage_from.Normalize();
	
	viewAxis.ProjectVector( damage_from, localDamageVector );

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( health > 0 ) {
		playerView.DamageImpulse( localDamageVector, &damageDef->dict );
	}

	// do the damage
	if ( damage > 0 ) {

		if ( !gameLocal.isMultiplayer ) {
			float scale = g_damageScale.GetFloat();
			/*if ( g_useDynamicProtection.GetBool() && g_skill.GetInteger() < 2 ) {
				if ( gameLocal.time > lastDmgTime + 500 && scale > 0.25f ) {
					scale -= 0.05f;
					g_damageScale.SetFloat( scale );
				}
			}*/

			if ( scale > 0.0f ) {
				damage *= scale;
			}
		}

		if ( damage < 1 ) {
			damage = 1;
		}

//		int oldHealth = health;
		health -= damage;

		// greebo: Update mission statistics, we've taken damage
		gameLocal.m_MissionData->PlayerDamaged(damage);

		if ( health <= 0 ) {

			if ( health < -999 ) {
				health = -999;
			}

			isTelefragged = damageDef->dict.GetBool( "telefrag" );

			lastDmgTime = gameLocal.time;
			Killed( inflictor, attacker, damage, dir, location );

		} else {
			// force a blink
			blink_time = 0;

			if (!damageDef->dict.GetBool( "no_pain" )) {
				// let the anim script know we took damage
				AI_PAIN = Pain( inflictor, attacker, damage, dir, location, &damageDef->dict );
				if(AI_PAIN)
					gameLocal.Printf("Player:AI_PAIN set to true\n");
			}
			
			// FIX: if drowning, stop pain SFX and play drown SFX on voice channel
			if ( damageDef->dict.GetBool( "no_air" ) ) 
			{
				if ( !armorSave && health > 0 ) 
				{
					StopSound( SND_CHANNEL_VOICE, false );
					StartSound( "snd_airGasp", SND_CHANNEL_VOICE, 0, false, NULL );
				}
			}

			if ( !g_testDeath.GetBool() ) {
				lastDmgTime = gameLocal.time;
			}
		}
	} else {
		// don't accumulate impulses
		if ( af.IsLoaded() ) {
			// clear impacts
			af.Rest();

			// physics is turned off by calling af.Rest()
			BecomeActive( TH_PHYSICS );
		}
	}

	lastDamageDef = damageDef->Index();
	lastDamageDir = damage_from;
	lastDamageLocation = location;
}

/*
===========
idPlayer::Teleport
============
*/
void idPlayer::Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination ) {
	idVec3 org;

	if ( weapon.GetEntity() ) {
		weapon.GetEntity()->LowerWeapon();
	}

	SetOrigin( origin + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	if ( !gameLocal.isMultiplayer && GetFloorPos( 16.0f, org ) ) {
		SetOrigin( org );
	}

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	GetPhysics()->SetLinearVelocity( vec3_origin );

	SetViewAngles( angles );

	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	if ( gameLocal.isMultiplayer ) {
		playerView.Flash( colorWhite, 140 );
	}

	UpdateVisuals();

	teleportEntity = destination;

	if ( !gameLocal.isClient && !noclip ) {
		if ( gameLocal.isMultiplayer ) {
			// kill anything at the new position or mark for kill depending on immediate or delayed teleport
			gameLocal.KillBox( this, destination != NULL );
		} else {
			// kill anything at the new position
			gameLocal.KillBox( this, true );
		}
	}
}

/*
====================
idPlayer::SetPrivateCameraView
====================
*/
void idPlayer::SetPrivateCameraView( idCamera *camView ) {
	privateCameraView = camView;
	if ( camView ) {
		StopFiring();
		Hide();
	} else {
		if ( !spectating ) {
			Show();
		}
	}
}

/*
====================
idPlayer::DefaultFov

Returns the base FOV
====================
*/
float idPlayer::DefaultFov( void ) const {
	float fov;

	fov = g_fov.GetFloat();
	if ( gameLocal.isMultiplayer ) {
		if ( fov < 90.0f ) {
			return 90.0f;
		} else if ( fov > 110.0f ) {
			return 110.0f;
		}
	}

	return fov;
}

/*
====================
idPlayer::CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
float idPlayer::CalcFov( bool honorZoom ) {
	float fov;
	float defaultFov = DefaultFov();

	if ( fxFov ) {
		return defaultFov + 10.0f + cos( ( gameLocal.time + 2000 ) * 0.01 ) * 10.0f;
	}

	if ( influenceFov ) {
		return influenceFov;
	}

	// prevent FOV from zooming if the player is holding an object
	if( gameLocal.m_Grabber->GetSelected() ) {
		return defaultFov;
	}

	// Check if a transition is still ongoing
	bool zoomDone = zoomFov.IsDone(gameLocal.time);
	float curZoomFov = zoomFov.GetCurrentValue(gameLocal.time);

	// Take the interpolation value, if the zoom is enabled, take the default otherwise
	fov = (zoomFov.Enabled()) ? curZoomFov : defaultFov;

	// Check if we need the zoom interpolation anymore
	if (zoomFov.Enabled() && zoomDone && curZoomFov == defaultFov)
	{
		// Zoom is done and is equal to default fov, so we no longer need it
		zoomFov.SetEnabled(false);
		fov = DefaultFov();
	}

	// bound normal viewsize
	if ( fov < 1 ) {
		fov = 1;
	} else if ( fov > 179 ) {
		fov = 179;
	}

	return fov;
}

/*
==============
idPlayer::GunTurningOffset

generate a rotational offset for the gun based on the view angle
history in loggedViewAngles
==============
*/
idAngles idPlayer::GunTurningOffset( void ) {
	idAngles	a;

	a.Zero();

	if ( gameLocal.framenum < NUM_LOGGED_VIEW_ANGLES ) {
		return a;
	}

	idAngles current = loggedViewAngles[ gameLocal.framenum & (NUM_LOGGED_VIEW_ANGLES-1) ];

	idAngles	av, base;
	int weaponAngleOffsetAverages;
	float weaponAngleOffsetScale, weaponAngleOffsetMax;

	weapon.GetEntity()->GetWeaponAngleOffsets( &weaponAngleOffsetAverages, &weaponAngleOffsetScale, &weaponAngleOffsetMax );

	av = current;

	// calcualte this so the wrap arounds work properly
	for ( int j = 1 ; j < weaponAngleOffsetAverages ; j++ ) {
		idAngles a2 = loggedViewAngles[ ( gameLocal.framenum - j ) & (NUM_LOGGED_VIEW_ANGLES-1) ];

		idAngles delta = a2 - current;

		if ( delta[1] > 180 ) {
			delta[1] -= 360;
		} else if ( delta[1] < -180 ) {
			delta[1] += 360;
		}

		av += delta * ( 1.0f / weaponAngleOffsetAverages );
	}

	a = ( av - current ) * weaponAngleOffsetScale;

	for ( int i = 0 ; i < 3 ; i++ ) {
		if ( a[i] < -weaponAngleOffsetMax ) {
			a[i] = -weaponAngleOffsetMax;
		} else if ( a[i] > weaponAngleOffsetMax ) {
			a[i] = weaponAngleOffsetMax;
		}
	}

	return a;
}

/*
==============
idPlayer::GunAcceleratingOffset

generate a positional offset for the gun based on the movement
history in loggedAccelerations
==============
*/
idVec3	idPlayer::GunAcceleratingOffset( void ) {
	idVec3	ofs;

	float weaponOffsetTime, weaponOffsetScale;

	ofs.Zero();

	weapon.GetEntity()->GetWeaponTimeOffsets( &weaponOffsetTime, &weaponOffsetScale );

	int stop = currentLoggedAccel - NUM_LOGGED_ACCELS;
	if ( stop < 0 ) {
		stop = 0;
	}
	for ( int i = currentLoggedAccel-1 ; i > stop ; i-- ) {
		loggedAccel_t	*acc = &loggedAccel[i&(NUM_LOGGED_ACCELS-1)];

		float	f;
		float	t = gameLocal.time - acc->time;
		if ( t >= weaponOffsetTime ) {
			break;	// remainder are too old to care about
		}

		f = t / weaponOffsetTime;
		f = ( cos( f * 2.0f * idMath::PI ) - 1.0f ) * 0.5f;
		ofs += f * weaponOffsetScale * acc->dir;
	}

	return ofs;
}

/*
==============
idPlayer::CalculateViewWeaponPos

Calculate the bobbing position of the view weapon
==============
*/
void idPlayer::CalculateViewWeaponPos( idVec3 &origin, idMat3 &axis ) {
	float		scale;
	float		fracsin1;
	float		fracsin2;
	float		fracsin3;
	idAngles	angles;
	int			delta;

	// CalculateRenderView must have been called first
	const idVec3 &viewOrigin = firstPersonViewOrigin;
	const idMat3 &viewAxis = firstPersonViewAxis;

	// these cvars are just for hand tweaking before moving a value to the weapon def
	idVec3	gunpos( g_gun_x.GetFloat(), g_gun_y.GetFloat(), g_gun_z.GetFloat() );

	// as the player changes direction, the gun will take a small lag
	idVec3	gunOfs = GunAcceleratingOffset();
	origin = viewOrigin + ( gunpos + gunOfs ) * viewAxis;

	// on odd legs, invert some angles
	if ( bobCycle & 128 ) {
		scale = -xyspeed;
	} else {
		scale = xyspeed;
	}

	// gun angles from bobbing
	angles.roll		= scale * bobfracsin * 0.005f;
	angles.yaw		= scale * bobfracsin * 0.01f;
	angles.pitch	= xyspeed * bobfracsin * 0.005f;

	// gun angles from turning
	if ( gameLocal.isMultiplayer ) {
		idAngles offset = GunTurningOffset();
		offset *= g_mpWeaponAngleScale.GetFloat();
		angles += offset;
	} else {
		angles += GunTurningOffset();
	}

	idVec3 gravity = physicsObj.GetGravityNormal();

	// drop the weapon when landing after a jump / fall
	delta = gameLocal.time - landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		origin -= gravity * ( landChange*0.25f * delta / LAND_DEFLECT_TIME );
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		origin -= gravity * ( landChange*0.25f * (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME );
	}

	// speed sensitive idle drift
	// Dram: Changed so that each axis has it's own fracsin. Now they move independantly of each other
	scale = xyspeed + 40.0f;
	fracsin1 = scale * sin( MS2SEC( gameLocal.time * 0.42 ) ) * 0.006f;
	fracsin2 = scale * sin( MS2SEC( gameLocal.time * 1.63 ) ) * 0.008f;
	fracsin3 = scale * sin( MS2SEC( gameLocal.time * 2 ) ) * 0.01f;
	angles.roll		+= fracsin1;
	angles.yaw		+= fracsin2;
	angles.pitch	+= fracsin3;

	axis = angles.ToMat3() * viewAxis;
}

/*
===============
idPlayer::OffsetThirdPersonView
===============
*/
void idPlayer::OffsetThirdPersonView( float angle, float range, float height, bool clip ) {
	idVec3			view;
	idVec3			focusAngles;
	trace_t			trace;
	idVec3			focusPoint;
	float			focusDist;
	float			forwardScale, sideScale;
	idVec3			origin;
	idAngles		angles;
	idMat3			axis;
	idBounds		bounds;

	angles = viewAngles;
	GetViewPos( origin, axis );

	if ( angle ) {
		angles.pitch = 0.0f;
	}

	if ( angles.pitch > 45.0f ) {
		angles.pitch = 45.0f;		// don't go too far overhead
	}

	focusPoint = origin + angles.ToForward() * THIRD_PERSON_FOCUS_DISTANCE;
	focusPoint.z += height;
	view = origin;
	view.z += 8 + height;

	angles.pitch *= 0.5f;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();

	idMath::SinCos( DEG2RAD( angle ), sideScale, forwardScale );
	view -= range * forwardScale * renderView->viewaxis[ 0 ];
	view += range * sideScale * renderView->viewaxis[ 1 ];

	if ( clip ) {
		// trace a ray from the origin to the viewpoint to make sure the view isn't
		// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything
		bounds = idBounds( idVec3( -4, -4, -4 ), idVec3( 4, 4, 4 ) );
		gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
		if ( trace.fraction != 1.0f ) {
			view = trace.endpos;
			view.z += ( 1.0f - trace.fraction ) * 32.0f;

			// try another trace to this position, because a tunnel may have the ceiling
			// close enough that this is poking out
			gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
			view = trace.endpos;
		}
	}

	// select pitch to look at focus point from vieword
	focusPoint -= view;
	focusDist = idMath::Sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1.0f ) {
		focusDist = 1.0f;	// should never happen
	}

	angles.pitch = - RAD2DEG( atan2( focusPoint.z, focusDist ) );
	angles.yaw -= angle;

	renderView->vieworg = view;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();
	renderView->viewID = 0;
}

/*
===============
idPlayer::GetEyePosition
===============
*/
idVec3 idPlayer::GetEyePosition( void ) const {
	idVec3 org;
 
	// use the smoothed origin if spectating another player in multiplayer
	if ( gameLocal.isClient && entityNumber != gameLocal.localClientNum ) {
		org = smoothedOrigin;
	} else {
		org = GetPhysics()->GetOrigin();
	}

	/*!
	* Lean Mod
	* @author sophisticatedZombie (DH)
	* Move eye position due to leaning
	* angua: need to check whether physics type is correct, dead player uses AF physics
	*/
	idPhysics* physics = GetPhysics();
	if (physics->IsType(idPhysics_Player::Type))
	{
		org += static_cast<idPhysics_Player*>(physics)->GetViewLeanTranslation();
	}

	// This was in SDK untouched
	return org + ( GetPhysics()->GetGravityNormal() * -eyeOffset.z );
}

/*
===============
idPlayer::GetViewPos
===============
*/
void idPlayer::GetViewPos( idVec3 &origin, idMat3 &axis ) const {
	idAngles angles;

	// if dead, fix the angle and don't add any kick
	if ( health <= 0 ) {
		angles.yaw = viewAngles.yaw;
		angles.roll = 40;
		angles.pitch = -15;
		axis = angles.ToMat3();
		origin = GetEyePosition();
	} else {
		origin = GetEyePosition() + viewBob;
		angles = viewAngles + viewBobAngles + physicsObj.GetViewLeanAngles() + playerView.AngleOffset();

		axis = angles.ToMat3() * physicsObj.GetGravityAxis();

		// adjust the origin based on the camera nodal distance (eye distance from neck)
		origin += physicsObj.GetGravityNormal() * g_viewNodalZ.GetFloat();
		origin += axis[0] * g_viewNodalX.GetFloat() + axis[2] * g_viewNodalZ.GetFloat();
	}
}

/*
===============
idPlayer::CalculateFirstPersonView
===============
*/
void idPlayer::CalculateFirstPersonView( void )
{
	idPhysics* physics = GetPhysics();

	if ( ( pm_modelView.GetInteger() == 1 ) || ( ( pm_modelView.GetInteger() == 2 ) && ( health <= 0 ) ) ) 
	{
		//	Displays the view from the point of view of the "camera" joint in the player model

		idMat3 axis;
		idVec3 origin;
		idAngles ang = viewBobAngles + playerView.AngleOffset();

		/*!
		* Lean mod: 
		* @author: sophisticatedZombie
		*/
		if (physics->IsType(idPhysics_Player::Type))
		{
			ang += static_cast<idPhysics_Player*>(physics)->GetViewLeanAngles();
		}
		
		ang.yaw += viewAxis[ 0 ].ToYaw();
		
		jointHandle_t joint = animator.GetJointHandle( "camera" );
		animator.GetJointTransform( joint, gameLocal.time, origin, axis );
		firstPersonViewOrigin = ( origin + modelOffset ) * ( viewAxis * physicsObj.GetGravityAxis() ) + physicsObj.GetOrigin() + viewBob + physicsObj.GetViewLeanTranslation();
		firstPersonViewAxis = axis * ang.ToMat3() * physicsObj.GetGravityAxis();
	} else 
	{
		// offset for local bobbing and kicks
		GetViewPos( firstPersonViewOrigin, firstPersonViewAxis );
#if 0
		// shakefrom sound stuff only happens in first person
		firstPersonViewAxis = firstPersonViewAxis * playerView.ShakeAxis();
#endif
	}

	// Set the listener location (on the other side of a door if door leaning)
	if (physics->IsType(idPhysics_Player::Type) && 
		static_cast<idPhysics_Player*>(physics)->IsDoorLeaning() && 
		!gameLocal.inCinematic)
	{
		SetListenerLoc( m_DoorListenLoc );
	}
	else
	{
		SetListenerLoc( firstPersonViewOrigin );
	}
}

/*
==================
idPlayer::GetRenderView

Returns the renderView that was calculated for this tic
==================
*/
renderView_t *idPlayer::GetRenderView( void ) {
	return renderView;
}

/*
==================
idPlayer::CalculateRenderView

create the renderView for the current tic
==================
*/
void idPlayer::CalculateRenderView( void ) {
	int i;
	float range;

	if ( !renderView ) {
		renderView = new renderView_t;
	}
	memset( renderView, 0, sizeof( *renderView ) );

	// copy global shader parms
	for( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ ) {
		renderView->shaderParms[ i ] = gameLocal.globalShaderParms[ i ];
	}
	renderView->globalMaterial = gameLocal.GetGlobalMaterial();
	renderView->time = gameLocal.time;

	// calculate size of 3D view
	renderView->x = 0;
	renderView->y = 0;
	renderView->width = SCREEN_WIDTH;
	renderView->height = SCREEN_HEIGHT;
	renderView->viewID = 0;

	// check if we should be drawing from a camera's POV
	if ( !noclip && (gameLocal.GetCamera() || privateCameraView) ) {
		// get origin, axis, and fov
		if ( privateCameraView ) {
			privateCameraView->GetViewParms( renderView );
		} else {
			gameLocal.GetCamera()->GetViewParms( renderView );
		}
	} else {
		if ( g_stopTime.GetBool() ) {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			if ( !pm_thirdPerson.GetBool() ) {
				// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
				// allow the right player view weapons
				renderView->viewID = entityNumber + 1;
			}
		} else if ( pm_thirdPerson.GetBool() ) {
			OffsetThirdPersonView( pm_thirdPersonAngle.GetFloat(), pm_thirdPersonRange.GetFloat(), pm_thirdPersonHeight.GetFloat(), pm_thirdPersonClip.GetBool() );
		} else if ( pm_thirdPersonDeath.GetBool() ) {
			range = gameLocal.time < minRespawnTime ? ( gameLocal.time + RAGDOLL_DEATH_TIME - minRespawnTime ) * ( 120.0f / RAGDOLL_DEATH_TIME ) : 120.0f;
			OffsetThirdPersonView( 0.0f, 20.0f + range, 0.0f, false );
		} else {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
			// allow the right player view weapons
			renderView->viewID = entityNumber + 1;
		}
		
		// field of view
		gameLocal.CalcFov( CalcFov( true ), renderView->fov_x, renderView->fov_y );
	}

	if ( renderView->fov_y == 0 ) {
		common->Error( "renderView->fov_y == 0" );
	}

	if ( g_showviewpos.GetBool() ) {
		gameLocal.Printf( "%s : %s\n", renderView->vieworg.ToString(), renderView->viewaxis.ToAngles().ToString() );
	}
}

/*
=============
idPlayer::AddAIKill
=============
*/
void idPlayer::AddAIKill( void ) {
	// greebo: Disabled this routine, no soulcube in TDM
	return;
}

/*
=============
idPlayer::SetSoulCubeProjectile
=============
*/
void idPlayer::SetSoulCubeProjectile( idProjectile *projectile ) {
	soulCubeProjectile = projectile;
}

/*
=============
idPlayer::AddProjectilesFired
=============
*/
void idPlayer::AddProjectilesFired( int count ) {
	numProjectilesFired += count;
}

/*
=============
idPlayer::AddProjectileHites
=============
*/
void idPlayer::AddProjectileHits( int count ) {
	numProjectileHits += count;
}

/*
=============
idPlayer::SetLastHitTime
=============
*/
void idPlayer::SetLastHitTime( int time ) {
	idPlayer *aimed = NULL;

	if ( time && lastHitTime != time ) {
		lastHitToggle ^= 1;
	}
	lastHitTime = time;
	if ( !time ) {
		// level start and inits
		return;
	}
	if ( gameLocal.isMultiplayer && ( time - lastSndHitTime ) > 10 ) {
		lastSndHitTime = time;
		StartSound( "snd_hit_feedback", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
	}
	if ( cursor ) {
		cursor->HandleNamedEvent( "hitTime" );
	}
	if ( hud ) {
		if ( MPAim != -1 ) {
			if ( gameLocal.entities[ MPAim ] && gameLocal.entities[ MPAim ]->IsType( idPlayer::Type ) ) {
				aimed = static_cast< idPlayer * >( gameLocal.entities[ MPAim ] );
			}
			assert( aimed );
			// full highlight, no fade till loosing aim
			hud->SetStateString( "aim_text", gameLocal.userInfo[ MPAim ].GetString( "ui_name" ) );
			if ( aimed ) {
				hud->SetStateFloat( "aim_color", aimed->colorBarIndex );
			}
			hud->HandleNamedEvent( "aim_flash" );
			MPAimHighlight = true;
			MPAimFadeTime = 0;
		} else if ( lastMPAim != -1 ) {
			if ( gameLocal.entities[ lastMPAim ] && gameLocal.entities[ lastMPAim ]->IsType( idPlayer::Type ) ) {
				aimed = static_cast< idPlayer * >( gameLocal.entities[ lastMPAim ] );
			}
			assert( aimed );
			// start fading right away
			hud->SetStateString( "aim_text", gameLocal.userInfo[ lastMPAim ].GetString( "ui_name" ) );
			if ( aimed ) {
				hud->SetStateFloat( "aim_color", aimed->colorBarIndex );
			}
			hud->HandleNamedEvent( "aim_flash" );
			hud->HandleNamedEvent( "aim_fade" );
			MPAimHighlight = false;
			MPAimFadeTime = gameLocal.realClientTime;
		}
	}
}

/*
=============
idPlayer::SetInfluenceLevel
=============
*/
void idPlayer::SetInfluenceLevel( int level ) {
	if ( level != influenceActive ) {
		if ( level ) {
			for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
				if ( ent->IsType( idProjectile::Type ) ) {
					// remove all projectiles
					ent->PostEventMS( &EV_Remove, 0 );
				}
			}
			if ( weaponEnabled && weapon.GetEntity() ) {
				weapon.GetEntity()->EnterCinematic();
			}
		} else {
			physicsObj.SetLinearVelocity( vec3_origin );
			if ( weaponEnabled && weapon.GetEntity() ) {
				weapon.GetEntity()->ExitCinematic();
			}
		}
		influenceActive = level;
	}
}

/*
=============
idPlayer::SetInfluenceView
=============
*/
void idPlayer::SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent ) {
	influenceMaterial = NULL;
	influenceEntity = NULL;
	influenceSkin = NULL;
	if ( mtr && *mtr ) {
		influenceMaterial = declManager->FindMaterial( mtr );
	}
	if ( skinname && *skinname ) {
		influenceSkin = declManager->FindSkin( skinname );
		if ( head.GetEntity() ) {
			head.GetEntity()->GetRenderEntity()->shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		}
		UpdateVisuals();
	}
	influenceRadius = radius;
	if ( radius > 0.0f ) {
		influenceEntity = ent;
	}
}

/*
=============
idPlayer::SetInfluenceFov
=============
*/
void idPlayer::SetInfluenceFov( float fov ) {
	influenceFov = fov;
}

/*
================
idPlayer::OnLadder
================
*/
bool idPlayer::OnLadder( void ) const {
	return physicsObj.OnLadder();
}

CMultiStateMover* idPlayer::OnElevator(bool mustBeMoving) const 
{
	idEntity* ent = physicsObj.GetGroundEntity();

	// Return false if ground entity is not a mover
	if (ent == NULL || !ent->IsType(CMultiStateMover::Type)) return NULL;

	CMultiStateMover* mover = static_cast<CMultiStateMover*>(ent);

	if (mustBeMoving)
	{
		return (!mover->IsAtRest()) ? mover : NULL;
	}
	return mover;
}

/*
==================
idPlayer::Event_GetButtons
==================
*/
void idPlayer::Event_GetButtons( void ) {
	idThread::ReturnInt( usercmd.buttons );
}

/*
==================
idPlayer::Event_GetMove
==================
*/
void idPlayer::Event_GetMove( void ) {
	idVec3 move( usercmd.forwardmove, usercmd.rightmove, usercmd.upmove );
	idThread::ReturnVector( move );
}

/*
================
idPlayer::Event_GetViewAngles
================
*/
void idPlayer::Event_GetViewAngles( void ) {
	idThread::ReturnVector( idVec3( viewAngles[0], viewAngles[1], viewAngles[2] ) );
}

/*
==================
idPlayer::Event_StopFxFov
==================
*/
void idPlayer::Event_StopFxFov( void ) {
	fxFov = false;
}

/*
==================
idPlayer::StartFxFov 
==================
*/
void idPlayer::StartFxFov( float duration ) { 
	fxFov = true;
	PostEventSec( &EV_Player_StopFxFov, duration );
}

/*
==================
idPlayer::Event_EnableWeapon 
==================
*/
void idPlayer::Event_EnableWeapon( void ) {
	hiddenWeapon = gameLocal.world->spawnArgs.GetBool( "no_Weapons" );
	weaponEnabled = true;
	if ( weapon.GetEntity() ) {
		weapon.GetEntity()->ExitCinematic();
	}
}

/*
==================
idPlayer::Event_DisableWeapon
==================
*/
void idPlayer::Event_DisableWeapon( void ) {
	hiddenWeapon = gameLocal.world->spawnArgs.GetBool( "no_Weapons" );
	weaponEnabled = false;
	if ( weapon.GetEntity() ) {
		weapon.GetEntity()->EnterCinematic();
	}
}

/*
==================
idPlayer::Event_GetCurrentWeapon
==================
*/
void idPlayer::Event_GetCurrentWeapon( void ) {
	const char *weapon;

	if ( currentWeapon >= 0 ) {
		weapon = spawnArgs.GetString( va( "def_weapon%d", currentWeapon ) );
		idThread::ReturnString( weapon );
	} else {
		idThread::ReturnString( "" );
	}
}

/*
==================
idPlayer::Event_GetPreviousWeapon
==================
*/
void idPlayer::Event_GetPreviousWeapon( void ) {
	const char *weapon;

	if ( previousWeapon >= 0 ) {
		int pw = ( gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) ? 0 : previousWeapon;
		weapon = spawnArgs.GetString( va( "def_weapon%d", pw) );
		idThread::ReturnString( weapon );
	} else {
		idThread::ReturnString( spawnArgs.GetString( "def_weapon0" ) );
	}
}

/*
==================
idPlayer::Event_SelectWeapon
==================
*/
void idPlayer::Event_SelectWeapon( const char *weaponName )
{
	if ( gameLocal.isClient ) {
		gameLocal.Warning( "Cannot switch weapons from script in multiplayer" );
		return;
	}

	if ( hiddenWeapon && gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) {
		idealWeapon = weapon_fists;
		weapon.GetEntity()->HideWeapon();
		return;
	}

	int weaponNum = SlotForWeapon(weaponName);

	if (weaponNum != -1)
	{
		// Found, select it
		SelectWeapon(weaponNum, false);
	}
}

/*
==================
idPlayer::Event_GetWeaponEntity
==================
*/
void idPlayer::Event_GetWeaponEntity( void ) {
	idThread::ReturnEntity( weapon.GetEntity() );
}

/*
==================
idPlayer::Event_OpenPDA
==================
*/
void idPlayer::Event_OpenPDA( void ) {
	if ( !gameLocal.isMultiplayer ) {
		TogglePDA();
	}
}

/*
==================
idPlayer::Event_InPDA
==================
*/
void idPlayer::Event_InPDA( void ) {
	idThread::ReturnInt( objectiveSystemOpen );
}

/*
==================
idPlayer::TeleportDeath
==================
*/
void idPlayer::TeleportDeath( int killer ) {
	teleportKiller = killer;
}

/*
==================
idPlayer::Event_ExitTeleporter
==================
*/
void idPlayer::Event_ExitTeleporter( void ) {
	idEntity	*exitEnt;
	float		pushVel;

	// verify and setup
	exitEnt = teleportEntity.GetEntity();
	if ( !exitEnt ) {
		common->DPrintf( "Event_ExitTeleporter player %d while not being teleported\n", entityNumber );
		return;
	}

	pushVel = exitEnt->spawnArgs.GetFloat( "push", "300" );

	if ( gameLocal.isServer ) {
		ServerSendEvent( EVENT_EXIT_TELEPORTER, NULL, false, -1 );
	}

	SetPrivateCameraView( NULL );
	// setup origin and push according to the exit target
	SetOrigin( exitEnt->GetPhysics()->GetOrigin() + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	SetViewAngles( exitEnt->GetPhysics()->GetAxis().ToAngles() );
	physicsObj.SetLinearVelocity( exitEnt->GetPhysics()->GetAxis()[ 0 ] * pushVel );
	physicsObj.ClearPushedVelocity();
	// teleport fx
	playerView.Flash( colorWhite, 120 );

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	UpdateVisuals();

	StartSound( "snd_teleport_exit", SND_CHANNEL_ANY, 0, false, NULL );

	if ( teleportKiller != -1 ) {
		// we got killed while being teleported
		Damage( gameLocal.entities[ teleportKiller ], gameLocal.entities[ teleportKiller ], vec3_origin, "damage_telefrag", 1.0f, INVALID_JOINT );
		teleportKiller = -1;
	} else {
		// kill anything that would have waited at teleport exit
		gameLocal.KillBox( this );
	}
	teleportEntity = NULL;
}

/*
================
idPlayer::ClientPredictionThink
================
*/
void idPlayer::ClientPredictionThink( void ) {
	renderEntity_t *headRenderEnt;

	oldFlags = usercmd.flags;
	oldButtons = usercmd.buttons;

	usercmd = gameLocal.usercmds[ entityNumber ];

	if ( entityNumber != gameLocal.localClientNum ) 
	{
		// ignore attack button of other clients. that's no good for predictions

		usercmd.buttons &= ~BUTTON_ATTACK;
		// tdm: Also ignore block button
		usercmd.buttons &= ~BUTTON_ZOOM;
	}



	buttonMask &= usercmd.buttons;
	usercmd.buttons &= ~buttonMask;

	if ( objectiveSystemOpen ) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	// clear the ik before we do anything else so the skeleton doesn't get updated twice
	walkIK.ClearJointMods();

	if ( gameLocal.isNewFrame ) {
		if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) ) {
			PerformImpulse( usercmd.impulse );
		}
	}

	scoreBoardOpen = ( ( usercmd.buttons & BUTTON_SCORES ) != 0 || forceScoreBoard );

	AdjustSpeed();

	UpdateViewAngles();

	// update the smoothed view angles
	if ( gameLocal.framenum >= smoothedFrame && entityNumber != gameLocal.localClientNum ) {
		idAngles anglesDiff = viewAngles - smoothedAngles;
		anglesDiff.Normalize180();
		if ( idMath::Fabs( anglesDiff.yaw ) < 90.0f && idMath::Fabs( anglesDiff.pitch ) < 90.0f ) {
			// smoothen by pushing back to the previous angles
			viewAngles -= gameLocal.clientSmoothing * anglesDiff;
			viewAngles.Normalize180();
		}
		smoothedAngles = viewAngles;
	}
	smoothedOriginUpdated = false;

	if ( !af.IsActive() ) {
		AdjustBodyAngles();
	}

	if ( !isLagged ) {
		// don't allow client to move when lagged
		Move();
	} 

#if 0
	// update GUIs, Items, and character interactions
	UpdateFocus();
#endif

	// service animations
	if ( !spectating && !af.IsActive() ) {
		m_ButtonStateTracker.Update();
    	UpdateConditions();
		UpdateAnimState();
		CheckBlink();
	}

	// clear out our pain flag so we can tell if we recieve any damage between now and the next time we think
	AI_PAIN = false;

	// calculate the exact bobbed view position, which is used to
	// position the view weapon, among other things
	CalculateFirstPersonView();

	// this may use firstPersonView, or a thirdPerson / camera view
	CalculateRenderView();

	if ( !gameLocal.inCinematic && weapon.GetEntity() && ( health > 0 ) && !( gameLocal.isMultiplayer && spectating ) ) {
		UpdateWeapon();
	}

	UpdateHUD();

	if ( gameLocal.isNewFrame ) {
		UpdatePowerUps();
	}

	UpdateDeathSkin( false );

	if ( head.GetEntity() ) {
		headRenderEnt = head.GetEntity()->GetRenderEntity();
	} else {
		headRenderEnt = NULL;
	}

	if ( headRenderEnt ) {
		if ( influenceSkin ) {
			headRenderEnt->customSkin = influenceSkin;
		} else {
			headRenderEnt->customSkin = NULL;
		}
	}

	if ( gameLocal.isMultiplayer || g_showPlayerShadow.GetBool() ) {
		renderEntity.suppressShadowInViewID	= 0;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = 0;
		}
	} else {
		renderEntity.suppressShadowInViewID	= entityNumber+1;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = entityNumber+1;
		}
	}
	// never cast shadows from our first-person muzzle flashes
	renderEntity.suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	if ( headRenderEnt ) {
		headRenderEnt->suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	}

	if ( !gameLocal.inCinematic ) {
		UpdateAnimation();
	}

	if ( gameLocal.isMultiplayer ) {
		DrawPlayerIcons();
	}

	Present();

	UpdateDamageEffects();

	LinkCombat();

	if ( gameLocal.isNewFrame && entityNumber == gameLocal.localClientNum ) {
		playerView.CalculateShake();
	}

	// determine if portal sky is in pvs

	pvsHandle_t	clientPVS = gameLocal.pvs.SetupCurrentPVS( GetPVSAreas(), GetNumPVSAreas() );

	gameLocal.portalSkyActive = gameLocal.pvs.CheckAreasForPortalSky( clientPVS, GetPhysics()->GetOrigin() );

	gameLocal.pvs.FreeCurrentPVS( clientPVS );

}

/*
================
idPlayer::GetPhysicsToVisualTransform
================
*/
bool idPlayer::GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis ) {
	if ( af.IsActive() ) {
		af.GetPhysicsToVisualTransform( origin, axis );
		return true;
	}

	// smoothen the rendered origin and angles of other clients
	if ( gameLocal.isClient && gameLocal.framenum >= smoothedFrame && ( entityNumber != gameLocal.localClientNum || selfSmooth ) ) {

		// render origin and axis
		idMat3 renderAxis = viewAxis * GetPhysics()->GetAxis();
		idVec3 renderOrigin = GetPhysics()->GetOrigin() + modelOffset * renderAxis;

		// update the smoothed origin
		if ( !smoothedOriginUpdated ) {
			idVec2 originDiff = renderOrigin.ToVec2() - smoothedOrigin.ToVec2();
			if ( originDiff.LengthSqr() < Square( 100.0f ) ) {
				// smoothen by pushing back to the previous position
				if ( selfSmooth ) {

					assert( entityNumber == gameLocal.localClientNum );

					renderOrigin.ToVec2() -= net_clientSelfSmoothing.GetFloat() * originDiff;

				} else {

					renderOrigin.ToVec2() -= gameLocal.clientSmoothing * originDiff;
				}
			}
			smoothedOrigin = renderOrigin;

			smoothedFrame = gameLocal.framenum;
			smoothedOriginUpdated = true;
		}

		axis = idAngles( 0.0f, smoothedAngles.yaw, 0.0f ).ToMat3();
		origin = ( smoothedOrigin - GetPhysics()->GetOrigin() ) * axis.Transpose();

	} else {

		axis = viewAxis;
		origin = modelOffset;
	}
	return true;
}

/*
================
idPlayer::GetPhysicsToSoundTransform
================
*/
bool idPlayer::GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis ) {
	idCamera *camera;

	if ( privateCameraView ) {
		camera = privateCameraView;
	} else {
		camera = gameLocal.GetCamera();
	}

	if ( camera ) {
		renderView_t view;

		memset( &view, 0, sizeof( view ) );
		camera->GetViewParms( &view );
		origin = view.vieworg;
		axis = view.viewaxis;
		return true;
	} else {
		return idActor::GetPhysicsToSoundTransform( origin, axis );
	}
}

/*
================
idPlayer::WriteToSnapshot
================
*/
void idPlayer::WriteToSnapshot( idBitMsgDelta &msg ) const {
	physicsObj.WriteToSnapshot( msg );
	WriteBindToSnapshot( msg );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[0] );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[1] );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[2] );
	msg.WriteShort( health );
	msg.WriteBits( gameLocal.ServerRemapDecl( -1, DECL_ENTITYDEF, lastDamageDef ), gameLocal.entityDefBits );
	msg.WriteDir( lastDamageDir, 9 );
	msg.WriteShort( lastDamageLocation );
	msg.WriteBits( idealWeapon, idMath::BitsForInteger( 256 ) );
	msg.WriteBits( weapon.GetSpawnId(), 32 );
	msg.WriteBits( spectator, idMath::BitsForInteger( MAX_CLIENTS ) );
	msg.WriteBits( lastHitToggle, 1 );
	msg.WriteBits( weaponGone, 1 );
	msg.WriteBits( isLagged, 1 );
	msg.WriteBits( isChatting, 1 );
}

/*
================
idPlayer::ReadFromSnapshot
================
*/
void idPlayer::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	int		oldHealth, newIdealWeapon, weaponSpawnId;
	bool	newHitToggle, stateHitch;

	if ( snapshotSequence - lastSnapshotSequence > 1 ) {
		stateHitch = true;
	} else {
		stateHitch = false;
	}
	lastSnapshotSequence = snapshotSequence;

	oldHealth = health;

	physicsObj.ReadFromSnapshot( msg );
	ReadBindFromSnapshot( msg );
	deltaViewAngles[0] = msg.ReadDeltaFloat( 0.0f );
	deltaViewAngles[1] = msg.ReadDeltaFloat( 0.0f );
	deltaViewAngles[2] = msg.ReadDeltaFloat( 0.0f );
	health = msg.ReadShort();
	lastDamageDef = gameLocal.ClientRemapDecl( DECL_ENTITYDEF, msg.ReadBits( gameLocal.entityDefBits ) );
	lastDamageDir = msg.ReadDir( 9 );
	lastDamageLocation = msg.ReadShort();
	newIdealWeapon = msg.ReadBits( idMath::BitsForInteger( 256 ) );
	weaponSpawnId = msg.ReadBits( 32 );
	spectator = msg.ReadBits( idMath::BitsForInteger( MAX_CLIENTS ) );
	newHitToggle = msg.ReadBits( 1 ) != 0;
	weaponGone = msg.ReadBits( 1 ) != 0;
	isLagged = msg.ReadBits( 1 ) != 0;
	isChatting = msg.ReadBits( 1 ) != 0;

	// no msg reading below this

	if ( weapon.SetSpawnId( weaponSpawnId ) ) {
		if ( weapon.GetEntity() ) {
			// maintain ownership locally
			weapon.GetEntity()->SetOwner( this );
		}
		currentWeapon = -1;
	}

	if ( oldHealth > 0 && health <= 0 ) {
		if ( stateHitch ) {
			// so we just hide and don't show a death skin
			UpdateDeathSkin( true );
		}
		// die
		AI_DEAD = true;
		SetAnimState( ANIMCHANNEL_LEGS, "Legs_Death", 4 );
		SetAnimState( ANIMCHANNEL_TORSO, "Torso_Death", 4 );
		SetWaitState( "" );
		animator.ClearAllJoints();
		if ( entityNumber == gameLocal.localClientNum ) {
			playerView.Fade( colorBlack, 12000 );
		}
		StartRagdoll();
		physicsObj.SetMovementType( PM_DEAD );
		if ( !stateHitch ) {
			StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
		}
		if ( weapon.GetEntity() ) {
			weapon.GetEntity()->OwnerDied();
		}
	} else if ( oldHealth <= 0 && health > 0 ) {
		// respawn
		Init();
		StopRagdoll();
		SetPhysics( &physicsObj );
		physicsObj.EnableClip();
		SetCombatContents( true );
	} else if ( health < oldHealth && health > 0 ) {
		if ( stateHitch ) {
			lastDmgTime = gameLocal.time;
		} else {
			// damage feedback
			const idDeclEntityDef *def = static_cast<const idDeclEntityDef *>( declManager->DeclByIndex( DECL_ENTITYDEF, lastDamageDef, false ) );
			if ( def ) {
				playerView.DamageImpulse( lastDamageDir * viewAxis.Transpose(), &def->dict );
				AI_PAIN = Pain( NULL, NULL, oldHealth - health, lastDamageDir, lastDamageLocation, &def->dict );
				lastDmgTime = gameLocal.time;
			} else {
				common->Warning( "NET: no damage def for damage feedback '%d'\n", lastDamageDef );
			}
		}
	} else if ( health > oldHealth && !stateHitch ) {
		// just pulse, for any health raise
		healthPulse = true;
	}

	// If the player is alive, restore proper physics object

	if ( health > 0 && IsActiveAF() ) {

		StopRagdoll();

		SetPhysics( &physicsObj );

		physicsObj.EnableClip();

		SetCombatContents( true );

	}



	if ( idealWeapon != newIdealWeapon ) {
		if ( stateHitch ) {
			weaponCatchup = true;
		}
		idealWeapon = newIdealWeapon;
		UpdateHudWeapon();
	}

	if ( lastHitToggle != newHitToggle ) {
		SetLastHitTime( gameLocal.realClientTime );
	}

	if ( msg.HasChanged() ) {
		UpdateVisuals();
	}
}

/*
================
idPlayer::WritePlayerStateToSnapshot
================
*/
void idPlayer::WritePlayerStateToSnapshot( idBitMsgDelta &msg ) const {
	msg.WriteByte( bobCycle );
	msg.WriteLong( stepUpTime );
	msg.WriteFloat( stepUpDelta );
}

/*
================
idPlayer::ReadPlayerStateFromSnapshot
================
*/
void idPlayer::ReadPlayerStateFromSnapshot( const idBitMsgDelta &msg ) {
	bobCycle = msg.ReadByte();
	stepUpTime = msg.ReadLong();
	stepUpDelta = msg.ReadFloat();
}

/*
================
idPlayer::ServerReceiveEvent
================
*/
bool idPlayer::ServerReceiveEvent( int event, int time, const idBitMsg &msg ) {

	if ( idEntity::ServerReceiveEvent( event, time, msg ) ) {
		return true;
	}

	// client->server events
	switch( event ) {
		case EVENT_IMPULSE: {
			PerformImpulse( msg.ReadBits( 6 ) );
			return true;
		}
		default: {
			return false;
		}
	}
}

/*
================
idPlayer::ClientReceiveEvent
================
*/
bool idPlayer::ClientReceiveEvent( int event, int time, const idBitMsg &msg ) {
	switch ( event ) {
		case EVENT_EXIT_TELEPORTER:
			Event_ExitTeleporter();
			return true;
		case EVENT_ABORT_TELEPORTER:
			SetPrivateCameraView( NULL );
			return true;
		case EVENT_POWERUP: {
			// greebo: No EVENT_POWERUP handling at the moment
			return true;
		}
		case EVENT_SPECTATE: {
			bool spectate = ( msg.ReadBits( 1 ) != 0 );
			Spectate( spectate );
			return true;
		}
		case EVENT_ADD_DAMAGE_EFFECT: {
			if ( spectating ) {
				// if we're spectating, ignore
				// happens if the event and the spectate change are written on the server during the same frame (fraglimit)
				return true;
			}
			return idActor::ClientReceiveEvent( event, time, msg );
		}
		default: {
			return idActor::ClientReceiveEvent( event, time, msg );
		}
	}
//	return false;
}

/*
================
idPlayer::Hide
================
*/
void idPlayer::Hide( void ) {
	idWeapon *weap;

	idActor::Hide();
	weap = weapon.GetEntity();
	if ( weap ) {
		weap->HideWorldModel();
	}
}

/*
================
idPlayer::Show
================
*/
void idPlayer::Show( void ) {
	idWeapon *weap;
	
	idActor::Show();
	weap = weapon.GetEntity();
	if ( weap ) {
		weap->ShowWorldModel();
	}
}

/*
===============
idPlayer::StartAudioLog
===============
*/
void idPlayer::StartAudioLog( void ) {
	if ( hud ) {
		hud->HandleNamedEvent( "audioLogUp" );
	}
}

/*
===============
idPlayer::StopAudioLog
===============
*/
void idPlayer::StopAudioLog( void ) {
	if ( hud ) {
		hud->HandleNamedEvent( "audioLogDown" );
	}
}

/*
===============
idPlayer::ShowTip
===============
*/
void idPlayer::ShowTip( const char *title, const char *tip, bool autoHide ) {
	if ( tipUp ) {
		return;
	}
	hud->SetStateString( "tip", tip );
	hud->SetStateString( "tiptitle", title );
	hud->HandleNamedEvent( "tipWindowUp" ); 
	if ( autoHide ) {
		PostEventSec( &EV_Player_HideTip, 5.0f );
	}
	tipUp = true;
}

/*
===============
idPlayer::HideTip
===============
*/
void idPlayer::HideTip( void ) {
	hud->HandleNamedEvent( "tipWindowDown" ); 
	tipUp = false;
}

/*
===============
idPlayer::Event_HideTip
===============
*/
void idPlayer::Event_HideTip( void ) {
	HideTip();
}

/*
===============
idPlayer::ShowObjective
===============
*/
void idPlayer::ShowObjective( const char *obj ) {
	hud->HandleNamedEvent( obj );
	objectiveUp = true;
}

/*
===============
idPlayer::HideObjective
===============
*/
void idPlayer::HideObjective( void ) {
	hud->HandleNamedEvent( "closeObjective" );
	objectiveUp = false;
}

/*
===============
idPlayer::Event_StopAudioLog
===============
*/
void idPlayer::Event_StopAudioLog( void ) {
	StopAudioLog();
}

/*
===============
idPlayer::SetSpectateOrigin
===============
*/
void idPlayer::SetSpectateOrigin( void ) {
	idVec3 neworig;

	neworig = GetPhysics()->GetOrigin();
	neworig[ 2 ] += EyeHeight();
	neworig[ 2 ] += 25;
	SetOrigin( neworig );
}

/*
===============
idPlayer::RemoveWeapon
===============
*/
void idPlayer::RemoveWeapon( const char *weap ) {
	if ( weap && *weap ) {
		// greebo: Not yet implemented for TDM inventory
		//inventory.Drop( spawnArgs, spawnArgs.GetString( weap ), -1 );
	}
}

/*
===============
idPlayer::CanShowWeaponViewmodel
===============
*/
bool idPlayer::CanShowWeaponViewmodel( void ) const {
	return showWeaponViewModel;
}

void idPlayer::UpdateWeaponEncumbrance()
{
	// Get the currently selected weapon
	CInventoryItemPtr weapon = m_WeaponCursor->GetCurrentItem();

	if (weapon != NULL)
	{
		SetHinderance("weapon", weapon->GetMovementModifier(), 1.0f);
	}
}

/*
===============
idPlayer::SetLevelTrigger
===============
*/
void idPlayer::SetLevelTrigger( const char *levelName, const char *triggerName ) {
	if ( levelName && *levelName && triggerName && *triggerName ) {
		idLevelTriggerInfo lti;
		lti.levelName = levelName;
		lti.triggerName = triggerName;
		levelTriggers.Append( lti );
	}
}

/*
===============
idPlayer::Event_LevelTrigger
===============
*/
void idPlayer::Event_LevelTrigger( void ) {
	idStr mapName = gameLocal.GetMapName();
	mapName.StripPath();
	mapName.StripFileExtension();
	for ( int i = levelTriggers.Num() - 1; i >= 0; i-- ) {
		if ( idStr::Icmp( mapName, levelTriggers[i].levelName) == 0 ){
			idEntity *ent = gameLocal.FindEntity( levelTriggers[i].triggerName );
			if ( ent ) {
				ent->PostEventMS( &EV_Activate, 1, this );
			}
		}
	}
}

/*
===============
idPlayer::Event_Gibbed
===============
*/
void idPlayer::Event_Gibbed( void ) {
	// do nothing
}

/*
==================
idPlayer::Event_GetIdealWeapon 
==================
*/
void idPlayer::Event_GetIdealWeapon( void ) {
	const char *weapon;

	if ( idealWeapon >= 0 ) {
		weapon = spawnArgs.GetString( va( "def_weapon%d", idealWeapon ) );
		idThread::ReturnString( weapon );
	} else {
		idThread::ReturnString( "" );
	}
}

/*
===============
idPlayer::UpdatePlayerIcons
===============
*/
void idPlayer::UpdatePlayerIcons( void ) {
	int time = networkSystem->ServerGetClientTimeSinceLastPacket( entityNumber );
	if ( time > cvarSystem->GetCVarInteger( "net_clientMaxPrediction" ) ) {
		isLagged = true;
	} else {
		isLagged = false;
	}
	// TODO: chatting, PDA, etc?
}

/*
===============
idPlayer::DrawPlayerIcons
===============
*/
void idPlayer::DrawPlayerIcons( void ) {
	if ( !NeedsIcon() ) {
		playerIcon.FreeIcon();
		return;
	}
	playerIcon.Draw( this, headJoint );
}

/*
===============
idPlayer::HidePlayerIcons
===============
*/
void idPlayer::HidePlayerIcons( void ) {
	playerIcon.FreeIcon();
}

/*
===============
idPlayer::NeedsIcon
==============
*/
bool idPlayer::NeedsIcon( void ) {
	// local clients don't render their own icons... they're only info for other clients

	return entityNumber != gameLocal.localClientNum && ( isLagged || isChatting );

}

void idPlayer::AdjustLightgem(void)
{
	idVec3 vDifference;
	double fx, fy;
	double fDistance;
	double fLightgemVal;
	idVec3 vLightColor;
	idVec3 vPlayer;
	idLight *light, *helper;
	trace_t trace;
	CDarkModPlayer *pDM = g_Global.m_DarkModPlayer;
	int i, n, h = -1, l;
	bool bMinOneLight = false, bStump;

	DM_LOG(LC_FUNCTION, LT_DEBUG)LOGSTRING("[%s]\r", __FUNCTION__);

	NextFrame = true;
	fLightgemVal = 0;
	n = pDM->m_LightList.Num();

	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("%u entities found within lightradius\r", n);
	idVec3 vStart(GetEyePosition());
	idVec3 vPlayerPos(GetPhysics()->GetOrigin());
	idVec3 vPlayerSeg[LSG_COUNT];
	idVec3 vLightCone[ELC_COUNT];
	idVec3 vResult[2];
	EIntersection inter;

	vPlayerSeg[LSG_ORIGIN] = vPlayerPos;
	vPlayerSeg[LSG_DIRECTION] = vStart - vPlayerPos;

	for(i = 0; i < n; i++)
	{
		light = pDM->m_LightList[i].GetEntity();
		// greebo: The lightlist doesn't contain other entity types than idLight, right?
		/*if((light = dynamic_cast<idLight *>(pDM->m_LightList[i])) == NULL)
			continue;*/

		vPlayer = vPlayerPos;
		idVec3 vLight(light->GetPhysics()->GetOrigin());
		vPlayer.z = vLight.z;
		vDifference = vPlayer - vLight;
		fDistance = vDifference.Length();
		DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Ligth: [%s]  %i  px: %f   py: %f   pz: %f   -   lx: %f   ly: %f   lz: %f   Distance: %f\r", 
			light->name.c_str(), i, vPlayer.x, vPlayer.y, vPlayer.z, vLight.x, vLight.y, vLight.z, fDistance);

		// Fast and cheap test to see if the player could be in the area of the light.
		// Well, it is not exactly cheap, but it is the cheapest test that we can do at this point. :)
		if(fDistance > light->m_MaxLightRadius)
		{
			DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("%s is outside distance: %f/%f\r", light->name.c_str(), light->m_MaxLightRadius, fDistance);
			if(h == -1)
				h = i;
			continue;
		}

		if(light->IsPointlight())
		{
			light->GetLightCone(vLightCone[ELL_ORIGIN], vLightCone[ELA_AXIS], vLightCone[ELA_CENTER]);
			inter = IntersectLineEllipsoid(vPlayerSeg, vLightCone, vResult);

			// If this is a centerlight we have to move the origin from the original origin to where the
			// center of the light is supposed to be.
			// Centerlight means that the center of the ellipsoid is not the same as the origin. It has to
			// be adjusted because if it casts shadows we have to trace to it, and in this case the light
			// might be inside geometry and would be reported as not being visible even though it casts
			// a visible light outside the geometry it is embedded. If it is not a centerlight and has
			// cast shadows enabled, it wouldn't cast any light at all in such a case because it would
			// be blocked by the geometry.
			vLight = vLightCone[ELL_ORIGIN] + vLightCone[ELA_CENTER];
			DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("IntersectLineEllipsoid returned %u\r", inter);
		}
		else
		{
			bStump = false;
			light->GetLightCone(vLightCone[ELC_ORIGIN], vLightCone[ELA_TARGET], vLightCone[ELA_RIGHT], vLightCone[ELA_UP], vLightCone[ELA_START], vLightCone[ELA_END]);
			inter = IntersectLineCone(vPlayerSeg, vLightCone, vResult, bStump);
			DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("IntersectLineCone returned %u\r", inter);
		}

		// The line intersection can only return three states. Either the line is passing
		// through the lightcone in which case we will get two intersection points, the line
		// is not passing through which means that the player is fully outside, or the line
		// is touching the cone in exactly one point. The last case is not really helpfull in
		// our case and doesn't make a difference for the gameplay so we simply ignore it and
		// consider only cases where the player is at least partially inside the cone.
		if(inter != INTERSECT_FULL)
			continue;

		if(light->CastsShadow() == true)
		{
			gameLocal.clip.TracePoint(trace, vStart, vLight, CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP
				|CONTENTS_MOVEABLECLIP|CONTENTS_BODY|CONTENTS_CORPSE|CONTENTS_RENDERMODEL
				|CONTENTS_FLASHLIGHT_TRIGGER, this);
			DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("TraceFraction: %f\r", trace.fraction);
			if(trace.fraction < 1.0f)
			{
				DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Light [%s] can not be seen\r", light->name.c_str());
				continue;
			}
		}

		if(vResult[0].z < vResult[1].z)
			l = 0;
		else
			l = 1;

		if(vResult[l].z < vPlayerPos.z)
		{
			fx = vPlayerPos.x;
			fy = vPlayerPos.y;
		}
		else
		{
			fx = vResult[l].x;
			fy = vResult[l].y;
		}

		if(bMinOneLight == false)
			bMinOneLight = true;

		fLightgemVal += light->GetDistanceColor(fDistance, vLightCone[ELL_ORIGIN].x - fx, vLightCone[ELL_ORIGIN].y - fy);
		DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("%s in x/y: %f/%f   Distance:   %f/%f   Brightness: %f\r",
			light->name.c_str(), fx, fy, fLightgemVal, fDistance, light->m_MaxLightRadius);

		// Exchange the position of these lights, so that nearer lights are more
		// at the beginning of the list. You may not use the arrayentry from this point on now.
		// This sorting is not exactly good, but it is very cheap and we don't want to waste
		// time to sort an everchanging array.
		if(h != -1)
		{
			helper = pDM->m_LightList[h].GetEntity();
			pDM->m_LightList[h] = light;
			pDM->m_LightList[i] = helper;
			h = -1;
		}

		// No need to do further calculations when we are fully lit. 0 < n < 1
		if(fLightgemVal >= 1.0f)
				continue;

		// No need to do further calculations when we are fully lit. 0 < n < 1
		if(fLightgemVal >= 1.0f)
		{
			fLightgemVal = 1.0;
			break;
		}
	}

	pDM->m_LightgemValue = static_cast<int>(DARKMOD_LG_MAX * fLightgemVal);
	if(pDM->m_LightgemValue < DARKMOD_LG_MIN)
		pDM->m_LightgemValue = DARKMOD_LG_MIN;
	else
	if(pDM->m_LightgemValue > DARKMOD_LG_MAX)
		pDM->m_LightgemValue = DARKMOD_LG_MAX;

	// if the player is in a lit area and the lightgem would be totaly dark we set it to at least
	// one step higher.
	if(bMinOneLight == true && pDM->m_LightgemValue <= DARKMOD_LG_MIN)
		pDM->m_LightgemValue++;
}

void idPlayer::UpdateMoveVolumes( void )
{
	// copy step volumes from current cvar value
	m_stepvol_walk = cv_pm_stepvol_walk.GetFloat();
	m_stepvol_run = cv_pm_stepvol_run.GetFloat();
	m_stepvol_creep = cv_pm_stepvol_creep.GetFloat();

	m_stepvol_crouch_walk = cv_pm_stepvol_crouch_walk.GetFloat();
	m_stepvol_crouch_creep = cv_pm_stepvol_crouch_creep.GetFloat();
	m_stepvol_crouch_run = cv_pm_stepvol_crouch_run.GetFloat();
}

/*
=====================
idPlayer::GetMovementVolMod
=====================
*/

float idPlayer::GetMovementVolMod( void )
{
	float returnval;

	bool isCrouched = AI_CROUCH != 0;

	// figure out which of the 6 cases we have:
	if( !AI_RUN && !AI_CREEP )
	{
		returnval = (isCrouched) ? m_stepvol_crouch_walk : m_stepvol_walk;
	}
	// NOTE: running always has priority over creeping
	else if( AI_RUN )
	{
		returnval = (isCrouched) ? m_stepvol_crouch_run : m_stepvol_run;
	}
	else if( AI_CREEP )
	{
		returnval = (isCrouched) ? m_stepvol_crouch_creep : m_stepvol_creep;
	}
	else 
	{
		gameLocal.Error("idPlayer::GetMovementVolMod: Logic Error.");
	}
	//gameRenderWorld->DrawText(idStr(returnval), GetEyePosition() + viewAngles.ToForward()*20, 0.15f, colorWhite, viewAngles.ToMat3(), 1, 500);

	return returnval;
}

void idPlayer::InventoryUseKeyRelease(int holdTime)
{
	const CInventoryCursorPtr& crsr = InventoryCursor();
	CInventoryItemPtr it = crsr->GetCurrentItem();

	// Check for a held grabber entity, which should be put back into the inventory
	if (!AddGrabberEntityToInventory())
	{
		// Check if there is a valid item selected
		if (it != NULL && it->GetType() != CInventoryItem::IT_DUMMY)
		{
			UseInventoryItem(EReleased, it, holdTime, false);
		}
	}
}

void idPlayer::UseInventoryItem()
{
	// If the grabber item can be equipped/dequipped, use item does this
	if ( gameLocal.m_Grabber->GetSelected() || gameLocal.m_Grabber->GetEquipped() )
	{
		if( gameLocal.m_Grabber->ToggleEquip() )
		{
			return;
		}
	}

	// If the player has an item that is selected we need to check if this
	// is a usable item (like a key). In this case the use action takes
	// precedence over the frobaction.
	const CInventoryCursorPtr& crsr = InventoryCursor();
	CInventoryItemPtr it = crsr->GetCurrentItem();

	if (it != NULL && it->GetType() != CInventoryItem::IT_DUMMY)
	{
		bool couldBeUsed = UseInventoryItem(EPressed, it, 0, false); // false => not a frob action

		// Give optional visual feedback on the KeyDown event
		if (cv_tdm_inv_use_visual_feedback.GetBool())
		{
			m_overlays.broadcastNamedEvent(couldBeUsed ? "onInvPositiveFeedback" : "onInvNegativeFeedback");
		}
	}
}

bool idPlayer::UseInventoryItem(EImpulseState impulseState, const CInventoryItemPtr& item, int holdTime, bool isFrobUse)
{
	if (impulseState == EPressed)
	{
		// Pass the "inventoryUseItem" event to the GUIs
		m_overlays.broadcastNamedEvent("inventoryUseItem");
	}

	// Check if we're allowed to use items at all
	if (GetImmobilization() & EIM_ITEM_USE) return false;

	// Sanity check
	if (item == NULL) return false;

	idEntity* ent = item->GetItemEntity();
	if (ent == NULL) return false;

	idEntity* highlightedEntity = g_Global.m_DarkModPlayer->m_FrobEntity.GetEntity();

	bool itemIsUsable = ent->spawnArgs.GetBool("usable");
	
	if (highlightedEntity != NULL && itemIsUsable && highlightedEntity->CanBeUsedBy(item, isFrobUse))
	{
		// Pass the use call
		if (highlightedEntity->UseBy(impulseState, item))
		{
			// Item could be used, return TRUE, we're done
			return true;
		}
	}

	// Item could not be used on the highlighted entity, launch the use script

	idThread* thread = NULL;

	if (impulseState == EPressed)
	{
		thread = ent->CallScriptFunctionArgs("inventoryUse", true, 0, "eeed", ent, this, highlightedEntity, impulseState);
	}
	else if (impulseState == EReleased)
	{
		thread = ent->CallScriptFunctionArgs("inventoryUseKeyRelease", true, 0, "eeef", ent, this, highlightedEntity, static_cast<float>(holdTime));
	}

	if (thread != NULL)
	{
		thread->Execute(); // Start the thread immediately.

		// A working scriptfunction means the item could be used
		return true;
	}

	return false;
}

void idPlayer::DropInventoryItem()
{
	bool bDropped = false;
	CGrabber *grabber = gameLocal.m_Grabber;
	idEntity *heldEntity = grabber->GetSelected();
	idEntity *equippedEntity = grabber->GetEquipped();

	// Dequip or drop the item in the grabber hands first
	if( equippedEntity != NULL )
	{
		grabber->Dequip();
	}
	else if(heldEntity != NULL)
	{
		grabber->Update( this, false );
	}
	else 
	{
		// Grabber is empty (no item is held), drop the current inventory item
		const CInventoryCursorPtr& cursor = InventoryCursor();

		CInventoryItemPtr item = cursor->GetCurrentItem();
		CInventoryCategoryPtr category = cursor->GetCurrentCategory();

		// Do we have a droppable item in the first place?
		if (item != NULL && item->IsDroppable() && item->GetCount() > 0)
		{
			// Retrieve the actual entity behind the inventory item
			idEntity* ent = item->GetItemEntity();
			DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Attempting to drop inventory entity %s\r", ent->name.c_str());

			// greebo: Try to locate a drop script function on the entity's scriptobject
			const function_t* dropScript = ent->scriptObject.GetFunction(TDM_INVENTORY_DROPSCRIPT);
			
			if( dropScript != NULL )
			{
				// Call the custom drop script
				DM_LOG(LC_INVENTORY, LT_DEBUG)LOGSTRING("Running inventory drop script...\r");
				idThread* thread = new idThread(dropScript);
				thread->CallFunctionArgs(dropScript, true, "ee", ent, this);
				thread->DelayedStart(0);
				
				// The dropScript changes the inventory count itself, so don't set 
				// bDropped to true here.
			}
			// greebo: Only place the entity in the world, if there is no custom dropscript
			// The flashbomb for example is spawning projectiles on its own.
			else if( DropToHands( ent, item ) )
			{
				DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Item was successfully put in hands: %s\r", ent->name.c_str());
				bDropped = true;
			}
			else
			{
				// There wasn't space to drop the item
				StartSound( "snd_drop_item_failed", SND_CHANNEL_ITEM, 0, false, NULL );
				DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Grabber did not find space in the world to fit entity in hands: %s\r", ent->name.c_str());
			}

			// Decrease the inventory count (this will also clear empty categories)
			// This applies for both stackable as well as droppable items
			if( bDropped)
			{
				DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Item dropped, changing inventory count.\r");

				ChangeInventoryItemCount(item->GetName(), category->GetName(), -1); 
			}

			// Always update the HUD, the drop script might have changed the inventory count itself.
			inventoryHUDNeedsUpdate = true;
		}
	}
}

void idPlayer::SelectInventoryItem(const idStr& name)
{
	CInventoryItemPtr prev = InventoryCursor()->GetCurrentItem();
	idStr itemName(name);

	if (itemName.IsEmpty())
	{
		// Empty name specified, clear the inventory cursor
		itemName = TDM_DUMMY_ITEM;
	}
	
	// Try to lookup the item in the inventory
	CInventoryItemPtr item = Inventory()->GetItem(itemName);

	if (item != NULL)
	{
		// Item found, set the cursor to it
		InventoryCursor()->SetCurrentItem(item);

		// Trigger an update, passing the previous item along
		OnInventorySelectionChanged(prev);
	}
	else
	{
		gameLocal.Printf("Could not find item in player inventory: %s\n", itemName.c_str());
	}
}

void idPlayer::OnInventoryItemChanged()
{
	// Call the base class
	idEntity::OnInventoryItemChanged();

	inventoryHUDNeedsUpdate = true;
}

void idPlayer::OnInventorySelectionChanged(const CInventoryItemPtr& prevItem)
{
	// Call the base class first
	idEntity::OnInventorySelectionChanged(prevItem);

	// Set the "dirty" flag, the HUD needs a redraw
	inventoryHUDNeedsUpdate = true;

	// Notify the GUIs about the change event
	m_overlays.broadcastNamedEvent("inventorySelectionChange");

	// Get the current item
	const CInventoryCursorPtr& cursor = InventoryCursor();
	const CInventoryItemPtr& curItem = cursor->GetCurrentItem();

	if (prevItem != NULL && prevItem != curItem)
	{
		// We had a valid previous item, un-select the old one
		idEntity* prevItemEnt = prevItem->GetItemEntity();

		// greebo: Call the "UN-SELECT" script method on the "old" item
		if (prevItemEnt != NULL)
		{
			idThread* thread = prevItemEnt->CallScriptFunctionArgs(
				"inventory_item_unselect", 
				true, 0, 
				"eef", prevItemEnt, prevItem->GetOwner(), static_cast<float>(prevItem->GetOverlay())
			);

			if (thread != NULL) 
			{
				thread->Execute();
			}
		}

		// Remove the hinderance of the previous item
		SetHinderance("inventory_item", 1.0f, 1.0f);
	}

	if (curItem != NULL && prevItem != curItem)
	{
		// We have a valid curItem and it is different to the previously selected one
		idEntity* curItemEnt = curItem->GetItemEntity();

		// greebo: Call the "SELECT" script method on the newly selected item to give it a chance to initialise.
		if (curItemEnt != NULL)
		{
			idThread* thread = curItemEnt->CallScriptFunctionArgs(
				"inventory_item_select", 
				true, 0, 
				"eef", curItemEnt, curItem->GetOwner(), static_cast<float>(curItem->GetOverlay())
			);

			if (thread != NULL) 
			{
				thread->Execute();
			}
		}

		// Update the player's encumbrance based on the new item
		SetHinderance("inventory_item", curItem->GetMovementModifier(), 1.0f);
	}

	// Lastly, let's see if the category has changed
	if (curItem != NULL && prevItem != NULL && curItem->Category() != prevItem->Category())
	{
		idUserInterface* invGUI = m_overlays.getGui(m_InventoryOverlay);
		if (invGUI != NULL)
		{
			invGUI->HandleNamedEvent("onInventoryCategoryChanged");
		}
	}
}

bool idPlayer::DropToHands( idEntity *ent, CInventoryItemPtr item )
{
	bool rc = false;
	CGrabber *grabber = gameLocal.m_Grabber;

	if( !ent || !grabber )
		return false;

	// get player view data to apply yaw and roll to dropped object's orientation
	idVec3 playViewPos;
	idMat3 playViewAxis;
	GetViewPos( playViewPos, playViewAxis );
	idAngles viewYaw = playViewAxis.ToAngles();
	// ignore pitch and roll
	viewYaw[0] = 0;
	viewYaw[2] = 0;
	idMat3 playViewYaw = viewYaw.ToMat3();

	// Set up the initial orientation and point at which we want to drop
	idMat3 dropAxis;
	if( item != NULL )
		dropAxis = item->GetDropOrientation();
	else if( ent->spawnArgs.FindKey("drop_angles") )
	{
		idAngles dropAngles = ent->spawnArgs.GetAngles("drop_angles");
		dropAxis = dropAngles.ToMat3();
	}
	else
		dropAxis = ent->GetPhysics()->GetAxis();

	dropAxis *= playViewYaw;

	// flip dropped bodies if appropriate... move elsewhere?
	if( ent->IsType(idAFEntity_Base::Type) && ent->spawnArgs.GetBool("shoulderable") )
	{
		if( grabber->m_bDropBodyFaceUp )
			dropAxis *= idAngles(0.0f,0.0f,180.0f).ToMat3();
	}

	idVec3 dropPoint;
	if( ent->spawnArgs.FindKey("drop_point") )
	{
		dropPoint = ent->spawnArgs.GetVector("drop_point");
		dropPoint *= playViewYaw;
		dropPoint += playViewPos;
	}
	else
	{
		dropPoint = grabber->GetHoldPoint( ent );
	}

	// Stackables: Test that the item fits in world with dummy item first, before spawning the drop item
	// NOTE: TestDropItemRotations will overwrite DropAxis if the supplied doesn't work and it finds a working one
	if( TestDropItemRotations(ent, playViewPos, dropPoint, dropAxis) ) 
	{
		// Drop the item into the grabber hands 
		DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Item fits in hands.\r");
		
		// Stackable items only have one "real" entity for the whole item stack.
		// When the stack size == 1, this entity can be dropped as it is,
		// otherwise we need to spawn a new entity.
		if( item && item->IsStackable() && item->GetCount() > 1 ) 
		{
			DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Spawning new entity from stackable inventory item...\r");
			// Spawn a new entity of this type
			idEntity* spawnedEntity;
			const idDict* entityDef = gameLocal.FindEntityDefDict(ent->GetEntityDefName());
			gameLocal.SpawnEntityDef(*entityDef, &spawnedEntity);

			// Replace the entity to be dropped with the newly spawned one.
			ent = spawnedEntity;
		}

		grabber->PutInHands( ent, dropPoint, dropAxis );
		DM_LOG(LC_INVENTORY, LT_INFO)LOGSTRING("Item was successfully put in hands: %s\r", ent->name.c_str());
		rc = true;

		// check if the item drops straight to the ground, not going in hands first
		if( !ent->spawnArgs.GetBool("drop_to_hands","1") )
		{
			grabber->Update( this, false );
		}
	}

	return rc;
}

bool idPlayer::TestDropItemRotations( idEntity *ent, idVec3 viewPoint, idVec3 DropPoint, idMat3 &DropAxis )
{
	bool bReturnVal(false);

	idList<idMat3> Rotations;
	// first try the supplied orientation
	Rotations.Append( mat3_identity );
	// yaw rotations
	Rotations.Append( idAngles(0.0f, 90.0f, 0.0f).ToMat3() );
	Rotations.Append( idAngles(0.0f, 180.0f, 0.0f).ToMat3() );
	Rotations.Append( idAngles(0.0f, 270.0f, 0.0f).ToMat3() );
	// roll rotations
	Rotations.Append( idAngles(0.0f, 0.0f, 90.0f).ToMat3() );
	Rotations.Append( idAngles(0.0f, 0.0f, 180.0f).ToMat3() );
	Rotations.Append( idAngles(0.0f, 0.0f, 270.0f).ToMat3() );
	// pitch up and down
	// Dropping upside-down should always be a last resort
	if( !gameLocal.m_Grabber->m_bDropBodyFaceUp )
	{
		Rotations.Append( idAngles(-90.0f, 0.0f, 0.0f).ToMat3() );
		Rotations.Append( idAngles(90.0f, 0.0f, 0.0f).ToMat3() );
	}
	else
	{
		Rotations.Append( idAngles(90.0f, 0.0f, 0.0f).ToMat3() );
		Rotations.Append( idAngles(-90.0f, 0.0f, 0.0f).ToMat3() );
	}

	// test each orientation to see if it fits in the world
	idMat3 InitAxis = DropAxis;
	idMat3 TestAxis( mat3_identity );
	for( int i=0; i < Rotations.Num(); i++ )
	{
		TestAxis = Rotations[i] * InitAxis;
		if( gameLocal.m_Grabber->FitsInWorld( ent, viewPoint, DropPoint, TestAxis ) )
		{
			bReturnVal = true;
			DropAxis = TestAxis;
			break;
		}
	}

	return bReturnVal;
}

void idPlayer::Event_GetEyePos( void )
{
	idThread::ReturnVector( firstPersonViewOrigin );
}

void idPlayer::Event_SetImmobilization( const char *source, int type )
{
	SetImmobilization( source, type );
}

/*
=====================
idPlayer::Event_GetImmobilization
=====================
*/
void idPlayer::Event_GetImmobilization( const char *source )
{
	if (idStr::Length(source)) {
		idThread::ReturnInt( m_immobilization.GetInt(source) );
	} else {
		idThread::ReturnInt( GetImmobilization() );
	}
}

/*
=====================
idPlayer::Event_GetNextImmobilization
=====================
*/
void idPlayer::Event_GetNextImmobilization( const char *prefix, const char *lastMatch )
{
	// Code is plagarized from getNextKey()
	const idKeyValue *kv;
	const idKeyValue *previous;

	if ( *lastMatch ) {
		previous = m_immobilization.FindKey( lastMatch );
	} else {
		previous = NULL;
	}

	kv = m_immobilization.MatchPrefix( prefix, previous );
	if ( !kv ) {
		idThread::ReturnString( "" );
	} else {
		idThread::ReturnString( kv->GetKey() );
	}
}

/*
=====================
idPlayer::Event_SetHinderance
=====================
*/
void idPlayer::Event_SetHinderance( const char *source, float mCap, float aCap )
{
	SetHinderance( source, mCap, aCap );
}

/*
=====================
idPlayer::Event_SetTurnHinderance
=====================
*/
void idPlayer::Event_SetTurnHinderance( const char *source, float mCap, float aCap )
{
	SetTurnHinderance(source, mCap, aCap);
}

/*
=====================
idPlayer::Event_GetHinderance
=====================
*/
void idPlayer::Event_GetHinderance( const char *source )
{
	if (idStr::Length(source)) {
		idThread::ReturnVector( m_hinderance.GetVector( source, "1 1 0" ) );
	} else {
		float h = GetHinderance();
		idVec3 vec( h, h, 0.0f );
		idThread::ReturnVector( vec );
	}
}

/*
=====================
idPlayer::Event_GetTurnHinderance
=====================
*/
void idPlayer::Event_GetTurnHinderance( const char *source )
{
	if (idStr::Length(source)) {
		idThread::ReturnVector( m_TurnHinderance.GetVector( source, "1 1 0" ) );
	} else {
		float h = GetTurnHinderance();
		idVec3 vec( h, h, 0.0f );
		idThread::ReturnVector( vec );
	}
}

/*
=====================
idPlayer::Event_GetNextHinderance
=====================
*/
void idPlayer::Event_GetNextHinderance( const char *prefix, const char *lastMatch )
{
	// Code is plagarized from getNextKey()
	const idKeyValue *kv;
	const idKeyValue *previous;

	if ( *lastMatch ) {
		previous = m_hinderance.FindKey( lastMatch );
	} else {
		previous = NULL;
	}

	kv = m_hinderance.MatchPrefix( prefix, previous );
	if ( !kv ) {
		idThread::ReturnString( "" );
	} else {
		idThread::ReturnString( kv->GetKey() );
	}
}

/*
=====================
idPlayer::Event_GetNextTurnHinderance
=====================
*/
void idPlayer::Event_GetNextTurnHinderance( const char *prefix, const char *lastMatch )
{
	// Code is plagarized from getNextKey()
	const idKeyValue *kv;
	const idKeyValue *previous;

	if ( *lastMatch ) {
		previous = m_TurnHinderance.FindKey( lastMatch );
	} else {
		previous = NULL;
	}

	kv = m_TurnHinderance.MatchPrefix( prefix, previous );
	if ( !kv ) {
		idThread::ReturnString( "" );
	} else {
		idThread::ReturnString( kv->GetKey() );
	}
}

/*
=====================
idPlayer::Event_SetGui
=====================
*/
void idPlayer::Event_SetGui( int handle, const char *guiFile ) {
	if ( !uiManager->CheckGui(guiFile) ) {
		gameLocal.Warning( "Unable to load GUI file: %s\n", guiFile );
		goto Quit;
	}

	if ( !m_overlays.exists( handle ) ) {
		gameLocal.Warning( "Non-existant GUI handle: %d\n", handle );
		goto Quit;
	}

	// Entity GUIs are handled differently from regular ones.
	if ( handle == OVERLAYS_MIN_HANDLE ) {

		assert( hud );
		assert( m_overlays.isExternal( handle ) );

		// We're dealing with an existing unique GUI.
		// We need to read a new GUI into it.

		// Clear the state.
		const idDict &state = hud->State();
		const idKeyValue *kv;
		while ( ( kv = state.MatchPrefix( "" ) ) != NULL )
			hud->DeleteStateVar( kv->GetKey() );

		hud->InitFromFile( guiFile );

	} else if ( !m_overlays.isExternal( handle ) ) {

		bool result = m_overlays.setGui( handle, guiFile );
		assert( result );

		idUserInterface *gui = m_overlays.getGui( handle );
		if ( gui ) {
			gui->SetStateInt( "handle", handle );
			gui->Activate( true, gameLocal.time );
			// Let's set a good default value for whether or not the overlay is interactive.
			m_overlays.setInteractive( handle, gui->IsInteractive() );
		} else {
			gameLocal.Warning( "Unknown error: Unable to load GUI into overlay.\n" );
		}

	} else {
		gameLocal.Warning( "Cannot call setGui() on external handle: %d\n", handle );
	}

	Quit:
	return;
}

void idPlayer::Event_GetInventoryOverlay(void)
{
	idThread::ReturnInt(m_InventoryOverlay);
}

void idPlayer::Event_PlayStartSound( void )
{
	StartSound("snd_mission_start", SND_CHANNEL_ANY, 0, false, NULL);
}

void idPlayer::Event_MissionFailed( void )
{
	gameLocal.m_MissionData->Event_MissionFailed();
}

void idPlayer::Event_CustomDeath()
{
	// Run the custom death script
	idThread* thread = CallScriptFunctionArgs("custom_death", true, 0, "e", this);

	if (thread != NULL) {
		// Run immediately
		thread->Execute();
	}
}

void idPlayer::Event_LoadDeathMenu( void )
{
	forceRespawn = true;
}

/*
================
idPlayer::Event_HoldEntity
================
*/
void idPlayer::Event_HoldEntity( idEntity *ent )
{
	if ( ent )
	{
		bool successful = gameLocal.m_Grabber->PutInHands( ent, ent->GetPhysics()->GetAxis() );
		idThread::ReturnInt( successful );
	}
	else
	{
		gameLocal.m_Grabber->Update( this, false );
		idThread::ReturnInt( 1 );
	}
}

/*
================
idPlayer::Event_HeldEntity
================
*/
void idPlayer::Event_HeldEntity( void )
{
	idThread::ReturnEntity(gameLocal.m_Grabber->GetSelected());
}

void idPlayer::Event_RopeRemovalCleanup(idEntity *RopeEnt)
{
	if (GetPhysics()->IsType(idPhysics_Player::Type))
	{
		static_cast<idPhysics_Player*>(GetPhysics())->RopeRemovalCleanup( RopeEnt );
	}
}

void idPlayer::Event_SetObjectiveState( int ObjIndex, int State )
{
	// when calling this function externally, the "user" indices are
	// used, so subtract one from the index
	gameLocal.m_MissionData->SetCompletionState( ObjIndex - 1, State );
}

void idPlayer::Event_GetObjectiveState( int ObjIndex )
{
	int CompState = gameLocal.m_MissionData->GetCompletionState( ObjIndex - 1 );
	idThread::ReturnInt( CompState );
}

void idPlayer::Event_SetObjectiveComp( int ObjIndex, int CompIndex, int bState )
{
	gameLocal.m_MissionData->SetComponentState_Ext( ObjIndex, CompIndex, (bState != 0) );
}

void idPlayer::Event_GetObjectiveComp( int ObjIndex, int CompIndex )
{
	bool bCompState = gameLocal.m_MissionData->GetComponentState( ObjIndex -1, CompIndex -1 );

	idThread::ReturnInt( (int) bCompState );
}

void idPlayer::Event_ObjectiveUnlatch( int ObjIndex )
{
	gameLocal.m_MissionData->UnlatchObjective( ObjIndex - 1 );
}

void idPlayer::Event_ObjectiveComponentUnlatch( int ObjIndex, int CompIndex )
{
	gameLocal.m_MissionData->UnlatchObjectiveComp( ObjIndex - 1, CompIndex -1 );
}

void idPlayer::Event_SetObjectiveVisible( int ObjIndex, bool bVal )
{
	gameLocal.m_MissionData->Event_SetObjVisible( ObjIndex, bVal );
}

void idPlayer::Event_SetObjectiveOptional( int ObjIndex, bool bVal )
{
	gameLocal.m_MissionData->Event_SetObjMandatory( ObjIndex, !bVal );
}

void idPlayer::Event_SetObjectiveOngoing( int ObjIndex, bool bVal )
{
	gameLocal.m_MissionData->Event_SetObjOngoing( ObjIndex, bVal );
}

void idPlayer::Event_SetObjectiveEnabling( int ObjIndex, const char *strIn )
{
	idStr StrArg = strIn;
	gameLocal.m_MissionData->Event_SetObjEnabling( ObjIndex, StrArg );
}

void idPlayer::Event_GiveHealthPool( float amount ) {
	// Pass the call to the proper member method
	GiveHealthPool(amount);
}

void idPlayer::Event_WasDamaged( void )
{
	idThread::ReturnInt(m_bDamagedThisFrame);
}

void idPlayer::Event_StartZoom(float duration, float startFOV, float endFOV)
{
	// greebo: Start the new transition from startFOV >> endFOV, this enables the idInterpolate
	zoomFov.Init(gameLocal.time, duration, startFOV, endFOV);
}

void idPlayer::Event_EndZoom(float duration)
{
	// greebo: Make a transition from the current FOV back to the default FOV, this enables the idInterpolate
	zoomFov.Init(gameLocal.time, duration, zoomFov.GetCurrentValue(gameLocal.time), DefaultFov());
}

void idPlayer::Event_ResetZoom()
{
	// Reset the FOV to the default values, this enables the idInterpolate
	zoomFov.Init(gameLocal.time, 0, DefaultFov(), DefaultFov());
}

void idPlayer::Event_GetFov()
{
	// greebo: Return the current fov
	idThread::ReturnFloat(CalcFov(true));
}

void idPlayer::FrobCheck()
{
	// greebo: Don't run this when dead
	if (AI_DEAD) 
	{
		return;
	}

	// greebo: Don't run the frobcheck when we're dragging items around
	if (m_bGrabberActive)
	{
		return;
	}

	idVec3 eyePos = GetEyePosition();
	float maxFrobDistance = g_Global.m_MaxFrobDistance;

	// greebo: Let the currently selected inventory item affect the frob distance (lockpicks, for instance)
	CInventoryItemPtr curItem = InventoryCursor()->GetCurrentItem();
	
	idVec3 start = eyePos;
	idVec3 end = start + viewAngles.ToForward() * maxFrobDistance;

	// Do frob trace first, along view axis, record distance traveled
	// Frob collision mask:
	int cm = CONTENTS_SOLID|CONTENTS_OPAQUE|CONTENTS_BODY
		|CONTENTS_CORPSE|CONTENTS_RENDERMODEL
		|CONTENTS_FROBABLE;

	trace_t trace;
	gameLocal.clip.TracePoint(trace, start, end, cm, this);

	float traceDist = g_Global.m_MaxFrobDistance * trace.fraction;

	if( trace.fraction < 1.0f )
	{
		idEntity *ent = gameLocal.entities[ trace.c.entityNum ];

		DM_LOG(LC_FROBBING, LT_INFO)LOGSTRING("Frob: Direct hit on entity %s\r", ent->name.c_str());
		
		// This is taking locked items into account
		bool lockedItemCheck = true;

		// Inventory items might impose a reduction of the frob distance to some entities
		if (curItem != NULL && ent->CanBeUsedBy(curItem, true) && traceDist > curItem->GetFrobDistanceCap())
		{
			// Failed the distance check for locked items, disable this entity
			lockedItemCheck = false;
		}

		// greebo: Check if the frobbed entity is the bindmaster of the currently climbed rope
		bool isRopeMaster = physicsObj.OnRope() && physicsObj.GetRopeEntity()->GetBindMaster() == ent;

		// ishtvan: Check if the frobbed entity is a dynamically added AF body linked to another entity
		if( ent->IsType(idAFEntity_Base::Type) )
		{
			idAFEntity_Base *afEnt = static_cast<idAFEntity_Base *>(ent);
			idAFBody *AFbod = afEnt->GetAFPhysics()->GetBody( afEnt->BodyForClipModelId(trace.c.id) );

			if( AFbod->GetRerouteEnt() && AFbod->GetRerouteEnt()->m_bFrobable )
			{
				ent = AFbod->GetRerouteEnt();
			}
		}
	
		// only frob frobable, non-hidden entities within their frobdistance
		// also, do not frob the ent we are currently holding in our hands
		if( ent->m_bFrobable && lockedItemCheck && !isRopeMaster && !ent->IsHidden() && 
			traceDist < ent->m_FrobDistance && ent != gameLocal.m_Grabber->GetSelected())
		{
			DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("Entity %s was within frobdistance\r", ent->name.c_str());

			// Mark as frobbed for this frame
			ent->SetFrobbed(true);
			g_Global.m_DarkModPlayer->m_FrobTrace = trace;

			// we have found our frobbed entity, so exit
			return;
		}
	}

	// If the trace didn't hit anything frobable, do the radius test
	DM_LOG(LC_FROBBING,LT_INFO)LOGSTRING("No entity frobbed by direct LOS frob, trying frob radius.\r");
	
	idBounds frobBounds(trace.endpos);
	frobBounds.ExpandSelf( cv_frob_width.GetFloat() );

	// Optional debug drawing of frob bounds
	if( cv_frob_debug_bounds.GetBool() )
		gameRenderWorld->DebugBounds( colorBlue, frobBounds );

	static idEntity* frobRangeEnts[MAX_GENTITIES];

	int numFrobEnt = gameLocal.clip.EntitiesTouchingBounds(frobBounds, -1, frobRangeEnts, MAX_GENTITIES);

	idVec3 vecForward = viewAngles.ToForward();
	float bestDot = 0;
	idEntity* bestEnt = NULL;

	for( int i=0; i < numFrobEnt; i++ )
	{
		idEntity *ent = frobRangeEnts[i];

		if (ent == NULL) continue;

		if (!ent->m_FrobDistance || ent->IsHidden() || !ent->m_bFrobable) continue;

		// Get the frob distance from the entity candidate
		float frobDist = ent->m_FrobDistance;
		idVec3 delta = ent->GetPhysics()->GetOrigin() - eyePos;
		
		float entDistance = delta.LengthFast();

		if (entDistance > frobDist) continue; // too far

		// Inventory items might impose a reduction of the frob distance to some entities
		if (curItem != NULL && ent->CanBeUsedBy(curItem, true) && 
			entDistance > curItem->GetFrobDistanceCap())
		{
			// Failed inventory item distance check, disable this entity
			continue;
		}

		delta.NormalizeFast();
		float currentDot = delta * vecForward;
		currentDot *= ent->m_FrobBias;

		if( currentDot > bestDot )
		{
			bestDot = currentDot;
			bestEnt = ent;
		}
	}

	if( bestEnt != NULL && bestEnt != gameLocal.m_Grabber->GetSelected() )
	{
		DM_LOG(LC_FROBBING,LT_INFO)LOGSTRING("Frob radius expansion found best entity %s\r", bestEnt->name.c_str() );

		// Mark the entity as frobbed this frame
		bestEnt->SetFrobbed(true);
		g_Global.m_DarkModPlayer->m_FrobTrace = trace;
	}
}

int idPlayer::GetImmobilization( const char *source )
{
	// greebo: Return named immobilizations or return the sum of all immobilizations
	return (idStr::Length(source) > 0) ? m_immobilization.GetInt(source) : GetImmobilization();
}

void idPlayer::SetImmobilization( const char *source, int type )
{
	if (idStr::Length(source))
	{
		// The user cannot set the update bit directly.
		type &= ~EIM_UPDATE;

		if (type != 0)
		{
			m_immobilization.SetInt( source, type );
		}
		else
		{
			m_immobilization.Delete( source );
		}

		m_immobilizationCache |= EIM_UPDATE;
	}
	else
	{
		gameLocal.Warning( "source was empty; no immobilization set\n" );
	}
}

void idPlayer::SetHinderance( const char *source, float mCap, float aCap )
{
	if (idStr::Length(source))
	{
		// Clamp the values to [0,1]
		mCap = idMath::ClampFloat(0, 1, mCap);
		aCap = idMath::ClampFloat(0, 1, aCap);
		
		if (mCap < 1.0f || aCap < 1.0f)
		{
			// Store the values into a vector and into the hinderance dictionary
			m_hinderance.SetVector( source, idVec3(mCap, aCap, 0.0f) );
		}
		else
		{
			// greebo: Both values are 1 == no hinderance, delete the value
			m_hinderance.Delete( source );
		}

		m_hinderanceCache = -1;
	}
	else
	{
		gameLocal.Warning( "source was empty; no hinderance set\n" );
	}
}

void idPlayer::SetTurnHinderance( const char *source, float mCap, float aCap )
{
	if (idStr::Length(source))
	{
		// Clamp the values to [0,1]
		mCap = idMath::ClampFloat(0, 1, mCap);
		aCap = idMath::ClampFloat(0, 1, aCap);

		if (mCap < 1.0f || aCap < 1.0f)
		{
			// Store the values into a vector and into the hinderance dictionary
			m_TurnHinderance.SetVector( source, idVec3(mCap, aCap, 0.0f) );
		}
		else
		{
			m_TurnHinderance.Delete( source );
		}

		m_TurnHinderanceCache = -1;
	}
	else
	{
		gameLocal.Warning( "source was empty; no turn hinderance set\n" );
	}
}

float idPlayer::GetJumpHinderance() 
{
	// Has something changed since the cache was last calculated?
	if (m_JumpHinderanceCache < 0.0f) 
	{
		// Recalculate the hinderance from scratch.
		float mCap = 1.0f, aCap = 1.0f;

		for (const idKeyValue* kv = m_JumpHinderance.MatchPrefix( "", NULL ); kv != NULL; kv = m_JumpHinderance.MatchPrefix("", kv))
		{
			idVec3 vec = m_JumpHinderance.GetVector(kv->GetKey());
			mCap *= vec[0];

			if ( aCap > vec[1] ) 
			{
				aCap = vec[1];
			}
		}

		if ( aCap > mCap ) 
		{
			aCap = mCap;
		}

		m_JumpHinderanceCache = aCap;
	}

	return m_JumpHinderanceCache;
}

void idPlayer::SetJumpHinderance( const char *source, float mCap, float aCap )
{
	if (idStr::Length(source))
	{
		// Clamp the values to [0,1]
		mCap = idMath::ClampFloat(0, 1, mCap);
		aCap = idMath::ClampFloat(0, 1, aCap);

		if (mCap < 1.0f || aCap < 1.0f)
		{
			// Store the values into a vector and into the hinderance dictionary
			m_JumpHinderance.SetVector( source, idVec3(mCap, aCap, 0.0f) );
		}
		else
		{
			m_JumpHinderance.Delete( source );
		}

		m_JumpHinderanceCache = -1;
	}
	else
	{
		gameLocal.Warning( "source was empty; no jump hinderance set\n" );
	}
}

void idPlayer::PlayFootStepSound()
{
	if ( !GetPhysics()->HasGroundContacts() ) {
		return;
	}

	// This implements a certain dead time before the next footstep is allowed to be played
	if (gameLocal.time <= lastFootstepPlaytime + cv_pm_min_stepsound_interval.GetInteger())
	{
		return;
	}

	idStr localSound, sound;
	
	// DarkMod: make the string to identify the movement speed (crouch_run, creep, etc)
	// Currently only players have movement flags set up this way, not AI.  We could change that later.
	idStr moveType("");

	UpdateMoveVolumes();

	// angua: check whether the player has just landed from jumping or a fall
	if (!hasLanded)
	{
		if (AI_CROUCH)
		{
			moveType = "_crouch";
		}

		if (AI_RUN)
		{
			moveType += "_run";
		}
		else if (AI_CREEP)
		{
			moveType += "_creep";
		}
		else
		{
			moveType += "_walk";
		}
	}

	// start footstep sound based on material type
	const idMaterial* material = GetPhysics()->GetContact( 0 ).material;
	if ( material != NULL ) 
	{
		DM_LOG(LC_SOUND,LT_DEBUG)LOGSTRING("Player %s stepped on entity %s, material %s \r", name.c_str(), gameLocal.entities[GetPhysics()->GetContact( 0 ).entityNum]->name.c_str(), material->GetName() );  
		g_Global.GetSurfName(material, localSound);
		if (hasLanded)
		{
			localSound = "snd_jump_" + localSound;
		}
		else
		{
			localSound = "snd_footstep_" + localSound;
		}

		DM_LOG(LC_SOUND,LT_DEBUG)LOGSTRING("Found surface type sound: %s\r", localSound.c_str() ); 
		sound = spawnArgs.GetString( localSound.c_str() );
	}

	waterLevel_t waterLevel = physicsObj.GetWaterLevel();
	// If player is walking in liquid, replace the bottom surface sound with water sounds
	if (waterLevel == WATERLEVEL_FEET )
	{
		if (hasLanded)
		{
			localSound = "snd_jump_puddle";
		}
		else
		{
			localSound = "snd_footstep_puddle";
		}
		sound = spawnArgs.GetString( localSound );
	}
	else if (waterLevel == WATERLEVEL_WAIST)
	{
		localSound = "snd_footstep_wading";
		sound = spawnArgs.GetString( localSound );
	}
	// greebo: Added this to disable the walking sound when completely underwater
	// this should be replaced by snd_
	else if (waterLevel == WATERLEVEL_HEAD)
	{
		localSound = "snd_footstep_swim";
		sound = spawnArgs.GetString( localSound );
	}

	if ( sound.IsEmpty() && waterLevel != WATERLEVEL_HEAD ) 
	{
		localSound = "snd_footstep";
	}
	
	sound = spawnArgs.GetString( localSound );

	// if a sound was not found for that specific material, use default
	if( sound.IsEmpty() && waterLevel != WATERLEVEL_HEAD )
	{
		sound = spawnArgs.GetString( "snd_footstep" );
		localSound = "snd_footstep";
	}

	// The player always considers the movement type when propagating
	if (!hasLanded)
	{
		localSound += moveType;
	}

	if ( !sound.IsEmpty() ) 
	{
		// apply the movement type modifier to the volume
		const idSoundShader* sndShader = declManager->FindSound( sound );
		SetSoundVolume( sndShader->GetParms()->volume + GetMovementVolMod() );
		StartSoundShader( sndShader, SND_CHANNEL_BODY, 0, false, NULL );
		SetSoundVolume( 0.0f );

		// propagate the suspicious sound to other AI
		PropSoundDirect( localSound, true, false );

		lastFootstepPlaytime = gameLocal.time;
	}
}

/*
================
idPlayer::RangedThreatTo

Return nonzero if this entity could potentially
attack the given (target) entity at range, or
entities in general if target is NULL.
i.e. Return 1 if we have a projectile weapon
equipped, and 0 otherwise.
================
*/
float idPlayer::RangedThreatTo(idEntity* target) {
	idWeapon* weaponEnt = weapon.GetEntity();
	
	return weaponEnt->IsRanged();
}

void idPlayer::SetListenerLoc( idVec3 loc )
{
	m_ListenerLoc = loc;
}

idVec3 idPlayer::GetListenerLoc( void )
{
	return m_ListenerLoc;
}

void idPlayer::SetDoorListenLoc( idVec3 loc )
{
	m_DoorListenLoc = loc;
}

idVec3 idPlayer::GetDoorListenLoc( void )
{
	return m_DoorListenLoc;
}

CInventoryItemPtr idPlayer::AddToInventory(idEntity *ent)
{
	// Pass the call to the base class first
	CInventoryItemPtr returnValue = idEntity::AddToInventory(ent);

	// Has this item been added to a weapon item?
	CInventoryWeaponItemPtr weaponItem = boost::dynamic_pointer_cast<CInventoryWeaponItem>(returnValue);

	CInventoryItemPtr prev;

	if (weaponItem != NULL)
	{
		// greebo: This is a weapon-related inventory item, use the weapon inventory cursor
		// Do it only if the respective CVAR is set
		if (cv_frob_weapon_selects_weapon.GetBool())
		{
			m_WeaponCursor->SetCurrentItem(returnValue);
			SelectWeapon(weaponItem->GetWeaponIndex(), false);
		}
	}
	else if (returnValue != NULL)
	{
		// Ordinary inventory item, set the cursor onto it
		prev = InventoryCursor()->GetCurrentItem();
		// Focus the cursor on the newly added item
		InventoryCursor()->SetCurrentItem(returnValue);

		// Fire the script events and update the HUD
		OnInventorySelectionChanged(prev);
	}

	return returnValue;
}

void idPlayer::PerformFrob(EImpulseState impulseState, idEntity* target)
{
	// greebo: Don't perform frobs on hidden or NULL entities
	if (target == NULL || target->IsHidden())
	{
		return;
	}

	// if we only allow "simple" frob actions and this isn't one, play forbidden sound
	if( (GetImmobilization() & EIM_FROB_COMPLEX) && !target->m_bFrobSimple )
	{
		// TODO: Rename this "uh-uh" sound to something more general?
		StartSound( "snd_drop_item_failed", SND_CHANNEL_ITEM, 0, false, NULL );	
		return;
	}


	CDarkModPlayer* pDM = g_Global.m_DarkModPlayer;
	assert(pDM != NULL); // must not be NULL

	// greebo: Check the frob entity, this might be the same as the argument
	// Retrieve the entity before trying to add it to the inventory, the pointer
	// might be cleared after calling AddToInventory().
	idEntity* highlightedEntity = pDM->m_FrobEntity.GetEntity();

	if (impulseState == EPressed)
	{
		// Fire the STIM_FROB response on key down (if defined) on this entity
		target->ResponseTrigger(this, ST_FROB);
	}

	// Do we allow use on frob?
	if (cv_tdm_inv_use_on_frob.GetBool()) 
	{
		// Check if we have a "use" relationship with the currently selected inventory item (key => door)
		CInventoryItemPtr item = InventoryCursor()->GetCurrentItem();

		// Only allow items with UseOnFrob == TRUE to be used when frobbing
		if (item != NULL && item->UseOnFrob() && highlightedEntity->CanBeUsedBy(item, true))
		{
			// Try to use the item
			bool couldBeUsed = UseInventoryItem(impulseState, item, gameLocal.msec, true); // true => is frob action

			// Give optional visual feedback on the KeyDown event
			if (impulseState == EPressed && cv_tdm_inv_use_visual_feedback.GetBool())
			{
				m_overlays.broadcastNamedEvent(couldBeUsed ? "onInvPositiveFeedback" : "onInvNegativeFeedback");
			}

			return;
		}
	}

	// Inventory item could not be used with the highlighted entity, proceed with ordinary frob action

	// These actions are only applicable for EPressed buttonstate
	if (impulseState == EPressed)
	{
		// Trigger the frob action script on key down
		target->FrobAction(true);

		DM_LOG(LC_FROBBING, LT_DEBUG)LOGSTRING("USE: frob target: %s \r", target->name.c_str());

		// First we have to check whether that entity is an inventory 
		// item. In that case, we have to add it to the inventory and
		// hide the entity.
		CInventoryItemPtr addedItem = AddToInventory(target);

		// Check if the frobbed entity is the one currently highlighted by the player
		if (addedItem != NULL && highlightedEntity == target) 
		{
			// Item has been added to the inventory, clear the entity pointer
			pDM->m_FrobEntity = NULL;
		}

		// Grab it if it's a grabable class
		if (target->IsType(idMoveable::Type) || target->IsType(idAFEntity_Base::Type) || 
			target->IsType(idMoveableItem::Type) || target->IsType(idAFAttachment::Type))
		{
			// allow override of default grabbing behavior
			if( !target->spawnArgs.GetBool("grabable","1") )
				return;

			// Do not pick up live, conscious AI
			if( target->IsType( idAI::Type ) )
			{
				idAI *AItarget = static_cast<idAI *>(target);
				if( AItarget->health > 0 && !AItarget->IsKnockedOut() )
					return;
			}

			gameLocal.m_Grabber->Update( this );
		}
	}
}

void idPlayer::PerformFrob()
{
	// Ignore frobs if player-frobbing is immobilized.
	if ( GetImmobilization() & EIM_FROB )
	{
		return;
	}

	// if the grabber is currently holding something and frob is pressed,
	// release it.  Do not frob anything new since you're holding an item.
	if( gameLocal.m_Grabber->GetSelected() )
	{
		gameLocal.m_Grabber->Update( this );
		return;
	}

	CDarkModPlayer* pDM = g_Global.m_DarkModPlayer;

	// Get the currently frobbed entity
	idEntity* frob = pDM->m_FrobEntity.GetEntity();

	// Relay the function to the specialised method
	PerformFrob(EPressed, frob);
}

void idPlayer::PerformFrobKeyRepeat()
{
	// Ignore frobs if player-frobbing is immobilized.
	if ( GetImmobilization() & EIM_FROB ) return;

	// Get the currently frobbed entity
	CDarkModPlayer* pDM = g_Global.m_DarkModPlayer;
	idEntity* frob = pDM->m_FrobEntity.GetEntity();

	// Relay the function to the specialised method
	PerformFrob(ERepeat, frob);
}

void idPlayer::PerformFrobKeyRelease()
{
	// Ignore frobs if player-frobbing is immobilized.
	if ( GetImmobilization() & EIM_FROB ) return;

	// Get the currently frobbed entity
	CDarkModPlayer* pDM = g_Global.m_DarkModPlayer;
	idEntity* frob = pDM->m_FrobEntity.GetEntity();

	// Relay the function to the specialised method
	PerformFrob(EReleased, frob);
}

void idPlayer::setHealthPoolTimeInterval(int newTimeInterval, float factor, int stepAmount) {
	healthPoolTimeInterval = newTimeInterval;
	healthPoolTimeIntervalFactor = factor;
	healthPoolStepAmount = stepAmount;
}

bool idPlayer::AddGrabberEntityToInventory()
{
	CGrabber* grabber = gameLocal.m_Grabber;
	idEntity* heldEntity = grabber->GetSelected();

	if (heldEntity != NULL)
	{
		CInventoryItemPtr item = AddToInventory(heldEntity);

		if (item != NULL)
		{
			// greebo: Release any items from the grabber, this immobilized the player somehow before
			grabber->Update( this, false );

			// greebo: Prevent the grabber from checking the added entity (it may be 
			// entirely removed from the game, which would cause crashes).
			grabber->RemoveFromClipList(heldEntity);

			return true;
		}
	}

	return false;
}

int idPlayer::GetLightgemModifier(int curLightgemValue)
{
	// Take the compiled lightgem modifier as starting point
	int returnValue = curLightgemValue + m_LightgemModifier;

	// greebo: Take the current velocity into account
	// This is a multiplicative modifier and is applied first
	{
		// Get the velocity, but don't take "inherited" speed into account.
		idVec3 velocityVec = physicsObj.GetLinearVelocity() - physicsObj.GetPushedLinearVelocity();

		const idVec3& gravityDir = physicsObj.GetGravityNormal();
		velocityVec -= (velocityVec * gravityDir) * gravityDir;
		
		float velocity = velocityVec.LengthFast();
		float minVelocity = cv_lg_velocity_mod_min_velocity.GetFloat();
		float maxVelocity = cv_lg_velocity_mod_max_velocity.GetFloat();

		float velocityFactor = (velocity - minVelocity) / (maxVelocity - minVelocity);

		// Force the factor into [0..1]
		if (velocityFactor > 1) velocityFactor = 1;
		if (velocityFactor < 0) velocityFactor = 0;

		float factor = 1.0f + velocityFactor*cv_lg_velocity_mod_amount.GetFloat();
		returnValue = static_cast<int>(returnValue * factor);
	}

	// Check the weapon/inventory items
	if (m_WeaponCursor != NULL)
	{
		CInventoryItemPtr weapon = m_WeaponCursor->GetCurrentItem();
		if (weapon != NULL)
		{
			returnValue += weapon->GetLightgemModifier();
		}
	}

	CInventoryItemPtr item = InventoryCursor()->GetCurrentItem();
	if (item != NULL)
	{
		returnValue += item->GetLightgemModifier();
	}

	// Take the crouching into account
	if (physicsObj.IsCrouching())
	{
		returnValue += cv_lg_crouch_modifier.GetInteger();
	}

	// No need to cap the value, this is done in idGameLocal again.

	return returnValue;
}

void idPlayer::Event_SetLightgemModifier(const char* modifierName, int amount)
{
	if (amount != 0)
	{
		DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Setting modifier %s to %d\r", modifierName, amount);
		m_LightgemModifierList[std::string(modifierName)] = amount;
	}
	else 
	{
		DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Removing modifier %s as %d was passed\r", modifierName, amount);
		// Zero value passed, remove the named value
		std::string modifierNameStr(modifierName);
		std::map<std::string, int>::iterator i = m_LightgemModifierList.find(modifierNameStr);

		if (i != m_LightgemModifierList.end()) {
			// Value found, remove it
			m_LightgemModifierList.erase(i);
			DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("Removed.\r");
		}
	}

	// Recalculate the lightgem modifier value
	m_LightgemModifier = 0;

	for (std::map<std::string, int>::const_iterator i = m_LightgemModifierList.begin(); 
	     i != m_LightgemModifierList.end();
		 ++i)
	{
		// Add the value to the lightgem modifier
		m_LightgemModifier += i->second;
	}

	DM_LOG(LC_LIGHT, LT_DEBUG)LOGSTRING("New lightgem modifier value: %d\r", m_LightgemModifier);
}

void idPlayer::Event_ReadLightgemModifierFromWorldspawn()
{
	int modifierValue(0);

	idEntity* worldspawn = gameLocal.entities[ENTITYNUM_WORLD];

	if (worldspawn != NULL)
	{
		// Read the modifier value from the worldspawn, default to zero
		modifierValue = worldspawn->spawnArgs.GetInt("lightgem_adjust", "0");
	}

	Event_SetLightgemModifier("world", modifierValue);
}

void idPlayer::SendHUDMessage(const idStr& text)
{
	if (text.IsEmpty()) {
		return;
	}

	hudMessages.Append(text);
}

void idPlayer::SendInventoryPickedUpMessage(const idStr& text)
{
	if (text.IsEmpty()) return;

	inventoryPickedUpMessages.Append(text);
}

void idPlayer::Event_Pausegame()
{
	gameLocal.PauseGame(true);
}

void idPlayer::Event_Unpausegame()
{
	gameLocal.PauseGame(false);
}

void idPlayer::Event_MissionSuccess()
{
	// Set the gamestate
	gameLocal.SetMissionResult(MISSION_COMPLETE);
	gameLocal.sessionCommand = "disconnect";
}

void idPlayer::Event_TriggerMissionEnd() 
{
	if (hudMessages.Num() > 0)
	{
		// There are still HUD messages pending, postpone this event
		PostEventMS(&EV_TriggerMissionEnd, 3000);
		return;
	}

	gameLocal.PrepareForMissionEnd();

	idVec4 fadeColor(0,0,0,1);
	playerView.Fade(fadeColor, 1500);

	// Schedule an mission success event right after fadeout
	PostEventMS(&EV_Mission_Success, 1500);
}

void idPlayer::Event_GetLocation()
{
	idThread::ReturnEntity( GetLocation() );
}

void idPlayer::Event_StartGamePlayTimer()
{
	gameLocal.m_GamePlayTimer.Clear();
	gameLocal.m_GamePlayTimer.Start();
}

void idPlayer::Event_CheckAAS() 
{
	if (gameLocal.GameState() >= GAMESTATE_ACTIVE)
	{
		idList<idStr> aasNames;

		for (idAI* ai = gameLocal.spawnedAI.Next(); ai != NULL; ai = ai->aiNode.Next())
		{
			if (ai->GetAAS() == NULL)
			{
				idStr aasName = ai->spawnArgs.GetString("use_aas");
				aasNames.AddUnique(aasName);
			}
		}

		for (int i = 0; i < aasNames.Num(); i++)
		{
			SendHUDMessage("Warning: " + aasNames[i] + " is out of date!");
		}
	}
}

void idPlayer::Event_ChangeWeaponProjectile(const char* weaponName, const char* projectileDefName)
{
	// Just wrap to the actual method
	ChangeWeaponProjectile(weaponName, projectileDefName);
}

void idPlayer::Event_ResetWeaponProjectile(const char* weaponName)
{
	ResetWeaponProjectile(weaponName);
}

void idPlayer::Event_ChangeWeaponName(const char* weaponName, const char* newName)
{
	ChangeWeaponName(weaponName, newName);
}

void idPlayer::Event_GetCurWeaponName()
{
	CInventoryWeaponItemPtr weaponItem = GetCurrentWeaponItem();
	if (weaponItem == NULL) return;

	idThread::ReturnString( weaponItem->GetWeaponName().c_str() );
}

void idPlayer::ClearActiveInventoryMap()
{
	idEntity* mapEnt = m_ActiveInventoryMapEnt.GetEntity();

	if (mapEnt == NULL) return;

	// Call the method inventory_map::clear(entity userEnt)
	idThread* thread = mapEnt->CallScriptFunctionArgs("clear", true, 0, "ee", mapEnt, this);

	if (thread != NULL) 
	{
		// Run the thread at once
		thread->Execute();
	}

	m_ActiveInventoryMapEnt = NULL;
}

void idPlayer::Event_ClearActiveInventoryMap()
{
	ClearActiveInventoryMap();
}

void idPlayer::Event_SetActiveInventoryMapEnt(idEntity* mapEnt)
{
	// Check for a previously active map and clear it if necessary
	if (m_ActiveInventoryMapEnt.GetEntity() != NULL)
	{
		ClearActiveInventoryMap();
	}

	m_ActiveInventoryMapEnt = mapEnt;
}
