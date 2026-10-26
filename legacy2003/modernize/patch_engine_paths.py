#!/usr/bin/env python3
"""Remove an unsafe strcpy and empty-string underread from Sys_mkdir."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'engine'/'sys_dll.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_SAFE_DIRECTORY_PATHS' not in s:
    anchor='#include "glquake.h"\n'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'#include "../modernize/safe_paths.h"\n')
    old='''void Sys_mkdir ( const char *path )
{
	char testpath[ MAX_OSPATH ];

	// Remove any terminal backslash or /
	strcpy( testpath, path );
	if ( strlen( testpath ) > 0 &&
		testpath[ strlen( testpath ) - 1 ] == '/' ||
		testpath[ strlen( testpath ) - 1 ] == '\\\\' )
	{
		testpath[ strlen( testpath ) - 1 ] = 0;
	}'''
    if old not in s:
        a=s.index('void Sys_mkdir ( const char *path )')
        b=s.index('\n\tif( g_pFileSystem->FileExists',a)
        old=s[a:b]
        assert 'strcpy( testpath, path );' in old
    new='''// HL2MODERN_SAFE_DIRECTORY_PATHS
void Sys_mkdir ( const char *path )
{
	char testpath[ MAX_OSPATH ];
	// Reject oversized or empty paths instead of reading before/after the buffer.
	if ( !HL2ModernPaths::CopyDirectory(path, testpath, sizeof(testpath)) )
	{
		Warning("Sys_mkdir: invalid or oversized directory path.\\n");
		return;
	}
'''
    s=s.replace(old,new)
    # keep path canonical for check AND creation; no unsafe unmatched input.
    a=s.index('void Sys_mkdir ( const char *path )')
    b=s.index('\n}',a)
    method=s[a:b].replace('CreateDirHierarchy( path, "GAME" )','CreateDirHierarchy( testpath, "GAME" )')
    s=s[:a]+method+s[b:]
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Sys_mkdir bounded path handling installed')
