#!/usr/bin/env python3
"""Repair the first modern Windows CRT blockers without changing legacy APIs."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def patch(root=ROOT):
    changes = {
        'public/vstdlib/strtools.h': [
            ('return strrchr( s, c );', 'return const_cast<char *>(strrchr( s, c ));'),
            ('return strstr( s1, search );', 'return const_cast<char *>(strstr( s1, search ));'),
        ],
        'vstdlib/strtools.cpp': [
            ('#ifdef _LINUX\n#include <ctype.h>\n#endif', '#include <ctype.h> // Character conversion is used on Windows too.'),
            ('return strstr( s1, search );', 'return const_cast<char *>(strstr( s1, search ));'),
        ],
        'public/tier0/memoverride.cpp': [
            ('size_t _msize( void *pMem )\n{\n\treturn g_pMemAlloc->GetSize(pMem);\n}',
             'size_t _msize( void *pMem )\n{\n\treturn g_pMemAlloc->GetSize(pMem);\n}\n\n'
             '#if defined(_MSC_VER) && _MSC_VER >= 1900\n'
             '// HL2MODERN_UCRT_MSIZE_BASE: UCRT helpers use the allocator base entry too.\n'
             'size_t __cdecl _msize_base( void *pMem ) _CRT_NOEXCEPT\n'
             '{\n\treturn g_pMemAlloc->GetSize(pMem);\n}\n'
             '#endif'),
            ('long _crtBreakAlloc;',
             '// Modern CRT headers expose these names as accessor macros.\n'
             '// Keep the legacy allocator globals as variables in this unit.\n'
             '#ifdef _crtBreakAlloc\n#undef _crtBreakAlloc\n#endif\n'
             '#ifdef _crtDbgFlag\n#undef _crtDbgFlag\n#endif\n'
             'long _crtBreakAlloc;'),
        ],
    }
    for relative, replacements in changes.items():
        path = root / relative
        before = path.read_bytes()
        newline = b'\r\n' if b'\r\n' in before else b'\n'
        text = before.decode('latin1').replace('\r\n', '\n')
        for old, new in replacements:
            if new in text:
                continue
            if text.count(old) != 1:
                raise ValueError('Unexpected CRT patch input: ' + relative)
            text = text.replace(old, new)
        after = text.encode('latin1').replace(b'\n', newline)
        if before != after:
            path.write_bytes(after)


if __name__ == '__main__':
    patch()
