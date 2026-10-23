#!/usr/bin/env python3
"""Opt-in Steamworks init, callback pumping and teardown for the Windows client.

No third-party SDK binaries are acquired. The bridge only calls documented
Steamworks global entry points after loading a DLL from the executable folder.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'engine'/'sys_mainwind.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_STEAM_CLIENT_LIFECYCLE' not in s:
    anchor='#include "gl_matsysiface.h"\n'
    assert s.count(anchor)==1
    s=s.replace(anchor, anchor+'#include "vstdlib/ICommandLine.h"\n#include "../modernize/steam_client_bridge.h"\n')
    anchor='static CGame g_Game;\n'
    assert s.count(anchor)==1
    s=s.replace(anchor, '''// HL2MODERN_STEAM_CLIENT_LIFECYCLE
// Client-only, explicit opt-in. Steamworks belongs to this process's
// lifetime, NOT to map changes or multiplayer player connections.
static HL2ModernSteam::ClientBridge g_HL2ModernSteamClient;

'''+anchor)
    anchor='''	m_hInstance = (HINSTANCE)pvInstance;

	m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );'''
    assert s.count(anchor)==1
    s=s.replace(anchor, '''	m_hInstance = (HINSTANCE)pvInstance;

	m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );''')
    anchor='''#ifndef SWDS
	thread->Init();
#endif
	// Create engine window, etc.'''
    assert s.count(anchor)==1
    s=s.replace(anchor, '''#ifndef SWDS
	// The owner must provide a real App ID (Steam launch or development ID).
	// No restart through Steam is attempted by this experimental integration.
	if ( CommandLine()->FindParm( "-steamworks" ) &&
	     !CommandLine()->FindParm( "-nosteamworks" ) )
	{
		if ( g_HL2ModernSteamClient.Start() )
			Msg( "Steamworks bootstrap initialized.\\n" );
		else
			Warning( "Steamworks bootstrap unavailable (steam_api.dll, client, or App ID). Continuing offline.\\n" );
	}
	thread->Init();
#endif
	// Create engine window, etc.''')
    anchor='''bool CGame::Shutdown( void )
{
#ifndef SWDS'''
    assert s.count(anchor)==1
    s=s.replace(anchor, '''bool CGame::Shutdown( void )
{
#ifndef SWDS
	g_HL2ModernSteamClient.Stop();''')
    anchor='''void CGame::DispatchAllStoredGameMessages()
{
	if ( g_pVCR->GetMode() == VCR_Playback )'''
    assert s.count(anchor)==1
    s=s.replace(anchor, '''void CGame::DispatchAllStoredGameMessages()
{
#ifndef SWDS
	// Process Steam callbacks on the same thread that drives game messages.
	g_HL2ModernSteamClient.Pump();
#endif
	if ( g_pVCR->GetMode() == VCR_Playback )''')
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Opt-in client Steamworks lifecycle bootstrap installed')
