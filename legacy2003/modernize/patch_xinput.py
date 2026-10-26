#!/usr/bin/env python3
"""Patch WinMM game controller integration with dynamically loaded XInput.

Requires Windows SDK Xinput.h on Visual Studio 2026; no XInput import lib.
Legacy WinMM remains the fallback if no Xbox-compatible pad is present.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'cl_dll'/'in_joystick.cpp'
s=p.read_bytes().decode('latin1').replace('\r\n','\n')
if 'HL2MODERN_XINPUT_BRIDGE' not in s:
    anchor='#include <windows.h>'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n#include <Xinput.h> // Windows SDK: XInput gamepads\n')
    at='static JOYINFOEX ji;'
    assert s.count(at)==1
    bridge='''
// HL2MODERN_XINPUT_BRIDGE
// XInput is loaded at runtime for compatibility with machines that have no
// XInput DLL. These are Windows system libraries, not bundled binaries.
static HMODULE s_hXInputLibrary = NULL;
typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, XINPUT_STATE *);
static XInputGetStateFn s_pXInputGetState = NULL;
static bool s_bXInputActive = false;
static DWORD s_nXInputSlot = 0;

static bool ProbeXInput( void )
{
    const char *dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for ( int i = 0; i < 3; ++i )
    {
        HMODULE module = LoadLibraryA( dlls[i] );
        if ( !module )
            continue;
        XInputGetStateFn fn = (XInputGetStateFn)GetProcAddress( module, "XInputGetState" );
        if ( fn )
        {
            s_hXInputLibrary = module;
            s_pXInputGetState = fn;
            for ( DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot )
            {
                XINPUT_STATE state;
                ZeroMemory( &state, sizeof(state) );
                if ( fn( slot, &state ) == ERROR_SUCCESS )
                {
                    s_nXInputSlot = slot;
                    return true;
                }
            }
            // Loaded library retained for optional hotplug detection elsewhere.
            return false;
        }
        FreeLibrary( module );
    }
    return false;
}

static DWORD ToJoyAxis( SHORT value, bool invert )
{
    // Both WinMM and XInput code paths use a 0..65535 axis with 32768 center.
    long v = 32768 + (invert ? -(long)value : (long)value);
    return (DWORD)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
}

static void FillFromXInput( JOYINFOEX *out, const XINPUT_GAMEPAD &pad )
{
    out->dwXpos = ToJoyAxis( pad.sThumbLX, true );
    out->dwYpos = ToJoyAxis( pad.sThumbLY, true );
    out->dwZpos = ToJoyAxis( pad.sThumbRX, false );
    out->dwRpos = ToJoyAxis( pad.sThumbRY, true );
    out->dwUpos = out->dwVpos = 32768;
    // First 4 are JOY1..JOY4, next 8 are auxiliary buttons.
    const WORD buttons[] = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB
    };
    for ( int i = 0; i < 10; ++i )
        if ( pad.wButtons & buttons[i] )
            out->dwButtons |= (1u << i);
    if ( pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
        out->dwButtons |= (1u << 10);
    if ( pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
        out->dwButtons |= (1u << 11);

    const bool up = (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    const bool down = (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    const bool left = (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    const bool right = (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    out->dwPOV = JOY_POVCENTERED;
    if (up && right) out->dwPOV = 4500;
    else if (down && right) out->dwPOV = 13500;
    else if (down && left) out->dwPOV = 22500;
    else if (up && left) out->dwPOV = 31500;
    else if (up) out->dwPOV = JOY_POVFORWARD;
    else if (right) out->dwPOV = JOY_POVRIGHT;
    else if (down) out->dwPOV = JOY_POVBACKWARD;
    else if (left) out->dwPOV = JOY_POVLEFT;
}
'''
    s=s.replace(at,at+'\n'+bridge)
    anchor='''	// verify joystick driver is present
'''
    replacement='''	// Prefer the XInput pad when connected. Use -noxinput to force WinMM.
	if ( !CommandLine()->FindParm("-noxinput") && ProbeXInput() )
	{
		s_bXInputActive = true;
		m_nJoystickButtons = 12;
		m_fJoystickHasPOVControl = 1;
		m_nJoystickOldButtons = 0;
		m_nJoystickOldPOVState = 0;
		m_fJoystickAvailable = 1;
		m_fJoystickAdvancedInit = 0;
		Msg("XInput gamepad found (slot %u).\\n", (unsigned)s_nXInputSlot);
		return;
	}

'''+anchor
    assert s.count(anchor)==1
    s=s.replace(anchor,replacement)
    anchor='''		m_rgAxes[JOY_AXIS_Y].AxisMap = AxisForward;
	}
	else
'''
    replacement='''		m_rgAxes[JOY_AXIS_Y].AxisMap = AxisForward;
		if ( s_bXInputActive )
		{
			// Left stick: movement. Right stick: independent camera axes.
			m_rgAxes[JOY_AXIS_X].AxisMap = AxisSide;
			m_rgAxes[JOY_AXIS_Z].AxisMap = AxisTurn;
			m_rgAxes[JOY_AXIS_R].AxisMap = AxisLook;
		}
	}
	else
'''
    assert s.count(anchor)==1
    s=s.replace(anchor,replacement)
    anchor='''			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
'''
    replacement='''			// Include diagonal hat/D-pad directions (45,135,225,315 deg).
			DWORD angle = ji.dwPOV % 36000;
			if (angle >= 31500 || angle <= 4500)
				povstate |= 0x01;
			if (angle >= 4500 && angle <= 13500)
				povstate |= 0x02;
			if (angle >= 13500 && angle <= 22500)
				povstate |= 0x04;
			if (angle >= 22500 && angle <= 31500)
				povstate |= 0x08;
'''
    assert s.count(anchor)==1
    s=s.replace(anchor,replacement)
    anchor='''	ji.dwFlags = (DWORD)m_nJoystickFlags;

	if (joyGetPosEx'''
    replacement='''	ji.dwFlags = (DWORD)m_nJoystickFlags;

	if ( s_bXInputActive )
	{
		XINPUT_STATE state;
		ZeroMemory( &state, sizeof(state) );
		DWORD result = s_pXInputGetState( s_nXInputSlot, &state );
		if ( result != ERROR_SUCCESS )
		{
			for ( DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot )
			{
				if ( slot == s_nXInputSlot ) continue;
				if ( s_pXInputGetState( slot, &state ) == ERROR_SUCCESS )
				{
					s_nXInputSlot = slot;
					result = ERROR_SUCCESS;
					break;
				}
			}
		}
		// Disconnection emits neutral input, releasing buttons instead of
		// leaving the previous controller state held forever.
		ji.dwXpos = ji.dwYpos = ji.dwZpos = ji.dwRpos = 32768;
		ji.dwUpos = ji.dwVpos = 32768;
		ji.dwPOV = JOY_POVCENTERED;
		if ( result == ERROR_SUCCESS )
			FillFromXInput( &ji, state.Gamepad );
		return 1;
	}

	if (joyGetPosEx'''
    assert s.count(anchor)==1
    s=s.replace(anchor,replacement)
    p.write_bytes(s.replace('\n','\r\n').encode('latin1'))
# Improve hotplug and camera input after initial integration.
s=p.read_bytes().decode('latin1').replace('\r\n','\n')
if 'HL2MODERN_XINPUT_HOTPLUG' not in s:
    anchor='\t// verify joystick is available and that the user wants to use it\n\tif (!m_fJoystickAvailable || !in_joystick.GetInt() )'
    new='\t// HL2MODERN_XINPUT_HOTPLUG: probe for controllers attached after startup.\n\tif ( !m_fJoystickAvailable && s_pXInputGetState && in_joystick.GetInt() )\n\t{\n\t\tfor ( DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot )\n\t\t{\n\t\t\tXINPUT_STATE state;\n\t\t\tZeroMemory( &state, sizeof(state) );\n\t\t\tif ( s_pXInputGetState( slot, &state ) == ERROR_SUCCESS )\n\t\t\t{\n\t\t\t\ts_nXInputSlot = slot;\n\t\t\t\ts_bXInputActive = true;\n\t\t\t\tm_nJoystickButtons = 12;\n\t\t\t\tm_fJoystickHasPOVControl = 1;\n\t\t\t\tm_nJoystickOldButtons = m_nJoystickOldPOVState = 0;\n\t\t\t\tm_fJoystickAvailable = 1;\n\t\t\t\tm_fJoystickAdvancedInit = 0;\n\t\t\t\tMsg("XInput gamepad connected (slot %u).\\n", (unsigned)slot);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t}\n\t}\n\t// verify joystick is available and that the user wants to use it\n\tif (!m_fJoystickAvailable || !in_joystick.GetInt() )'
    assert s.count(anchor)==1
    s=s.replace(anchor,new)
    old='\t\tcase AxisLook:\n\t\t\tif (in_jlook.state & 1)'
    new='\t\tcase AxisLook:\n\t\t\t// The right stick must look without requiring the legacy +jlook bind.\n\t\t\tif ((in_jlook.state & 1) || s_bXInputActive)'
    assert s.count(old)==1
    s=s.replace(old,new)
    p.write_bytes(s.replace('\n','\r\n').encode('latin1'))

print('XInput bridge ready',p)
