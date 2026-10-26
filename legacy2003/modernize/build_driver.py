#!/usr/bin/env python3
"""Ordered VS2026 HL2 Windows build with per-project logs and machine-readable results.

The original 61-step batches are the source of truth. This script only runs
MSBuild for those steps; it does not fabricate missing SDKs or change code.
`--dry-run` is platform independent, for inspecting command ordering.
"""
from __future__ import annotations
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from convert_dsp import ROOT, parse_build_order


def plan(mode, root=ROOT):
    project_names = {str(f.relative_to(root)).replace('\\','/').casefold(): f
                     for f in root.rglob('*.dsp')}
    result = []
    for step in parse_build_order():
        if step['type'] != mode:
            continue
        name = step['dsp'].replace('\\','/').casefold()
        dsp = project_names.get(name)
        if dsp is None:
            raise ValueError(f"Missing original build step project: {step['dsp']}")
        project = dsp.with_suffix('.vcxproj')
        if not project.is_file():
            raise ValueError(f'Missing generated project: {project}')
        result.append({
            'step': len(result)+1,
            'original': step['dsp'],
            'configuration': step['configuration'],
            'project': str(project.relative_to(root)).replace('\\','/'),
        })
    if len(result) == 0:
        raise ValueError(f'No {mode} build steps in original batch files')
    return result


def command(msbuild, root, item):
    return [msbuild, str(root/item['project']), '/nologo', '/m:1', '/t:Build',
            '/p:Configuration='+item['configuration'], '/p:Platform=Win32',
            '/v:normal']


def run(mode, root=ROOT, msbuild=None, dry_run=False, continue_on_error=False,
        log_dir=None):
    steps = plan(mode, root)
    compiler = msbuild or shutil.which('msbuild')
    if not dry_run and not compiler:
        raise RuntimeError('MSBuild is not available. Open a VS2026 x86 Native Tools prompt.')
    report = {'mode':mode, 'dry_run':dry_run,
              'verified_compilation':False, 'expected_steps':len(steps), 'steps':[],
              'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
    if log_dir is None:log_dir=root/'.modern_build_logs'/mode.lower()
    if not dry_run: log_dir.mkdir(parents=True,exist_ok=True)
    for item in steps:
        entry=dict(item)
        args=command(compiler or 'msbuild',root,item)
        entry['command']=args
        if dry_run:
            entry['status']='not_run'
            report['steps'].append(entry)
            continue
        output_path=log_dir/f"{item['step']:02d}_{Path(item['project']).stem}.log"
        print(f"[{item['step']}/{len(steps)}] {item['project']} ({mode})",flush=True)
        start=time.monotonic()
        with output_path.open('w',encoding='utf-8',errors='replace') as log:
            proc=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT,
                                check=False)
        entry['elapsed_seconds']=round(time.monotonic()-start,2)
        entry['return_code']=proc.returncode
        entry['status']='passed' if proc.returncode==0 else 'failed'
        entry['log']=str(output_path.resolve())
        report['steps'].append(entry)
        if proc.returncode and not continue_on_error:
            break
    statuses=[s['status'] for s in report['steps']]
    report['steps_executed']=sum(s in ('passed','failed') for s in statuses)
    report['failed_steps']=sum(s=='failed' for s in statuses)
    report['verified_compilation']=(not dry_run and len(statuses)==len(steps)
                                    and all(s=='passed' for s in statuses))
    if not dry_run:
        (log_dir/'build_report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print('Saved build report to',log_dir/'build_report.json')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['Debug','Release'])
    parser.add_argument('--dry-run',action='store_true')
    parser.add_argument('--continue-on-error',action='store_true',
                        help='Attempt later projects even after earlier failures (may cascade).')
    parser.add_argument('--msbuild',help='Path to MSBuild executable (or test double).')
    args=parser.parse_args()
    try:
        report=run(args.mode,msbuild=args.msbuild,dry_run=args.dry_run,
                   continue_on_error=args.continue_on_error)
    except (RuntimeError,ValueError,OSError) as err:
        parser.exit(2,str(err)+'\n')
    if args.dry_run:
        for s in report['steps']:
            print(f"{s['step']:02d}. {s['project']} [{s['configuration']}]")
    else:
        print('Completed',report['steps_executed'],'of',report['expected_steps'],
              'steps; failures:',report['failed_steps'])
    if report['failed_steps']:return 1
    return 0

if __name__=='__main__':sys.exit(main())
