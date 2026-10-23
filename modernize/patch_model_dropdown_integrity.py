#!/usr/bin/env python3
"""Guard the legacy VGUI combo index and eliminate stale HL2 preview selection.

Idempotent; compatible with legacy C++98 and does not change other game branches.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def patch(rel, transform):
    path=ROOT/rel
    original=path.read_bytes()
    crlf=b'\r\n' in original
    src=original.decode('latin-1').replace('\r\n','\n')
    out=transform(src)
    if out != src:
        path.write_bytes((out.replace('\n','\r\n') if crlf else out).encode('latin-1'))

def combo(src):
    marker='HL2MODERN_COMBO_INDEX_GUARD'
    if marker in src:return src
    needle='''\tif (text[0] == '#')'''
    assert src.count(needle)==1
    src=src.replace(needle,'''\tif (safeText[0] == '#')''',1)
    # Unknown/edited entries must not retain a previously selected model command.
    needle='''void CLabeledCommandComboBox::OnTextChanged( char const *text )
{
\tint i;'''
    assert src.count(needle)==1
    src=src.replace(needle,'''void CLabeledCommandComboBox::OnTextChanged( char const *text )
{
\t// HL2MODERN_COMBO_INDEX_GUARD: never keep the previous command when the
\t// current label is not present (for example after list reload or edits).
\tm_iCurrentSelection = -1;
\tint i;''',1)
    needle='''\tif (m_iCurrentSelection == -1)
\t\treturn NULL;

\tCOMMANDITEM *item = &m_Items[ m_iCurrentSelection ];'''
    assert src.count(needle)==1
    src=src.replace(needle,'''\tif (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())
\t\treturn NULL;

\tCOMMANDITEM *item = &m_Items[ m_iCurrentSelection ];''',1)
    return src

def model(src):
    if 'HL2MODERN_ANIMATED_PREVIEW' in src:return src
    marker='HL2MODERN_PREVIEW_CLEAR_STALE'
    if marker in src:return src
    needle='''\t\tbool hasPortrait = false; // HL2MODERN_PREVIEW_ASSET_STATUS'''
    assert src.count(needle)==1
    return src.replace(needle,'''\t\t// HL2MODERN_PREVIEW_CLEAR_STALE: invalid/removed entries must not
\t\t// continue showing a portrait belonging to the previous model.
\t\tif (m_pModelImage) m_pModelImage->SetVisible(false);
\t\tbool hasPortrait = false; // HL2MODERN_PREVIEW_ASSET_STATUS''',1)

patch('gameui/LabeledCommandComboBox.cpp',combo)
patch('gameui/OptionsSubMultiplayer.cpp',model)
print('Model dropdown indices and stale portraits guarded.')
