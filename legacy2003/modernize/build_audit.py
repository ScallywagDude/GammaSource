#!/usr/bin/env python3
"""Audit the *active* 2003 HL2 build configurations before running MSBuild.

Checks paths with Windows case-insensitive semantics, distinguishes missing input
from out-of-tree SDK paths, and reports a project-by-project repair queue. This
is a diagnostic, not a Windows build or a substitute for a Windows compiler.
"""
from __future__ import annotations
import argparse, collections, json, os
from pathlib import Path
from convert_dsp import ROOT, get_flaginfo, parse, parse_build_order

SOURCE_EXT={'.c','.cpp','.cxx','.cc','.rc','.idl','.asm','.s','.vsh','.psh','.fxc','.fx'}
GENERATED_EXT={'.vsh','.psh','.fxc'}


def winpath(project_dir: Path, spec: str) -> Path | None:
    path=spec.strip().strip('"').replace('\\','/')
    if '$(' in path or '%(' in path or '%' in path: return None
    # Drive roots are intentionally not interpreted on POSIX.
    if len(path)>1 and path[1]==':': return None
    return Path(os.path.normpath(str(project_dir / path)))


def audit(root: Path=ROOT) -> dict:
    files={str(f.relative_to(root)).replace('\\','/').casefold():f
           for f in root.rglob('*') if f.is_file()}
    dirs={str(f.relative_to(root)).replace('\\','/').casefold():f
          for f in root.rglob('*') if f.is_dir()}
    cache={}
    entries=[]
    per_mode=collections.Counter()
    raw=parse_build_order()
    project_paths={str(p.relative_to(root)).replace('\\','/').casefold():p for p in root.rglob('*.dsp')}
    for step, item in enumerate(raw,1):
        dsp=item['dsp']
        pth=project_paths.get(dsp.casefold(),root/dsp)
        if not pth.exists():
            entries.append({**item,'step':step,'missing_project':True,'missing_inputs':[], 'missing_include_dirs':[], 'out_of_tree':[], 'case_only_references':0})
            continue
        project=cache.setdefault(dsp.lower(),parse(pth))
        cfg=item['configuration']
        errors=[];out_of_tree=[];missing_includes=[];case_only=[]
        for f in project['files']:
            if f['excluded'].get(cfg, f['excluded'].get('*',False)):
                continue
            spec=f['path']
            path=winpath(pth.parent,spec)
            if path is None:continue
            try:rel=path.relative_to(root).as_posix().casefold()
            except ValueError:
                out_of_tree.append({'file':spec,'category':'source_reference'});continue
            if rel not in files:
                suffix=path.suffix.casefold()
                errors.append({'file':spec,'type':('generated_shader' if suffix in GENERATED_EXT else 'compilation_input' if suffix in SOURCE_EXT else 'header_or_resource'), 'project_relative':rel})
            elif not path.exists():
                case_only.append(spec)
        flags=project['configs'].get(cfg,{}).get('flags',{}).get('CPP','')
        for inc in get_flaginfo(flags)['includes']:
            path=winpath(pth.parent,inc)
            if path is None:continue
            try:rel=path.relative_to(root).as_posix().casefold()
            except ValueError:
                out_of_tree.append({'file':inc,'category':'include_dir'});continue
            if rel not in dirs and rel not in files:
                missing_includes.append(inc)
        result={**item,'step':step,'missing_project':False,
                'missing_inputs':errors,'missing_include_dirs':sorted(set(missing_includes)),
                'out_of_tree':out_of_tree,'case_only_references':len(case_only)}
        entries.append(result)
        per_mode[cfg.split()[0]]+=1
    summaries={}
    for mode in ('Debug','Release'):
        matches=[e for e in entries if e['type']==mode]
        missing=[e for e in matches if e['missing_project'] or any(x['type']=='compilation_input' for x in e['missing_inputs'])]
        summaries[mode]={
            'steps':len(matches),
            'projects_with_missing_compilation_inputs':len(missing),
            'missing_compilation_inputs':sum(sum(x['type']=='compilation_input' for x in e['missing_inputs']) for e in matches),
            'missing_shaders':sum(sum(x['type']=='generated_shader' for x in e['missing_inputs']) for e in matches),
            'missing_include_directories':sum(len(e['missing_include_dirs']) for e in matches),
            'case_only_references':sum(e['case_only_references'] for e in matches),
            'first_project_with_missing_inputs':missing[0]['dsp'] if missing else None,
        }
    return {'format_version':1,'description':'Static source inventory; never implies build success',
            'summary':summaries,'build_order':entries}


def report_md(data: dict) -> str:
    lines=['# HL2 2003 Windows build: missing input audit','','Only references listed in the original Debug/Release HL2 build batch files are examined.',
           'Checks use Windows-style case-insensitive file matching. This does **not** verify successful compilation or linking.','',
           '| Configuration | Build steps | Projects lacking compiled source files | Missing compiled inputs | Missing shader source | Missing include directories |',
           '|---|---:|---:|---:|---:|---:|']
    for mode, s in data['summary'].items():
        lines.append(f"| {mode} | {s['steps']} | {s['projects_with_missing_compilation_inputs']} | {s['missing_compilation_inputs']} | {s['missing_shaders']} | {s['missing_include_directories']} |")
    lines.extend(['','## Repair queue (active missing compilation inputs)',''])
    for e in data['build_order']:
        missing=[r for r in e['missing_inputs'] if r['type']=='compilation_input']
        if not (missing or e['missing_project']):continue
        lines.append(f"### {e['type']} step {e['step']}: `{e['dsp']}` (`{e['configuration']}`)")
        if e['missing_project']:lines.append('- The original project file is missing.')
        for r in missing[:12]:lines.append(f"- `{r['file']}`")
        if len(missing)>12:lines.append(f'- ... and {len(missing)-12} additional missing inputs')
        lines.append('')
    lines.extend(['## How to use','','Run `python modernize/build_audit.py` after extracting and overlaying the source.','Review `build_audit.json` for complete per-project inputs. Restore legitimately available source and SDKs before attempting each build.','Do not replace missing inputs with stub implementations merely to make the build appear successful.',''])
    return '\n'.join(lines)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--fail-on-missing',action='store_true');args=ap.parse_args()
    data=audit()
    (ROOT/'modernize/build_audit.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
    (ROOT/'modernize/BUILD_AUDIT.md').write_text(report_md(data),encoding='utf-8')
    print(json.dumps(data['summary'],indent=2))
    if args.fail_on_missing and any(v['missing_compilation_inputs'] or v['projects_with_missing_compilation_inputs'] for v in data['summary'].values()):
        raise SystemExit(1)

if __name__=='__main__':main()
