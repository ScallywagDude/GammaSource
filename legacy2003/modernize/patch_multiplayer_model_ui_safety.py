#!/usr/bin/env python3
"""Fix model selector command bounds, index mapping and stale portrait state."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def rewrite(path, transformations):
    file=ROOT/path; raw=file.read_bytes(); crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n')
    if 'HL2MODERN_MODEL_UI_SAFETY' in s:return
    for before,after in transformations:
        if s.count(before)!=1:raise RuntimeError('%s: expected one match: %s (got %d)'%(path,before[:75],s.count(before)))
        s=s.replace(before,after,1)
    file.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))

rewrite('gameui/LabeledCommandComboBox.cpp',[
    ('\tm_Items.RemoveAll();','\tm_Items.RemoveAll();\n\tm_iCurrentSelection = -1;\n\tm_iStartSelection = -1; // HL2MODERN_MODEL_UI_SAFETY'),
    ('\tstrcpy( item->name, text );', '''\tconst char *safeText = text ? text : "";
\tconst char *safeCommand = engineCommand ? engineCommand : "";
\tstrncpy(item->name, safeText, sizeof(item->name)-1);
\titem->name[sizeof(item->name)-1] = 0;'''),
    ('\tstrcpy( item->command, engineCommand );', '''\tstrncpy(item->command, safeCommand, sizeof(item->command)-1);
\titem->command[sizeof(item->command)-1] = 0;'''),
    ('void CLabeledCommandComboBox::ActivateItem(int index)\n{\n\tif ( index< m_Items.Count() )',
     'void CLabeledCommandComboBox::ActivateItem(int index)\n{\n\tif ( index >= 0 && index < m_Items.Count() )'),
    ('void CLabeledCommandComboBox::SetInitialItem(int index)\n{\n\tif ( index< m_Items.Count() )',
     'void CLabeledCommandComboBox::SetInitialItem(int index)\n{\n\tif ( index >= 0 && index < m_Items.Count() )'),
    ('\t\tint comboBoxID = m_Items[index].comboBoxID;\n\t\tActivateItem(comboBoxID);',
     '\t\tActivateItem(index); // internal index, not ComboBox handle'),
    ('\tif (m_iCurrentSelection == -1)\n\t\treturn;', '\tif (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())\n\t\treturn;'),
])
rewrite('gameui/OptionsSubMultiplayer.cpp',[
    ('\t\tstrcpy( currentmodel, p );','\t\tstrncpy( currentmodel, p, sizeof(currentmodel)-1 );\n\t\tcurrentmodel[sizeof(currentmodel)-1] = 0; // HL2MODERN_MODEL_UI_SAFETY'),
    ('''\tm_pModelList->GetText( modelname, 256 );
\tif( !modelname[ 0 ] )
\t\treturn;''', '''\tm_pModelList->GetText( modelname, sizeof(modelname) );
\tif( !modelname[0] )
\t{
\t\t// A branch with no installed models must not retain its last portrait.
\t\tif (m_pModelImage) m_pModelImage->SetVisible(false);
\t\tif (m_pPreviewLabel) m_pPreviewLabel->SetText("No installed compatible models");
\t\treturn;
\t}'''),
])
rewrite('gameui/BitmapImagePanel.cpp',[
    ('''// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;''', '''using namespace vgui; // HL2MODERN_MODEL_UI_SAFETY: only one memdbgon include'''),
    ('''\t\tif (!filename) filename="";
\tstrncpy(m_szTexture, filename, sizeof(m_szTexture)-1);''', '\t\tstrncpy(m_szTexture, filename, sizeof(m_szTexture)-1);'),
    ('''\tm_szTexture[sizeof(m_szTexture)-1]=0;

\tif ( m_bUploaded )''', '''\tm_szTexture[sizeof(m_szTexture)-1]=0;
\tif (!m_szTexture[0])
\t{
\t\t// Avoid uploading a zero-length filename when the selected image disappears.
\t\tm_bUploaded = false;
\t\treturn;
\t}

\tif ( m_bUploaded )'''),
])
print('Fixed VGUI item IDs, bounded text, empty selections, and portrait fallback.')
