#include "../houndeye_sonic_policy.h"
#include <cassert>
#include <cstdio>
#include "test_fp_values.h"
int main()
{
    using namespace HL2ModernHoundeye;
    assert(SonicFalloff(0.0f,500.0f,1.0f) == 1.0f);
    assert(SonicFalloff(250.0f,500.0f,1.0f) == 0.5f);
    assert(SonicFalloff(250.0f,500.0f,0.5f) == 0.25f);
    assert(SonicFalloff(499.0f,500.0f,1.0f) > 0.0f);
    assert(SonicFalloff(500.0f,500.0f,1.0f) == 0.0f);
    assert(SonicFalloff(501.0f,500.0f,1.0f) == 0.0f);
    assert(SonicFalloff(20.0f,0.0f,1.0f) == 0.0f);
    assert(SonicFalloff(-1.0f,500.0f,1.0f) == 0.0f);
    assert(SonicFalloff(20.0f,500.0f,-0.5f) == 0.0f);
    assert(SonicFalloff(0.0f,500.0f,3.0f) == 1.0f);
    assert(SonicFalloff(0.0f,500.0f,HL2ModernTestFP::NaNFloat()) == 0.0f);
    assert(BoundedDamage(25.0f,0.5f) == 12.5f);
    assert(BoundedDamage(-5.0f,1.0f) == 0.0f);
    assert(BoundedDamage(2000.0f,1.0f) == 0.0f);
    assert(BoundedDamage(0.0f,1.0f) == 0.0f);
    assert(BoundedDamage(30.0f,HL2ModernTestFP::NaNFloat()) == 0.0f);
    std::puts("houndeye sonic tests passed");
    return 0;
}
