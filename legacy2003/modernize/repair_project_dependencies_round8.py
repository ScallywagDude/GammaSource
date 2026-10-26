#!/usr/bin/env python3
"""Declare build-order dependencies for linked libraries built inside this source tree.

The old projects name libraries by relative path without ProjectReference edges.
MSBuild can otherwise link consumers before those libraries exist. Keep existing
link arguments unchanged and disable transitive auto-linking in the references.
"""
from pathlib import Path
import os
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEPENDENCIES = {
    'tier0.lib': ROOT / 'tier0/tier0.vcxproj',
    'vstdlib.lib': ROOT / 'vstdlib/vstdlib.vcxproj',
    'vtf.lib': ROOT / 'vtf/vtf.vcxproj',
    'vgui_controls.lib': ROOT / 'vgui2/controls/vgui_controls.vcxproj',
    'mxtk.lib': ROOT / 'utils/mxtk/msvc/mxtoolkitwin32.vcxproj',
}
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}


def project_guid(path):
    tree = ET.parse(path)
    guid = tree.findtext('m:PropertyGroup[@Label="Globals"]/m:ProjectGuid', namespaces=NS)
    if not guid or not re.fullmatch(r'\{[0-9A-Fa-f-]{36}\}', guid):
        raise ValueError('Missing project GUID: ' + str(path))
    return guid


def patch_dx9_includes():
    # The IHV test target shares locald3dtypes.h with shaderdx8 and also
    # needs the bundled DX9 headers. The original DSP only names DX8.
    target = ROOT / 'materialsystem/shaderdx8/shaderdx8_ihvtest.vcxproj'
    source = target.read_text(encoding='utf-8-sig')
    anchor = '..\\..\\dx8sdk\\include;'
    additional = '..\\..\\dx9sdk\\include;'
    # Only append when missing: the conversion script can regenerate projects.
    if additional not in source:
        if anchor not in source:
            raise ValueError('Missing legacy DirectX include directories')
        source = source.replace(anchor, anchor + additional)
        ET.fromstring(source)
        target.write_text(source, encoding='utf-8')
        return True
    return False


def patch():
    patch_dx9_includes()
    guids = {name: project_guid(path) for name, path in DEPENDENCIES.items()}
    changed = []
    for path in sorted(ROOT.rglob('*.vcxproj')):
        source = path.read_text(encoding='utf-8-sig')
        tree = ET.fromstring(source)
        linked = set()
        for group in tree.findall('m:ItemDefinitionGroup', NS):
            for section in ('Link', 'Lib'):
                node = group.find('m:' + section + '/m:AdditionalDependencies', NS)
                if node is not None and node.text:
                    for item in node.text.split(';'):
                        name = item.strip().replace('/', '\\').split('\\')[-1].lower()
                        if name in DEPENDENCIES:
                            linked.add(name)
        already = {ref.get('Include', '').replace('/', '\\').lower()
                   for ref in tree.findall('.//m:ProjectReference', NS)}
        add = []
        for libname in sorted(linked):
            dependency = DEPENDENCIES[libname]
            if dependency.resolve() == path.resolve():
                continue
            relative = os.path.relpath(dependency, path.parent).replace('/', '\\')
            if relative.lower() in already:
                continue
            add.append((relative, guids[libname]))
        if not add:
            continue
        if '</Project>' not in source:
            raise ValueError('Unexpected project XML: ' + str(path))
        references = '  <ItemGroup>\n'
        for relative, guid in add:
            references += ('    <ProjectReference Include="' + relative + '">\n'
                           '      <Project>' + guid + '</Project>\n'
                           '      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n'
                           '      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n'
                           '    </ProjectReference>\n')
        references += '  </ItemGroup>\n'
        source = source.replace('</Project>', references + '</Project>', 1)
        ET.fromstring(source)
        path.write_text(source, encoding='utf-8')
        changed.append((path.relative_to(ROOT), len(add)))
    return changed


if __name__ == '__main__':
    changes = patch()
    print('Project dependency ordering added to', len(changes), 'projects;',
          sum(n for _, n in changes), 'references')
