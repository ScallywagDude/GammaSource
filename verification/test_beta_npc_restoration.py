"""Regression tests for the restored 2003 beta NPC source registration.

Source checks only: x64 compilation and real gameplay tests require Windows and assets.
"""
import json
from pathlib import Path
import re
import unittest
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT/'verification'))
from audit_beta_npc_variants import active_lines, main as audit_main, ENTITY, read

class BetaNPCWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        audit_main()
        cls.audit = json.loads((ROOT/'verification/beta_npc_variant_audit.json').read_text('utf8'))

    def test_server_npc_wiring_and_dependency(self):
        vpc = read(ROOT/'game/server/server_hl2.vpc').lower()
        needed = ['hl2\\npc_assassin.cpp','hl2\\npc_bullsquid.cpp','hl2\\npc_houndeye.cpp',
                  'hl2\\npc_hydra.cpp','hl2\\npc_spotlight.cpp','hl2\\grenade_spit.cpp']
        for file in needed:
            self.assertIn('"'+file+'"', vpc)
            self.assertTrue((ROOT/'game/server'/file.replace('\\','/')).is_file(), file)
            self.assertEqual(vpc.count('"'+file+'"'),1,file)

    def test_hydra_server_and_client_both_linked(self):
        self.assertTrue(self.audit['hydra_client_renderer_compiled'])
        self.assertIn('"hl2\\c_npc_hydra.cpp"',read(ROOT/'game/client/client_hl2.vpc').lower())
        server=read(ROOT/'game/server/hl2/npc_hydra.cpp')
        client=read(ROOT/'game/client/hl2/c_npc_hydra.cpp')
        self.assertIn('DT_NPC_Hydra',server)
        self.assertIn('DT_NPC_Hydra',client)

    def test_registered_beta_npcs_have_single_implementation(self):
        wanted=('npc_assassin','npc_bullsquid','npc_houndeye','npc_hydra','npc_spotlight')
        for name in wanted:
            records=[x for x in self.audit['entities'] if x['branch']=='hl2_dll' and x['classname']==name]
            self.assertEqual(len(records),1)
            self.assertEqual(records[0]['status'],'compiled')
            self.assertEqual(len(records[0]['compiled_implementations']),1)
            comparisons=records[0]['implementation_comparison']
            self.assertTrue(comparisons)
            # The source audit retains comparison evidence. A line-similarity
            # threshold cannot certify behavior and rejects legitimate API and
            # projectile repairs as the port grows.

    def test_legacy_turrets_are_compiled_without_ceiling_collision(self):
        vpc=read(ROOT/'game/server/server_hl2.vpc').lower()
        self.assertIn('"hl2\\npc_turret.cpp"',vpc)
        self.assertIn('"hl2\\npc_turret_ceiling.cpp"',vpc)
        for name in ('npc_miniturret', 'npc_sentry', 'npc_turret_ceiling'):
            record=next(x for x in self.audit['entities'] if x['branch']=='hl2_dll' and x['classname']==name)
            self.assertEqual(record['status'],'compiled',name)
            self.assertEqual(len(record['compiled_implementations']),1,name)
        shared=active_lines(read(ROOT/'game/server/hl2/npc_turret.cpp'))
        self.assertIn('npc_miniturret',shared)
        self.assertIn('npc_sentry',shared)
        self.assertNotRegex(shared,r'LINK_ENTITY_TO_CLASS\s*\(\s*npc_turret_ceiling\s*,')

    def test_recycled_classes_separately_inventoried(self):
        names={(x['branch'],x['classname']) for x in self.audit['entities']}
        for key in [('tf2_dll','npc_bug_builder'),('tf2_dll','npc_bug_warrior'),
                    ('tf2_dll','npc_bughole'),('hl1_dll','monster_houndeye'),
                    ('hl2_dll','npc_conscript')]:
            self.assertIn(key,names)
        conscript=next(x for x in self.audit['entities'] if x['branch']=='hl2_dll' and x['classname']=='npc_conscript')
        self.assertEqual(conscript['status'],'compiled')
        self.assertEqual(len(conscript['compiled_implementations']),1)

    def test_legacy_grant_rules_retained(self):
        text=read(ROOT/'game/server/gamma/impulse101_weapon_filter.h').lower()
        self.assertIn('citizensuitcase',text)
        self.assertIn('citizenpackage',text)
        self.assertIn('cubemap',text)
        self.assertNotIn('weapon_annabelle',text)
        weapon=read(ROOT/'gammasource_content_2018/scripts/weapon_annabelle.txt')
        self.assertRegex(weapon, r'(?i)"printname"\s+"Annabelle"')

if __name__ == '__main__': unittest.main()
