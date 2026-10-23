#!/usr/bin/env python3
"""Prevent sticky XInput buttons and provide configurable radial deadzones."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'cl_dll'/'in_joystick.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_XINPUT_DEADZONE' not in s:
    anchor='#include <Xinput.h> // Windows SDK: XInput gamepads\n'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'#include "../modernize/controller_axis_policy.h"\n')
    anchor='static void FillFromXInput( JOYINFOEX *out, const XINPUT_GAMEPAD &pad )\n{'
    assert s.count(anchor)==1
    s=s.replace(anchor,'''// HL2MODERN_XINPUT_DEADZONE
// Set these to zero for raw stick axes; ranges are clamped to 0..30000.
static ConVar joy_xinput_left_deadzone("joy_xinput_left_deadzone", "7849", 0,
    "Left stick radial XInput deadzone (0-30000).");
static ConVar joy_xinput_right_deadzone("joy_xinput_right_deadzone", "8689", 0,
    "Right stick radial XInput deadzone (0-30000).");

static void FillFromXInput( JOYINFOEX *out, const XINPUT_GAMEPAD &pad )
{''')
    old='''    out->dwXpos = ToJoyAxis( pad.sThumbLX, true );
    out->dwYpos = ToJoyAxis( pad.sThumbLY, true );
    out->dwZpos = ToJoyAxis( pad.sThumbRX, false );
    out->dwRpos = ToJoyAxis( pad.sThumbRY, true );'''
    new='''    int lx = pad.sThumbLX, ly = pad.sThumbLY;
    int rx = pad.sThumbRX, ry = pad.sThumbRY;
    HL2ModernController::RadialDeadzone(lx, ly, joy_xinput_left_deadzone.GetInt());
    HL2ModernController::RadialDeadzone(rx, ry, joy_xinput_right_deadzone.GetInt());
    out->dwXpos = ToJoyAxis( static_cast<SHORT>(lx), true );
    out->dwYpos = ToJoyAxis( static_cast<SHORT>(ly), true );
    out->dwZpos = ToJoyAxis( static_cast<SHORT>(rx), false );
    out->dwRpos = ToJoyAxis( static_cast<SHORT>(ry), true );'''
    assert s.count(old)==1
    s=s.replace(old,new)
    anchor='''		ji.dwPOV = JOY_POVCENTERED;
		if ( result == ERROR_SUCCESS )'''
    assert s.count(anchor)==1
    s=s.replace(anchor,'''\t\tji.dwPOV = JOY_POVCENTERED;
		ji.dwButtons = 0; // button bitmask must never persist across frames/disconnects
		if ( result == ERROR_SUCCESS )'''.replace('\\t','\t'))
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('XInput deadzone tuning and sticky-button fix ready')
