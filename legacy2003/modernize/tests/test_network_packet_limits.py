"""Check that fragment metadata is bounded before stack-buffer copies."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/'engine/net_chan.cpp').read_text(encoding='latin1')

class FragmentBoundsTests(unittest.TestCase):
    def test_fragment_lengths_checked_before_ack_mutation(self):
        assert SRC.index('const int packetBits = MSG_GetReadBuf()->GetNumBitsLeft()') < SRC.index('chan->incoming_sequence = sequence')
        for check in ('frag_offset[i] < 0','frag_length[i] < 0',
                      'frag_offset[i] > packetBits',
                      'frag_length[i] > packetBits - frag_offset[i]',
                      'frag_length[i] > 2048 * 8'):
            self.assertIn(check,SRC)

    def test_no_stack_copy_without_checked_read(self):
        self.assertIn('if ( !temp.ReadBits( buffer, bits ) ||',SRC)
        self.assertIn('!pbuf->frag_message.WriteBits( buffer, bits ) )',SRC)
        self.assertIn('return false;',SRC)

    def test_metadata_bounds_model(self):
        def safe(offset,length,available,stored):
            return offset>=0 and length>=0 and offset<=available and length<=available-offset and (not stored or length<=2048*8)
        for invalid in [(-1,1,32,True),(0,-1,32,True),(33,0,32,True),
                        (3,30,32,True),(0,16_385,20000,True),(2147483647,1,32,True)]:
            self.assertFalse(safe(*invalid))
        for valid in [(0,0,0,False),(0,64,128,True),(16,16384,16400,True),
                      (0,20000,20000,False)]:
            self.assertTrue(safe(*valid))
if __name__=='__main__':unittest.main()
