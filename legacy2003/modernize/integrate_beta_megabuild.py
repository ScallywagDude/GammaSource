#!/usr/bin/env python3
"""Integrate real Half-Life 2 Beta Mega Build CONTENT with GammaSource.

No original game files are edited. The Win32 engine automatically searches the
physical `gammasource_content` layer before the base game. This stages assets
from a Mega Build hl2 directory or an extracted Mega Patch, never binaries.

DX80 model v37 -> v36 conversion uses the table-rebuilding converter from the
Win32 repair handoff and checks original VTX geometry. Incompatible objects are
skipped instead of poisoning an otherwise buildable engine. This program does
not claim a full Windows runtime test or import source code from executables.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
import tempfile
import zipfile

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE/'model_conversion'))
import model37

CONTENT_DIRS={'materials','models','sound','maps','scripts','resource','particles',
              'scenes','media','cfg','expressions','mapsrc','soundcache'}
CONTENT_EXTS={'.vtf','.vmt','.bsp','.mdl','.vtx','.phy','.vvd','.vtx','.wav',
              '.mp3','.ogg','.txt','.cfg','.res','.vcd','.vpk','.ain','.pcf',
              '.spr','.wad','.dat','.nav','.dem','.avi','.bik','.jpg','.png',
              '.tga','.bmp','.dx80.vtx'}
HUD_FIXED={'materials/sprites/hud1.vtf','materials/sprites/hud1.vmt','scripts/mod_textures.txt'}
# These filenames may affect engine/runtime compatibility; never import a
# binary Mega Build engine into the compiled GammaSource x86 ABI.
BLOCKED_EXTS={'.dll','.exe','.asi','.sys','.bat','.cmd','.ps1','.py','.so','.dylib','.lib',
              '.pdb','.obj','.vcxproj','.sln','.pak'}
MAX_SINGLE_BYTES=1024*1024*1024


def vtf_dimensions(data):
    if len(data)<64 or data[:4]!=b'VTF\0':return None
    major,minor,header=struct.unpack_from('<III',data,4)
    # GammaSource's vtf/vtf.cpp is a VTF7.1 reader with a fixed 64-byte
    # header and fixed offsets. VTF7.2+ often adds headers or resource data
    # and would load distorted texels despite looking higher-resolution.
    if major!=7 or minor>1 or header!=64:return None
    w,h=struct.unpack_from('<HH',data,16)
    if not 1<=w<=16384 or not 1<=h<=16384:return None
    return (w,h)


def game_path(raw):
    """Map Mega Build hl2/ or Mega Patch paths to an engine game path."""
    raw=str(raw).replace('\\','/')
    bits=PurePosixPath(raw).parts
    if (not bits or raw.startswith('/') or any(x in ('..','.') for x in bits)
            or ':' in bits[0]):return None
    bits=tuple(x for x in bits if x)
    lowered=[x.lower() for x in bits]
    # Avoid mixing modded campaigns (e3_2002, e3_2003, etc.) into hl2.
    if 'hl2' in lowered:
        anchor=len(lowered)-1-lowered[::-1].index('hl2')
        bits=bits[anchor+1:]
        lowered=lowered[anchor+1:]
    elif lowered[0] in CONTENT_DIRS:
        pass
    elif lowered[0] in ('megapatch','mega_patch','mega patch','hl2beta','hl2_2000'):
        bits=bits[1:]
        lowered=lowered[1:]
    else:
        return None
    if not bits or lowered[0] not in CONTENT_DIRS or len(bits)<2:return None
    ext=Path(bits[-1]).suffix.lower()
    if ext in BLOCKED_EXTS or ext not in CONTENT_EXTS:return None
    # Avoid loading other game binaries/disguised package content.
    rel='/'.join(bits)
    return rel


def sha(b):return hashlib.sha256(b).hexdigest()


def import_model(blob,path,sidecars):
    if not blob.startswith(b'IDST') or len(blob)<372:return None,'not an IDST studio model'
    v=struct.unpack_from('<i',blob,4)[0]
    if v==36:
        return (blob,sidecars),'native v36'
    if v!=37:
        return None,'unsupported model version '+str(v)
    vtx=sidecars.get('.dx80.vtx')
    if vtx is None:return None,'missing DX80 VTX6 for MDL37'
    try:
        result,meta=model37.convert(blob)
        model37.check_vtx(vtx,result)
        valid={'.dx80.vtx':vtx}
        for suffix,src in sidecars.items():
            if suffix=='.dx80.vtx':continue
            try:
                model37.check_vtx(src,result)
                valid[suffix]=src
            except (model37.FormatError,ValueError,struct.error):pass
        return (result,valid),'converted 37->36 (vtx checked, '+str(len(meta['sequences']))+' sequences)'
    except (model37.FormatError,ValueError,struct.error) as exc:
        return None,str(exc)


def staged_copy(destination,relative,data,force=False):
    dest=destination/relative
    if relative.casefold() in HUD_FIXED and dest.is_file():
        return 'protected_gamma_hud'
    # Check every parent: a pre-existing game-asset junction inside the
    # destination must never redirect a write into the original installation.
    check=dest
    while check != destination.parent:
        if check.is_symlink() or (hasattr(check,'is_junction') and check.is_junction()):
            return 'skipped_symlink'
        if check==destination:break
        check=check.parent
    if dest.is_file():
        existing=dest.read_bytes()
        if existing==data:return 'unchanged'
        if dest.suffix.lower()=='.vtf' and not force:
            old,neo=vtf_dimensions(existing),vtf_dimensions(data)
            if old and neo and (old[0]*old[1]>neo[0]*neo[1]):return 'kept_higher_resolution'
        # Do not remove originals: previous GammaSource files remain recoverable.
        backup=dest.with_name(dest.name+'.before-mega.bak')
        if not backup.exists():shutil.copy2(dest,backup)
    dest.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=dest.parent,delete=False) as tmp:
        tmp.write(data)
        temp_path=Path(tmp.name)
    try:os.replace(temp_path,dest)
    finally:
        if temp_path.exists():temp_path.unlink()
    return 'installed'


def collect_files(source,report):
    """Read installation folder or ZIP into keyed content collection.

    Streaming folder traversal avoids copying game files until requested; ZIP
    members only load once selected. Uses OS-provided paths and regular files.
    """
    def enumerate_folder(root):
        root=Path(root)
        for directory,dirs,files in os.walk(root,followlinks=False):
            dirs[:]=[d for d in dirs if not (Path(directory)/d).is_symlink()]
            for name in files:
                path=Path(directory)/name
                if path.is_symlink() or not path.is_file():continue
                relative=path.relative_to(root).as_posix()
                result=game_path(relative)
                if result and path.stat().st_size<=MAX_SINGLE_BYTES:
                    yield result,lambda path=path:path.read_bytes(),relative
    def enumerate_zip(path):
        # Readers are consumed after enumeration, so preserve the ZIP file
        # handle through each deferred read. The lambdas keep it alive until
        # integration of this source completes.
        archive=zipfile.ZipFile(path)
        for info in archive.infolist():
            if info.is_dir() or info.file_size>MAX_SINGLE_BYTES:continue
            result=game_path(info.filename)
            if not result:continue
            # Reject symlink members and archive extraction traversal.
            if (info.external_attr>>16)&0o170000==0o120000:continue
            yield result,lambda info=info,archive=archive:archive.read(info),info.filename
    return enumerate_zip(source) if Path(source).is_file() else enumerate_folder(source)


def integrate(roots,destination,allow_lower_resolution=False):
    destination=Path(destination)
    destination.mkdir(parents=True,exist_ok=True)
    if destination.is_symlink():raise ValueError('destination must be a physical directory')
    count=Counter();model_report=[];imported=[];errors=[]
    for source in roots:
        source=Path(source)
        if not source.exists():raise FileNotFoundError('Mega Build source not found: '+str(source))
        # For a build directory, use the root/hl2 branch, not campaign mods.
        if source.is_dir() and (source/'hl2').is_dir():source=source/'hl2'
        selected={}
        for relative,read,original in collect_files(source,errors):
            selected[relative.casefold()]=(relative,read,original)
        # Avoid importing sidecars independently: a new MDL37 must be converted
        # with its exact accompanying VTX before anything reaches the overlay.
        covered=set()
        for key,(relative,read,original) in sorted(selected.items()):
            if key in covered or relative.casefold() in HUD_FIXED:continue
            if not relative.lower().endswith('.mdl'):continue
            stem=relative[:-4]
            try:
                sidecars={}
                for suffix in ('.dx80.vtx','.dx7_2bone.vtx','.dx7_3bone.vtx'):
                    neighbor=selected.get((stem+suffix).casefold())
                    if neighbor:sidecars[suffix]=neighbor[1]()
                converted,reason=import_model(read(),relative,sidecars)
                if converted is None:
                    model_report.append({'path':relative,'status':'skipped','reason':reason})
                    count['skipped_incompatible_model']+=1
                    # Exclude its sidecars so the source engine's correct
                    # model/vtx pair is never mixed with an incompatible set.
                    for suffix in sidecars:covered.add((stem+suffix).casefold())
                    continue
                mdl,sidecars=converted
                mode=staged_copy(destination,relative,mdl,allow_lower_resolution)
                count[mode]+=1
                if mode=='installed':imported.append(relative)
                model_report.append({'path':relative,'status':mode,'format':reason})
                for suffix,part in sidecars.items():
                    companion=stem+suffix
                    mode=staged_copy(destination,companion,part,allow_lower_resolution)
                    count[mode]+=1
                    if mode=='installed':imported.append(companion)
                    covered.add(companion.casefold())
            except (OSError,zipfile.BadZipFile,RuntimeError) as exc:
                errors.append({'path':relative,'reason':str(exc)})
        # Stage ordinary game assets (textures, maps, etc.), keeping high res.
        for key,(relative,read,original) in sorted(selected.items()):
            if key in covered or relative.casefold() in HUD_FIXED:continue
            # Never stage a raw mdl that couldn't be processed as a pair.
            if relative.lower().endswith('.mdl'):continue
            if relative.lower().endswith(('.dx80.vtx','.dx7_2bone.vtx','.dx7_3bone.vtx')):
                # Sidecars with no corresponding model are generally valid,
                # but should not overwrite a converted v36 model's geometry.
                stem=relative.rsplit('.',2)[0]
                if (stem+'.mdl').casefold() in selected:continue
            try:
                data=read()
                if relative.lower().endswith('.vtf') and vtf_dimensions(data) is None:
                    count['skipped_unsupported_vtf']+=1
                    continue
                mode=staged_copy(destination,relative,data,allow_lower_resolution)
                count[mode]+=1
                if mode=='installed':imported.append(relative)
            except (OSError,zipfile.BadZipFile,RuntimeError) as exc:
                errors.append({'path':relative,'reason':str(exc)})
        count['source_files_seen']+=len(selected)
    report={'source_roots':[str(p) for p in roots],'destination':str(destination),
      'results':dict(count),'installed_paths':imported,'models':model_report,
      'errors':errors,'complete':not errors,'rendering_verified':False,
      'mega_build_code_replaced':False,
      'note':'Source tree or archive content only; compiled engine DLLs are never imported.'}
    (destination/'MEGA_BUILD_ASSET_MANIFEST.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


def find_default_mega():
    sources=[]
    env=os.environ.get('GAMMASOURCE_MEGA_ROOT') or os.environ.get('HL2_BETA_MEGA_ROOT')
    if env:sources.append(Path(env))
    home=Path.home()
    sources.extend((home/'Documents/HL2BetaMegaCuck',home/'Games/HL2BetaMegaBuild',home/'Downloads/HL2BetaMegaBuild'))
    return next((p for p in sources if p.is_dir()),None)


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mega-root',action='append',type=Path,help='Mega Build installation/hl2 folder or downloaded ZIP; repeat for standalone patches')
    p.add_argument('--destination',type=Path,default=HERE.parent/'gammasource_content')
    p.add_argument('--allow-lower-resolution',action='store_true',help='replace high-res VTF even if Mega asset is smaller')
    args=p.parse_args(argv)
    roots=args.mega_root or ([find_default_mega()] if find_default_mega() else [])
    if not roots:
        print('No Mega Build installed at a default path. Pass --mega-root to import real assets.',file=sys.stderr)
        return 2
    result=integrate(roots,args.destination,args.allow_lower_resolution)
    print(json.dumps({'results':result['results'],'errors':len(result['errors']),
      'models_skipped':sum(x['status']=='skipped' for x in result['models'])},indent=2))
    return 1 if result['errors'] else 0

if __name__=='__main__':sys.exit(main())
