#!/usr/bin/env python3
"""Conservative fixes for the bundled legacy QHTM Win32 projects.

The older QHTM code predates modern C++ dependent-type parsing and Windows
headers. Keep fixes confined to this library; no AMD64 or gameplay changes.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
changed = []

def patch_text(rel, fix):
    p = ROOT / rel
    original = p.read_bytes()
    decoded = original.decode('latin1')
    updated = fix(decoded)
    if updated != decoded:
        p.write_bytes(updated.encode('latin1'))
        changed.append(rel)


def fix_debughlp(src):
    # Win32 headers already declare LPCSTR, LPCWSTR and BOOL. #ifndef only
    # detects macros, not typedefs, so the historical guards never worked.
    return re.sub(r'#ifndef LPCSTR\s*\n\s*typedef const char \* LPCSTR;.*?#endif\s*//\s*BOOL',
                  '// Use the Windows SDK definitions of LPCSTR, LPCWSTR and BOOL.',
                  src, count=1, flags=re.S)


def fix_map(src):
    old = ('\t\t\toperator = ( const BucketEntry& ){ m_Value = rhs.m_Value; '
           'm_Key = rhs.m_Key; m_pNext = rhs.m_pNext; }\n'
           '\t\t\tBucketEntry( const BucketEntry &rhs ){ operator =( rhs ) }')
    new = ('\t\t\tBucketEntry &operator = ( const BucketEntry &rhs ) '
           '{ m_Value = rhs.m_Value; m_Key = rhs.m_Key; '
           'm_pNext = rhs.m_pNext; return *this; }\n'
           '\t\t\tBucketEntry( const BucketEntry &rhs ) { *this = rhs; }')
    if old not in src and 'BucketEntry &operator = ( const BucketEntry &rhs )' not in src:
        raise AssertionError('Unexpected Map.h layout')
    src = src.replace(old, new)
    # The method bodies use HashIt/ElementsTheSame. Define those helpers
    # before the template bodies for conforming two-phase lookup.
    src = src.replace('\t#include "Map.inl"\n', '')
    if '\t#include "Map.inl"' not in src:
        src = src.replace('\n}\n#endif //#ifdef MAP_CONTAINER_H',
                          '\n\t#include "Map.inl"\n}\n#endif //#ifdef MAP_CONTAINER_H')
    return src


def fix_map_impl(src):
    return re.sub(r'(?:typename\s+)*CMap<Key, Value>::BucketEntry \*CMap<Key, Value>::Find',
                  'typename CMap<Key, Value>::BucketEntry *CMap<Key, Value>::Find', src)


def fix_map_iter(src):
    return re.sub(r'(?:typename\s+)*CMap<Key, Value>::BucketEntry \*m_',
                  'typename CMap<Key, Value>::BucketEntry *m_', src)


def fix_map_iter_impl(src):
    src = re.sub(r'(?:typename\s+)*CMap<Key,Value>::BucketEntry \*pEntry',
                 'typename CMap<Key,Value>::BucketEntry *pEntry', src)
    # Do not access an uninitialized next-bucket pointer on an empty table.
    if 'm_pBucket = NULL;\n\tm_pNext = NULL;' not in src:
        src = src.replace('m_pBucket = NULL;\n\n\t//\n\t// Set pointers',
                          'm_pBucket = NULL;\n\tm_pNext = NULL;\n\n\t//\n\t// Set pointers', 1)
    return src


def fix_winhelper(src):
    # An explicit copy constructor prevents normal by-value return on MSVC.
    return src.replace('inline explicit CSize( const CSize& size )',
                       'inline CSize( const CSize& size )')


def fix_project(src):
    root = r'$(ProjectDir)..\;$(ProjectDir)..\QHTM\;'
    def repl(match):
        pre, contents, post = match.groups()
        if root in contents:
            return match.group(0)
        m = re.search(r'<AdditionalIncludeDirectories>(.*?)</AdditionalIncludeDirectories>',
                      contents, flags=re.S)
        if m:
            contents = contents[:m.start(1)] + root + contents[m.start(1):]
        else:
            contents = '\n      <AdditionalIncludeDirectories>' + root + '%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>' + contents
        return pre + contents + post
    return re.sub(r'(<ItemDefinitionGroup\b[^>]*>\s*<ClCompile>)(.*?)(</ClCompile>)',
                  repl, src, flags=re.S)


def run():
    patch_text('vgui2/QHTM/DebugHlp.h', fix_debughlp)
    patch_text('vgui2/QHTM/Map.h', fix_map)
    patch_text('vgui2/QHTM/Map.inl', fix_map_impl)
    patch_text('vgui2/QHTM/MapIter.h', fix_map_iter)
    patch_text('vgui2/QHTM/MapIter.inl', fix_map_iter_impl)
    patch_text('vgui2/QHTM/WinHelper.h', fix_winhelper)
    for path in sorted((ROOT / 'vgui2' / 'QHTM').rglob('*.vcxproj')):
        patch_text(path.relative_to(ROOT).as_posix(), fix_project)
    print('QHTM Win32 repairs:', len(changed), 'files modified')
    return changed

if __name__ == '__main__':
    run()
