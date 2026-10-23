#include "../model_preview_policy.h"
#include <assert.h>
#include <math.h>

int main()
{
    using namespace HL2ModernModelPreview;
    assert(FrameDelta(-1) == 0);
    assert(FrameDelta(0) == 0);
    assert(fabs(FrameDelta(0.016) - 0.016) < 0.00001);
    assert(FrameDelta(10) == 0.1f); // returning from a hidden Options tab
    assert(WrapYaw(360) == 0);
    assert(WrapYaw(-1) == 359);
    assert(WrapYaw(1085) == 5);
    assert(AnimationCycle(20, 0) == 0);
    assert(AnimationCycle(-1, 2) == 0);
    assert(fabs(AnimationCycle(3, 2) - 0.5f) < 0.00001);
    assert(CameraDistance(10, 0, 100) == 0);
    // Project a bounding sphere into tall, square, wide and very wide panels.
    // Its angular radius must remain inside both FOVs with positive padding.
    const int widths[] = { 48, 100, 250, 900 };
    const int heights[] = { 180, 100, 100, 120 };
    const float radius = 48;
    for (int i = 0; i < 4; ++i)
    {
        const float distance = CameraDistance(radius, widths[i], heights[i]);
        assert(distance > radius);
        const double angularRadius = asin(radius / distance);
        const double fovX = 20.0 * 3.14159265358979323846 / 180.0;
        const double fovY = atan(tan(fovX) * heights[i] / widths[i]);
        assert(angularRadius < fovX);
        assert(angularRadius < fovY);
        // Doubling model size doubles distance, preserving proportions.
        assert(fabs(CameraDistance(radius * 2, widths[i], heights[i]) - distance * 2) < 0.01);
    }
    return 0;
}
