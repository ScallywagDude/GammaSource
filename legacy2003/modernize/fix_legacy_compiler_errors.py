#!/usr/bin/env python3
"""Idempotent repairs for verified VC6->MSVC2026 source compilation errors.

Run from anywhere with: python modernize/fix_legacy_compiler_errors.py
Each replacement is scoped to a specific source file and fails on unexpected input.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
changes = []


def patch(rel, old, new, *, expected=None):
    p = ROOT / rel
    raw = p.read_bytes()
    # Preserve source encodings and line-endings exactly.
    s = raw.decode('latin1')
    count = s.count(old)
    if count == 0 and new in s:
        return
    if count == 0 or (expected is not None and count != expected):
        raise RuntimeError('%s: expected %s occurrence(s) of %r; found %s' %
                           (rel, expected, old, count))
    s = s.replace(old, new)
    p.write_bytes(s.encode('latin1'))
    changes.append((rel, count))


# Derived VGUI pages need to reference CBaseGamesPage::m_PanelMap in
# IMPLEMENT_PANELMAP; a private base entry blocks generated message maps.
patch('Tracker/AdminServer/BaseGamesPage.h',
      '\ttypedef vgui::Frame BaseClass;\n\n\tDECLARE_PANELMAP();',
      '\ttypedef vgui::Frame BaseClass;\n\nprotected: // Derived page maps refer to this base map.\n\tDECLARE_PANELMAP();', expected=1)

# VC6 accepted an implicit bound member-method reference in callback arguments.
# Standard C++ requires the address of the explicitly qualified method.
for rel, call, methods in [
    ('utils/hlfaceposer/choreoview.cpp', 'TraverseWidgets', ['Select','SelectAllEvents','Deselect']),
    ('utils/hlfaceposer/phonemeeditor.cpp', 'TraverseWords',
     ['ITER_AddFocusRectSelectedWords','ITER_MoveSelectedWords','ITER_SelectSpanningWords',
      'ITER_CountSelectedWords','ITER_ExtendSelectedWordEndTimes']),
    ('utils/hlfaceposer/phonemeeditor.cpp', 'TraversePhonemes',
     ['ITER_AddFocusRectSelectedPhonemes','ITER_MoveSelectedPhonemes',
      'ITER_CountSelectedPhonemes','ITER_ExtendSelectedPhonemeEndTimes']),
    ('utils/hlfaceposer/phonemeeditor.cpp', 'TraversePhrases',
     ['ITER_AddFocusRectSelectedPhrases','ITER_CountSelectedPhrases']),
    ('utils/hlfaceposer/PhonemeEditor_CloseCaption.cpp', 'TraversePhrases',
     ['ITER_MoveSelectedPhrases','ITER_ExtendSelectedPhraseEndTimes']),
]:
    owner = {'choreoview.cpp':'CChoreoView', 'mxexpressiontray.cpp':'mxExpressionTray'}.get(Path(rel).name, 'PhonemeEditor')
    for meth in methods:
        # Replace just callback argument, not method definitions.
        old = '%s( %s,' % (call, meth)
        new = '%s( &%s::%s,' % (call, owner, meth)
        patch(rel,old,new)

# Toolbar button callbacks are the fourth positional argument.
for method in ('ET_Undo', 'ET_Redo'):
    patch('utils/hlfaceposer/mxexpressiontray.cpp', ', %s, true,' % method,
          ', &mxExpressionTray::%s, true,' % method, expected=1)

# The first string ended with a quote directly against the macro name, so
# MSVC tokenized it as a user-defined literal suffix rather than concatenation.
patch('utils/hlfaceposer/phonemeeditor.cpp', '"*"WORD_DATA_EXTENSION', '"*" WORD_DATA_EXTENSION', expected=2)
patch('utils/hlfaceposer/phonemeeditor.cpp', '"WORD_DATA_EXTENSION', '" WORD_DATA_EXTENSION', expected=2)

# Standard-library headers removed by newer Visual Studio toolsets.
for rel in ('utils/NormalMapper/NmFileIO.cpp',
            'utils/NormalMapper/ArgFileIO.cpp',
            'utils/NormalMapper/TGAIO.cpp'):
    patch(rel, '#include <iostream.h>', '#include <iostream>\nusing std::cerr;', expected=1)
    patch(rel, '#include <fstream.h>', '#include <fstream>', expected=1)
patch('utils/mxtk/src/win32/mxwindow.cpp', '#include <ostream.h>', '#include <ostream>', expected=1)
# vraddll doesn't use the obsolete strstream type; drop unused header.
patch('utils/vrad/vraddll.cpp', '#include <strstrea.h>\n', '', expected=1)

# C++11 treats a string literal immediately followed by a macro name as a
# user-defined literal suffix. Insert whitespace between adjacent tokens.
for rel in ('master/hlmasterdlg.cpp', 'master/hlmodmaster/hlmodmasterdlg.cpp'):
    patch(rel, '"__DATE__', '" __DATE__', expected=1)
patch('utils/hlfaceposer/mdlviewer.cpp',
      'Build Date: "__DATE__""', 'Build Date: " __DATE__ ""', expected=1)

# The literal percent character in a user-visible diagnostic is a formatting
# error (va() expects a format specifier before its value).
patch('utils/hlfaceposer/addsoundentry.cpp', "Sound '%' already exists", "Sound '%s' already exists", expected=1)
patch('utils/hlfaceposer/addsoundentry.cpp', "Script '%' does not exist", "Script '%s' does not exist", expected=1)

if __name__ == '__main__':
    print('Fixed %d source occurrences in %d files' % (sum(c for _,c in changes),len(set(p for p,_ in changes))))
    for f in sorted(set(x for x,_ in changes)):
        print('  ', f)
