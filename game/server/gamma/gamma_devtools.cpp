//========= GammaSource ============//
//
// Developer helper: run a console command a set time after a map starts.
// Used to drive automated in-game checks, e.g.
//   +gamma_mapstart_cmd "exec mytest" +gamma_mapstart_delay 5
//
//=================================//

#include "cbase.h"
#include "igamesystem.h"
#include "ammodef.h"
#include "baseviewmodel_shared.h"
#include "vphysics_interface.h"
#include "baseflex.h"
#include "baseanimatingoverlay.h"
#include "sceneentity.h"
#include "entitylist.h"
#include "ai_basenpc.h"
#include "ai_memory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar gamma_mapstart_cmd( "gamma_mapstart_cmd", "", 0, "Console command to run once, gamma_mapstart_delay seconds after each map starts" );
static ConVar gamma_mapstart_delay( "gamma_mapstart_delay", "3", 0, "Seconds after map start before gamma_mapstart_cmd runs" );

// Run through the local player's console when there is one, so commands that act on
// 'the command client' (npc_create, ent_create, give...) work too.
static void RunAsPlayer( const char *pCmd )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
		engine->ClientCommand( pPlayer->edict(), "%s\n", pCmd );
	else
		engine->ServerCommand( UTIL_VarArgs( "%s\n", pCmd ) );
}

class CGammaMapStartCmd : public CAutoGameSystemPerFrame
{
public:
	CGammaMapStartCmd() : CAutoGameSystemPerFrame( "CGammaMapStartCmd" ), m_flRunAt( -1 ) {}

	virtual void LevelInitPostEntity() { m_flRunAt = gpGlobals->curtime + gamma_mapstart_delay.GetFloat(); }
	virtual void LevelShutdownPreEntity() { m_flRunAt = -1; m_Delayed.Purge(); }

	struct Delayed_t { float flAt; CUtlString cmd; };
	CUtlVector<Delayed_t> m_Delayed;

	virtual void FrameUpdatePostEntityThink()
	{
		for ( int i = m_Delayed.Count() - 1; i >= 0; i-- )
		{
			if ( gpGlobals->curtime >= m_Delayed[i].flAt )
			{
				RunAsPlayer( m_Delayed[i].cmd.Get() );
				m_Delayed.Remove( i );
			}
		}
		if ( m_flRunAt < 0 || gpGlobals->curtime < m_flRunAt )
			return;
		m_flRunAt = -1;
		const char *pCmd = gamma_mapstart_cmd.GetString();
		if ( pCmd && *pCmd )
		{
			RunAsPlayer( pCmd );
		}
	}

private:
	float m_flRunAt;
};
static CGammaMapStartCmd s_GammaMapStartCmd;

// gamma_trace_down <x> <y> <z>: report what a downward trace hits.
CON_COMMAND( gamma_trace_down, "gamma_trace_down <x> <y> <z>: trace straight down and print the hit" )
{
	if ( args.ArgC() < 4 )
		return;
	Vector start( atof( args[1] ), atof( args[2] ), atof( args[3] ) );
	trace_t tr;
	UTIL_TraceLine( start, start - Vector( 0, 0, 4096 ), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
	Msg( "gamma_trace_down: hit z=%.2f fraction=%.3f displacement=%d surface=%s\n", tr.endpos.z, tr.fraction,
		( tr.surface.flags & SURF_WARP ) ? -1 : ( tr.DidHitWorld() && tr.dispFlags ? 1 : 0 ), tr.surface.name ? tr.surface.name : "" );
}

// gamma_trace_hull <x> <y> <z> [step]: player-sized box trace straight down, in one sweep or in steps.
CON_COMMAND( gamma_trace_hull, "gamma_trace_hull <x> <y> <z> [step]: player box trace straight down" )
{
	if ( args.ArgC() < 4 )
		return;
	Vector start( atof( args[1] ), atof( args[2] ), atof( args[3] ) );
	float step = args.ArgC() > 4 ? atof( args[4] ) : 4096.0f;
	Vector mins( -16, -16, 0 ), maxs( 16, 16, 72 );
	trace_t tr;
	for ( float travelled = 0; travelled < 4096; travelled += step )
	{
		UTIL_TraceHull( start, start - Vector( 0, 0, step ), mins, maxs, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
		if ( tr.fraction < 1.0f || tr.startsolid )
			break;
		start = tr.endpos;
	}
	Msg( "gamma_trace_hull: hit z=%.2f fraction=%.3f startsolid=%d surface=%s\n", tr.endpos.z, tr.fraction, tr.startsolid ? 1 : 0, tr.surface.name ? tr.surface.name : "" );
}

// gamma_playerinfo: the local player's movement state, for checking movement problems.
CON_COMMAND( gamma_playerinfo, "Print the local player's movement state" )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;
	Vector vel = pPlayer->GetAbsVelocity();
	Msg( "gamma_playerinfo: pos %.1f %.1f %.1f vel %.1f %.1f %.1f maxspeed %.1f flags 0x%x movetype %d buttons 0x%x lagcomp %d cmd fwd %.0f side %.0f\n",
		pPlayer->GetAbsOrigin().x, pPlayer->GetAbsOrigin().y, pPlayer->GetAbsOrigin().z, vel.x, vel.y, vel.z,
		pPlayer->MaxSpeed(), pPlayer->GetFlags(), pPlayer->GetMoveType(), pPlayer->m_nButtons,
		pPlayer->IsBot() ? 1 : 0, pPlayer->GetLastUserCommand()->forwardmove, pPlayer->GetLastUserCommand()->sidemove );
	surfacedata_t *pSurface = pPlayer->GetSurfaceData();
	Msg( "GAMMA_MOVEMENT surface_speed=%.3f surface_jump=%.3f surface_friction=%.3f lagged=%.3f locked=%d\n",
		pSurface ? pSurface->game.maxSpeedFactor : -1.0f,
		pSurface ? pSurface->game.jumpFactor : -1.0f,
		pSurface ? pSurface->physics.friction : -1.0f, pPlayer->GetLaggedMovementValue(), pPlayer->IsPlayerLockedInPlace() ? 1 : 0 );
}

// gamma_delay <seconds> <command...>: run a command later in this map.
CON_COMMAND( gamma_delay, "gamma_delay <seconds> <command>: run a console command after a delay" )
{
	if ( args.ArgC() < 3 )
		return;
	int i = s_GammaMapStartCmd.m_Delayed.AddToTail();
	s_GammaMapStartCmd.m_Delayed[i].flAt = gpGlobals->curtime + atof( args[1] );
	s_GammaMapStartCmd.m_Delayed[i].cmd = args.ArgS() + V_strlen( args[1] ) + 1;
}

// Keep the roster/ammo check in the game: this catches config and mounted-script
// errors which a successful DLL build cannot detect.
CON_COMMAND_F( gamma_weapon_audit, "Print owned weapon scripts, ammo limits and models", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;
	CAmmoDef *pAmmo = GetAmmoDef();
	int nWeapons = 0, nErrors = 0;
	for ( int i = 1; i < pAmmo->m_nAmmoIndex; ++i )
	{
		Ammo_t *pType = pAmmo->GetAmmoOfIndex( i );
		Msg( "GAMMA_AMMO name=%s max=%d held=%d player_damage=%d npc_damage=%d\n",
			pType->pName, pAmmo->MaxCarry( i ), pPlayer->GetAmmoCount( i ), pAmmo->PlrDamage( i ), pAmmo->NPCDamage( i ) );
	}
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
		if ( !pWeapon )
			continue;
		++nWeapons;
		const FileWeaponInfo_t &info = pWeapon->GetWpnData();
		int primary = pWeapon->GetPrimaryAmmoType(), secondary = pWeapon->GetSecondaryAmmoType();
		bool bBadAmmo = ( info.szAmmo1[0] && Q_stricmp( info.szAmmo1, "None" ) &&
			( primary < 0 || pAmmo->MaxCarry( primary ) == 0 ) ) ||
			( info.szAmmo2[0] && Q_stricmp( info.szAmmo2, "None" ) &&
			( secondary < 0 || pAmmo->MaxCarry( secondary ) == 0 ) );
		if ( bBadAmmo )
			++nErrors;
		Msg( "GAMMA_WEAPON name=%s clip=%d/%d ammo=%s reserve=%d max=%d ammo2=%s max2=%d interval=%.3f view=%s world=%s status=%s\n",
			pWeapon->GetClassname(), pWeapon->Clip1(), pWeapon->GetMaxClip1(), info.szAmmo1,
			primary >= 0 ? pPlayer->GetAmmoCount( primary ) : 0, primary >= 0 ? pAmmo->MaxCarry( primary ) : -1,
			info.szAmmo2, secondary >= 0 ? pAmmo->MaxCarry( secondary ) : -1, pWeapon->GetFireRate(),
			pWeapon->GetViewModel(), pWeapon->GetWorldModel(), bBadAmmo ? "BAD_AMMO" : "OK" );
	}
	Msg( "GAMMA_WEAPON_AUDIT weapons=%d ammo_errors=%d\n", nWeapons, nErrors );
}

CON_COMMAND_F( gamma_weaponinfo, "Print the active weapon's ammo and animation state", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	CBaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( !pWeapon )
		return;
	CBaseViewModel *pVM = pPlayer->GetViewModel();
	Msg( "GAMMA_ACTIVE time=%.3f name=%s clip=%d reserve=%d secondary=%d next_primary=%.3f next_secondary=%.3f view=%s sequence=%d duration=%.3f\n",
		gpGlobals->curtime, pWeapon->GetClassname(), pWeapon->Clip1(),
		pWeapon->GetPrimaryAmmoType() >= 0 ? pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) : 0,
		pWeapon->GetSecondaryAmmoType() >= 0 ? pPlayer->GetAmmoCount( pWeapon->GetSecondaryAmmoType() ) : 0,
		pWeapon->m_flNextPrimaryAttack - gpGlobals->curtime, pWeapon->m_flNextSecondaryAttack - gpGlobals->curtime,
		pVM ? STRING( pVM->GetModelName() ) : "none", pVM ? pVM->GetSequence() : -1,
		pVM ? pVM->SequenceDuration() : 0.0f );
}

// Actor diagnostics keep native scene playback intact while exposing its pose,
// layer and facial-controller state to isolated cfg-driven runtime tests.
CON_COMMAND_F(gamma_actorinfo,"gamma_actorinfo [targetname/classname]: actor pose, layers and facial weights",FCVAR_CHEAT)
{
    const char *match=args.ArgC()>1 ? args[1] : "npc_alyx";
    for (CBaseEntity *entity=gEntList.FirstEnt();entity;entity=gEntList.NextEnt(entity))
    {
        if (Q_stricmp(entity->GetClassname(),match) && Q_stricmp(STRING(entity->GetEntityName()),match)) continue;
        CBaseAnimating *actor=dynamic_cast<CBaseAnimating *>(entity);
        if (!actor) continue;
        CStudioHdr *hdr=actor->GetModelPtr();
        const int sequence=actor->GetSequence();
        if (!hdr || sequence<0 || sequence>=hdr->GetNumSeq()) continue;
        CBaseFlex *flex=dynamic_cast<CBaseFlex *>(actor);
        Msg("GAMMA_ACTOR time=%.3f id=%d name=%s class=%s model=%s sequence=%d label=%s cycle=%.3f rate=%.3f autolayers=%d scene=%d speaking=%d\n",
            gpGlobals->curtime,entity->entindex(),STRING(entity->GetEntityName()),entity->GetClassname(),STRING(entity->GetModelName()),
            sequence,hdr->pSeqdesc(sequence).pszLabel(),actor->GetCycle(),actor->GetPlaybackRate(),hdr->pSeqdesc(sequence).numautolayers,
            flex && IsRunningScriptedScene(flex,false),flex && IsRunningScriptedSceneWithSpeech(flex,false));
        CBaseAnimatingOverlay *overlay=dynamic_cast<CBaseAnimatingOverlay *>(actor);
        if (overlay) for (int i=0;i<overlay->GetNumAnimOverlays();++i)
        {
            CAnimationLayer *layer=overlay->GetAnimOverlay(i);
            const int seq=layer->m_nSequence;
            if (!layer->IsActive() || seq<0 || seq>=hdr->GetNumSeq()) continue;
            Msg("GAMMA_ACTOR_LAYER id=%d layer=%d label=%s cycle=%.3f weight=%.3f rate=%.3f autolayers=%d\n",
                entity->entindex(),i,hdr->pSeqdesc(seq).pszLabel(),layer->m_flCycle.Get(),layer->m_flWeight.Get(),layer->m_flPlaybackRate,hdr->pSeqdesc(seq).numautolayers);
        }
        if (flex) for (int i=0;i<flex->GetNumFlexControllers();++i)
        {
            const LocalFlexController_t index=(LocalFlexController_t)i;
            const char *name=flex->GetFlexControllerName(index);
            const float value=flex->GetFlexWeight(index);
            if (fabs(value)<0.001f && !Q_stristr(name,"jaw") && !Q_stristr(name,"mouth")) continue;
            const mstudioflexcontroller_t *controller=hdr->pFlexcontroller(index);
            Msg("GAMMA_ACTOR_FLEX id=%d name=%s value=%.4f range=%.3f..%.3f global=%d\n",entity->entindex(),name,value,controller->min,controller->max,controller->localToGlobal);
        }
    }
}

CON_COMMAND_F(gamma_actor_scene,"gamma_actor_scene <targetname/class> <scene.vcd> [replace]: start an isolated diagnostic scene",FCVAR_CHEAT)
{
    if (args.ArgC()<3) return;
    CBaseEntity *entity=gEntList.FindEntityByName(NULL,args[1]);
    if (!entity) entity=gEntList.FindEntityByClassname(NULL,args[1]);
    CBaseFlex *actor=dynamic_cast<CBaseFlex *>(entity);
    if (!actor) { Warning("GAMMA_ACTOR_SCENE actor not found: %s\n",args[1]);return; }
    if (args.ArgC()>3 && !Q_stricmp(args[3],"replace")) RemoveAllScenesInvolvingActor(actor);
    const float duration=InstancedScriptedScene(actor,args[2]);
    Msg("GAMMA_ACTOR_SCENE actor=%s scene=%s duration=%.3f\n",args[1],args[2],duration);
}

static Vector GammaActorEyeTarget( CBaseEntity *actor )
{
    Vector target=actor->EyePosition();
    CBaseAnimating *animated=dynamic_cast<CBaseAnimating *>(actor);
    if (animated)
    {
        const int eyes=animated->LookupAttachment("eyes");
        QAngle angles;
        Vector attachment;
        if (eyes>0 && animated->GetAttachment(eyes,attachment,angles)) target=attachment;
    }
    return target;
}

// A separate view entity keeps diagnostics from firing touch triggers by
// teleporting the real player into a scene's authored activation volume.
class CGammaObserverCamera : public CBaseEntity
{
    DECLARE_CLASS(CGammaObserverCamera,CBaseEntity);
public:
    virtual void Spawn()
    {
        BaseClass::Spawn();
        SetMoveType(MOVETYPE_NONE);
        SetSolid(SOLID_NONE);
        AddEffects(EF_NODRAW);
        DispatchUpdateTransmitState();
    }
    virtual int UpdateTransmitState() { return SetTransmitState(FL_EDICT_ALWAYS); }
};
LINK_ENTITY_TO_CLASS(gamma_observer_camera,CGammaObserverCamera);

CON_COMMAND_F(gamma_observe_actor,"gamma_observe_actor <targetname/class> [distance]: observe without moving the real player or touching map triggers",FCVAR_CHEAT)
{
    if (args.ArgC()<2) return;
    CBaseEntity *actor=gEntList.FindEntityByName(NULL,args[1]);
    if (!actor) actor=gEntList.FindEntityByClassname(NULL,args[1]);
    CBasePlayer *player=UTIL_GetLocalPlayer();
    if (!actor || !player) return;
    CBaseEntity *camera=gEntList.FindEntityByName(NULL,"gamma_diagnostic_observer");
    if (!camera)
    {
        camera=CreateEntityByName("gamma_observer_camera");
        if (!camera) return;
        camera->KeyValue("targetname","gamma_diagnostic_observer");
        if (DispatchSpawn(camera)<0) { UTIL_Remove(camera); return; }
        camera->Activate();
    }
    const float distance=args.ArgC()>2 ? clamp((float)atof(args[2]),32.0f,512.0f) : 120.0f;
    Vector forward;AngleVectors(QAngle(0,actor->GetAbsAngles().y,0),&forward);
    const Vector target=GammaActorEyeTarget(actor);
    const Vector origin=target+forward*distance+Vector(0,0,8);
    QAngle angles;VectorAngles(target-origin,angles);
    camera->Teleport(&origin,&angles,NULL);
    player->SetViewEntity(camera);
    const Vector playerOrigin=player->GetAbsOrigin();
    Msg("GAMMA_OBSERVER actor=%s camera=%d origin=%.2f,%.2f,%.2f player_origin=%.2f,%.2f,%.2f\n",args[1],camera->entindex(),origin.x,origin.y,origin.z,playerOrigin.x,playerOrigin.y,playerOrigin.z);
}

CON_COMMAND_F(gamma_observe_off,"Return from the diagnostic view without changing player placement",FCVAR_CHEAT)
{
    CBasePlayer *player=UTIL_GetLocalPlayer();
    if (player) player->SetViewEntity(player);
    CBaseEntity *camera=gEntList.FindEntityByName(NULL,"gamma_diagnostic_observer");
    if (camera) UTIL_Remove(camera);
}

CON_COMMAND_F(gamma_actor_view,"gamma_actor_view <targetname/class> [distance]: teleport player in front of an actor; use gamma_observe_actor for native scene tests",FCVAR_CHEAT)
{
    if (args.ArgC()<2) return;
    CBaseEntity *actor=gEntList.FindEntityByName(NULL,args[1]);
    if (!actor) actor=gEntList.FindEntityByClassname(NULL,args[1]);
    CBasePlayer *player=UTIL_GetLocalPlayer();
    if (!actor || !player) return;
    const float distance=args.ArgC()>2 ? clamp((float)atof(args[2]),48.0f,256.0f) : 100.0f;
    Vector forward;AngleVectors(QAngle(0,actor->GetAbsAngles().y,0),&forward);
    const Vector target=GammaActorEyeTarget(actor);
    const Vector camera=target+forward*distance+Vector(0,0,8);
    QAngle view;VectorAngles(target-camera,view);
    player->SetMoveType(MOVETYPE_NOCLIP);
    player->SetAbsOrigin(camera-player->GetViewOffset());
    player->SetAbsVelocity(vec3_origin);
    player->SnapEyeAngles(view);
    Msg("GAMMA_ACTOR_VIEW actor=%s distance=%.1f camera=%.1f,%.1f,%.1f\n",args[1],distance,camera.x,camera.y,camera.z);
}

static CBaseEntity *GammaNPCSubject( const char *name )
{
    return gEntList.FindEntityByName( NULL, name && *name ? name : "gamma_npc_subject" );
}

CON_COMMAND_F( gamma_npc_info, "gamma_npc_info [targetname]: native NPC health, AI, model and placement snapshot", FCVAR_CHEAT )
{
    CBaseEntity *entity = GammaNPCSubject( args.ArgC() > 1 ? args[1] : NULL );
    if ( !entity ) { Warning("GAMMA_NPC_INFO missing=1\n"); return; }
    CAI_BaseNPC *npc = entity->MyNPCPointer();
    CBaseAnimating *anim = entity->GetBaseAnimating();
    CStudioHdr *hdr = anim ? anim->GetModelPtr() : NULL;
    const int sequence = anim ? anim->GetSequence() : -1;
    const char *label = hdr && sequence >= 0 && sequence < hdr->GetNumSeq() ? hdr->pSeqdesc(sequence).pszLabel() : "<invalid>";
    CBaseEntity *enemy = npc ? npc->GetEnemy() : NULL;
    const Vector &origin = entity->GetAbsOrigin();
    Msg("GAMMA_NPC_INFO time=%.3f id=%d name=%s class=%s npc=%d health=%d max_health=%d alive=%d classification=%d model=%s bones=%d sequences=%d sequence=%d label=%s cycle=%.3f state=%d activity=%d ideal_activity=%d schedule=%s enemy=%s enemy_health=%d pos=%.2f,%.2f,%.2f vel=%.2f,%.2f,%.2f capabilities=%x\n",
        gpGlobals->curtime, entity->entindex(), STRING(entity->GetEntityName()), entity->GetClassname(), npc != NULL,
        entity->GetHealth(), entity->GetMaxHealth(), entity->IsAlive(), entity->Classify(), STRING(entity->GetModelName()),
        hdr ? hdr->numbones() : 0, hdr ? hdr->GetNumSeq() : 0, sequence, label, anim ? anim->GetCycle() : 0,
        npc ? npc->GetState() : -1, npc ? npc->GetActivity() : -1, npc ? npc->GetIdealActivity() : -1,
        npc && npc->GetCurSchedule() ? npc->GetCurSchedule()->GetName() : "<none>",
        enemy ? enemy->GetClassname() : "<none>", enemy ? enemy->GetHealth() : -1,
        origin.x, origin.y, origin.z, entity->GetAbsVelocity().x, entity->GetAbsVelocity().y, entity->GetAbsVelocity().z,
        npc ? npc->CapabilitiesGet() : 0 );
}

CON_COMMAND_F( gamma_npc_spawn, "gamma_npc_spawn <classname> <x> <y> <z> [yaw] [equipment]: isolated native spawn named gamma_npc_subject", FCVAR_CHEAT )
{
    if ( args.ArgC() < 5 ) return;
    const bool allowed = CBaseEntity::IsPrecacheAllowed();
    CBaseEntity::SetAllowPrecache( true );
    CBaseEntity *entity = CreateEntityByName( args[1] );
    if ( entity )
    {
        EHANDLE handle = entity;
        entity->KeyValue( "targetname", "gamma_npc_subject" );
        if ( args.ArgC() > 6 ) entity->KeyValue( "additionalequipment", args[6] );
        entity->SetAbsOrigin( Vector(atof(args[2]), atof(args[3]), atof(args[4])) );
        entity->SetAbsAngles( QAngle(0, args.ArgC() > 5 ? atof(args[5]) : 0, 0) );
        const int result = DispatchSpawn( entity );
        if ( result >= 0 && handle && !handle->IsMarkedForDeletion() ) handle->Activate();
        Msg("GAMMA_NPC_SPAWN class=%s result=%d entity=%d\n", args[1], result, handle ? handle->entindex() : -1);
        if ( handle ) RunAsPlayer( "gamma_npc_info gamma_npc_subject" );
    }
    else Warning("GAMMA_NPC_SPAWN class=%s factory_missing=1\n", args[1]);
    CBaseEntity::SetAllowPrecache( allowed );
}

CON_COMMAND_F( gamma_npc_target, "gamma_npc_target <x> <y> <z>: give the probe NPC a real damageable bullseye enemy", FCVAR_CHEAT )
{
    CBaseEntity *subject = GammaNPCSubject( NULL );
    CAI_BaseNPC *npc = subject ? subject->MyNPCPointer() : NULL;
    if ( !npc || args.ArgC() < 4 ) return;
    const bool allowed = CBaseEntity::IsPrecacheAllowed();
    CBaseEntity::SetAllowPrecache( true );
    CBaseEntity *target = CreateEntityByName( "npc_bullseye" );
    if ( target )
    {
        target->KeyValue( "targetname", "gamma_npc_target" );
        target->KeyValue( "health", "100000" );
        target->SetAbsOrigin( Vector(atof(args[1]), atof(args[2]), atof(args[3])) );
        if ( DispatchSpawn(target) >= 0 )
        {
            target->Activate();
            npc->AddEntityRelationship( target, D_HT, 99 );
            npc->UpdateEnemyMemory( target, target->GetAbsOrigin() );
            npc->SetEnemy( target );
            Msg("GAMMA_NPC_TARGET npc=%d target=%d health=%d\n", npc->entindex(), target->entindex(), target->GetHealth());
        }
    }
    CBaseEntity::SetAllowPrecache( allowed );
}
