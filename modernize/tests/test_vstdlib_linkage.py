"""Preserve the allocator's static CRT and link the matching fresh tier0."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
import convert_dsp as converter

NS={'m':converter.NS}


class VstdlibLinkageTests(unittest.TestCase):
    def test_crt_patch_replays_without_changing_current_sources(self):
        import patch_windows_crt_compat
        files=[ROOT/'public/tier0/memoverride.cpp',ROOT/'public/vstdlib/strtools.h',ROOT/'vstdlib/strtools.cpp']
        before={p:p.read_bytes() for p in files}
        patch_windows_crt_compat.patch()
        self.assertEqual(before,{p:p.read_bytes() for p in files})

    def test_ucrt_msize_base_uses_the_same_allocator_as_msize(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('portable C++ compiler unavailable')
        source=(ROOT/'public/tier0/memoverride.cpp').read_bytes().decode('latin1')
        match=re.search(r'size_t __cdecl _msize_base\( void \*pMem \) _CRT_NOEXCEPT\s*\{[^}]+\}',source)
        self.assertIsNotNone(match)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            unit=root/'msize.cpp'
            output=root/('msize.exe' if os.name=='nt' else 'msize')
            unit.write_text('#include <stddef.h>\n#include <assert.h>\n'
                '#ifdef __cdecl\n#undef __cdecl\n#endif\n'
                '#ifdef _CRT_NOEXCEPT\n#undef _CRT_NOEXCEPT\n#endif\n'
                '#define __cdecl\n#define _CRT_NOEXCEPT throw()\n'
                'struct Allocator { void *seen; size_t GetSize(void *p) { seen=p; return 1234; } };\n'
                'Allocator allocator; Allocator *g_pMemAlloc=&allocator;\n'+match.group()+'\n'
                'int main() { int allocation=0; assert(_msize_base(&allocation)==1234); '
                'assert(allocator.seen==&allocation); }\n')
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(unit),'-o',str(output)],
                                  text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(output)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_explicit_runtime_switches_are_exact_and_last_one_wins(self):
        for switch,expected in (('/MT','MultiThreaded'),('/MTd','MultiThreadedDebug'),
                                ('/MD','MultiThreadedDLL'),('/MDd','MultiThreadedDebugDLL'),
                                ('/ML','MultiThreaded'),('/MLd','MultiThreadedDebug')):
            self.assertEqual(converter.runtime_library('/nologo '+switch+' /W4','Release'),expected)
        self.assertEqual(converter.runtime_library('/MT /MDd','Release'),'MultiThreadedDebugDLL')
        self.assertIsNone(converter.runtime_library('/I"/MT-path" /DNAME=/MD','Release'))

    def test_legacy_default_is_opt_in_static_runtime(self):
        self.assertIsNone(converter.runtime_library('/W4','Release'))
        self.assertEqual(converter.runtime_library('/W4','Release',True),'MultiThreaded')
        self.assertEqual(converter.runtime_library('/W4','Debug',True),'MultiThreadedDebug')
        self.assertEqual(converter.runtime_library('/MD','Debug',True),'MultiThreadedDLL')

    def test_source_library_exclusions_and_duplicates_are_preserved(self):
        project={'files':[
            {'path':r'..\lib\a.lib','excluded':{}},
            {'path':r'..\LIB\A.LIB','excluded':{}},
            {'path':'source.cpp','excluded':{}},
            {'path':'debug.lib','excluded':{'Release':True}},
            {'path':'never.lib','excluded':{'*':True}}]}
        self.assertEqual(converter.source_libraries(project,'Release'),[r'..\lib\a.lib'])
        self.assertEqual(converter.source_libraries(project,'Debug'),[r'..\lib\a.lib','debug.lib'])

    def test_vstdlib_uses_matching_isolated_tier0_import_library(self):
        project=converter.parse(ROOT/'vstdlib/vstdlib.dsp')
        xml=ET.parse(ROOT/'vstdlib/vstdlib.vcxproj').getroot()
        for cfg,expected in (('Release','MultiThreaded'),('Debug','MultiThreadedDebug')):
            expected_library='..\\tier0\\build\\'+cfg+'\\tier0.lib'
            self.assertEqual(converter.vstdlib_linkage(project,cfg),[expected_library])
            group=next(g for g in xml.findall('m:ItemDefinitionGroup',NS)
                       if "'"+cfg+"|Win32'" in g.get('Condition',''))
            self.assertEqual(group.findtext('m:ClCompile/m:RuntimeLibrary',namespaces=NS),expected)
            dependencies=group.findtext('m:Link/m:AdditionalDependencies',namespaces=NS).split(';')
            self.assertIn(expected_library,dependencies)
            self.assertNotIn(r'..\lib\public\tier0.lib',dependencies)
            self.assertIsNone(group.find('m:Link/m:ForceFileOutput',NS))


if __name__=='__main__':unittest.main()
