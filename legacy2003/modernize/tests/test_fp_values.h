#ifndef HL2MODERN_TEST_FP_VALUES_H
#define HL2MODERN_TEST_FP_VALUES_H
// Actual non-finite runtime inputs without a C++11-dependent <limits> header.
// Volatile division is defined floating-point arithmetic on the IEEE runtime
// used by these native tests. Assertions establish that the intended values
// were produced before they are passed to the production numerical policies.
#include <assert.h>
namespace HL2ModernTestFP {
inline float NaNFloat() {
    volatile float zero=0.0f;
    const float result=zero/zero;
    assert(result!=result);
    return result;
}
inline double NaNDouble() {
    volatile double zero=0.0;
    const double result=zero/zero;
    assert(result!=result);
    return result;
}
inline float InfinityFloat() {
    volatile float zero=0.0f;
    const float result=1.0f/zero;
    assert(result>0.0f && result+result==result);
    return result;
}
}
#endif
