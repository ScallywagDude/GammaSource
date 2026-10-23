#ifndef HL2_MODERN_MODEL_PREVIEW_POLICY_H
#define HL2_MODERN_MODEL_PREVIEW_POLICY_H
#include <math.h>

namespace HL2ModernModelPreview
{
inline float FrameDelta(double delta)
{
    if (delta <= 0.0) return 0.0f;
    return delta > 0.1 ? 0.1f : (float)delta;
}
inline float WrapYaw(float yaw)
{
    float result = (float)fmod(yaw, 360.0f);
    return result < 0.0f ? result + 360.0f : result;
}
inline float AnimationCycle(float elapsed, float duration)
{
    if (elapsed <= 0.0f || duration <= 0.0f) return 0.0f;
    return (float)fmod(elapsed / duration, 1.0f);
}
// A sphere of this radius must fit vertically AND horizontally. PerspectiveX
// fixes horizontal FOV, so a wide viewport has the tighter vertical angle.
inline float CameraDistance(float radius, int width, int height)
{
    if (radius <= 0.0f || width <= 0 || height <= 0) return 0.0f;
    const float tangentX = (float)tan(20.0 * 3.14159265358979323846 / 180.0);
    const float tangentY = tangentX * (float)height / (float)width;
    const float tangent = tangentX < tangentY ? tangentX : tangentY;
    return radius * (float)sqrt(1.0f + tangent * tangent) / tangent * 1.08f;
}
}
#endif
