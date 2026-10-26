#include "../lightstyle_policy.h"
#include <cassert>
#include <stdio.h>
#include "test_fp_values.h"

int main()
{
    using namespace HL2ModernLightstyles;
    assert(LetterBrightness('a') == 0);
    assert(LetterBrightness('m') == 264);
    assert(LetterBrightness('z') == 550);
    assert(LetterBrightness('0') == 0);
    assert(LetterBrightness('~') == 550);
    assert(Sample(0, 0, 0., false) == 256);
    assert(Sample("", 0, 0., true) == 256);
    assert(Sample("az", 2, 0., false) == 0);
    assert(Sample("az", 2, 0.05, false) == 0);
    assert(Sample("az", 2, 0.05, true) == 275);
    assert(Sample("az", 2, 0.10, true) == 550);
    assert(Sample("az", 2, 0.15, true) == 275);
    assert(Sample("az", 2, 0.20, true) == 0);
    assert(Sample("m", 1, 50.0, true) == 264);
    assert(Sample("az", 2, -1.0, true) == 0);
    assert(Sample("az", 2, 1.0e100, true) == 0);
    const double nan = HL2ModernTestFP::NaNDouble();
    assert(Sample("az", 2, nan, true) == 0);
    assert(Sample("~", 1, 0., false) == 550);
    assert(Sample("!", 1, 0., false) == 0);
    assert(Sample("az", 1024, 0., true) == 0);
    puts("lightstyle policy tests passed");
    return 0;
}
