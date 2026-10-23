#!/usr/bin/env python3
"""Preserve 2003 entity layouts while using explicit member callback addresses.

All edited legacy files retain their original encoding and newline bytes.
The callback rewrite changes syntax only; null and already explicit pointers
are retained. Weapon behavior files maintained separately are excluded.
"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
BEHAVIOR_FILES={'dlls/hl2_dll/weapon_hopwire.cpp','dlls/hl2_dll/grenade_hopwire.cpp',
                'dlls/hl2_dll/weapon_manhack.cpp','dlls/hl2_dll/player_manhack.cpp'}
# Bare names diagnosed as member callbacks by the real MSVC HL2 server build.
BARE_CALLBACKS=set(['AccelerateThink', 'ActThink', 'ActThinkEndActOverlayTime', 'ActiveThink', 'AimThink', 'AnimThink', 'Animate', 'Attack', 'AugerThink', 'AutoSearchThink', 'BaitTouch', 'BarbedWireThink', 'BarnacleThink', 'BaseFourWheeledVehicleDeployThink', 'BaseFourWheeledVehicleStopTheRodeoMadnessThink', 'BaseFourWheeledVehicleThink', 'BaseMannedGunThink', 'BaseObjectThink', 'BayThink', 'BeamBreakThink', 'BeepThink', 'BiteTouch', 'BoostObjectThink', 'BoostPlayerThink', 'BounceTouch', 'BoxTouch', 'BrickbatThink', 'BrickbatTouch', 'BugBaitTouch', 'BugHoleThink', 'BullRushTouch', 'BullseyeThink', 'BurrowUse', 'BurstThink', 'CallDyingThink', 'CallNPCThink', 'ChangeFOVThink', 'ChaserThink', 'ChunkRemove', 'ChunkTouch', 'ClimbTouch', 'CloseTouch', 'CombatThink', 'ConstrainThink', 'CrashTouch', 'Cycle', 'DartTouch', 'DeathThink', 'DelayDeathThink', 'Deploy', 'Detonate', 'DetonateUse', 'DieTouch', 'DyingThink', 'EMPThink', 'EjectTouch', 'ExceededRangeThink', 'Explode', 'ExplodeThink', 'ExplodeTouch', 'ExtinguishThink', 'FXThink', 'FadeIn', 'FadeOut', 'FallHack', 'FallThink', 'FallTouch', 'FireThink', 'FizzleThink', 'FlareBurnTouch', 'FlareThink', 'FlareTouch', 'FlockLeaderThink', 'FlyThink', 'FlyTouch', 'FollowThink', 'FollowUse', 'FollowerUse', 'GrenadeAR2Think', 'GrenadeAR2Touch', 'GrenadeBeamTouch', 'GrenadeEnergyTouch', 'GrenadeHomerTouch', 'GrenadeMP5Touch', 'GrenadeScannerTouch', 'GrenadeSpitTouch', 'GrenadeTouch', 'GrubTouch', 'HarpoonTouch', 'HelicopterThink', 'HitTouch', 'HookThink', 'IdleThink', 'IgniteThink', 'ImpactThink', 'ImpaleThink', 'ItemTouch', 'KillBeam', 'KillThink', 'LaserThink', 'Launch', 'LauncherThink', 'LeapAttackTouch', 'LeapTouch', 'LimpetThink', 'LiveThink', 'LookThink', 'LostPowerThink', 'MakerThink', 'ManagerThink', 'ManagerUse', 'MeteorSkyboxThink', 'MeteorSpawnerThink', 'MeteorWorldThink', 'MissileRechargeThink', 'MissileTouch', 'MolotovThink', 'MolotovTouch', 'MortarExplode', 'NextFireThink', 'Off', 'PendulumUse', 'PostTeleportThink', 'PowerupThink', 'PreDetonate', 'RallyThink', 'Recharge', 'RechargeThink', 'RefireThink', 'ReloadingThink', 'RemoveThink', 'RepelUse', 'ResourcePumpThink', 'RespawnTimerThink', 'Retire', 'RockThink', 'RockTouch', 'RocketTouch', 'RopeBreakThink', 'RopeTouch', 'SLAMThink', 'SUB_CallUseToggle', 'SUB_DoNothing', 'SUB_FadeOut', 'SUB_Remove', 'SapperThink', 'SatchelThink', 'SatchelTouch', 'SatchelUse', 'SearchThink', 'SeekThink', 'SelfHealThink', 'SentryRotate', 'ShieldActiveThink', 'ShieldBeamThink', 'ShieldRechargeThink', 'ShieldRotationThink', 'ShieldThink', 'ShockTouch', 'SittingThink', 'Smoke', 'Spark', 'SpawnChunkThink', 'SpeakerThink', 'SpikeTouch', 'SpitThink', 'StalkerThink', 'Start', 'StartRagdoll', 'StartTrack', 'StickyTouch', 'StingerTouch', 'Stop', 'SuppressThink', 'Swing', 'TFPlayerDeathThink', 'Team1RespawnDelayThink', 'Team2RespawnDelayThink', 'TetherThink', 'Think', 'ThrowThink', 'TippedThink', 'ToggleUse', 'TrackTarget', 'TrackThink', 'TrackTouch', 'TumbleThink', 'TunnelThink', 'TurnOff', 'TurretThink', 'VehicleDeteriorationThink', 'VehiclePassengerThink', 'VortigauntThink', 'WaitTillDead', 'WalkerThink', 'WarnThink', 'WarningThink', 'WatchCounts'])
CALL=re.compile(r'\bSet(?:Think|ContextThink|Touch|Use|Blocked|MoveDone)\s*\(\s*'
                r'(?P<target>[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)(?=\s*[,\)])')
LITERAL=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',re.S)


def callbacks(text):
    masked=LITERAL.sub(lambda m: ''.join('\n' if c=='\n' else ' ' for c in m.group()),text)
    replacements=[]
    for match in CALL.finditer(masked):
        target=match.group('target')
        if '::' not in target and target not in BARE_CALLBACKS:continue
        value='&'+(target if '::' in target else 'ThisClass::'+target)
        replacements.append((match.start('target'),match.end('target'),value))
    for start,end,value in reversed(replacements):text=text[:start]+value+text[end:]
    return text


def replace_once(text,old,new,label):
    if new in text:return text
    if text.count(old)!=1:raise ValueError('Unexpected compiler patch input: '+label)
    return text.replace(old,new)


def source_path(root,relative):
    candidate=root/relative
    if candidate.is_file():return candidate
    # The upload retains mixed-case filenames referenced differently by VC6.
    current=root
    for component in Path(relative).parts:
        matches=[entry for entry in current.iterdir() if entry.name.casefold()==component.casefold()]
        if len(matches)!=1:raise ValueError('Missing or ambiguous source path: '+relative)
        current=matches[0]
    return current


def patch(root=ROOT):
    changed=[]
    for folder in ('dlls','game_shared'):
        for path in sorted((root/folder).rglob('*')):
            if path.suffix.lower() not in ('.cpp','.h'):continue
            relative=path.relative_to(root).as_posix()
            if relative.casefold() in BEHAVIOR_FILES:continue
            before=path.read_bytes()
            after=callbacks(before.decode('latin1')).encode('latin1')
            if before!=after:path.write_bytes(after);changed.append(relative)
    replacements={
        'filesystem/filesystem_stdio/BaseFileSystem.cpp':[
            ('directoryf for %i files", packfile, numpackfiles',
             'directoryf for %i files", packfile.GetPathString(), numpackfiles'),
            ('\tstrncpy( pString, ctime( &fileTime ), maxCharsIncludingTerminator );\n\tpString[maxCharsIncludingTerminator-1] = \'\\0\';',
             '\tif (!pString || maxCharsIncludingTerminator <= 0) return;\n'
             '\tconst time_t nativeTime = static_cast<time_t>(fileTime);\n'
             '\tconst char *formatted = ctime( &nativeTime );\n'
             '\tstrncpy( pString, formatted ? formatted : "", maxCharsIncludingTerminator );\n'
             '\tpString[maxCharsIncludingTerminator-1] = \'\\0\';'),
        ],
        'cl_dll/hudelement.h':[
            ('#include "hud.h"','#include "networkvar.h"\n#include "hud.h"'),
        ],
        'cl_dll/thermalmaterialproxy.cpp':[
            ('// FIXME, enable this later\nreturn;',
             '// FIXME, enable this later\nreturn;\n#if 0 // Dormant implementation references the absent HueToRGB helper.'),
            ('\tm_ThermalVar->SetVecValue( color[0], color[1], color[2] );\n}',
             '\tm_ThermalVar->SetVecValue( color[0], color[1], color[2] );\n#endif\n}'),
        ],
        'public/datamap.h':[
            ('#ifndef offsetof\n#define offsetof(s,m)\t(size_t)&(((s *)0)->m)\n#endif',
             '#ifndef offsetof\n#define offsetof(s,m)\t(size_t)&(((s *)0)->m)\n#endif\n\n'
             '// HL2MODERN_DATAMAP_OFFSET: legacy descriptors include indexed network wrappers.\n'
             '#define DATAMAP_OFFSET(type, member) ((size_t)&(((type *)0)->member))'),
        ],
        'public/dt_send.h':[
            ('offsetof(currentSendDTClass::MakeANetworkVar_##varName, varName[i])',
             '(offsetof(currentSendDTClass::MakeANetworkVar_##varName, varName) + (i) * sizeof(((currentSendDTClass*)0)->varName[0]))'),
            ('offsetof(currentSendDTClass::MakeANetworkVar_##varName, varName.m_Value[i])',
             '(offsetof(currentSendDTClass::MakeANetworkVar_##varName, varName) + (i) * sizeof(((currentSendDTClass*)0)->varName.m_Value[0]))'),
            ('offsetof(currentSendDTClass, varName.m_Value[i])',
             '(offsetof(currentSendDTClass, varName) + (i) * sizeof(((currentSendDTClass*)0)->varName.m_Value[0]))'),
        ],
        'dlls/ai_trackpather.h':[
            ('class CAI_TrackPather : public CAI_BaseNPC\n{',
             'class CAI_TrackPather : public CAI_BaseNPC\n{\npublic: // Metadata is consumed by derived entity registration.'),
        ],
        'dlls/npc_talker.h':[
            ('class CNPCSimpleTalker : public CAI_PlayerAlly\n{',
             'class CNPCSimpleTalker : public CAI_PlayerAlly\n{\npublic: // Derived schedule and class metadata.'),
            ('\tDECLARE_DATADESC();\n\tDEFINE_CUSTOM_AI;\n};\n\n#endif',
             'public: // Registration metadata; behavioral state above remains private.\n'
             '\tDECLARE_DATADESC();\n\tDEFINE_CUSTOM_AI;\n};\n\n#endif'),
        ],
        'dlls/hl2_dll/npc_vortigaunt.h':[
            ('class CNPC_Vortigaunt : public CNPCSimpleTalker\n{',
             'class CNPC_Vortigaunt : public CNPCSimpleTalker\n{\npublic: // External activity registration needs the class alias.'),
        ],
        'game_shared/util_shared.cpp':[
            ('class CTraceFilterEntity : public CTraceFilterSimple\n{',
             'class CTraceFilterEntity : public CTraceFilterSimple\n{\npublic: // Derived filters reference the base-class alias.'),
        ],
        'dlls/basecombatcharacter.h':[],
        'public/utlfixedlinkedlist.h':[
            ('CUtlFixedLinkedList<T, I>::FindAndRemove','CUtlFixedLinkedList<T>::FindAndRemove'),
        ],
        'public/utlmultilist.h':[
            ('CUtlMultiList<T,I>::ListHandle_t CUtlMultiList<T,I>::CreateList()',
             'typename CUtlMultiList<T,I>::ListHandle_t CUtlMultiList<T,I>::CreateList()'),
        ],
        'public/dt_recv.h':[],
        'public/vgui/MessageMap.h':[],
        'public/vgui_controls/Button.h':[
            ('\ttypedef Label BaseClass;','protected: // Derived controls may call inherited behavior through this alias.\n\ttypedef Label BaseClass;'),
        ],
        'public/vgui_controls/Label.h':[
            ('\ttypedef vgui::Panel BaseClass;','protected: // Derived labels use the public panel behavior alias.\n\ttypedef vgui::Panel BaseClass;'),
        ],
        'dlls/items.h':[
            ('\tDECLARE_DATADESC();','public: // Derived pickup types register their base datamap.\n\tDECLARE_DATADESC();'),
        ],
        'dlls/tf2_dll/tf_class_commando.cpp':[
            ('SetClassTouch( m_pPlayer, BullRushTouch );','SetClassTouch( m_pPlayer, &CPlayerClassCommando::BullRushTouch );'),
        ],
        'vgui2/game_controls/SpectatorGUI.cpp':[
            ('const_cast<vgui::Panel *>( data->GetPtr("panel") )','static_cast<vgui::Panel *>( data->GetPtr("panel") )'),
        ],
        'cl_dll/tf2_hud/c_env_meteor.cpp':[
            ('static g_MeteorCounter = 0;','static int g_MeteorCounter = 0;'),
        ],
        'cl_dll/c_baseentity.cpp':[],
        'cl_dll/ragdoll.cpp':[],
        'cl_dll/hud_pdump.cpp':[],
        'cl_dll/physics_main_client.cpp':[],
        'game_shared/predictioncopy.cpp':[],
        'engine/cd.cpp':[],
        'engine/gas2masm/gas2masm.c':[],
        'engine/vgui_basepanel.h':[
            ('class CBasePanel : public vgui::Panel\n{','class CBasePanel : public vgui::Panel\n{\nprotected: // Derived debug panels use the base behavior alias.'),
        ],
        'engine/info.cpp':[
            ('#include "quakedef.h"','#include "quakedef.h"\n#include <ctype.h>'),
        ],
        'engine/Host_cmd.cpp':[
            (r'"Build: "__TIME__" "__DATE__" (%i)\n"',r'"Build: " __TIME__ " " __DATE__ " (%i)\n"'),
        ],
        'engine/audio/private/snd_dsp.cpp':[
            ('ABS((float) rise)','fabs((float) rise)'),
        ],
        'engine/audio/private/snd_dma.cpp':[
            ('static g_snd_trace_count = 0;','static int g_snd_trace_count = 0;'),
            ('static g_snd_last_trace_chan = 0;','static int g_snd_last_trace_chan = 0;'),
        ],
        'game_shared/animation.cpp':[
            ('#include "util.h"','#if !defined(CLIENT_DLL)\n#include "util.h"\n#endif'),
        ],
        'game_shared/cstrike/weapon_deagle.cpp':[
            ('#include "util.h"','#if !defined(CLIENT_DLL)\n#include "util.h"\n#endif'),
        ],
        'game_shared/cstrike/weapon_knife.cpp':[
            ('#include "util.h"','#if !defined(CLIENT_DLL)\n#include "util.h"\n#endif'),
        ],
        'dlls/AI_Behavior_Follow.cpp':[
            ('pGroup->followers.insert(pGroup->followers.end())',
             'pGroup->followers.insert(pGroup->followers.end(), AI_Follower_t())'),
        ],
        'dlls/physics_main.cpp':[('#include "typeinfo.h"','#include <typeinfo>')],
        'public/KeyValues.cpp':[("char *subStr = strchr(keyName, '/');","const char *subStr = strchr(keyName, '/');")],
        'public/bitbuf.cpp':[('intval = (int)abs(f);','intval = (int)fabs(f);')],
        'public/fmtstr.h':[
            ('// Use this for pass-through formatting','// Pointer-to-format compatibility form; arguments belong to this call.'),
            ('FmtStrVSNPrintf( szBuf, nBufSize, ppszFormat )','FmtStrVSNPrintf( szBuf, nBufSize, formatValue, lastNamed )'),
            ('va_start(arg_ptr, (*(ppszFormat)));','va_start(arg_ptr, lastNamed);'),
            ('(*(ppszFormat)), arg_ptr','(formatValue), arg_ptr'),
            ('\t\tva_end(arg_ptr);','\t\t(void)result; \\\n\t\tva_end(arg_ptr);'),
            ('FmtStrVSNPrintf(m_szBuf, SIZE_BUF, &pszFormat);','FmtStrVSNPrintf(m_szBuf, SIZE_BUF, pszFormat, pszFormat);'),
            ('FmtStrVSNPrintf(m_szBuf, SIZE_BUF, ppszFormat);','FmtStrVSNPrintf(m_szBuf, SIZE_BUF, *ppszFormat, ppszFormat);'),
            ('\t// Use for access',
             '\t// Forward an existing argument list explicitly; no caller-stack assumptions.\n'
             '\tvoid VSprintf(const char *pszFormat, va_list args)\n'
             '\t{\n\t\tQ_vsnprintf(m_szBuf, SIZE_BUF-1, pszFormat, args);\n'
             '\t\tm_szBuf[SIZE_BUF-1]=0;\n\t}\n\n'
             '\t// Use for access'),
        ],
        'dlls/hl2_dll/npc_BaseZombie.cpp':[('static s_iAngryZombies = 0;','static int s_iAngryZombies = 0;')],
        'modernize/model_preview_studio_validation.h':[
            ('h->id != IDSTUDIOHEADER',"h->id != (('T'<<24)+('S'<<16)+('D'<<8)+'I')"),
        ],
        'dlls/animating.cpp':[
            ('for ( int b = 0; b < set->numhitboxes; b++ )\n\t{\n\t\tmstudiobbox_t *pbox = set->pHitbox( b );',
             'for ( int hitbox = 0; hitbox < set->numhitboxes; hitbox++ )\n\t{\n\t\tmstudiobbox_t *pbox = set->pHitbox( hitbox );'),
        ],
    }
    float_abs={
        'dlls/AI_NetworkManager.cpp':['abs(pNode->m_vOrigin.z - floorZ)'],
        'dlls/Movement.cpp':['abs(ang[i])'],
        'dlls/effects.cpp':['abs( m_flSpeed )'],
        'dlls/hl2_dll/npc_Manhack.cpp':['abs(GetAbsOrigin().z - floorZ)'],
        'dlls/hl2_dll/npc_SScanner.cpp':['abs(GetLocalOrigin().z - floorZ)'],
        'dlls/hl2_dll/npc_mortarsynth.cpp':['abs(GetLocalOrigin().z - floorZ)'],
        'dlls/plats.cpp':['abs(m_flSpeed)'],
        'dlls/player.cpp':['abs(gpGlobals->curtime - m_tbdPrev)'],
        'cl_dll/c_func_conveyor.cpp':['abs( flConveyorSpeed )'],
        'cl_dll/c_te_legacytempents.cpp':['abs( pTemp->GetVelocity()[2] )'],
        'cl_dll/in_camera.cpp':['abs( cur - goal )','abs( camAngles[ 2 ] - cam_idealdist.GetFloat() )'],
        'engine/audio/private/snd_dsp.cpp':['abs((fstep - 1.0) * timeslice)'],
    }
    for relative,expressions in float_abs.items():
        replacements.setdefault(relative,[]).extend((expression,'f'+expression) for expression in expressions)
    for relative in ('dlls/BaseEntity.cpp','dlls/vguiscreen.cpp'):
        replacements.setdefault(relative,[]).extend([
            ('#include "cbase.h"','#include "cbase.h"\n#include "../modernize/legacy_entity_compat.h"'),
            ("\tchar *s = strchr( szKeyName, '#' );\n\tif ( s )\n\t{\n\t\t*s = '\\0';\n\t}",
             '\tchar keyName[256];\n'
             '\tif (!HL2ModernEntityCompat::CopyKeyWithoutSuffix(szKeyName,keyName,sizeof(keyName))) return false;\n'
             '\tszKeyName=keyName;'),
        ])
    for relative in ('dlls/GameInterface.cpp','game_shared/multiplay_gamerules.cpp'):
        replacements.setdefault(relative,[]).append(
            ('#include "cbase.h"','#include "cbase.h"\n#include "../modernize/legacy_entity_compat.h"'))
    for relative,items in replacements.items():
        path=source_path(root,relative);before=path.read_bytes()
        newline=b'\r\n' if b'\r\n' in before else b'\n'
        text=before.decode('latin1').replace('\r\n','\n')
        for old,new in items:
            # Offset forms occur in several macro variants by design.
            if relative in float_abs:
                text=re.sub(r'(?<![A-Za-z_])'+re.escape(old),lambda match:new,text)
            elif relative=='public/fmtstr.h':
                if new not in text:text=text.replace(old,new)
            elif relative in ('public/dt_send.h','dlls/physics_main.cpp'):
                text=text.replace(old,new)
            else:text=replace_once(text,old,new,relative)
        if relative=='public/datamap.h':
            text=re.sub(r'(?<!define )\boffsetof\(type,\s*(name|localname)\)',r'DATAMAP_OFFSET(type, \1)',text)
        if relative=='public/utlfixedlinkedlist.h':
            text=re.sub(r'(?m)^((?:inline\s+)?)CUtlFixedLinkedList<T>::I\b',r'\1typename CUtlFixedLinkedList<T>::I',text)
            text=text.replace('template<class T, class I>','template<class T>')
        if relative=='public/dt_recv.h':
            text=text.replace('offsetof(currentRecvDTClass, varName)','((size_t)&(((currentRecvDTClass*)0)->varName))')
        if relative=='public/vgui/MessageMap.h':
            text=text.replace('(vgui::MessageFunc_t)(type::func)','(vgui::MessageFunc_t)(&type::func)')
            text=text.replace('(vgui::MessageFunc_t)type::func','(vgui::MessageFunc_t)&type::func')
        if relative=='engine/cd.cpp':
            text=re.sub(r'(AddThreadItem\(\s*)(_[A-Za-z_]\w*)(?=\s*,)',r'\1&CCDAudio::\2',text)
        if relative=='engine/gas2masm/gas2masm.c':text=re.sub(r'\binline\b','input_line',text)
        if relative=='engine/audio/private/snd_dsp.cpp':
            text=re.sub(r'&\((prc_\w+_t)\)(\w+)',r'(\1)&\2',text)
        if relative in ('cl_dll/c_baseentity.cpp','cl_dll/ragdoll.cpp'):
            text=re.sub(r'CUtlFixedLinkedList< (?!typename)(CInterpolatedVar(?:Array)?<[^>]+>::CInterpolatedVarEntry)',r'CUtlFixedLinkedList< typename \1',text)
        if relative in ('cl_dll/c_baseentity.cpp','cl_dll/hud_pdump.cpp','cl_dll/physics_main_client.cpp','game_shared/predictioncopy.cpp'):
            text=re.sub(r'#include [<"]typeinfo\.h[>"]','#include <typeinfo>',text)
        if relative=='public/dt_send.h':
            text=text.replace('offsetof(currentSendDTClass, varName), sizeof(((currentSendDTClass*)0)->varName)',
                '((size_t)&(((currentSendDTClass*)0)->varName)), sizeof(((currentSendDTClass*)0)->varName)')
        if relative in ('dlls/GameInterface.cpp','game_shared/multiplay_gamerules.cpp'):
            text=re.sub(r'engine->IsMapValid\((map|current)\)',r'HL2ModernEntityCompat::IsMapValid(engine,\1)',text)
        if relative=='dlls/basecombatcharacter.h' and '// Interaction IDs are allocated by NPC schedule registration.' not in text:
            text=re.sub(r'(\tstatic int\s+GetInteractionID\(\);[^\n]*)',
                'public: // Interaction IDs are allocated by NPC schedule registration.\n'+r'\1'+'\nprivate:',text,count=1)
        after=text.encode('latin1').replace(b'\n',newline)
        if before!=after:path.write_bytes(after);changed.append(relative)
    return changed


if __name__=='__main__':
    print('Entity compiler compatibility updated:',len(patch()),'files')
