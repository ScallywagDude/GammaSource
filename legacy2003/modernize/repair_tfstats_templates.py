#!/usr/bin/env python3
"""Make the original TFStats template declarations valid ISO C++98."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'utils/tfstats/timeindexedlist.h'

def run():
    before = TARGET.read_bytes()
    src = before.decode('latin1')
    if '#include <map>' not in src:
        src = src.replace('#include <list>', '#include <list>\n#include <map>', 1)
    for expr in (r'std::list<listtype>::iterator',
                 r'std::map<T,time_t>::iterator',
                 r'CTimeIndexedList<T>::iterator',
                 r'CTimeIndexedList<T>::accumulatorIterator',
                 r'CTimeIndexedList<T>::accumulator'):
        # Dependent nested types need typename, including out-of-class
        # return types. A negative lookbehind prevents repeated insertion.
        src = re.sub(r'(?<!typename )' + expr, 'typename ' + expr, src)
    # The two std:: aliases sit in a template context and typename applies.
    out = src.encode('latin1')
    if out != before:
        TARGET.write_bytes(out)
        print('TFStats timeindexedlist.h: modernized dependent type declarations')
        return 1
    print('TFStats timeindexedlist.h: already patched')
    return 0
if __name__ == '__main__':
    run()
