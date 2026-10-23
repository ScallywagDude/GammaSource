"""Only three single-token structural repairs are defensible without artwork."""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import material_syntax_repairs_v18 as repair
from asset_case_patch_v18 import apply_plan


class MaterialSyntaxRepairs(unittest.TestCase):
    def fixed(self,text,name='example.vmt'):
        edit,refusal=repair.propose(text,name)
        self.assertIsNone(refusal)
        self.assertIsNotNone(edit)
        return text[:edit['offset']]+edit['after']+text[edit['offset']+len(edit['before']):],edit

    def test_missing_final_brace_preserves_comments_and_crlf(self):
        original='"UnlitGeneric"\r\n{\r\n "$basetexture" "fixture/example" // } ignored'
        fixed,edit=self.fixed(original)
        self.assertEqual(edit['reason'],'append_single_missing_final_brace')
        self.assertTrue(fixed.endswith('\r\n}\r\n'))
        self.assertTrue(fixed.startswith(original))
        self.assertTrue(repair.valid_material(fixed))

    def test_extra_terminal_brace_and_bare_filename_only(self):
        valid='"UnlitGeneric" { "$basetexture" "fixture/example" }'
        fixed,edit=self.fixed(valid+'\n}\n// keep comment {')
        self.assertEqual(edit['reason'],'remove_single_extra_terminal_brace')
        self.assertTrue(fixed.endswith('// keep comment {'))
        fixed,edit=self.fixed(valid+'\nexample\n')
        self.assertEqual(edit['reason'],'remove_bare_terminal_filename_stem')
        self.assertTrue(repair.valid_material(fixed))

    def test_ambiguous_proxy_missing_value_and_lexical_damage_rejected(self):
        cases=[
            '"LightmappedTextureDetail" { "$basetexture" }',
            '"LightmappedTextureDetail" { "Proxies" { "MaterialModify" } }',
            '"UnlitGeneric" { "$basetexture" "unterminated',
            '"UnlitGeneric" { "$basetexture" "valid" /* unfinished',
            '"UnlitGeneric" { "Proxies" {',
            '"UnlitGeneric" {}\n"example"',
            '"UnlitGeneric" {}\nnot_the_filename',
            '"UnlitGeneric" {}\n}\n}',
            '"UnlitGeneric" {}\n"SecondShader" {}',
        ]
        for text in cases:
            with self.subTest(text=text):
                edit,reason=repair.propose(text,'example.vmt')
                self.assertIsNone(edit)
                self.assertNotEqual(reason,'already_valid')

    def test_plan_apply_hashes_and_originals_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)/'assets'
            (root/'materials').mkdir(parents=True)
            source=root/'materials/example.vmt'
            original=b'"UnlitGeneric" { "$basetexture" "fixture/example"\r\n'
            source.write_bytes(original)
            plan,diff=repair.prepare({'hl2':[root]})
            self.assertEqual(plan['summary']['repaired'],1)
            self.assertEqual(plan['files'][0]['edits'][0]['before'],'')
            self.assertIn('+}',diff)
            output=Path(directory)/'overlay'
            self.assertEqual(apply_plan(plan,{'hl2':[root]},output),1)
            self.assertEqual(source.read_bytes(),original)
            fixed=output/'hl2/root0/materials/example.vmt'
            self.assertEqual(repair.sha(fixed.read_bytes()),plan['files'][0]['modified_sha256'])
            self.assertTrue(repair.valid_material(fixed.read_text()))


if __name__=='__main__':unittest.main()
