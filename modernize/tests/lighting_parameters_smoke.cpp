#include "../lighting_parameters.h"
#include <cassert>
#include <math.h>
#include "test_fp_values.h"
#include <cstdio>
int main()
{
    float screen=2.4f, tex=2.0f, overbright=4.0f;
    HL2ModernLighting::SanitizeGammaParameters(screen,tex,overbright);
    assert(screen == 2.4f && tex == 2.0f && overbright == 4.0f);
    screen=0.0f; tex=-9.0f; overbright=0.0f;
    HL2ModernLighting::SanitizeGammaParameters(screen,tex,overbright);
    assert(screen == 2.2f && tex == 2.2f && overbright == 1.0f);
    screen=HL2ModernTestFP::NaNFloat();
    tex=HL2ModernTestFP::InfinityFloat();
    overbright=HL2ModernTestFP::NaNFloat();
    HL2ModernLighting::SanitizeGammaParameters(screen,tex,overbright);
    assert(screen == 2.2f && tex == 2.2f && overbright == 1.0f);
    assert(HL2ModernLighting::SchlickFresnel5(1.0f) == 0.0f);
    assert(HL2ModernLighting::SchlickFresnel5(0.0f) == 1.0f);
    assert(HL2ModernLighting::SchlickFresnel5(-5.0f) == 1.0f);
    assert(HL2ModernLighting::SchlickFresnel5(5.0f) == 0.0f);
    assert(HL2ModernLighting::SchlickFresnel5(0.5f) > 0.0f);
    float x = HL2ModernLighting::SchlickFresnel5(0.5f);
    assert(fabs(x - 0.03125f) < 1.e-6f);
    std::puts("lighting parameter tests passed");
}
