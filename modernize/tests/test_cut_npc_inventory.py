from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import generate_cut_npcs as generator


class NpcSourceEvidenceTests(unittest.TestCase):
    def test_comments_macros_and_disabled_factory(self):
        source='''// LINK_ENTITY_TO_CLASS(npc_comment, Fake);
        #define MAIN_MODEL "models/real.mdl"
        /* engine->PrecacheModel("models/wrong.mdl"); */
        engine->PrecacheModel(MAIN_MODEL);
        #if 0
        LINK_ENTITY_TO_CLASS(npc_disabled, Fake);
        #else
        LINK_ENTITY_TO_CLASS(npc_active, Real);
        #endif
        #ifdef UNCONFIGURED_FEATURE
        LINK_ENTITY_TO_CLASS(npc_conditional, Feature);
        #endif'''
        self.assertEqual(generator.active_registrations(source),{'npc_active'})
        self.assertEqual(generator.call_references(generator.strip_comments(source),['PrecacheModel']),['models/real.mdl'])

    def test_inventory_uses_primary_models_and_real_hl2_inputs(self):
        entries={entry['name']:entry for entry in generator.inventory()}
        self.assertEqual(entries['npc_combineguard']['model_reference'],'models/combine_guard.mdl')
        self.assertEqual(entries['npc_ichthyosaur']['model_reference'],'models/ichthyosaur.mdl')
        self.assertEqual(entries['npc_lightstalk']['model_reference'],'models/lightstalk.mdl')
        self.assertEqual(entries['npc_hydra']['model_reference'],'models/Hydra.mdl')
        self.assertEqual(entries['npc_rollerbull']['model_reference'],'models/roller.mdl')
        self.assertFalse(entries['npc_conscript']['registration_active'])
        self.assertFalse(entries['npc_conscript']['prototype_candidate'])
        for entry in entries.values():
            self.assertTrue(all('HL2|' in config for config in entry.get('included_build_configurations',[])))
            self.assertFalse(entry['assets_verified'])
            self.assertFalse(entry['runtime_verified'])


if __name__=='__main__':unittest.main()
