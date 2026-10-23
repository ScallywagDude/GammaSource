#ifndef HL2MODERN_LIGHTSTYLE_POLICY_H
#define HL2MODERN_LIGHTSTYLE_POLICY_H

// Engine-independent reference for 2003 animated lightstyles. Values use the
// original Source convention: 'a' = 0, 'm' = 264, 'z' = 550.
// No runtime or renderer dependency; compatible with MSVC's older C++ mode.
namespace HL2ModernLightstyles
{
    inline int LetterBrightness( char value )
    {
        // Malformed network data must not turn into extreme light intensities.
        if ( value < 'a' ) value = 'a';
        if ( value > 'z' ) value = 'z';
        return ( value - 'a' ) * 22;
    }

    inline int Sample( const char *pattern, int length, double seconds,
                       bool smooth )
    {
        if ( !pattern || length <= 0 ) return 256;
        // Callers receive at most 63 characters from the network. Guard any
        // malformed length too, and avoid indexing out of the fixed buffer.
        if ( length > 63 ) length = 63;
        const double ticks = seconds * 10.0;
        // The comparison also handles NaN and infinities without CRT helpers.
        if ( !( ticks >= 0.0 && ticks < 2147483647.0 ) )
            return LetterBrightness( pattern[0] );
        const int frame = static_cast<int>( ticks );
        const int index = frame % length;
        const int current = LetterBrightness( pattern[index] );
        if ( !smooth || length == 1 ) return current;
        const int next = LetterBrightness( pattern[( index + 1 ) % length] );
        const double fraction = ticks - static_cast<double>( frame );
        const double brightness = current + ( next - current ) * fraction;
        return static_cast<int>( brightness + 0.5 );
    }
}
#endif
