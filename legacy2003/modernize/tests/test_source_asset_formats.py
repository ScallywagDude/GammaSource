"""Malformed content must never turn into a successfully verified asset."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import source_asset_formats as formats


def fixture():
    data = bytearray(6000)
    data[:4] = b'IDST'
    for offset, value in {4:36, 76:len(data), 156:1, 160:372, 188:1,
                          192:568, 208:1, 212:4852, 216:1, 220:4944,
                          244:1, 248:4884}.items():
        struct.pack_into('<i',data,offset,value)
    for base, name_offset, name in [(372,5000,b'root'), (568,5020,b'idle'),
                                    (4852,5040,b'body'), (4884,5060,b'Mouth')]:
        struct.pack_into('<i',data,base,name_offset-base)
        data[name_offset:name_offset+len(name)+1] = name+b'\0'
    struct.pack_into('<i',data,572,5080-568)
    data[5080:5089] = b'ACT_IDLE\0'
    struct.pack_into('<i',data,4944,5100)
    data[5100:5115] = b'models/example\0'
    return data


class ModelMetadataTests(unittest.TestCase):
    def inspect(self, data):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'model.mdl'
            path.write_bytes(data)
            return formats.inspect_mdl(path)

    def test_supplied_version_metadata(self):
        result=self.inspect(fixture())
        self.assertEqual(result['status'],'metadata_verified',result['issues'])
        self.assertEqual(result['bone_names'],['root'])
        self.assertEqual(result['sequence_names'],['idle'])
        self.assertEqual(result['activity_names'],['ACT_IDLE'])
        self.assertEqual(result['material_names'],['body'])
        self.assertEqual(result['material_directories'],['models/example'])
        self.assertEqual(result['attachments'],['Mouth'])

    def test_truncation_offsets_counts_and_unterminated_strings(self):
        malformed=[]
        malformed.append(fixture()[:90])
        for offset,value in ((76,20),(160,8),(156,-1),(188,65537),(212,5990),(248,5980),(372,-400),(4892,2)):
            data=fixture();struct.pack_into('<i',data,offset,value);malformed.append(data)
        data=fixture();data[5000:]=b'x'*(len(data)-5000);malformed.append(data)
        for data in malformed:
            with self.subTest(header=data[:8]):
                result=self.inspect(data)
                self.assertEqual(result['status'],'invalid',result)
                self.assertTrue(result['issues'])

    def test_newer_version_not_decoded_as_2003_format(self):
        data=fixture();struct.pack_into('<i',data,4,37)
        result=self.inspect(data)
        self.assertEqual(result['status'],'unsupported_version')
        self.assertEqual(result['sequence_names'],[])


if __name__=='__main__':
    unittest.main()
