"""Tests the actual original-build order runner using a fake MSBuild binary."""
from pathlib import Path
import json, os, stat, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
from build_driver import plan, run

class BuildDriverTests(unittest.TestCase):
    def test_original_order_is_used_for_both_configurations(self):
        for mode in ('Debug','Release'):
            jobs=plan(mode)
            self.assertEqual(len(jobs),61)
            self.assertEqual([i['step'] for i in jobs],list(range(1,62)))
            self.assertTrue(all((ROOT/item['project']).is_file() for item in jobs))
            self.assertTrue(all(i['configuration'].lower().startswith(mode.lower()) for i in jobs))
            result=run(mode,dry_run=True)
            self.assertFalse(result['verified_compilation'])
            self.assertEqual(len(result['steps']),61)
            self.assertTrue(all(i['status']=='not_run' for i in result['steps']))

    @unittest.skipIf(os.name=='nt','Unix fake compiler only; Windows MSBuild is tested separately')
    def test_failed_build_stops_and_captures_log(self):
        with tempfile.TemporaryDirectory() as work:
            directory=Path(work)
            fake=directory/'msbuild_fake'
            fake.write_text('#!/bin/sh\necho synthetic-compiler-failure >&2\nexit 7\n')
            fake.chmod(fake.stat().st_mode|stat.S_IXUSR)
            logs=directory/'logs'
            report=run('Debug',msbuild=str(fake),log_dir=logs)
            self.assertEqual(report['steps_executed'],1)
            self.assertEqual(report['failed_steps'],1)
            self.assertFalse(report['verified_compilation'])
            self.assertEqual(report['steps'][0]['return_code'],7)
            self.assertIn('synthetic-compiler-failure',(logs/('01_'+Path(report['steps'][0]['project']).stem+'.log')).read_text())
            loaded=json.loads((logs/'build_report.json').read_text())
            self.assertEqual(loaded['failed_steps'],1)
