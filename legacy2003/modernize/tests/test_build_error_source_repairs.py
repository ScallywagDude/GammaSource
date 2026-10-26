"""Regression checks for the October 2026 verified legacy compiler fixes."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

def source(path):
    return (ROOT / path).read_bytes().decode('latin1')

class BuildErrorRepairs(unittest.TestCase):
    def test_message_map_base_is_inheritable(self):
        txt = source('Tracker/AdminServer/BaseGamesPage.h')
        self.assertIn('protected: // Derived page maps refer to this base map.\n\tDECLARE_PANELMAP();', txt)

    def test_member_callback_arguments_are_qualified(self):
        for rel,owner,functions in [
            ('utils/hlfaceposer/choreoview.cpp','CChoreoView',('Select','SelectAllEvents','Deselect')),
            ('utils/hlfaceposer/mxexpressiontray.cpp','mxExpressionTray',('ET_Undo','ET_Redo')),
            ('utils/hlfaceposer/phonemeeditor.cpp','PhonemeEditor',('ITER_SelectSpanningWords','ITER_MoveSelectedWords')),
        ]:
            text=source(rel)
            for name in functions:
                with self.subTest(rel=rel,method=name):
                    self.assertIn('&%s::%s' % (owner,name),text)

    def test_macro_string_literal_spacing(self):
        for rel in ('master/hlmasterdlg.cpp','master/hlmodmaster/hlmodmasterdlg.cpp',
                    'utils/hlfaceposer/mdlviewer.cpp'):
            with self.subTest(rel=rel):
                text=source(rel)
                self.assertNotIn('"__DATE__',text)
        text=source('utils/hlfaceposer/phonemeeditor.cpp')
        self.assertNotIn('"WORD_DATA_EXTENSION',text)
        self.assertNotIn('"*"WORD_DATA_EXTENSION',text)

    def test_retired_headers_replaced(self):
        for rel in ('utils/NormalMapper/NmFileIO.cpp','utils/NormalMapper/ArgFileIO.cpp',
                    'utils/NormalMapper/TGAIO.cpp'):
            with self.subTest(rel=rel):
                text=source(rel)
                self.assertIn('#include <iostream>',text)
                self.assertIn('#include <fstream>',text)
                self.assertNotIn('#include <fstream.h>',text)
        self.assertNotIn('#include <ostream.h>',source('utils/mxtk/src/win32/mxwindow.cpp'))
        self.assertNotIn('#include <strstrea.h>',source('utils/vrad/vraddll.cpp'))

    def test_diagnostic_format_strings(self):
        text=source('utils/hlfaceposer/addsoundentry.cpp')
        self.assertIn("Sound '%s' already exists",text)
        self.assertIn("Script '%s' does not exist",text)

if __name__=='__main__':
    unittest.main()
