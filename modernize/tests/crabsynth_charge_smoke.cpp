#include "../crabsynth_charge_policy.h"
#include <cassert>
#include "test_fp_values.h"
using namespace HL2ModernCrabCharge;
int main()
{
    const float r = 120.0f, w = 100.0f, c = 40.0f;
    assert(Classify(121,0,r,w,c) == APPROACH);
    assert(Classify(120,0,r,w,c) == HIT_CENTER);
    assert(Classify(40,-40,r,w,c) == HIT_CENTER);
    assert(Classify(40,40,r,w,c) == HIT_CENTER);
    assert(Classify(40,-41,r,w,c) == HIT_LEFT);
    assert(Classify(40,41,r,w,c) == HIT_RIGHT);
    assert(Classify(40,-101,r,w,c) == MISS);
    assert(Classify(40,101,r,w,c) == MISS);
    assert(Classify(-1,0,r,w,c) == MISS);
    assert(Classify(0,0,r,w,c) == HIT_CENTER);
    assert(Classify(10,0,0,w,c) == MISS);
    assert(Classify(10,0,r,20,c) == MISS);
    const float nan = HL2ModernTestFP::NaNFloat();
    assert(Classify(nan,0,r,w,c) == MISS);
    assert(Classify(10,nan,r,w,c) == MISS);
    return 0;
}
