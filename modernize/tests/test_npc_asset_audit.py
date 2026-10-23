from pathlib import Path
import subprocess,sys,tempfile,json,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import npc_asset_audit as audit

class NpcAssetAuditTests(unittest.TestCase):
    def test_reference_sanitization(self):
        self.assertEqual(audit.normalize_reference(r'models\\MyNPC.MDL'),'models/mynpc.mdl')
        for item in ('../models/bad.mdl','/models/x.mdl','C:/models/x.mdl','scripts/x.txt',None):
            self.assertIsNone(audit.normalize_reference(item))
    def test_unpacked_files_model_status_and_disabled_factories(self):
        entries=[
            {'name':'npc_good','model_reference':'models/A.mdl','registration_active':True,'included_in_debug_hl2':True},
            {'name':'npc_missing','model_reference':'models/B.mdl','registration_active':True},
            {'name':'npc_no_reference','model_reference':None,'registration_active':True},
            {'name':'npc_disabled','model_reference':'models/a.mdl','registration_active':False},
        ]
        with tempfile.TemporaryDirectory() as d:
            model_dir=Path(d)/'models'
            model_dir.mkdir()
            (model_dir/'a.MDL').write_bytes(b'fake file; presence only')
            result=audit.audit(entries,[d])
            self.assertEqual(result['scanned_unpacked_mdl_count'],1)
            self.assertEqual([i['model_file_status'] for i in result['npcs']],
                ['found_unpacked','not_found_unpacked','unknown_reference','found_unpacked'])
            self.assertFalse(result['npcs'][3]['factory_registration_in_source'])
            self.assertTrue(all(not n['npc_runtime_verified'] for n in result['npcs']))
            no_roots=audit.audit(entries,[])
            self.assertEqual(no_roots['npcs'][0]['model_file_status'],'not_checked')
            self.assertEqual(audit.audit(entries,[model_dir])['npcs'][0]['model_file_status'],'found_unpacked')
    def test_cli_json(self):
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'report.json'
            subprocess.run([sys.executable,str(ROOT/'npc_asset_audit.py'),'--out',str(out)],check=True,capture_output=True,text=True)
            data=json.loads(out.read_text())
            entries=json.loads((ROOT/'cut_npc_inventory.json').read_text())
            self.assertEqual({item['classname'] for item in data['npcs']},{item['name'] for item in entries})
            self.assertTrue(any(i['classname']=='npc_conscript' and not i['factory_registration_in_source'] for i in data['npcs']))
            self.assertGreater(len(data['warning']),10)
    def test_present_model_is_not_implicitly_valid(self):
        with tempfile.TemporaryDirectory() as d:
            model=Path(d)/'models'/'a.mdl'
            model.parent.mkdir();model.write_bytes(b'not a model')
            result=audit.audit([{'name':'npc_test','model_reference':'models/A.mdl'}],[d])['npcs'][0]
            self.assertEqual(result['model_file_status'],'found_unpacked')
            self.assertEqual(result['model_case_status'],'case_mismatch')
            self.assertEqual(result['model_content']['status'],'invalid')
            self.assertFalse(result['npc_runtime_verified'])
if __name__=='__main__':unittest.main()
