#!/usr/bin/env python3
"""Convert this VC6 .dsp tree into VS2026 MSBuild projects without deleting originals.

Usage: python modernize/convert_dsp.py [--check]
Limited to file/project settings: does not fix compiler errors or import licensed SDKs.
"""
from __future__ import annotations
import argparse, collections, hashlib, json, os, pathlib, re, sys, uuid
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
NS = 'http://schemas.microsoft.com/developer/msbuild/2003'
ET.register_namespace('', NS)
X = lambda n: '{%s}%s' % (NS, n)
VERSION = '18.0'
PROJECT_TYPE = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
CFG_RE = re.compile(r'^# Name "(.+?)"', re.I)
IF_RE = re.compile(r'^!(?:IF|ELSEIF)\s+"\$\(CFG\)"\s*==\s*"(.+?)"', re.I)
OPT_RE = re.compile(r'^# ADD (CPP|RSC|LINK32|LIB32|MTL)\s+(.*)$')
OUT_RE = re.compile(r'(?i)/out:(?:"([^"]+)"|(\S+))')
INC_RE = re.compile(r'(?i)(?<!\S)/I\s*(?:"([^"]+)"|(\S+))')
DEF_RE = re.compile(r'(?i)(?<!\S)/D\s*(?:"([^"]+)"|(\S+))')
LIBPATH_RE = re.compile(r'(?i)/libpath:(?:"([^"]+)"|(\S+))')
LIB_RE = re.compile(r'(?i)(?:"([^"\r\n]+\.lib)"|(?<![\w.])(\S+\.lib))')
PROJECT_RE = re.compile(r'^\s*Project:\s*"([^"]+)"\s*=\s*(.+?\.dsp)', re.I)
BAT_RE = re.compile(r'(?i)^\s*%MSDEV%\s+([^\s]+?\.dsp)\s+%CONFIG%"([^"]+)"')
ET.Element # imported for static tools

def node(p, key, val=None, attrs=None):
    e = ET.SubElement(p, X(key), attrs or {})
    if val is not None: e.text = str(val)
    return e

def choose_cfg(full):
    m = re.search(r'\s-\swin32\s+', full, re.I)
    if not m: return full
    return full[m.end():].strip()

def opt_values(flags, rx):
    return [x or y for x,y in rx.findall(flags)]

def get_switch(flags, key):
    m = re.search(r'(?i)(?<!\S)/' + re.escape(key) + r'(?:"([^"]+)"|(\S+))', flags)
    return (m.group(1) or m.group(2)) if m else None

def norm_path(s):
    return s.strip('"').replace('/', '\\')

def relative_project_name(path, root=None):
    """Stable display/hash spelling for both PureWindowsPath and POSIX paths."""
    return path.relative_to(ROOT if root is None else root).as_posix()

def project_guid(path, root=None):
    return '{'+str(uuid.uuid5(uuid.NAMESPACE_URL,relative_project_name(path,root).lower())).upper()+'}'

def filter_guid(path, name, root=None):
    # VC6 filter names intentionally retain their internal Windows separators;
    # only the project-relative path is canonicalized, matching v17 POSIX output.
    return '{'+str(uuid.uuid5(uuid.NAMESPACE_URL,relative_project_name(path,root)+'@'+name)).upper()+'}'

def write_crlf(path, text):
    """Write Windows text once; TextIO must not translate CRLF into CRCRLF."""
    normalized=text.replace('\r\n','\n').replace('\r','\n')
    path.write_bytes(normalized.replace('\n','\r\n').encode('utf-8'))

def staging_command(project_directory, old_dest):
    """Preserve explicit DSP staging metadata without performing any copy.

    Relative destinations retain the in-tree constraint. An explicit absolute
    Windows destination is existing project behavior, not a host-relative path;
    interpreting it through Path.resolve() made v17 output OS-dependent.
    """
    clean=old_dest.split(' >')[0].strip().rstrip('\\').strip()
    if not clean or '$(' in clean or '%' in clean:return None
    windows=pathlib.PureWindowsPath(clean)
    if windows.is_absolute():
        isdir=not windows.suffix
        folder=windows if isdir else windows.parent
        dest=old_dest if isdir else clean
        folder_name=norm_path(str(folder))
    else:
        # Drive-relative and root-relative Windows paths depend on process
        # state. Do not reinterpret them as relative POSIX paths on another OS.
        if windows.drive or windows.root:return None
        final=(project_directory/clean.replace('\\','/')).resolve()
        if not final.is_relative_to(ROOT.resolve()):return None
        isdir=final.is_dir() or not final.suffix
        folder=final if isdir else final.parent
        dest=old_dest if isdir else clean
        folder_name=norm_path(os.path.relpath(folder,project_directory))
    return 'if not exist "'+folder_name+'" mkdir "'+folder_name+'"\r\ncopy /Y "$(TargetPath)" "'+norm_path(dest)+'"'

def parse(path):
    txt = path.read_text(encoding='latin-1', errors='replace')
    lines = txt.splitlines()
    names = [m.group(1) for line in lines if (m:=CFG_RE.match(line))]
    names = list(dict.fromkeys(names))
    if not names:
        names = [m.group(1) for line in lines if (m:=re.search(r'^!IF\s+"\$\(CFG\)"\s*==\s*"([^"]+)"',line))]
    kind = re.search(r'^# TARGTYPE.*(0x[0-9a-f]{4})', txt, re.M|re.I)
    kind = {'0x0101':'Application', '0x0102':'DynamicLibrary', '0x0103':'Application', '0x0104':'StaticLibrary'}.get(kind.group(1).lower() if kind else '', 'Application')
    subtype = 'Console' if kind == 'Application' and re.search(r'^# TARGTYPE "Win32 \(x86\) Console',txt,re.M) else 'Windows'
    project_name = re.search(r'^# Microsoft Developer Studio Project File - Name="([^"]+)"',txt,re.M)
    title = project_name.group(1) if project_name else path.stem
    before_target = txt[:txt.find('# Begin Target')] if '# Begin Target' in txt else txt
    cfg_lines = {}
    current = None
    for line in before_target.splitlines():
        if m:=IF_RE.match(line):
            current = m.group(1);cfg_lines[current]=[]
        elif line.startswith('!ENDIF'):
            current = None
        elif current:
            cfg_lines[current].append(line)
    configs = {}
    for original in names:
        flags = {'CPP':'','RSC':'','LINK32':'','LIB32':'','MTL':''}
        props = {}
        for line in cfg_lines.get(original,[]):
            if m:=OPT_RE.match(line): flags[m.group(1)] = m.group(2)
            if m:=re.match(r'^# PROP (Output_Dir|Intermediate_Dir|Use_MFC|Ignore_Export_Lib)\s+(.*)$',line): props[m.group(1)] = m.group(2).strip('"')
        configs[choose_cfg(original)] = {'original':original, 'flags':flags,'props':props,'postcopy':[m.group(1).strip().strip('\"') for line in cfg_lines.get(original,[]) if (m:=re.match(r'^\s*copy\s+\$\(TargetPath\)\s+([^\r\n]+)',line,re.I))]}
    files = []
    for m in re.finditer(r'(?ms)^# Begin Source File\s*\n(.*?)^# End Source File\s*$',txt):
        data=m.group(1)
        pathmatch = re.search(r'^SOURCE\s*=\s*(.+)$',data,re.M)
        if not pathmatch: continue
        f = pathmatch.group(1).strip().strip('"')
        excluded = {}
        settings = {}
        current = None
        for line in data.splitlines():
            if match:=IF_RE.match(line): current=choose_cfg(match.group(1))
            elif line.startswith('!ENDIF'): current=None
            elif re.match(r'^# PROP Exclude_From_Build\s+1\b',line):
                if current: excluded[current]=True
                else: excluded['*']=True
            elif match:=OPT_RE.match(line):
                settings.setdefault(current or '*',{})[match.group(1)] = match.group(2)
            elif match:=re.match(r'^# SUBTRACT CPP\s+(.*)$',line):
                settings.setdefault(current or '*',{})['CPP_SUBTRACT'] = match.group(1)
        # Preserve per-file shader/custom-build commands that VC6 encoded
        # inside source file sections, including distinct Debug/Release recipes.
        custom={}; current=None; collecting=False; custom_lines=[]; deps=[]
        for line in data.splitlines():
            if match:=IF_RE.match(line): current=choose_cfg(match.group(1))
            elif line.startswith('!ENDIF'): current=None
            elif line.startswith('USERDEP__'):
                deps=re.findall(r'"([^"]+)"',line)
            elif line.startswith('# Begin Custom Build'):
                collecting=True;custom_lines=[]
            elif line.startswith('# End Custom Build') and collecting:
                recipe=parse_custom_block('\n'.join(custom_lines))
                recipe['dependencies']=deps[:]
                if recipe['command'] and recipe['output']:
                    custom[current or '*']=recipe
                collecting=False
            elif collecting: custom_lines.append(line)
        files.append({'path':f,'excluded':excluded,'settings':settings,'custom':custom})
    # Restore original Solution Explorer folder names from VC6 groups.
    groups=[]; byname={}
    for line in lines:
        if m:=re.match(r'^# Begin Group "([^"]+)"',line):groups.append(m.group(1))
        elif line.startswith('# End Group') and groups:groups.pop()
        elif m:=re.match(r'^SOURCE\s*=\s*(.*)$',line):
            byname.setdefault(norm_path(m.group(1).strip().strip('"')).lower(),'\\'.join(groups))
    for f in files:f['filter']=byname.get(norm_path(f['path']).lower(),'')
    return {'path':path,'name':title,'kind':kind,'subsystem':subtype, 'configs':configs,'files':files}

def link_libraries(flags):
    """Only treat standalone *.lib tokens as libraries, not /OUT or /IMPLIB.

    VC6 link switches often contain .lib filenames. The old regex grabbed
    /implib:"something.lib" (and /out:"something.lib") as dependencies.
    """
    tokens=re.findall(r'(?:[^\s"]|"[^"]*")+',flags)
    return [norm_path(token) for token in tokens
            if not token.startswith(('/', '-')) and token.strip('"').lower().endswith('.lib')]


def parse_custom_block(block):
    """Return VC6 per-file custom build recipe with explicit outputs and deps."""
    output=None;commands=[];deps=[]
    for line in block.splitlines():
        if line.startswith('USERDEP__'):
            deps.extend(re.findall(r'"([^"]+)"',line))
        if (match:=re.match(r'^\s*"([^"]+)"\s*:\s*',line)):
            output=match.group(1)
        elif (match:=re.match(r'^\s*([^\s:]+)\s*:\s*\$\(SOURCE\)',line)):
            output=match.group(1)
        elif line.startswith('\t') and line.strip():
            commands.append(line.strip())
    return {'command':'\r\n'.join(commands), 'output':output, 'dependencies':deps}


def msbuild_custom(value):
    # The shipped VC6 Perl loader cannot run reliably on current Windows.
    # Keep the original shader script/arguments, but use a probed interpreter.
    value = re.sub(r'(?im)^([ \t]*)\.\.[\\/]\.\.[\\/]devtools[\\/]bin[\\/]perl(?:\.exe)?(?=[ \t])',
                   lambda match: match.group(1) + r'py -3 "..\..\modernize\run_shader_tool.py" --perl "$(HL2ModernPerl)"', value)
    # %(Filename) is the MSBuild item metadata equivalent of VC6 InputName.
    return (value.replace('$(InputName)', '%(Filename)')
                 .replace('$(InputPath)', '%(FullPath)')
                 .replace('$(SOURCE)', '%(FullPath)')
                 .replace('$(INTDIR)', '$(IntDir)')
                 .replace('$(OUTDIR)', '$(OutDir)'))

def get_flaginfo(flags):
    return {'includes':[norm_path(v) for v in opt_values(flags,INC_RE)],
            'defines':opt_values(flags,DEF_RE),
            'libraries':link_libraries(flags),
            'libpaths':[norm_path(v) for v in opt_values(flags,LIBPATH_RE)],
            'out':get_switch(flags, 'out:'),
            'pchuse':get_switch(flags, 'Yu'),
            'pchcreate':get_switch(flags, 'Yc')}


def runtime_library(flags, configuration, legacy_default=False):
    """Preserve explicit CRT choices; VC6's missing /M* meant static /ML.

    Modern MSBuild defaults to /MD. Allocator-overriding modules cannot silently
    inherit that change. /ML is unavailable today; /MT retains static linkage.
    """
    choices={'mt':'MultiThreaded', 'mtd':'MultiThreadedDebug',
             'md':'MultiThreadedDLL', 'mdd':'MultiThreadedDebugDLL',
             'ml':'MultiThreaded', 'mld':'MultiThreadedDebug'}
    switches=re.findall(r'(?i)(?<!\S)/(MTd?|MDd?|MLd?)(?=\s|$)',flags)
    if switches:return choices[switches[-1].lower()]
    if legacy_default:
        return 'MultiThreadedDebug' if configuration.lower().startswith('debug') else 'MultiThreaded'
    return None


def source_libraries(project, configuration):
    """VC6 .lib SOURCE items are linker inputs with per-file exclusions."""
    result=[]
    for item in project['files']:
        path=norm_path(item['path'])
        if pathlib.PureWindowsPath(path).suffix.lower()!='.lib':continue
        if item['excluded'].get(configuration,item['excluded'].get('*',False)):continue
        if path.casefold() not in {p.casefold() for p in result}:result.append(path)
    return result


def vstdlib_linkage(project, configuration):
    """Restore .lib source inputs and resolve foundation DLLs to fresh builds."""
    result=[]
    mode='Debug' if configuration.lower().startswith('debug') else 'Release'
    for path in source_libraries(project,configuration):
        name=pathlib.PureWindowsPath(path).name.casefold()
        dependencies={'tier0.lib':'tier0', 'vstdlib.lib':'vstdlib',
                      'vgui_controls.lib':'vgui2/controls',
                      'game_controls.lib':'vgui2/game_controls',
                      'shaderlib.lib':'shaderlib', 'vtf.lib':'vtf',
                      'unitlib.lib':'unitlib', 'appframework.lib':'appframework',
                      'vgui_surfacelib.lib':'vgui2/vgui_surfacelib'}
        if name in dependencies:
            target=ROOT/dependencies[name]/'build'/mode/name
            path=os.path.relpath(target,project['path'].parent).replace('/', '\\')
        result.append(path)
    return result

def write_xml(doc, output):
    ET.indent(doc, space='  ')
    output.write_bytes(b'<?xml version="1.0" encoding="utf-8"?>\r\n' + ET.tostring(doc, encoding='utf-8', xml_declaration=False))

def gen_project(p, report):
    fpath = p['path'].with_suffix('.vcxproj')
    keys = list(p['configs'])
    if not keys:
        report['no_configs'].append(relative_project_name(p['path']))
        return
    root = ET.Element(X('Project'), {'DefaultTargets':'Build'})
    cg = node(root,'ItemGroup',attrs={'Label':'ProjectConfigurations'})
    for cfg in keys:
        c = node(cg,'ProjectConfiguration',attrs={'Include':cfg+'|Win32'})
        node(c,'Configuration',cfg);node(c,'Platform','Win32')
    gl = node(root,'PropertyGroup',attrs={'Label':'Globals'})
    guid = project_guid(p['path'])
    p['guid']=guid
    node(gl,'VCProjectVersion','18.0');node(gl,'ProjectGuid',guid)
    node(gl,'Keyword','Win32Proj');node(gl,'RootNamespace',p['name'])
    node(root,'Import',attrs={'Project':'$(VCTargetsPath)\\Microsoft.Cpp.Default.props'})
    for cfg,info in p['configs'].items():
        condition=f"'$(Configuration)|$(Platform)'=='{cfg}|Win32'"
        props=info['props']
        g=node(root,'PropertyGroup',attrs={'Condition':condition,'Label':'Configuration'})
        # Some original projects advertise an EXE target but explicitly link a
        # DLL (notably launcher). The linker recipe is authoritative per config.
        config_kind = ('DynamicLibrary' if re.search(r'(?:^|\s)/dll(?:\s|$)', info['flags']['LINK32'], re.I) else p['kind'])
        node(g,'ConfigurationType',config_kind)
        node(g,'UseDebugLibraries','true' if 'debug' in cfg.lower() else 'false')
        node(g,'PlatformToolset','v145')
        node(g,'CharacterSet','MultiByte')
        node(g,'WindowsTargetPlatformVersion','10.0')
        if props.get('Use_MFC') in ('1','2'): node(g,'UseOfMfc','Static' if props['Use_MFC']=='1' else 'Dynamic')
    node(root,'Import',attrs={'Project':'$(VCTargetsPath)\\Microsoft.Cpp.props'})
    node(root,'ImportGroup',attrs={'Label':'ExtensionSettings'})
    for cfg in keys:
        node(root,'ImportGroup',attrs={'Label':'PropertySheets','Condition':f"'$(Configuration)|$(Platform)'=='{cfg}|Win32'"})
    node(root,'PropertyGroup',attrs={'Label':'UserMacros'})
    if any('run_shader_tool.py' in msbuild_custom(recipe['command'])
           for file in p['files'] for recipe in file['custom'].values()):
        tools=node(root,'PropertyGroup',attrs={'Label':'ModernShaderTools'})
        node(tools,'HL2ModernPerl','$(HL2_MODERN_PERL)',
             {'Condition':"'$(HL2ModernPerl)'=='' and '$(HL2_MODERN_PERL)'!=''"})
        node(tools,'HL2ModernPerl','perl',{'Condition':"'$(HL2ModernPerl)'==''"})
    for cfg,info in p['configs'].items():
        condition=f"'$(Configuration)|$(Platform)'=='{cfg}|Win32'"
        pg=node(root,'PropertyGroup',attrs={'Condition':condition})
        # Isolate output artifacts from legacy builds and other configurations.
        node(pg,'OutDir','$(ProjectDir)build\\$(Configuration)\\')
        node(pg,'IntDir','$(ProjectDir)build\\obj\\$(MSBuildProjectName)\\$(Configuration)\\')
        flags=info['flags']
        cinfo=get_flaginfo(flags['CPP']); linfo=get_flaginfo(flags['LINK32'] or flags['LIB32'])
        orig_out = linfo['out']
        if orig_out:
            node(pg,'TargetName',pathlib.PureWindowsPath(norm_path(orig_out)).stem)
            suffix=pathlib.PureWindowsPath(norm_path(orig_out)).suffix
            if suffix: node(pg,'TargetExt',suffix)
        elif p['kind']=='StaticLibrary': node(pg,'TargetExt','.lib')
        idg=node(root,'ItemDefinitionGroup',attrs={'Condition':condition})
        cc=node(idg,'ClCompile')
        if cinfo['includes']:
            includes=list(cinfo['includes'])
            # Current platform types must precede the bundled 2002 basetsd.h;
            # the legacy DirectX SDK remains the fallback for D3DX headers.
            if relative_project_name(p['path']).lower().startswith('materialsystem/shaderdx8/'):
                includes.insert(0,'$(WindowsSDK_IncludePath)')
            node(cc,'AdditionalIncludeDirectories',';'.join(includes)+';%(AdditionalIncludeDirectories)')
        defs=cinfo['defines']
        if '_MBCS' not in defs and 'UNICODE' not in defs and '_UNICODE' not in defs: defs.append('_MBCS')
        node(cc,'PreprocessorDefinitions',';'.join(defs)+';%(PreprocessorDefinitions)')
        node(cc,'WarningLevel','Level4' if '/W4' in flags['CPP'] else 'Level3')
        node(cc,'Optimization','Disabled' if '/Od' in flags['CPP'] else 'MaxSpeed' if any(x in flags['CPP'] for x in ('/O2','/Ox')) else 'Disabled')
        node(cc,'DebugInformationFormat','ProgramDatabase')
        # Many 2003 headers expect MSVC language/CRT behavior. C++17+ postponed until sources compile.
        node(cc,'ConformanceMode','false')
        # VC6 kept a for-loop declaration visible in its enclosing scope.
        node(cc,'ForceConformanceInForLoopScope','false')
        node(cc,'SDLCheck','false')
        node(cc,'PrecompiledHeader','Use' if cinfo['pchuse'] else 'NotUsing')
        if cinfo['pchuse']: node(cc,'PrecompiledHeaderFile',cinfo['pchuse'])
        if '/GR' in flags['CPP']: node(cc,'RuntimeTypeInfo','true')
        if '/EHsc' in flags['CPP'] or '/GX' in flags['CPP']: node(cc,'ExceptionHandling','Sync')
        rt=runtime_library(flags['CPP'],cfg,legacy_default=True)
        if rt: node(cc,'RuntimeLibrary',rt)
        if '/Gz' in flags['CPP']: node(cc,'CallingConvention','StdCall')
        if '/Gd' in flags['CPP']: node(cc,'CallingConvention','Cdecl')
        rc=node(idg,'ResourceCompile'); ri=get_flaginfo(flags['RSC'])
        if ri['defines']: node(rc,'PreprocessorDefinitions',';'.join(ri['defines'])+';%(PreprocessorDefinitions)')
        if p['kind']=='StaticLibrary':
            lk=node(idg,'Lib')
        else:
            lk=node(idg,'Link')
            if p['kind']=='Application': node(lk,'SubSystem',p['subsystem'])
            node(lk,'GenerateDebugInformation','true' if '/debug' in flags['LINK32'].lower() else 'false')
        libs=linfo['libraries']
        libs=list(dict.fromkeys(libs+vstdlib_linkage(p,cfg)))
        if libs: node(lk,'AdditionalDependencies',';'.join(libs)+';%(AdditionalDependencies)')
        if p['kind']!='StaticLibrary':
            # VC6 /ML libraries request LIBC/LIBCD, retired in modern MSVC.
            # Their C runtime references resolve against this module's selected CRT.
            node(lk,'IgnoreSpecificDefaultLibraries','LIBC.lib;LIBCD.lib;%(IgnoreSpecificDefaultLibraries)')
            if any(pathlib.PureWindowsPath(lib).name.casefold()=='s3tc.lib' for lib in libs):
                # S3's supplied encoder archive predates SafeSEH; no source is present.
                # Keep the historical x86 exception-table contract for its users.
                node(lk,'ImageHasSafeExceptionHandlers','false')
        if linfo['libpaths']: node(lk,'AdditionalLibraryDirectories',';'.join(linfo['libpaths'])+';%(AdditionalLibraryDirectories)')
        defs_file=get_switch(flags['LINK32'],'def:')
        if defs_file and p['kind']!='StaticLibrary': node(lk,'ModuleDefinitionFile',norm_path(defs_file))
        # Retain original staging metadata needed by later legacy projects,
        # including explicitly declared absolute Windows build destinations.
        # Conversion writes project text only; no staging command is executed.
        for old_dest in info.get('postcopy',[]):
            command=staging_command(p['path'].parent,old_dest)
            if command is None:continue
            post=node(idg,'PostBuildEvent')
            node(post,'Command',command,{'Condition':"'$(HL2ModernStageLegacy)'=='true'"})
            break
    groups=collections.defaultdict(list)
    missing=[];case_only=[]; outside=[]
    seen_files=set()
    for f in p['files']:
        normkey=norm_path(f['path']).lower()
        if normkey in seen_files: continue
        seen_files.add(normkey)
        rel=norm_path(f['path'])
        ext=pathlib.PureWindowsPath(rel).suffix.lower()
        typ=('ClCompile' if ext in ('.cpp','.cxx','.cc','.c') else 'ClInclude' if ext in ('.h','.hpp','.hxx','.inl') else 'ResourceCompile' if ext=='.rc' else 'Midl' if ext=='.idl' else 'None')
        if f['custom'] and ext not in ('.cpp','.cxx','.cc','.c','.rc','.idl'):typ='CustomBuild'
        if ext in ('.asm','.s'):
            typ='CustomBuild' if ext=='.s' and relative_project_name(p['path']).casefold()=='engine/engine.dsp' else 'None'
        src=(p['path'].parent / f['path'].replace('\\','/')).resolve()
        if not src.exists():
            if str(src).lower() in EXISTING_LOWER:case_only.append(f['path'])
            elif not src.is_relative_to(ROOT.resolve()):outside.append(f['path'])
            else:missing.append(f['path'])
        groups[typ].append(f)
    for typ, files in groups.items():
        grp=node(root,'ItemGroup')
        for f in files:
            fe=node(grp,typ,attrs={'Include':norm_path(f['path'])})
            for cfg in keys:
                condition=f"'$(Configuration)|$(Platform)'=='{cfg}|Win32'"
                if f['excluded'].get(cfg,f['excluded'].get('*',False)) and typ in ('ClCompile','ResourceCompile','Midl','CustomBuild'):
                    node(fe,'ExcludedFromBuild','true',{'Condition':condition})
                if typ=='CustomBuild':
                    recipe=f['custom'].get(cfg,f['custom'].get('*'))
                    if recipe:
                        # The build command is preserved, not replaced with a
                        # pretend modern HLSL compiler: some shader profiles
                        # still require the original fxc_prep.pl chain.
                        engine_asm=relative_project_name(p['path']).casefold()=='engine/engine.dsp' and pathlib.PureWindowsPath(f['path']).suffix.lower()=='.s'
                        if engine_asm:
                            node(fe,'Command',r'py -3 "$(ProjectDir)..\modernize\run_legacy_masm.py" --source "%(FullPath)" --output "$(IntDir)%(Filename).obj"',{'Condition':condition})
                            node(fe,'Outputs',r'$(IntDir)%(Filename).obj',{'Condition':condition})
                            node(fe,'LinkObjects','true',{'Condition':condition})
                        else:
                            node(fe,'Command',msbuild_custom(recipe['command']),{'Condition':condition})
                            node(fe,'Outputs',msbuild_custom(recipe['output']),{'Condition':condition})
                        dependencies=[msbuild_custom(d) for d in recipe['dependencies']]
                        if engine_asm:
                            dependencies += [r'..\modernize\run_legacy_masm.py',r'gas2masm\gas2masm.c',
                                             r'asm_i386.h',r'quakeasm.h',r'audio\private\snd_asm_i386.h']
                        if 'run_shader_tool.py' in msbuild_custom(recipe['command']):
                            dependencies += [r'..\..\modernize\run_shader_tool.py',
                                             r'..\..\modernize\legacy_shader_tool.py',
                                             r'..\..\modernize\build_modern_shaders.py']
                        if dependencies:
                            node(fe,'AdditionalInputs',';'.join(dependencies)+';%(AdditionalInputs)',{'Condition':condition})
                    elif f['custom']:
                        node(fe,'ExcludedFromBuild','true',{'Condition':condition})
                opts=dict(f['settings'].get('*',{}))
                opts.update(f['settings'].get(cfg,{}))
                cpp=opts.get('CPP','')
                removed_cpp=opts.get('CPP_SUBTRACT','')
                removed_pch=bool(re.search(r'(?i)(?:^|\s)/(?:YX|Yc|Yu)(?=\s|$)',removed_cpp))
                if removed_pch and not cpp:
                    node(fe,'PrecompiledHeader','NotUsing',{'Condition':condition})
                if cpp and typ=='ClCompile':
                    inf=get_flaginfo(cpp)
                    if inf['includes']: node(fe,'AdditionalIncludeDirectories',';'.join(inf['includes'])+';%(AdditionalIncludeDirectories)', {'Condition':condition})
                    if inf['defines']:node(fe,'PreprocessorDefinitions',';'.join(inf['defines'])+';%(PreprocessorDefinitions)',{'Condition':condition})
                    if inf['pchcreate']:node(fe,'PrecompiledHeader','Create',{'Condition':condition});node(fe,'PrecompiledHeaderFile',inf['pchcreate'],{'Condition':condition})
                    elif inf['pchuse']:node(fe,'PrecompiledHeader','Use',{'Condition':condition});node(fe,'PrecompiledHeaderFile',inf['pchuse'],{'Condition':condition})
                    elif '/Y-' in cpp or removed_pch: node(fe,'PrecompiledHeader','NotUsing',{'Condition':condition})
    node(root,'Import',attrs={'Project':'$(VCTargetsPath)\\Microsoft.Cpp.targets'})
    node(root,'ImportGroup',attrs={'Label':'ExtensionTargets'})
    write_xml(root, fpath)
    report['converted']+=1;report['files']+=len(p['files']);report['configurations']+=len(keys)
    if missing:report['missing'].append({'project':relative_project_name(p['path']), 'count':len(missing), 'examples':missing[:10]})
    if outside:report['outside_root'].append({'project':relative_project_name(p['path']), 'count':len(outside), 'examples':outside[:10]})
    report['case_only_references']+=len(case_only)
    gen_filters(p,groups)

def gen_filters(p,groups):
    # Visual Studio reads .vcxproj.filters alongside the project; no changes to old .dsp.
    froot=ET.Element(X('Project'), {'ToolsVersion':'4.0'})
    filter_names=set()
    for t,fs in groups.items():
        for f in fs:
            raw=f.get('filter','')
            chunks=raw.split('\\') if raw else []
            for j in range(1,len(chunks)+1):filter_names.add('\\'.join(chunks[:j]))
    if filter_names:
        g=node(froot,'ItemGroup')
        for name in sorted(filter_names):
            n=node(g,'Filter',attrs={'Include':name})
            fid=filter_guid(p['path'],name)
            node(n,'UniqueIdentifier',fid)
    for typ,fs in groups.items():
        g=node(froot,'ItemGroup')
        for f in fs:
            el=node(g,typ,attrs={'Include':norm_path(f['path'])})
            if f.get('filter'):node(el,'Filter',f['filter'])
    write_xml(froot,p['path'].with_suffix('.vcxproj.filters'))

def generate_sln(file, projects, solution_configs=('Debug HL2','Release HL2')):
    content=['Microsoft Visual Studio Solution File, Format Version 12.00','# Visual Studio Version 18', 'VisualStudioVersion = 18.0.0.0','MinimumVisualStudioVersion = 10.0.40219.1']
    for p in projects:
        rel=os.path.relpath(p['path'].with_suffix('.vcxproj'), file.parent).replace('/','\\')
        content.append(f'Project("{PROJECT_TYPE}") = "{p["name"]} ({relative_project_name(p["path"])})", "{rel}", "{p["guid"]}"')
        content.append('EndProject')
    content+=['Global','\tGlobalSection(SolutionConfigurationPlatforms) = preSolution']
    for cfg in solution_configs: content.append(f'\t\t{cfg}|Win32 = {cfg}|Win32')
    content.append('\tEndGlobalSection')
    content.append('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution')
    for p in projects:
        available=list(p['configs'])
        for scfg in solution_configs:
            candidates=[scfg,scfg.split()[0], scfg.split()[0]+' HL2']
            resolved=next((c for c in candidates if c in available),None)
            if resolved is None:
                resolved=next((c for c in available if c.lower().startswith(scfg.split()[0].lower())),available[0])
            content.append(f'\t\t{p["guid"]}.{scfg}|Win32.ActiveCfg = {resolved}|Win32')
            # Never set Build.0 for unrelated projects; use strictly ordered build script.
    content+=['\tEndGlobalSection','\tGlobalSection(SolutionProperties) = preSolution','\t\tHideSolutionNode = FALSE','\tEndGlobalSection','EndGlobal','']
    write_crlf(file,'\n'.join(content))

def parse_build_order():
    ordered=[]
    for batch in ('build_common.bat','build_tools.bat','build_engine.bat','build_worldcraft.bat','build_hl2_game.bat','build_launcher.bat','build_hl2_servers.bat'):
        content=(ROOT / batch).read_text(encoding='latin1').splitlines()
        group=None
        for line in content:
            stripped=line.strip().lower()
            if stripped==':build_debug':group='Debug'
            elif stripped==':build_release':group='Release'
            if group is None or stripped.startswith(('rem ','@rem ', '::')): continue
            match=BAT_RE.match(line)
            if not match:continue
            project_rel=match.group(1).replace('\\','/')
            original=match.group(2)
            cfg=choose_cfg(original)
            if (group == 'Debug' and not cfg.lower().startswith('debug')) or (group=='Release' and not cfg.lower().startswith('release')):continue
            ordered.append({'batch':batch,'type':group,'dsp':project_rel,'configuration':cfg})
    return ordered

def build_cmd(ordered, projects, report):
    index={str(p['path'].relative_to(ROOT)).replace('\\','/').lower():p for p in projects}
    all_lines=['@echo off','setlocal EnableExtensions','set "ROOT=%~dp0"','set "BUILD_MODE=%~1"','if /I not "%BUILD_MODE%"=="Debug" if /I not "%BUILD_MODE%"=="Release" (echo Usage: build_hl2_vs2026.cmd Debug^|Release & exit /b 2)',
               'where msbuild >nul 2>&1','if errorlevel 1 (echo MSBuild not found. Run in an x86 Native Tools Command Prompt for VS 2026. & exit /b 2)',
               'echo Sequential HL2 2003 build - %BUILD_MODE%']
    missing=[]; per_mode=collections.defaultdict(list)
    for ent in ordered:
        p=index.get(ent['dsp'].lower())
        if not p:
            missing.append(ent);continue
        if ent['configuration'] not in p['configs']:
            match=next((cfg for cfg in p['configs'] if cfg.lower()==ent['configuration'].lower()),None)
            if not match:
                missing.append(ent);continue
            ent={**ent,'configuration':match}
        per_mode[ent['type']].append((ent,p))
    for mode in ('Debug','Release'):
        all_lines.append(f'if /I "%BUILD_MODE%"=="{mode}" goto {mode.lower()}')
    for mode in ('Debug','Release'):
        all_lines.append(':'+mode.lower())
        for ent,p in per_mode[mode]:
            proj=str(p['path'].relative_to(ROOT).with_suffix('.vcxproj')).replace('/','\\')
            all_lines.append('echo [build] '+proj+' ['+ent['configuration']+']')
            all_lines.append(f'msbuild "%ROOT%{proj}" /nologo /m:1 /t:Build /p:Configuration="{ent["configuration"]}" /p:Platform=Win32 /v:minimal')
            all_lines.append('if errorlevel 1 exit /b 1')
        all_lines.append('echo Finished '+mode+' in original batch-file order.');all_lines.append('exit /b 0')
    write_crlf(ROOT/'build_hl2_vs2026.cmd','\n'.join(all_lines)+'\n')
    report['build_steps']={key:len(val) for key,val in per_mode.items()};report['unresolved_build_steps']=missing

def main():
    arg=argparse.ArgumentParser();arg.add_argument('--check',action='store_true',help='Verify generated .vcxproj is parseable');args=arg.parse_args()
    global EXISTING_LOWER
    EXISTING_LOWER={str(path.resolve()).lower() for path in ROOT.rglob('*') if path.is_file()}
    # Path ordering itself differs between Windows (case-insensitive) and
    # POSIX. Sort the same canonical display spelling used by v17 on POSIX.
    dsp_paths=sorted(ROOT.rglob('*.dsp'),key=relative_project_name)
    projects=[parse(p) for p in dsp_paths]
    report={'converted':0,'files':0,'configurations':0,'no_configs':[], 'missing':[], 'outside_root':[], 'case_only_references':0}
    for p in projects: gen_project(p,report)
    projects=[p for p in projects if 'guid' in p]
    generate_sln(ROOT/'HL2_2003_AllProjects_VS2026.sln', projects)
    order=parse_build_order()
    index={str(p['path'].relative_to(ROOT)).replace('\\','/').lower():p for p in projects}
    hl2=[];seen=set()
    for item in order:
        p=index.get(item['dsp'].lower())
        if p and p['guid'] not in seen:hl2.append(p);seen.add(p['guid'])
    generate_sln(ROOT/'HL2_2003_VS2026.sln', hl2)
    build_cmd(order,projects,report)
    if args.check:
        broken=[]
        for p in projects:
            try: ET.parse(p['path'].with_suffix('.vcxproj'))
            except Exception as e:broken.append(f'{p["path"]}: {e}')
        report['xml_errors']=broken
    (ROOT/'modernize'/'conversion_report.json').write_bytes(json.dumps(report,indent=2).encode('utf-8'))
    print(json.dumps({k:v if not isinstance(v,list) else len(v) for k,v in report.items()},indent=2))
    if report.get('xml_errors'):sys.exit(1)

if __name__=='__main__':main()
