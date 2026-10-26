"""Beta Combine Guard + Elite: C++98 armor-policy and source checks.

These are limited to portable policy helpers and source invariants; they do not
claim a successful engine compilation or real NPC gameplay.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
GUARD=ROOT/'game/server/gamma/npc_combineguard.cpp'
ELITE=ROOT/'game/server/gamma/npc_combinee.cpp'
HEADER=ROOT/'game/server/gamma/beta_guard_armor_safety.h'

class BetaGuardRound3(unittest.TestCase):
    def test_cxx98_armor_policy(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++98 compiler unavailable')
        with tempfile.TemporaryDirectory() as tmp:
            harness=Path(tmp)/'test.cpp'
            exe=Path(tmp)/'test'
            harness.write_text('''#include "beta_guard_armor_safety.h"
#include <cassert>
int main() {
  bool slots[] = {false, true, false, true, true, false};
  assert(BetaGuardCountIntactArmor(slots,6)==3);
  assert(BetaGuardPickIntactArmor(slots,6,0)==1);
  assert(BetaGuardPickIntactArmor(slots,6,1)==3);
  assert(BetaGuardPickIntactArmor(slots,6,2)==4);
  assert(BetaGuardPickIntactArmor(slots,6,3)==-1);
  assert(BetaGuardPickIntactArmor(slots,6,-1)==-1);
  assert(BetaGuardCountIntactArmor(0,6)==0);
  assert(BetaGuardPickIntactArmor(0,6,0)==-1);
  for(int i=0;i<6;++i) slots[i]=false;
  assert(BetaGuardCountIntactArmor(slots,6)==0);
  assert(BetaGuardPickIntactArmor(slots,6,0)==-1);
  return 0;
}
''')
            args=[compiler,'-std=c++98','-Wall','-Wextra','-Werror','-I',str(HEADER.parent),str(harness),'-o',str(exe)]
            if 'clang' in Path(compiler).name:
                args += ['-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(args,check=True,capture_output=True,text=True,timeout=30)
            subprocess.run([str(exe)],check=True,capture_output=True,text=True,timeout=30)

    def test_no_unbounded_armor_selection(self):
        s=GUARD.read_text()
        self.assertIn('BetaGuardCountIntactArmor',s)
        self.assertIn('BetaGuardPickIntactArmor',s)
        self.assertNotRegex(s,r'while\s*\(\s*m_armorPieces\[\s*iArmorPiece\s*\]\.destroyed')
        self.assertIn('if ( remaining > 0 )',s)
        self.assertRegex(s, r'if\s*\(\s*AllArmorDestroyed\(\)\s*\)')

    def test_model_asset_missing_does_not_block_armor_gameplay(self):
        s=GUARD.read_text()
        destroy=s[s.index('void CNPC_CombineGuard::DestroyArmorPiece( int pieceID )'):s.index('void CNPC_CombineGuard::DamageArmorPiece(',s.index('void CNPC_CombineGuard::DestroyArmorPiece( int pieceID )'))]
        self.assertIn('m_armorPieces[pieceID].destroyed = true;',destroy)
        self.assertIn('if ( attachmentValid && m_nGibModel > 0 )',destroy)
        self.assertIn('if ( pieceID < GetNumBodyGroups() )',destroy)
        self.assertIn('m_nGibModel = 0;',s)
        self.assertRegex(s,r'if \( i < GetNumBodyGroups\(\) \)\s*SetBodygroup')

    def test_original_guard_physics_schedule_and_beam(self):
        s=GUARD.read_text()
        for token in ['COND_COMBINEGUARD_CLOBBERED','SCHED_COMBINEGUARD_TOPPLE','SCHED_COMBINEGUARD_HELPLESS','ACT_COMBINEGUARD_GETUP','SetHealth( MAX( 1, sk_combineguard_health.GetInt() ) )','DispatchTraceAttack( damage, vecAiming, &tr )']:
            self.assertIn(token,s)
        self.assertRegex(s,r'IsArmorPiece\( i \) && !m_armorPieces\[ i \]\.destroyed')

    def test_settings_and_beta_provenance(self):
        cfg=(ROOT/'gammasource_overlay/cfg/skill.cfg').read_text()
        for key in ('sk_combine_elite_health', 'sk_combine_elite_kick',
                    'sk_combineguard_health', 'sk_combineguard_beam_damage'):
            self.assertRegex(cfg, rf'(?m)^{key}\s+"[0-9]+"')
        self.assertTrue((ROOT/'legacy2003/dlls/hl2_dll/npc_combineguard.cpp').exists())
        self.assertTrue((ROOT/'legacy2003/dlls/hl2_dll/npc_combinee.cpp').exists())

    def test_elite_beta_class_and_model_not_retail_swap(self):
        s=ELITE.read_text()
        self.assertIn('LINK_ENTITY_TO_CLASS( npc_combineelite, CNPC_CombineE );',s)
        self.assertIn('"models/Combine_Elite.mdl"',s)
        self.assertIn('"sk_combine_elite_health","100"',s)
        self.assertNotIn('models/combine_super_soldier.mdl',s)

if __name__=='__main__': unittest.main()
