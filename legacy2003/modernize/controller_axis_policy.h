#ifndef HL2MODERN_CONTROLLER_AXIS_POLICY_H
#define HL2MODERN_CONTROLLER_AXIS_POLICY_H

// Shared, dependency-free XInput radial deadzone with continuous response.
// The Windows integration owns SHORT conversion and XInput state polling.
#include <cmath>
namespace HL2ModernController
{
    inline int ClampDeadzone(int value)
    {
        if (value < 0) return 0;
        if (value > 30000) return 30000;
        return value;
    }

    inline void RadialDeadzone(int &x, int &y, int deadzone)
    {
        deadzone = ClampDeadzone(deadzone);
        const double dx = static_cast<double>(x);
        const double dy = static_cast<double>(y);
        const double magnitude = std::sqrt(dx * dx + dy * dy);
        if (magnitude <= deadzone || magnitude < 0.001)
        {
            x = 0; y = 0; return;
        }
        double adjusted = (magnitude - deadzone) / (32767.0 - deadzone);
        if (adjusted > 1.0) adjusted = 1.0;
        const double factor = adjusted * 32767.0 / magnitude;
        const double outx = dx * factor;
        const double outy = dy * factor;
        x = static_cast<int>(outx + (outx >= 0 ? 0.5 : -0.5));
        y = static_cast<int>(outy + (outy >= 0 ? 0.5 : -0.5));
        if (x < -32768) x = -32768;
        if (x > 32767) x = 32767;
        if (y < -32768) y = -32768;
        if (y > 32767) y = 32767;
    }
}
#endif
