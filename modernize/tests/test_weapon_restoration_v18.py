"""v18: real branch registration, installed-asset guards and safe prototype work."""
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
import weapon_branch_audit as inventory

def source(rel):return (ROOT/rel).read_bytes().decode('latin1')

class WeaponRestorationV18(unittest.TestCase):
    def test_actual_shared_registrations_are_branch_compiled(self):
        data=inventory.audit()
        for branch in ('hl2','hl1'):
            row=next(r for r in data['branches'][branch] if r['classname']=='weapon_physgun')
            self.assertEqual(row['implementation_file'],'dlls/physgun.cpp')
        for branch in ('cstrike','tf2'):
            self.assertNotIn('weapon_physgun',[r['classname'] for r in data['branches'][branch]])
        self.assertNotIn('weapon_brickbat',[r['classname'] for r in data['branches']['hl2']])
        self.assertIn('weapon_brickbat',[r['classname'] for r in data['unregistered_prototypes']['hl2']])
        self.assertEqual(data,json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text()))

    def test_comment_stripping_preserves_paths(self):
        text='"models/a//b.mdl" // LINK_ENTITY_TO_CLASS(weapon_fake,A)\nLINK_ENTITY_TO_CLASS(weapon_real,B); /* ignored */'
        result=inventory.strip_comments(text)
        self.assertIn('"models/a//b.mdl"',result)
        self.assertEqual(inventory.LINK.findall(result),['weapon_real'])

    def test_current_registration_inventory_has_no_unevaluated_preprocessor_guards(self):
        files={r['implementation_file'] for rows in inventory.audit()['branches'].values() for r in rows}
        for rel in files:
            stack=[]
            for line in inventory.strip_comments(source(rel)).splitlines():
                text=line.strip()
                if re.match(r'#\s*(if|ifdef|ifndef)\b',text):stack.append(text)
                elif re.match(r'#\s*endif\b',text) and stack:stack.pop()
                if inventory.LINK.search(line):self.assertEqual(stack,[],rel+': '+line)

    def test_manifest_does_not_grant_unusable_source(self):
        manifest=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        hl2=next(r for r in manifest if r['branch']=='hl2')
        tf2=next(r for r in manifest if r['branch']=='tf2')
        self.assertNotIn('weapon_manhack',hl2['compiled_impulse101_extra'])
        self.assertIn('weapon_manhack',hl2['excluded_source_or_internal'])
        self.assertNotIn('weapon_objectselection',tf2['compiled_impulse101_extra'])
        self.assertNotIn('weapon_twohandedcontainer',tf2['compiled_impulse101_extra'])
        self.assertIn('weapon_twohandedcontainer',tf2['excluded_source_or_internal'])
        for row in manifest:
            self.assertEqual(len(row['compiled_impulse101_extra']),len(set(row['compiled_impulse101_extra'])))
            self.assertFalse(row['assets_verified'])
            self.assertNotIn('weapon_cubemap',row['compiled_impulse101_extra'])

    def test_safe_spawn_checks_before_give_and_cleanup_after_touch(self):
        text=source('dlls/player.cpp')
        begin=text.index('CBaseCombatWeapon *CBasePlayer::GiveCheatWeapon(')
        block=text[begin:text.index('void CBasePlayer::RefillCheatWeaponAmmo(',begin)]
        self.assertLess(block.index('ReadWeaponDataFromFileForSlot'),block.index('GiveNamedItem(name)'))
        self.assertLess(block.index('FileExists(info->szViewModel'),block.index('GiveNamedItem(name)'))
        self.assertIn('gEvilImpulse101 = previousImpulse;',block)
        self.assertIn('weapon && !weapon->GetOwner()',block)
        self.assertIn('UTIL_Remove(weapon)',block)
        self.assertIn('HL2ModernWeapons::AmmoNeeded(GetAmmoDef()->MaxCarry(type),GetAmmoCount(type))',text)
        for rel in ('dlls/hl2_dll/hl2_player.cpp','dlls/hl1_dll/hl1_player.cpp','dlls/cstrike/cs_player.cpp','dlls/tf2_dll/tf_player.cpp'):
            self.assertIn('HL2MODERN_BRANCH_CHEAT_GATE',source(rel))
            self.assertIn('RefillCheatWeaponAmmo();',source(rel))

    def test_source_restoration_corrects_real_disabled_paths(self):
        rifle=source('dlls/hl2_dll/weapon_irifle.cpp')
        self.assertTrue(rifle.startswith('//'))
        self.assertNotIn('GiveAmmo( 90',rifle)
        self.assertLess(rifle.index('if ( pFlare == NULL )'),rifle.index('--m_iClip1'))
        tripwire=source('dlls/hl2_dll/weapon_tripwire.cpp')
        spawn=tripwire.split('void CWeapon_Tripwire::Spawn( )',1)[1].split('void CWeapon_Tripwire::Precache',1)[0]
        self.assertNotIn('UTIL_Remove',spawn)
        self.assertIn('if ( !pMine ) return;',tripwire)
        self.assertIn('if( !pOwner || !pOwner->IsAlive() )',source('dlls/hl2_dll/weapon_immolator.cpp'))
        self.assertIn('if ( placed ) DecrementAmmo(pPlayer)',source('dlls/hl2_dll/weapon_thumper.cpp'))

    def test_real_atlas_dimensions_and_twohanded_server_updates(self):
        text=source('cl_dll/hud.cpp')
        self.assertIn('HL2MODERN_ATLAS_DIMENSIONS',text)
        self.assertIn('AtlasRect(icon->rc.left,icon->rc.top,icon->rc.right,icon->rc.bottom,wide,tall)',text)
        self.assertIn('cropw>Width()-cropx',text)
        loader=text.split('void LoadHudTextures(',1)[1].split('void FreeHudTextureList(',1)[0]
        self.assertIn('const char *shortName = pTemp->GetName()',loader)
        self.assertIn('KeyValues *spriteData=HL2ModernHud::SelectSpriteData(pTemp)',loader)
        self.assertIn('SpriteMaterialName(texture,normalizedTexture,sizeof(normalizedTexture),spriteData!=pTemp)',loader)
        self.assertIn('const int wide=spriteData->GetInt("width",0)',loader)
        text=source('game_shared/tf2/weapon_twohandedcontainer.cpp')
        self.assertNotIn('m_hRightWeapon != NULL && m_hRightWeapon->IsPredicted()',text)
        self.assertIn('HL2MODERN_TWOHANDED_SERVER',text)

    def test_cpp98_grant_policy(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable in PATH')
        with tempfile.TemporaryDirectory() as td:
            output=Path(td)/('policy.exe' if os.name=='nt' else 'policy')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                                  str(ROOT/'modernize/tests/weapon_grant_policy_smoke.cpp'),'-o',str(output)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            run=subprocess.run([str(output)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)

    def test_final_and_historical_weapon_generators_preserve_v18(self):
        paths=['dlls/player.cpp','dlls/player.h','dlls/hl2_dll/hl2_player.cpp','dlls/hl1_dll/hl1_player.cpp',
               'dlls/cstrike/cs_player.cpp','dlls/tf2_dll/tf_player.cpp','game_shared/basecombatweapon_shared.cpp',
               'cl_dll/cstrike/cs_hud_weaponselection.cpp','cl_dll/tf2_hud/hud_weaponselection.cpp',
               'dlls/hl2_dll/weapon_irifle.cpp','dlls/hl2_dll/weapon_tripwire.cpp','dlls/hl2_dll/weapon_thumper.cpp',
               'dlls/hl2_dll/weapon_immolator.cpp','cl_dll/hud.cpp','game_shared/tf2/weapon_twohandedcontainer.cpp',
               'game_shared/hl2/basehlcombatweapon_shared.h','modernize/weapon_impulse101_manifest.json',
               'modernize/weapon_build_report.json','modernize/weapon_branch_inventory.json']
        before={p:(ROOT/p).read_bytes() for p in paths}
        for script in ('patch_impulse101_branches.py','patch_grants_buildaware.py','patch_weapon_grant_diagnostics.py','patch_weapon_restoration_v18.py'):
            result=subprocess.run([sys.executable,str(ROOT/'modernize'/script)],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,script+': '+result.stderr)
        self.assertEqual(before,{p:(ROOT/p).read_bytes() for p in paths})

if __name__=='__main__':unittest.main()
