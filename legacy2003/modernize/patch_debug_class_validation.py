#!/usr/bin/env python3
"""Allow Debug class-registration checks to traverse private base declarations."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def patch(root=ROOT):
    path = root / 'public/networkvar.h'
    before = path.read_bytes()
    newline = b'\r\n' if b'\r\n' in before else b'\n'
    text = before.decode('latin1').replace('\r\n', '\n')
    marker = '// HL2MODERN_DEBUG_CLASS_BRIDGE'
    if marker in text:
        return
    bridge = '''// HL2MODERN_DEBUG_CLASS_BRIDGE: grant only the registration check access.
template <typename T> inline int HL2ModernCheckBaseClass(const char *name)
{
    return T::CheckDeclareClass(name);
}

'''
    text = text.replace('#ifdef _DEBUG\n', bridge + '#ifdef _DEBUG\n', 1)
    declaration = 'template <typename T> friend int CheckDeclareClass_Access(T *); \\\n'
    text = text.replace(declaration, declaration + '\t\ttemplate <typename T> friend int HL2ModernCheckBaseClass(const char *); \\\n')
    text = text.replace('return BaseClass::CheckDeclareClass( #baseClassName );',
                        'return HL2ModernCheckBaseClass<BaseClass>( #baseClassName );')
    path.write_bytes(text.encode('latin1').replace(b'\n', newline))


if __name__ == '__main__':
    patch()
