#!/usr/bin/env python3
"""Idempotent prototype NPC asset-safety overlay; no substitute models."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def replace(source, before, after):
    path = ROOT / source
    data = path.read_bytes()
    old, new = before.encode('latin1'), after.encode('latin1')
    if new in data:
        return
    if data.count(old) != 1:
        raise RuntimeError(f'{source}: expected exactly one patch site')
    path.write_bytes(data.replace(old, new))


def main():
    replace('dlls/hl2_dll/npc_assassin.cpp',
        '\tengine->PrecacheModel( "models/fassassin.mdl" );',
        '\tengine->PrecacheModel( "models/fassassin.mdl" );\r\n'
        '\t// HL2MODERN_NPC_ASSET_SAFETY: effects created by Spawn need level precaching.\r\n'
        '\tengine->PrecacheModel( "sprites/redglow1.vmt" );\r\n'
        '\tengine->PrecacheModel( "sprites/bluelaser1.vmt" );')
    for kind, member, factory, material in (
        ('eye glow', 'm_pEyeSprite', 'CSprite::SpriteCreate', 'sprites/redglow1.vmt'),
        ('eye trail', 'm_pEyeTrail', 'CSpriteTrail::SpriteTrailCreate', 'sprites/bluelaser1.vmt')):
        path = ROOT/'dlls/hl2_dll/npc_assassin.cpp'
        data = path.read_bytes()
        old = (factory+'( "'+material+'", GetLocalOrigin(), false );').encode('latin1')
        new = ('attachment > 0 ? '+factory+'( "'+material+'", GetLocalOrigin(), false ) : NULL;').encode('latin1')
        if new not in data:
            if data.count(old)!=1: raise RuntimeError('Assassin eye effect patch mismatch')
            path.write_bytes(data.replace(old,new))
    replace('dlls/hl2_dll/npc_assassin.cpp',
        '\tGetAttachment( handName, muzzlePos, muzzleAngle );',
        '\t// An incompatible model must not fire from an invalid muzzle transform.\r\n'
        '\tif ( !GetAttachment( handName, muzzlePos, muzzleAngle ) )\r\n\t\treturn;')
    replace('dlls/hl2_dll/npc_bullsquid.cpp',
        '\t\t\t\tGetAttachment( "Mouth", vSpitPos, vSpitAngle);',
        '\t\t\t\t// HL2MODERN_NPC_ASSET_SAFETY: require the original Mouth attachment.\r\n'
        '\t\t\t\tif ( !GetAttachment( "Mouth", vSpitPos, vSpitAngle ) )\r\n\t\t\t\t\tbreak;')
    replace('dlls/hl2_dll/npc_mortarsynth.cpp',
        '\t\t\tGetAttachment( i, vShootStart, vShootAng );',
        '\t\t\t// HL2MODERN_NPC_ASSET_SAFETY: skip a missing energy-gun attachment.\r\n'
        '\t\t\tif ( !GetAttachment( i, vShootStart, vShootAng ) )\r\n\t\t\t\tcontinue;')
    replace('dlls/hl2_dll/npc_combineguard.cpp',
        '\tGetAttachment( m_MuzzleAttachment, gunMatrix );',
        '\t// HL2MODERN_NPC_ASSET_SAFETY: avoid using an unavailable muzzle matrix.\r\n'
        '\tif ( m_MuzzleAttachment <= 0 || !GetAttachment( m_MuzzleAttachment, gunMatrix ) )\r\n\t\treturn false;')
    replace('dlls/hl2_dll/npc_lightstalk.cpp',
        '\tm_pGlow->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, kRenderFxGlowShell );\r\n'
        '\tm_pGlow->SetScale( 1.0 );\r\n\tm_pGlow->SetAttachment( this, 1 );',
        '\t// HL2MODERN_NPC_ASSET_SAFETY: effect allocation can fail.\r\n'
        '\tif ( m_pGlow )\r\n\t{\r\n'
        '\t\tm_pGlow->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, kRenderFxGlowShell );\r\n'
        '\t\tm_pGlow->SetScale( 1.0 );\r\n\t\tm_pGlow->SetAttachment( this, 1 );\r\n\t}')
    replace('dlls/hl2_dll/npc_launcher.cpp',
        '\tengine->PrecacheModel(STRING(m_sMissileModel));\r\n'
        '\tenginesound->PrecacheSound( STRING(m_sLaunchSound));\r\n'
        '\tenginesound->PrecacheSound( STRING(m_sFlySound));',
        '\t// HL2MODERN_NPC_ASSET_SAFETY: these map-supplied asset names are optional.\r\n'
        '\tif ( m_sMissileModel != NULL_STRING && STRING(m_sMissileModel)[0] )\r\n'
        '\t\tengine->PrecacheModel(STRING(m_sMissileModel));\r\n'
        '\tif ( m_sLaunchSound != NULL_STRING && STRING(m_sLaunchSound)[0] )\r\n'
        '\t\tenginesound->PrecacheSound( STRING(m_sLaunchSound));\r\n'
        '\tif ( m_sFlySound != NULL_STRING && STRING(m_sFlySound)[0] )\r\n'
        '\t\tenginesound->PrecacheSound( STRING(m_sFlySound));')
    replace('dlls/hl2_dll/npc_launcher.cpp',
        'void CNPC_Launcher::LaunchGrenade( CBaseEntity* pEnemy )\r\n{',
        'void CNPC_Launcher::LaunchGrenade( CBaseEntity* pEnemy )\r\n{\r\n'
        '\t// An empty MissileModel is not a restorable projectile asset.\r\n'
        '\tif ( m_sMissileModel == NULL_STRING || !STRING(m_sMissileModel)[0] )\r\n\t\treturn;')
    print('Prototype NPC asset-safety overlay applied (or already present)')


if __name__ == '__main__':
    main()
