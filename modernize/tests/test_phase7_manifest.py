"""The Phase 7 gate rejects contradictory reports and incomplete build evidence."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
import phase7_manifest_check as gate


def fixture(verified=False):
    inventory, assets, builds = {}, {}, {}
    manifests = []
    for short, branch in gate.BRANCH.items():
        name = 'weapon_' + branch
        source = 'dlls/' + branch + '/weapon.cpp'
        inventory[branch] = [dict(classname=name, implementation_file=source,
                                  grant_exclusion_reason=None,
                                  **{key: verified for key in gate.EVIDENCE})]
        assets[branch] = {'weapons': [dict(classname=name, implementation_file=source,
                                         runtime_verified=verified,
                                         status='assets_present_unverified_runtime', issues=[])]}
        builds[branch] = dict(registered=1, server_excluded=[], client_source_not_shared=[],
            weapons=[dict(classname=name, source=source,
                          debug=dict(server=True, client=True),
                          release=dict(server=True, client=True),
                          server_compiled_all_configs=True, client_compiled_all_configs=True)])
        manifests.append(dict(branch=short, registered=1, impulse101_extra=[name],
            compiled_impulse101_extra=[name], excluded_not_compiled=[],
            excluded_source_or_internal={}, base_or_abstract_excluded=[],
            windows_build_verified=verified, assets_verified=verified,
            viewmodels_verified=verified, hud_icons_verified=verified,
            texture_scale_verified=verified))
    return [{'branches': inventory}, manifests, {'branches': assets}, {'branches': builds}]


class Phase7ManifestTests(unittest.TestCase):
    def add_excluded(self, data, name, reason):
        weapon=copy.deepcopy(data[0]['branches']['hl2'][0])
        weapon.update(classname=name, grant_exclusion_reason=reason,
                      **{key:False for key in gate.EVIDENCE})
        data[0]['branches']['hl2'].append(weapon)
        build=copy.deepcopy(data[3]['branches']['hl2']['weapons'][0])
        build['classname']=name
        data[3]['branches']['hl2']['weapons'].append(build)
        data[3]['branches']['hl2']['registered']=2
        asset=copy.deepcopy(data[2]['branches']['hl2']['weapons'][0])
        asset.update(classname=name,runtime_verified=False,
                     status='unverified_no_asset_directory',issues=['no gameplay art'])
        data[2]['branches']['hl2']['weapons'].append(asset)
        data[1][0]['registered']=2
        data[1][0]['excluded_source_or_internal'][name]=reason

    def test_non_gameplay_classification_does_not_fabricate_runtime_evidence(self):
        data=fixture(True)
        self.add_excluded(data,'weapon_cubemap','shared debugging tool')
        problems,rows=gate.validate(data)
        self.assertEqual(problems,[])
        self.assertEqual(sum(row[1] for row in rows),5)
        self.assertEqual(sum(row[5] for row in rows),4)
        self.assertEqual(sum(row[4] for row in rows),4)

    def test_concrete_unfinished_weapon_cannot_hide_behind_an_exclusion(self):
        data=fixture(True)
        self.add_excluded(data,'weapon_manhack','unfinished prototype')
        problems,rows=gate.validate(data)
        self.assertEqual(problems,[])
        self.assertEqual(sum(row[5] for row in rows),5)
        self.assertEqual(sum(row[4] for row in rows),4)

    def test_published_reports_remain_incomplete(self):
        problems, rows = gate.validate(gate.read(ROOT))
        self.assertEqual(problems, [])
        self.assertEqual(sum(row[1] for row in rows), 114)
        self.assertEqual(sum(row[4] for row in rows), 0)

    def test_build_rows_and_counts_must_cover_inventory(self):
        for change in ('empty', 'duplicate', 'count'):
            data = fixture()
            report = data[3]['branches']['hl2']
            if change == 'empty':
                report['weapons'] = []
                report['registered'] = 0
            elif change == 'duplicate':
                report['weapons'].append(copy.deepcopy(report['weapons'][0]))
                with self.assertRaisesRegex(ValueError, 'duplicate'):
                    gate.validate(data)
                continue
            else:
                report['registered'] = 0
            self.assertTrue(gate.validate(data)[0], change)

    def test_each_build_configuration_must_agree_with_summaries(self):
        for side in ('server', 'client'):
            for mode in ('debug', 'release'):
                for change_aggregate in (False, True):
                    with self.subTest(side=side, mode=mode, aggregate=change_aggregate):
                        data = fixture()
                        row = data[3]['branches']['hl2']['weapons'][0]
                        row[mode][side] = False
                        if change_aggregate:
                            row[side + '_compiled_all_configs'] = False
                        self.assertTrue(gate.validate(data)[0])

    def test_consistently_excluded_server_cannot_be_granted(self):
        data = fixture()
        report = data[3]['branches']['hl2']
        row = report['weapons'][0]
        row['debug']['server'] = False
        row['server_compiled_all_configs'] = False
        report['server_excluded'] = [row['classname']]
        self.assertTrue(gate.validate(data)[0])

    def test_report_source_paths_must_match_inventory(self):
        for report, field in ((2, 'implementation_file'), (3, 'source')):
            data = fixture()
            data[report]['branches']['hl2']['weapons'][0][field] = 'dlls/wrong.cpp'
            self.assertTrue(gate.validate(data)[0])

    def test_inventory_internal_exclusions_cannot_be_removed_from_manifest(self):
        data = fixture()
        data[0]['branches']['hl2'][0]['grant_exclusion_reason'] = 'incomplete prototype'
        self.assertTrue(gate.validate(data)[0])

    def test_requested_grants_cannot_disappear_silently(self):
        data = fixture()
        data[1][0]['compiled_impulse101_extra'] = []
        self.assertTrue(gate.validate(data)[0])

    def test_compiled_weapons_cannot_be_declared_uncompiled_to_hide_grants(self):
        data = fixture(True)
        entry = data[1][0]
        entry['excluded_not_compiled'] = entry['impulse101_extra'][:]
        entry['compiled_impulse101_extra'] = []
        self.assertTrue(gate.validate(data)[0])

    def test_uncompiled_exclusions_must_be_requested_candidates(self):
        data = fixture(True)
        data[1][0]['excluded_not_compiled'] = ['weapon_unrequested']
        self.assertTrue(gate.validate(data)[0])

    def test_failed_or_unknown_assets_cannot_report_runtime_completion(self):
        for status in ('unverified_no_asset_directory', 'missing_weapon_script',
                       'invalid_weapon_script', 'asset_issues', 'unknown'):
            with self.subTest(status=status):
                data = fixture(True)
                data[2]['branches']['hl2']['weapons'][0]['status'] = status
                problems, rows = gate.validate(data)
                self.assertTrue(problems)
                self.assertEqual(next(row[4] for row in rows if row[0] == 'hl2'), 0)

    def test_runtime_completion_requires_explicit_empty_asset_issues(self):
        for issues in (['viewmodel:missing'], None, 'no issues'):
            with self.subTest(issues=issues):
                data = fixture(True)
                asset = data[2]['branches']['hl2']['weapons'][0]
                if issues is None:
                    del asset['issues']
                else:
                    asset['issues'] = issues
                if isinstance(issues, str):
                    with self.assertRaisesRegex(ValueError, 'asset issues'):
                        gate.validate(data)
                else:
                    self.assertTrue(gate.validate(data)[0])

    def test_malformed_collection_types_raise_report_errors(self):
        cases = [None, {}, fixture(), fixture(), fixture(), fixture(), fixture()]
        cases[2][0]['branches'] = []
        cases[3][1] = {'hl2': {}}
        cases[4][2]['branches']['hl2'] = []
        cases[5][0]['branches']['hl2'] = [None]
        cases[6][3]['branches']['hl2']['weapons'][0]['debug'] = []
        for data in cases:
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    gate.validate(data)

    def test_all_evidence_fields_require_json_booleans(self):
        for value in ('true', 1, None):
            for report in ('inventory', 'build', 'manifest', 'asset'):
                data = fixture()
                if report == 'inventory':
                    data[0]['branches']['hl2'][0]['runtime_verified'] = value
                elif report == 'build':
                    data[3]['branches']['hl2']['weapons'][0]['debug']['server'] = value
                elif report == 'manifest':
                    data[1][0]['windows_build_verified'] = value
                else:
                    data[2]['branches']['hl2']['weapons'][0]['runtime_verified'] = value
                with self.subTest(report=report, value=value):
                    with self.assertRaisesRegex(ValueError, 'JSON boolean'):
                        gate.validate(data)

    def test_runtime_evidence_must_agree_with_asset_report(self):
        data = fixture(True)
        data[2]['branches']['hl2']['weapons'][0]['runtime_verified'] = False
        self.assertTrue(gate.validate(data)[0])

    def test_inventory_flags_alone_do_not_establish_completion(self):
        for field in ('windows_build_verified', 'assets_verified', 'viewmodels_verified',
                      'hud_icons_verified', 'texture_scale_verified'):
            data = fixture(True)
            data[1][0][field] = False
            problems, rows = gate.validate(data)
            self.assertEqual(problems, [])
            self.assertEqual(next(row[4] for row in rows if row[0] == 'hl2'), 0, field)

    def test_empty_reports_cannot_pass_by_vacuous_completion(self):
        data = fixture()
        for branch in gate.BRANCH.values():
            data[0]['branches'][branch] = []
            data[2]['branches'][branch]['weapons'] = []
            data[3]['branches'][branch]['weapons'] = []
            data[3]['branches'][branch]['registered'] = 0
        for entry in data[1]:
            entry['registered'] = 0
            entry['impulse101_extra'] = []
            entry['compiled_impulse101_extra'] = []
        self.assertTrue(gate.validate(data)[0])

    def test_cli_exit_codes_distinguish_complete_incomplete_and_corrupt(self):
        # Synthetic flags exercise reporting semantics; they are not gameplay proof.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for expected, data in ((0, fixture(True)), (1, fixture()), (2, fixture())):
                if expected == 2:
                    data[3]['branches']['hl2']['weapons'] = []
                for filename, payload in zip(gate.FILES, data):
                    path = root / filename
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(json.dumps(payload), encoding='utf-8')
                result = subprocess.run([sys.executable, str(ROOT / 'phase7_manifest_check.py'), str(root)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
            archive = root / 'reports.zip'
            with zipfile.ZipFile(archive, 'w') as target:
                for filename in gate.FILES:
                    target.write(root / filename, filename)
            self.assertEqual(gate.read(root), gate.read(archive))


if __name__ == '__main__':
    unittest.main()
