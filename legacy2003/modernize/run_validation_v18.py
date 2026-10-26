#!/usr/bin/env python3
"""Run reproducible offline release checks and retain actual logs/results.

No Windows engine build or game launch is implied. Portable native policy tests
are enabled when a C++ compiler is available. Optional shader compilers are only
inventoried by --plan. Tests verify legacy generator idempotence in this tree.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]


def protected_snapshot():
    extensions={'.cpp','.h','.c','.dsp','.vcxproj','.filters','.py','.fgd','.fxc','.vsh','.psh','.hlsl','.vert','.frag'}
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
        for p in ROOT.rglob('*') if p.is_file() and p.suffix in extensions and '__pycache__' not in p.parts}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-bin',type=Path,help='Prepend a compiler directory for portable C++ tests')
    parser.add_argument('--verify-replay',action='store_true',help='Also run apply_modernization.py and reject source drift')
    args=parser.parse_args(argv)
    env=os.environ.copy()
    if args.compiler_bin:
        if not args.compiler_bin.is_dir():
            parser.error('Compiler directory not found')
        env['PATH']=str(args.compiler_bin)+os.pathsep+env.get('PATH','')
    out=ROOT/'modernize/reports/validation'
    out.mkdir(parents=True,exist_ok=True)
    before=protected_snapshot()
    jobs=[('weapon_inventory',['modernize/weapon_branch_audit.py']),
          ('weapon_build',['modernize/weapon_build_audit.py']),
          ('weapon_assets_source',['modernize/weapon_asset_audit.py']),
          ('npc_assets_source',['modernize/npc_asset_audit.py','--out','modernize/reports/NPC_ASSET_AUDIT_V18.json']),
          ('structure',['modernize/validate.py']),
          ('shader_plan',['modernize/build_modern_shaders.py','--plan'])]
    if args.verify_replay:
        jobs.append(('replay',['modernize/apply_modernization.py']))
    jobs.append(('regression_tests',['-m','unittest','discover','-s','modernize/tests','-p','test_*.py','-v']))
    results=[]
    for name,arguments in jobs:
        start=time.monotonic()
        proc=subprocess.run([sys.executable]+arguments,cwd=ROOT,env=env,text=True,
            encoding='utf-8',errors='replace',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/(name+'.log')).write_text(proc.stdout,encoding='utf-8')
        item={'name':name,'arguments':arguments,'exit_code':proc.returncode,
              'seconds':round(time.monotonic()-start,3),'log':'modernize/reports/validation/'+name+'.log'}
        if name=='regression_tests':
            match=re.search(r'Ran (\d+) tests?',proc.stdout)
            item['tests_run']=int(match.group(1)) if match else None
            skip=re.search(r'skipped=(\d+)',proc.stdout)
            item['skipped']=int(skip.group(1)) if skip else 0
        results.append(item)
        print(name+': '+('PASS' if proc.returncode==0 else 'FAIL'),flush=True)
    after=protected_snapshot()
    drift=sorted(p for p in set(before)|set(after) if before.get(p)!=after.get(p))
    report={'schema':1,'jobs':results,'source_drift':drift,
        'compiler_bin':str(args.compiler_bin) if args.compiler_bin else None,
        'compiler_found':shutil.which('g++',path=env['PATH']) or shutil.which('clang++',path=env['PATH']),
        'full_windows_build_verified':False,'source_engine_runtime_verified':False,
        'modern_renderer_backends_verified':False,
        'passed':not drift and all(r['exit_code']==0 for r in results)}
    (ROOT/'modernize/reports/VALIDATION_RESULTS_V18.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if drift:
        print('Unexpected generator/test source drift: '+', '.join(drift))
    return 0 if report['passed'] else 1


if __name__=='__main__':
    sys.exit(main())
