"""Native math, integration guards, and repeatability for restored cut-NPC combat."""
from pathlib import Path
import json, os, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]
class RestoredCombatTests(unittest.TestCase):
    def test_houndeye_sonic_cpp98(self):
        cc=shutil.which('g++') or shutil.which('clang++')
        if not cc: self.skipTest('C++98 compiler unavailable')
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/('sonic.exe' if os.name=='nt' else 'sonic')
            proc=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',
                str(ROOT/'modernize/tests/houndeye_sonic_smoke.cpp'),'-o',str(out)],
                capture_output=True,text=True)
            self.assertEqual(proc.returncode,0,proc.stderr)
            run=subprocess.run([str(out)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('houndeye sonic tests passed',run.stdout)
    def test_houndeye_server_source(self):
        txt=(ROOT/'dlls/hl2_dll/npc_houndeye.cpp').read_bytes().decode('latin1')
        self.assertEqual(txt.count('HL2MODERN_HOUNDEYE_SONIC_RESTORATION'),1)
        for pattern in ('HL2ModernHoundeye::SonicFalloff(',
                        'HL2ModernHoundeye::BoundedDamage(',
                        'sk_Houndeye_dmg_blast","25"',
                        'sk_Houndeye_health","60"',
                        'if (pEntity && pEntity->Classify() == CLASS_HOUNDEYE)',
                        'return SCHED_HOUND_ATTACK_STRAFE;',
                        'ADD_CUSTOM_CONDITION(CNPC_Houndeye,\tCOND_HOUND_GROUP_RALLEY);',
                        'UTIL_PrecacheOther("energy_wave");'):
            self.assertIn(pattern,txt)
        self.assertNotIn('m_flNextAttack = gpGlobals->curtime;\r\n\t\t\t\t\tSCHED_HOUND_ATTACK_STRAFE;',txt)
    def test_bullsquid_server_source(self):
        txt=(ROOT/'dlls/hl2_dll/npc_bullsquid.cpp').read_bytes().decode('latin1')
        self.assertEqual(txt.count('HL2MODERN_BULLSQUID_COMBAT_RESTORATION'),1)
        self.assertNotIn('monster_headcrab',txt)
        self.assertGreaterEqual(txt.count('CLASS_HEADCRAB'),5)
        for pattern in ('sk_bullsquid_health", "70"',
                        'sk_bullsquid_dmg_bite", "20"',
                        'sk_bullsquid_dmg_whip", "35"',
                        'const float whipDamage = sk_bullsquid_dmg_whip.GetFloat();',
                        'if (!pGrenade) break;',
                        'GetEnemy() && GetEnemy()->m_iHealth'):
            self.assertIn(pattern,txt)
    def test_inventory_is_honest(self):
        entries=json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
        h=next(x for x in entries if x['name']=='npc_houndeye')
        b=next(x for x in entries if x['name']=='npc_bullsquid')
        self.assertTrue(h['sonic_attack_source_patch'])
        self.assertTrue(b['melee_attack_source_patch'])
        for v in (h,b):
            self.assertFalse(v['runtime_verified'])
            self.assertFalse(v['assets_verified'])
    def test_regenerators_idempotent(self):
        for name,source in (('houndeye','npc_houndeye.cpp'),('bullsquid','npc_bullsquid.cpp')):
            p=ROOT/'dlls/hl2_dll'/source
            before=p.read_bytes()
            result=subprocess.run([sys.executable,str(ROOT/'modernize'/('patch_'+name+'.py'))],
                                  text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(p.read_bytes(),before)
        scripts=(ROOT/'modernize/apply_modernization.py').read_text()
        self.assertIn('patch_houndeye.py',scripts)
        self.assertIn('patch_bullsquid.py',scripts)
if __name__=='__main__': unittest.main()
