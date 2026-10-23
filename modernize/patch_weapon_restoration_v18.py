#!/usr/bin/env python3
"""Reproducible v18 weapon safety/restoration patch over the v17 source tree.

This implements source behavior; it does not supply or certify original art.
Run after the historical weapon patches and project conversion.
"""
from pathlib import Path
from json_output_policy import write_json
import json
import re
from weapon_branch_audit import audit as branch_audit, EXCLUDED
from weapon_build_audit import audit as build_audit

ROOT=Path(__file__).resolve().parents[1]

def patch(rel,change):
    p=ROOT/rel;raw=p.read_bytes();crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n');t=change(s)
    if t!=s:p.write_bytes((t.replace('\n','\r\n') if crlf else t).encode('latin1'))

def once(source,before,after):
    if after in source:return source
    if source.count(before)!=1:raise ValueError('patch target missing or ambiguous: '+before[:100])
    return source.replace(before,after,1)

HELPERS=r'''// HL2MODERN_SAFE_CHEAT_WEAPON: validate installed data before spawning.
// Called only from branch-local impulse lists; normal give/pickup stays intact.
CBaseCombatWeapon *CBasePlayer::GiveCheatWeapon( const char *name )
{
    extern int g_iWeaponCheat;
#if defined(HLDEMO_BUILD)
    return NULL;
#endif
    if ( !g_iWeaponCheat || !HL2ModernWeapons::WeaponClass(name) ) return NULL;
    CBaseCombatWeapon *owned = Weapon_OwnsThisType(name);
    if ( owned ) return owned;
    bool freeSlot = false;
    for ( int slot=0; slot<WeaponCount(); ++slot )
        if ( !GetWeapon(slot) ) { freeSlot=true; break; }
    if ( !freeSlot ) { Msg("impulse 101 inventory full: %s\n",name); return NULL; }

    WEAPON_FILE_INFO_HANDLE handle = GetInvalidWeaponInfoHandle();
    if ( !ReadWeaponDataFromFileForSlot(filesystem,name,&handle) )
    { Msg("impulse 101 missing weapon script: scripts/%s.txt\n",name); return NULL; }
    const FileWeaponInfo_t *info = GetFileWeaponInfoFromHandle(handle);
    if ( !HL2ModernWeapons::ModelPath(info->szViewModel) ||
         !HL2ModernWeapons::ModelPath(info->szWorldModel) ||
         !filesystem->FileExists(info->szViewModel,"GAME") ||
         !filesystem->FileExists(info->szWorldModel,"GAME") )
    { Msg("impulse 101 missing/invalid models for %s: %s / %s\n",name,info->szViewModel,info->szWorldModel); return NULL; }
    if ( (info->szAmmo1[0] && GetAmmoDef()->Index(info->szAmmo1)<0) ||
         (info->szAmmo2[0] && GetAmmoDef()->Index(info->szAmmo2)<0) )
    { Msg("impulse 101 undefined ammunition for %s\n",name); return NULL; }

    const int previousImpulse = gEvilImpulse101;
    gEvilImpulse101 = true;
    EHANDLE created = GiveNamedItem(name);
    gEvilImpulse101 = previousImpulse;
    owned = Weapon_OwnsThisType(name);
    if ( !owned && created && !created->IsMarkedForDeletion() )
    {
        // A rejected developer grant must not leave an invisible/world orphan.
        CBaseCombatWeapon *weapon = dynamic_cast<CBaseCombatWeapon*>((CBaseEntity*)created);
        if ( weapon && !weapon->GetOwner() ) UTIL_Remove(weapon);
    }
    return owned;
}

void CBasePlayer::RefillCheatWeaponAmmo( void )
{
    extern int g_iWeaponCheat;
    if ( !g_iWeaponCheat ) return;
    // Derive reserves from the actual owned weapon's branch ammo registry.
    // This includes prototype ammo and grenade equipment without foreign names.
    for ( int slot=0; slot<WeaponCount(); ++slot )
    {
        CBaseCombatWeapon *weapon = GetWeapon(slot);
        if ( !weapon ) continue;
        const int types[2] = { weapon->GetPrimaryAmmoType(), weapon->GetSecondaryAmmoType() };
        for ( int channel=0; channel<2; ++channel )
        {
            const int type=types[channel];
            if ( type<0 || type>=MAX_AMMO_TYPES || !GetAmmoDef()->GetAmmoOfIndex(type) ) continue;
            const int needed=HL2ModernWeapons::AmmoNeeded(GetAmmoDef()->MaxCarry(type),GetAmmoCount(type));
            if ( needed>0 ) GiveAmmo(needed,type);
        }
        if ( weapon->UsesClipsForAmmo1() && weapon->GetMaxClip1()>0 ) weapon->m_iClip1=weapon->GetMaxClip1();
        if ( weapon->UsesClipsForAmmo2() && weapon->GetMaxClip2()>0 ) weapon->m_iClip2=weapon->GetMaxClip2();
    }
}

'''

def player(s):
    for header in ('filesystem.h','studio.h','../modernize/weapon_grant_policy.h'):
        include='#include "'+header+'"'
        if include not in s:s=once(s,'#include "ammodef.h"','#include "ammodef.h"\n'+include)
        # Handle intermediate v18 patch runs that inserted the include twice.
        seen=False;lines=[]
        for line in s.splitlines(True):
            if line.strip()==include:
                if seen:continue
                seen=True
            lines.append(line)
        s=''.join(lines)
    if 'HL2MODERN_SAFE_CHEAT_WEAPON' not in s:
        needle='CBaseEntity\t*CBasePlayer::GiveNamedItem( const char *pszName, int iSubType )'
        s=once(s,needle,HELPERS+needle)
    if 'HL2MODERN_CHEAT_STUDIO_HEADER' not in s:
        needle='// HL2MODERN_SAFE_CHEAT_WEAPON:'
        helper='''// HL2MODERN_CHEAT_STUDIO_HEADER: a path is not proof of a compatible model.
static bool HL2Modern_CheatStudioModel(const char *path)
{
    FileHandle_t file=filesystem->Open(path,"rb","GAME");
    if ( file==FILESYSTEM_INVALID_HANDLE ) return false;
    unsigned char header[80];
    const int bytes=filesystem->Read(header,sizeof(header),file);
    const unsigned long length=filesystem->Size(file);
    filesystem->Close(file);
    return bytes==(int)sizeof(header) && HL2ModernWeapons::StudioHeader(header,(unsigned int)bytes,length,STUDIO_VERSION,sizeof(studiohdr_t));
}

'''
        s=once(s,needle,helper+needle)
    s=once(s,'!filesystem->FileExists(info->szWorldModel,"GAME") )',
               '!filesystem->FileExists(info->szWorldModel,"GAME") ||\n'
               '         !HL2Modern_CheatStudioModel(info->szViewModel) ||\n'
               '         !HL2Modern_CheatStudioModel(info->szWorldModel) )')
    s=s.replace('impulse 101 missing/invalid models for %s:','impulse 101 missing/invalid/unsupported models for %s:')
    # The base HL2 list also uses the validation and duplicate/capacity guard.
    begin=s.index('\tcase 101:',s.index('void CBasePlayer::CheatImpulseCommands'))
    end=s.index('\tcase 102:',begin)
    block=s[begin:end]
    block=re.sub(r'GiveNamedItem\( ("weapon_[a-z0-9_]+") \)',r'GiveCheatWeapon( \1 )',block)
    if 'RefillCheatWeaponAmmo();' not in block:
        block=block.replace('\t\tgEvilImpulse101\t\t= false;','\t\tRefillCheatWeaponAmmo();\n\t\tgEvilImpulse101\t\t= false;')
    return s[:begin]+block+s[end:]

def header(s):
    return once(s,'\tvirtual CBaseEntity\t\t*GiveNamedItem( const char *szName, int iSubType = 0 );',
                '\tvirtual CBaseEntity\t\t*GiveNamedItem( const char *szName, int iSubType = 0 );\n'
                '\tCBaseCombatWeapon\t\t*GiveCheatWeapon( const char *szName );\n'
                '\tvoid\t\t\t\t\tRefillCheatWeaponAmmo( void );')

def branch_player(s,cls):
    if cls=='CHL2_Player':
        s=once(s,'#include "globals.h"','#include "globals.h"\n#include "filesystem.h"')
    signature='void '+cls+'::CheatImpulseCommands( int iImpulse )\n{'
    gate='''
    // HL2MODERN_BRANCH_CHEAT_GATE: overrides must honor the base cheat gate.
    if ( iImpulse == 101 )
    {
#if defined(HLDEMO_BUILD)
        return;
#else
        extern int g_iWeaponCheat;
        if ( g_iWeaponCheat == 0 ) return;
#endif
    }'''
    if 'HL2MODERN_BRANCH_CHEAT_GATE' not in s:s=once(s,signature,signature+gate)
    if cls=='CHL1_Player':
        begin=s.index('\tcase 101:',s.index(signature));end=s.index('\tcase 0:',begin)
        block=s[begin:end]
        block=re.sub(r'GiveNamedItem\( ("weapon_[a-z0-9_]+") \)',r'GiveCheatWeapon( \1 )',block)
        if 'RefillCheatWeaponAmmo();' not in block:block=block.replace('\t\tgEvilImpulse101 = false;','\t\tRefillCheatWeaponAmmo();\n\t\tgEvilImpulse101 = false;')
        return s[:begin]+block+s[end:]
    s=s.replace('GiveNamedItem(weapons[k]);','GiveCheatWeapon(weapons[k]);')
    needle='\t\t\t\tgiven, already, failed);'
    if needle+'\n\t\t\tRefillCheatWeaponAmmo();' not in s:
        s=once(s,needle,needle+'\n\t\t\tRefillCheatWeaponAmmo();')
    if cls=='CCSPlayer':
        s=once(s,'\t\t\tAddAccount( 16000 );','\t\t\tAddAccount( 16000 );\n\t\t\t// Match this branch\'s assault-suit pickup without requiring another model.\n\t\t\tm_iKevlar = 2;\n\t\t\tSetArmorValue(100);')
        s=once(s,'\tif ( pCSWeapon )\n\t{\n\t\t// For rifles,',
                   '\t// HL2MODERN_CS_CHEAT_SLOTS: retain the full arsenal only during expanded cheats.\n'
                   '\textern int gEvilImpulse101;\n'
                   '\tif ( pCSWeapon && !(gEvilImpulse101 && sv_impulse101_allweapons.GetBool()) )\n\t{\n\t\t// For rifles,')
        s=once(s,'\tif ( Weapon_SlotOccupied( pWeapon ) )',
                   '\textern int gEvilImpulse101;\n\tif ( Weapon_SlotOccupied( pWeapon ) &&\n'
                   '\t\t!(gEvilImpulse101 && sv_impulse101_allweapons.GetBool()) )')
        # Filling by the actual inventory also covers HE/flashbang ammo.
        s=once(s,'\t\t\tgEvilImpulse101 = false;','\t\t\tRefillCheatWeaponAmmo();\n\t\t\tgEvilImpulse101 = false;')
    return s

def irifle(s):
    if s.startswith('f//'):s=s[1:]
    s=once(s,'void CWeaponIRifle::Precache( void )\n{\n\tBaseClass::Precache();',
               'void CWeaponIRifle::Precache( void )\n{\n\tUTIL_PrecacheOther("env_flare");\n\tBaseClass::Precache();')
    s=once(s,'\tm_iClip1 = m_iClip1 - 1;\n\n\tSendWeaponAnim( ACT_VM_PRIMARYATTACK );\n\tpOwner->m_flNextAttack = gpGlobals->curtime + 1;',
               '\t// HL2MODERN_IRIFLE_AMMO: consume only after a flare is created.\n'
               '\tif ( UsesClipsForAmmo1() ? m_iClip1<=0 : pOwner->GetAmmoCount(m_iPrimaryAmmoType)<=0 ) return;')
    s=once(s,'\tif ( pFlare == NULL )\n\t\treturn;\n\n\tVector forward;',
               '\tif ( pFlare == NULL )\n\t\treturn;\n'
               '\tif ( UsesClipsForAmmo1() ) --m_iClip1;\n\telse pOwner->RemoveAmmo(1,m_iPrimaryAmmoType);\n'
               '\tSendWeaponAnim(ACT_VM_PRIMARYATTACK);\n\tm_flNextPrimaryAttack=gpGlobals->curtime+GetFireRate();\n'
               '\tpOwner->m_flNextAttack=m_flNextPrimaryAttack;\n\n\tVector forward;')
    return once(s,'\tCBaseCombatCharacter *pOwner  = GetOwner();\n\tif (pOwner)\n\t{\n\t\tpOwner->GiveAmmo( 90, m_iPrimaryAmmoType);\n\t}\n\treturn BaseClass::Deploy();',
                  '\t// Switching weapons must not manufacture ammunition.\n\treturn BaseClass::Deploy();')

def tripwire(s):
    s=s.replace('SetTouch(TripwireTouch);','SetTouch(&CWeapon_Tripwire::TripwireTouch);')
    s=once(s,'void CWeapon_Tripwire::Spawn( )\n{\n\tUTIL_Remove(this);\n\treturn;\n',
               'void CWeapon_Tripwire::Spawn( )\n{\n\t// HL2MODERN_TRIPWIRE_SPAWN: restored existing deployment implementation.\n')
    s=once(s,'\t\t\tCTripwireGrenade *pMine = (CTripwireGrenade *)pEnt;\n\t\t\tpMine->SetOwner( GetOwner() );',
               '\t\t\tCTripwireGrenade *pMine = dynamic_cast<CTripwireGrenade *>(pEnt);\n'
               '\t\t\tif ( !pMine ) return;\n\t\t\tpMine->SetOwner( GetOwner() );')
    # Abort post-removal animation work rather than invoking methods on a dying weapon.
    s=s.replace('\t\t\t\tUTIL_Remove(this);\n\t\t\t}', '\t\t\t\tUTIL_Remove(this);\n\t\t\t\treturn;\n\t\t\t}')
    s=s.replace('\t\t\tUTIL_Remove(this);\n\t\t}', '\t\t\tUTIL_Remove(this);\n\t\t\treturn;\n\t\t}')
    return s

def thumper(s):
    s=s.replace('SetThink( ThumpThink );','SetThink( &CPortableThumper::ThumpThink );')
    s=s.replace('SetUse( ThumperUse );','SetUse( &CPortableThumper::ThumperUse );')
    s=once(s,'void CPortableThumper::Spawn( void )\n{','void CPortableThumper::Spawn( void )\n{\n\tPrecache(); // HL2MODERN_THUMPER_PLACEMENT')
    s=once(s,'\tif( !pActivator->IsPlayer() )','\tif( !pActivator || !pActivator->IsPlayer() )')
    s=once(s,'\tpPlayer->GiveNamedItem( "weapon_thumper" );\n\n\tUTIL_Remove( this );',
               '\tCBaseCombatWeapon *weapon=pPlayer->Weapon_OwnsThisType("weapon_thumper");\n'
               '\tif ( weapon )\n\t{\n\t\tif ( pPlayer->GiveAmmo(1,weapon->GetPrimaryAmmoType())<=0 ) return;\n\t}\n'
               '\telse\n\t{\n\t\tpPlayer->GiveNamedItem("weapon_thumper");\n'
               '\t\tif ( !pPlayer->Weapon_OwnsThisType("weapon_thumper") ) return;\n\t}\n\n\tUTIL_Remove( this );')
    s=once(s,'\tDecrementAmmo( pPlayer );\n\n\ttrace_t tr;',
               '\tif ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType)<=0 ) return;\n\n\ttrace_t tr;')
    return once(s,'\tif( tr.fraction != 1.0 )\n\t{\n\t\tCreate( "portable_thumper", tr.endpos, vec3_angle, NULL );\n\t\tm_flNextPrimaryAttack = gpGlobals->curtime + 0.5;\n\t}',
                  '\tif ( !tr.startsolid && tr.fraction<1.0 && tr.plane.normal.z>=0.5f )\n\t{\n'
                  '\t\tCBaseEntity *placed=Create("portable_thumper",tr.endpos,vec3_angle,pPlayer);\n'
                  '\t\tif ( placed ) DecrementAmmo(pPlayer);\n\t}')

def immolator(s):
    s=s.replace('SetThink( UpdateThink );','SetThink( &CWeaponImmolator::UpdateThink );')
    s=once(s,'\tvoid ItemPostFrame( void );','\tvoid ItemPostFrame( void );\n\tbool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );')
    s=once(s,'\tm_fMaxRange1 = 4096;','\tm_fMaxRange1 = 4096;\n\tm_vecImmolatorTarget = IMMOLATOR_TARGET_INVALID;')
    s=once(s,'\tCAI_BaseNPC* npcOwner = GetOwner()->MyNPCPointer();',
               '\tCBaseCombatCharacter *owner=GetOwner();\n\tCAI_BaseNPC* npcOwner=owner ? owner->MyNPCPointer() : NULL;\n\tif ( !npcOwner ) return false;')
    s=once(s,'\tif( pOwner && !pOwner->IsAlive() )','\tif( !pOwner || !pOwner->IsAlive() )')
    s=once(s,'\t\tImmolationDamage( CTakeDamageInfo( this, this, 1, DMG_BURN ), tr.endpos, m_flBurnRadius, CLASS_NONE );',
               '\t\tImmolationDamage( CTakeDamageInfo( this, GetOwner(), 1, DMG_BURN ), tr.endpos, m_flBurnRadius, CLASS_NONE );')
    s=once(s,'\t\t// The attack beam struck some kind of entity directly.',
               '\t\t// HL2MODERN_IMMOLATOR_DIRECT_HIT: use the existing burn tick on direct impacts.\n'
               '\t\tif ( tr.m_pEnt && tr.m_pEnt->m_takedamage!=DAMAGE_NO )\n'
               '\t\t\ttr.m_pEnt->TakeDamage(CTakeDamageInfo(this,GetOwner(),1,DMG_BURN));')
    if 'bool CWeaponImmolator::Holster' not in s:
        s=once(s,'void CWeaponImmolator::ItemPostFrame( void )',
                   'bool CWeaponImmolator::Holster( CBaseCombatWeapon *pSwitchingTo )\n{\n'
                   '\tStopImmolating();\n\treturn BaseClass::Holster(pSwitchingTo);\n}\n\n'
                   'void CWeaponImmolator::ItemPostFrame( void )')
    return s

def selection_positions(s):
    s=s.replace('Q_stricmp(a->GetClassname(),GetClassname())','Q_stricmp(a->GetWpnData().szClassName,GetWpnData().szClassName)')
    original='''int CBaseCombatWeapon::GetPosition( void ) const
{
	return GetWpnData().iPosition;
}'''
    replacement='''int CBaseCombatWeapon::GetPosition( void ) const
{
#if defined(CSTRIKE_DLL)
    // HL2MODERN_CS_SELECTION_COLLISIONS: expanded cheats can own several
    // rifles/pistols with the same script position. Rank the complete slot
    // consistently on client and server, without rewriting installed scripts.
    CBaseCombatCharacter *owner=GetOwner();
    if ( owner )
    {
        bool collision=false;
        int rank=0, count=0;
        for ( int i=0; i<owner->WeaponCount(); ++i )
        {
            CBaseCombatWeapon *a=owner->GetWeapon(i);
            if ( !a || a->GetSlot()!=GetSlot() ) continue;
            ++count;
            const int position=a->GetWpnData().iPosition;
            if ( position<GetWpnData().iPosition ||
                 (position==GetWpnData().iPosition && Q_stricmp(a->GetWpnData().szClassName,GetWpnData().szClassName)<0) ) ++rank;
            for ( int j=0; j<i; ++j )
            {
                CBaseCombatWeapon *b=owner->GetWeapon(j);
                if ( b && b->GetSlot()==GetSlot() && b->GetWpnData().iPosition==position ) collision=true;
            }
        }
        if ( collision && count<=MAX_WEAPON_POSITIONS ) return rank;
    }
#endif
	return GetWpnData().iPosition;
}'''
    return once(s,original,replacement)

def hud(s,branch):
    s=once(s,'#include "cbase.h"','#include "cbase.h"\n#include "../../modernize/weapon_hud_layout.h"')
    if branch=='cstrike':
        before='''				if ( pWeapon->GetSpriteActive() )
				{
					if (pWeapon == pSelectedWeapon)
					{
						col[3] = m_flSelectionAlphaOverride;
					}
					pWeapon->GetSpriteActive()->DrawSelf( xpos + m_flIconXPos, ypos + m_flIconYPos, col );
				}'''
        after='''                // HL2MODERN_CS_HUD_FIT: active/inactive art keeps its original aspect.
                CHudTexture const *sprite=pWeapon==pSelectedWeapon ? pWeapon->GetSpriteActive() : pWeapon->GetSpriteInactive();
                if ( !sprite ) sprite=pWeapon->GetSpriteActive();
                if ( sprite )
                {
                    if ( pWeapon==pSelectedWeapon ) col[3]=m_flSelectionAlphaOverride;
                    HL2ModernHud::Rect fitted;
                    if ( HL2ModernHud::FitIcon(sprite->Width(),sprite->Height(),xpos+(int)m_flIconXPos,ypos+(int)m_flIconYPos,
                        largeBoxWide-2*(int)m_flIconXPos,(int)m_flTextYPos-(int)m_flIconYPos-2,fitted) )
                        sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
                }'''
    else:
        before='''		info.icon->DrawSelf( sizefracw * iconXPos, sizefrach * iconYPos, sizefracw * iconWide, sizefrach * iconTall,
			iconColor ); '''
        after='''        // HL2MODERN_TF_HUD_FIT: animated panel growth must not stretch weapon art.
        HL2ModernHud::Rect fitted;
        if ( HL2ModernHud::FitIcon(info.icon->Width(),info.icon->Height(),(int)(sizefracw*iconXPos),
             (int)(sizefrach*iconYPos),(int)(sizefracw*iconWide),(int)(sizefrach*iconTall),fitted) )
            info.icon->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,iconColor);'''
    return once(s,before,after)

def atlas(s):
    s=once(s,'#include "cbase.h"','#include "cbase.h"\n#include "../modernize/weapon_hud_layout.h"')
    before='\tvgui::surface()->DrawGetTextureSize( t->textureId, wide, tall );'
    after=before+'''
    // HL2MODERN_ATLAS_DIMENSIONS: reject malformed rectangles using the
    // loaded texture's dimensions before dividing or sampling outside it.
    if ( !HL2ModernHud::AtlasRect(t->rc.left,t->rc.top,t->rc.right,t->rc.bottom,wide,tall) )
    {
        t->rc.right=t->rc.left;
        t->rc.bottom=t->rc.top;
        Q_memset(t->texCoords,0,sizeof(t->texCoords));
        return;
    }'''
    s=once(s,before,after)
    before='\t\tvgui::surface()->DrawGetTextureSize( icon->textureId, wide, tall );'
    after=before+'''
        if ( !HL2ModernHud::AtlasRect(icon->rc.left,icon->rc.top,icon->rc.right,icon->rc.bottom,wide,tall) )
        {
            icon->rc.right=icon->rc.left;
            icon->rc.bottom=icon->rc.top;
            Q_memset(icon->texCoords,0,sizeof(icon->texCoords));
            continue;
        }'''
    s=once(s,before,after)
    s=once(s,'void CHudTexture::DrawSelf( int x, int y, int w, int h, Color& clr ) const\n{\n\tif ( textureId == -1 )',
               'void CHudTexture::DrawSelf( int x, int y, int w, int h, Color& clr ) const\n{\n\tif ( textureId == -1 || Width()<=0 || Height()<=0 || w<=0 || h<=0 )')
    return once(s,'void CHudTexture::DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, Color& clr ) const\n{\n\tif ( textureId == -1 )',
                  'void CHudTexture::DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, Color& clr ) const\n{\n'
                  '\tif ( textureId == -1 || Width()<=0 || Height()<=0 || cropx<0 || cropy<0 || cropw<=0 || croph<=0 ||\n'
                  '\t\tcropx>Width() || cropy>Height() || cropw>Width()-cropx || croph>Height()-cropy )')

def legacy_hud(s):
    s=s.replace('SpriteMaterialName(texture,normalizedTexture,sizeof(normalizedTexture))',
                'SpriteMaterialName(texture,normalizedTexture,sizeof(normalizedTexture),spriteData!=pTemp)')
    before='''				const char *shortName = pTemp->GetName();
				const char *texture = pTemp->GetString( "file", "" );
				const int left = pTemp->GetInt( "x", 0 );
				const int top = pTemp->GetInt( "y", 0 );
				const int wide = pTemp->GetInt( "width", 0 );
				const int tall = pTemp->GetInt( "height", 0 );'''
    after='''                // HL2MODERN_HUD_LEGACY_RESOLUTION: preserve the original role
                // name while reading an existing 640/320 raster layout.
                // Font-only data has no raster file and remains unsupported.
                const char *shortName = pTemp->GetName();
                KeyValues *spriteData=HL2ModernHud::SelectSpriteData(pTemp);
                char normalizedTexture[sizeof(((CHudTexture *)0)->szTextureFile)];
                const char *texture=spriteData ? spriteData->GetString("file","") : "";
                if ( !spriteData || !HL2ModernHud::SpriteMaterialName(texture,normalizedTexture,sizeof(normalizedTexture),spriteData!=pTemp) )
                {
                    pTemp=pTemp->GetNextKey();
                    continue;
                }
                texture=normalizedTexture;
                const int left=spriteData->GetInt("x",0);
                const int top=spriteData->GetInt("y",0);
                const int wide=spriteData->GetInt("width",0);
                const int tall=spriteData->GetInt("height",0);'''
    return once(s,before,after)

def twohanded(s):
    s=s.replace('m_hLeftWeapon != left','m_hLeftWeapon.Get() != left').replace('m_hRightWeapon != right','m_hRightWeapon.Get() != right')
    return once(s,'\tif ( m_hRightWeapon != NULL && m_hRightWeapon->IsPredicted() )',
                  '\t// HL2MODERN_TWOHANDED_SERVER: prediction is only a client-side condition.\n\tif ( m_hRightWeapon != NULL )')

def basehl_datamap(s):
    return once(s,'class CBaseHLCombatWeapon : public CBaseCombatWeapon\n{\n#if !defined( CLIENT_DLL )',
                  'class CBaseHLCombatWeapon : public CBaseCombatWeapon\n{\npublic: // HL2MODERN_WEAPON_DATAMAP_ACCESS: derived save tables reference the base map.\n#if !defined( CLIENT_DLL )')

def main():
    patch('dlls/player.h',header);patch('dlls/player.cpp',player)
    for rel,cls in [('dlls/hl2_dll/hl2_player.cpp','CHL2_Player'),('dlls/hl1_dll/hl1_player.cpp','CHL1_Player'),
                    ('dlls/cstrike/cs_player.cpp','CCSPlayer'),('dlls/tf2_dll/tf_player.cpp','CBaseTFPlayer')]:
        patch(rel,lambda s,cls=cls:branch_player(s,cls))
    for rel,change in [('dlls/hl2_dll/weapon_irifle.cpp',irifle),('dlls/hl2_dll/weapon_tripwire.cpp',tripwire),
                       ('dlls/hl2_dll/weapon_thumper.cpp',thumper),('dlls/hl2_dll/weapon_immolator.cpp',immolator),
                       ('game_shared/basecombatweapon_shared.cpp',selection_positions),
                       ('cl_dll/hud.cpp',lambda s:legacy_hud(atlas(s))),
                       ('game_shared/tf2/weapon_twohandedcontainer.cpp',twohanded),
                       ('game_shared/hl2/basehlcombatweapon_shared.h',basehl_datamap)]:patch(rel,change)
    for branch,rel in [('cstrike','cl_dll/cstrike/cs_hud_weaponselection.cpp'),('tf2','cl_dll/tf2_hud/hud_weaponselection.cpp')]:
        patch(rel,lambda s,b=branch:hud(s,b))
    inventory=branch_audit()
    write_json(ROOT/'modernize/weapon_branch_inventory.json', inventory)
    builds=build_audit()
    write_json(ROOT/'modernize/weapon_build_report.json', builds)
    vanilla={'weapon_frag','weapon_crowbar','weapon_pistol','weapon_ar2','weapon_shotgun','weapon_smg1','weapon_gauss','weapon_physcannon','weapon_rpg'}
    manifest=[]
    for branch in ('hl2','cstrike','tf2','hl1'):
        label='cs' if branch=='cstrike' else branch
        registered=inventory['branches'][branch]
        requested=sorted(r['classname'] for r in registered if not r['grant_exclusion_reason'] and (branch!='hl2' or r['classname'] not in vanilla))
        # Retain the disabled brickbat as an explicit candidate, never an active registration.
        if branch=='hl2':requested=sorted(requested+['weapon_brickbat'])
        compiled={r['classname'] for r in builds['branches'][branch]['weapons'] if r['server_compiled_all_configs']}
        row={'branch':label,'schema_version':18,'registered':len(registered),'impulse101_extra':requested,
             'base_or_abstract_excluded':sorted(r['classname'] for r in registered if r['grant_exclusion_reason'] and 'base class' in r['grant_exclusion_reason']),
             'excluded_source_or_internal':{r['classname']:r['grant_exclusion_reason'] for r in registered if r['grant_exclusion_reason']},
             'compiled_impulse101_extra':[n for n in requested if n in compiled],
             'excluded_not_compiled':[n for n in requested if n not in compiled],
             'assets_verified':False,'viewmodels_verified':False,'hud_icons_verified':False,'texture_scale_verified':False,'windows_build_verified':False,
             'note':'Lists are branch-local, active-registered and build-filtered; missing scripts/models/ammo are rejected at runtime. Art, animations and gameplay require the separate asset audit and runtime tests.'}
        if branch=='hl1':row['existing_impulse101_override']=True
        manifest.append(row)
        if branch=='hl1':continue
        rel={'hl2':'dlls/hl2_dll/hl2_player.cpp','cstrike':'dlls/cstrike/cs_player.cpp','tf2':'dlls/tf2_dll/tf_player.cpp'}[branch]
        def array(s,names=row['compiled_impulse101_extra']):
            pattern=r'(const char \*const weapons\[\] = \{\n)(.*?)(\n\s*\};)'
            return re.sub(pattern,lambda m:m[1]+'\n'.join('\t\t\t\t"'+n+'",' for n in names)+m[3],s,count=1,flags=re.S)
        patch(rel,array)
    write_json(ROOT/'modernize/weapon_impulse101_manifest.json', manifest)
    print('v18 branch weapon restoration applied; asset and Windows runtime verification still required.')

if __name__=='__main__':main()
