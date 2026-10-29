// A standalone VGUI/MaterialSystem tool. No engine or client DLL is loaded.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "appframework/VguiMatSysApp.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier1/utlbuffer.h"
#include "tier3/tier3.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/ISystem.h"
#include "vgui/IScheme.h"
#include "vgui/IPanel.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/AnimationController.h"
#include "matsys_controls/matsyscontrols.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "gamma_launcher_ui.h"
#include "gamma_launcher_verify.h"
#include "gamma_launcher_commandline.h"
#include "../game/shared/gamma/gamma_player_characters.h"

static std::wstring g_GameDirectory;
static std::vector<std::wstring> g_GameOptions;
static std::wstring g_VerifyOutput;
static bool g_VerifyPlay = false;
static CGammaLauncherVerification *g_Verification = NULL;
static HANDLE g_VerifyLog = INVALID_HANDLE_VALUE;

static SpewRetval_t VerificationSpew( SpewType_t type, const char *message )
{
	if ( g_VerifyLog != INVALID_HANDLE_VALUE )
	{
		DWORD written;
		WriteFile( g_VerifyLog, message, static_cast<DWORD>( strlen( message ) ), &written, NULL );
	}
	OutputDebugStringA( message );
	return type == SPEW_ERROR ? SPEW_ABORT : SPEW_CONTINUE;
}

static std::string ToUTF8( const std::wstring &text )
{
	if ( text.empty() ) return std::string();
	int size = WideCharToMultiByte( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), NULL, 0, NULL, NULL );
	std::string result( size, '\0' );
	WideCharToMultiByte( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), &result[0], size, NULL, NULL );
	return result;
}

static std::wstring FromUTF8( const char *text )
{
	int size = MultiByteToWideChar( CP_UTF8, 0, text, -1, NULL, 0 );
	if ( size < 1 ) return std::wstring();
	std::vector<wchar_t> result( size );
	MultiByteToWideChar( CP_UTF8, 0, text, -1, result.data(), size );
	return std::wstring( result.data() );
}

static void CopyProfileName( char *output, int bytes, const char *name )
{
	int used = 0;
	for ( const unsigned char *p = reinterpret_cast<const unsigned char *>( name ); *p && used < bytes - 1; ++p )
	{
		// Source's startup command buffer must not interpret name text as a new
		// console command. Ordinary UTF-8 characters remain unchanged.
		if ( *p < 32 || *p == '"' || *p == '\\' || *p == ';' ) continue;
		output[used++] = *p;
	}
	output[used] = 0;
	if ( !used ) Q_strncpy( output, "Player", bytes );
}

static void LoadProfile( GammaLauncherProfile &profile )
{
	Q_strncpy( profile.playerName, "Player", sizeof( profile.playerName ) );
	Q_strncpy( profile.playerModel, "gordon", sizeof( profile.playerModel ) );
	profile.thirdPerson = false;
	CUtlBuffer config( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( g_pFullFileSystem->ReadFile( "cfg/config.cfg", "MOD", config ) )
	{
		char line[1024];
		while ( config.IsValid() && config.GetBytesRemaining() )
		{
			config.GetLine( line, sizeof( line ) );
			CCommand command;
			if ( !command.Tokenize( line ) || command.ArgC() < 2 ) continue;
			if ( !Q_stricmp( command[0], "name" ) ) CopyProfileName( profile.playerName, sizeof( profile.playerName ), command[1] );
			if ( !Q_stricmp( command[0], "gamma_player_model" ) ) Q_strncpy( profile.playerModel, command[1], sizeof( profile.playerModel ) );
			if ( !Q_stricmp( command[0], "cl_thirdperson" ) ) profile.thirdPerson = atoi( command[1] ) != 0;
		}
	}
	KeyValues *saved = new KeyValues( "GammaLauncherProfile" );
	if ( saved->LoadFromFile( g_pFullFileSystem, "cfg/gamma_launcher.vdf", "MOD" ) )
	{
		CopyProfileName( profile.playerName, sizeof( profile.playerName ), saved->GetString( "name", profile.playerName ) );
		Q_strncpy( profile.playerModel, saved->GetString( "model", profile.playerModel ), sizeof( profile.playerModel ) );
		profile.thirdPerson = saved->GetBool( "thirdperson", profile.thirdPerson );
	}
	saved->deleteThis();
	bool valid = false;
	for ( int i = 0; i < ARRAYSIZE( g_GammaPlayerCharacters ); ++i )
		valid |= !Q_stricmp( profile.playerModel, g_GammaPlayerCharacters[i].alias );
	if ( !valid ) Q_strncpy( profile.playerModel, "gordon", sizeof( profile.playerModel ) );
}

static bool PlayGame( const GammaLauncherProfile &selected, char *error, int errorBytes, void * )
{
	GammaLauncherProfile profile = selected;
	CopyProfileName( profile.playerName, sizeof( profile.playerName ), selected.playerName );
	bool valid = false;
	for ( int i = 0; i < ARRAYSIZE( g_GammaPlayerCharacters ); ++i )
		valid |= !Q_stricmp( profile.playerModel, g_GammaPlayerCharacters[i].alias );
	if ( !valid ) { Q_strncpy( error, "Choose a player model.", errorBytes ); if ( g_Verification ) g_Verification->RecordLaunchError( error ); return false; }
	KeyValues *saved = new KeyValues( "GammaLauncherProfile" );
	saved->SetString( "name", profile.playerName );
	saved->SetString( "model", profile.playerModel );
	saved->SetBool( "thirdperson", profile.thirdPerson );
	g_pFullFileSystem->CreateDirHierarchy( "cfg", "MOD" );
	bool wrote = saved->SaveToFile( g_pFullFileSystem, "cfg/gamma_launcher.vdf", "MOD" );
	saved->deleteThis();
	if ( !wrote ) { Q_strncpy( error, "The gammasource folder is not writable. Your choices could not be saved.", errorBytes ); if ( g_Verification ) g_Verification->RecordLaunchError( error ); return false; }
	const std::wstring engine = g_GameDirectory + L"\\hl2_launcher.exe";
	std::vector<std::wstring> args;
	args.push_back( engine ); args.push_back( L"-game" ); args.push_back( L"gammasource" ); args.push_back( L"-novid" );
	args.insert( args.end(), g_GameOptions.begin(), g_GameOptions.end() );
	args.push_back( L"+name" ); args.push_back( FromUTF8( profile.playerName ) );
	args.push_back( L"+gamma_player_model" ); args.push_back( FromUTF8( profile.playerModel ) );
	args.push_back( L"+cl_thirdperson" ); args.push_back( profile.thirdPerson ? L"1" : L"0" );
	const std::wstring line = GammaJoinArguments( args );
	if ( line.size() >= 32767 ) { Q_strncpy( error, "There are too many launch options.", errorBytes ); if ( g_Verification ) g_Verification->RecordLaunchError( error ); return false; }
	std::vector<wchar_t> buffer( line.begin(), line.end() ); buffer.push_back( 0 );
	STARTUPINFOW startup = {}; startup.cb = sizeof( startup );
	startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_SHOWNORMAL;
	PROCESS_INFORMATION process = {};
	if ( !CreateProcessW( engine.c_str(), buffer.data(), NULL, NULL, FALSE, 0, NULL,
		g_GameDirectory.c_str(), &startup, &process ) )
	{
		Q_snprintf( error, errorBytes, "The game could not start (Windows error %lu).", GetLastError() );
		if ( g_Verification ) g_Verification->RecordLaunchError( error );
		return false;
	}
	if ( g_Verification ) g_Verification->RecordLaunch( process.dwProcessId, line, profile );
	CloseHandle( process.hThread ); CloseHandle( process.hProcess );
	return true;
}

static void ExitLauncher( void * ) { if ( g_pVGui ) g_pVGui->Stop(); }

class CGammaLauncherApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;
public:
	virtual bool Create()
	{
		if ( !BaseClass::Create() ) return false;
		AppSystemInfo_t systems[] = {
			{ "studiorender.dll", STUDIO_RENDER_INTERFACE_VERSION },
			{ "vphysics.dll", VPHYSICS_INTERFACE_VERSION },
			{ "datacache.dll", DATACACHE_INTERFACE_VERSION },
			{ "datacache.dll", MDLCACHE_INTERFACE_VERSION }, { "", "" }
		};
		return AddSystems( systems );
	}
	virtual bool PreInit()
	{
		MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
		if ( !BaseClass::PreInit() ) return false;
		CreateInterfaceFn factory = GetFactory();
		return vgui::VGui_InitMatSysInterfacesList( "GammaSourceLauncher", &factory, 1 );
	}
	virtual int Main()
	{
		g_pMaterialSystem->ModInit();
		if ( !SetVideoMode() ) return 1;
		g_pDataCache->SetSize( 128 * 1024 * 1024 );
		StudioRenderConfig_t studio = {};
		studio.bEyeMove = true; studio.bTeeth = true; studio.bEyes = true;
		studio.bFlex = true; studio.drawEntities = 1; studio.maxDecalsPerModel = 50;
		g_pStudioRender->UpdateConfig( studio );
		vgui::system()->SetUserConfigFile( "cfg/gamma_launcher_ui.vdf", "MOD" );
		g_pVGuiLocalize->AddFile( "resource/valve_%language%.txt" );
		g_pVGuiLocalize->AddFile( "resource/vgui_%language%.txt" );
		CMatRenderContextPtr context( g_pMaterialSystem );
		// The tools surface derives its embedded-panel size and clipping bounds
		// from the active viewport. SetMode alone does not initialize that state.
		context->Viewport( 0, 0, GetWindowWidth(), GetWindowHeight() );
		vgui::VPANEL root = g_pVGuiSurface->GetEmbeddedPanel();
		g_pVGuiPanel->SetSize( root, GetWindowWidth(), GetWindowHeight() );
		g_pVGuiPanel->SetVisible( root, true );
		g_pVGui->Start();
		g_pVGui->RunFrame();
		GammaLauncherProfile profile; LoadProfile( profile );
		vgui::Panel *panel = GammaCreateLauncherUI( root, profile, PlayGame, ExitLauncher, this );
		GammaResizeLauncherUI( panel, GetWindowWidth(), GetWindowHeight() );
		ITexture *cubemap = g_pMaterialSystem->FindTexture( "editor/cubemap", TEXTURE_GROUP_CUBE_MAP, true );
		context->BindLocalCubemap( cubemap );
		g_pVGuiSurface->Invalidate( root );
		double last = Plat_FloatTime();
		CGammaLauncherVerification verification( g_VerifyOutput, g_VerifyPlay, GetWindowWidth(), GetWindowHeight() );
		g_Verification = &verification;
		if ( !verification.Begin( panel, last ) ) g_pVGui->Stop();
		while ( g_pVGui->IsRunning() )
		{
			const double now = Plat_FloatTime();
			AppPumpMessages();
			vgui::GetAnimationController()->UpdateAnimations( now );
			g_pMaterialSystem->BeginFrame( static_cast<float>( now - last ) );
			g_pStudioRender->BeginFrame();
			context->Viewport( 0, 0, GetWindowWidth(), GetWindowHeight() );
			root = g_pVGuiSurface->GetEmbeddedPanel();
			g_pVGuiPanel->SetSize( root, GetWindowWidth(), GetWindowHeight() );
			g_pVGuiPanel->SetVisible( root, true );
			context->ClearColor4ub( 76, 88, 68, 255 ); context->ClearBuffers( true, true );
			g_pVGui->RunFrame();
			g_pVGuiSurface->PaintTraverseEx( root, true );
			if ( !g_VerifyOutput.empty() ) g_pVGuiSurface->PaintTraverseEx( panel->GetVPanel(), false );
			verification.AfterPaint( context, panel, now );
			g_pStudioRender->EndFrame(); g_pMaterialSystem->EndFrame(); g_pMaterialSystem->SwapBuffers();
			last = now;
			Sleep( 8 );
		}
		verification.Closed();
		g_Verification = NULL;
		delete panel;
		context->BindLocalCubemap( NULL );
		g_pMaterialSystem->ModShutdown();
		return verification.ExitCode();
	}
private:
	virtual const char *GetAppName() { return "GammaSource Launcher"; }
	virtual bool AppUsesReadPixels() { return !g_VerifyOutput.empty(); }
	static LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wparam, LPARAM lparam )
	{
		if ( message == WM_CLOSE ) { ExitLauncher( NULL ); return 0; }
		if ( message == WM_NCHITTEST )
		{
			POINT point = { static_cast<short>( LOWORD( lparam ) ), static_cast<short>( HIWORD( lparam ) ) };
			ScreenToClient( window, &point ); RECT bounds; GetClientRect( window, &bounds );
			if ( point.y >= 0 && point.y < 28 && point.x >= 20 && point.x < bounds.right - 60 ) return HTCAPTION;
		}
		return DefWindowProcW( window, message, wparam, lparam );
	}
	virtual void *CreateAppWindow( const char *, bool, int wide, int tall )
	{
		WNDCLASSEXW type = {}; type.cbSize = sizeof( type ); type.style = CS_OWNDC | CS_DBLCLKS;
		type.lpfnWndProc = WindowProc; type.hInstance = static_cast<HINSTANCE>( GetAppInstance() );
		type.hCursor = LoadCursor( NULL, IDC_ARROW ); type.hIcon = LoadIcon( type.hInstance, MAKEINTRESOURCE( 1 ) );
		type.lpszClassName = L"GammaSourceStandaloneLauncher";
		RegisterClassExW( &type );
		RECT area; SystemParametersInfoW( SPI_GETWORKAREA, 0, &area, 0 );
		HWND window = CreateWindowExW( 0, type.lpszClassName, L"GammaSource Launcher", WS_POPUP | WS_CLIPSIBLINGS,
			area.left + ( area.right - area.left - wide ) / 2, area.top + ( area.bottom - area.top - tall ) / 2,
			wide, tall, NULL, NULL, type.hInstance, NULL );
		ShowWindow( window, SW_SHOWNORMAL ); UpdateWindow( window );
		return window;
	}
};

extern "C" __declspec( dllexport ) int GammaLauncherMain( HINSTANCE instance )
{
	wchar_t path[32768]; DWORD length = GetModuleFileNameW( NULL, path, ARRAYSIZE( path ) );
	if ( !length || length >= ARRAYSIZE( path ) ) return 1;
	g_GameDirectory.assign( path, length );
	g_GameDirectory.resize( g_GameDirectory.find_last_of( L"\\/" ) );
	int count = 0; wchar_t **arguments = CommandLineToArgvW( GetCommandLineW(), &count );
	if ( arguments )
	{
		for ( int i = 1; i < count; ++i )
		{
			if ( !_wcsicmp( arguments[i], L"-launcher_verify" ) && i + 1 < count ) { g_VerifyOutput = arguments[++i]; continue; }
			if ( !_wcsicmp( arguments[i], L"-launcher_verify_play" ) ) { g_VerifyPlay = true; continue; }
			g_GameOptions.push_back( arguments[i] );
		}
		LocalFree( arguments );
	}
	SetAppInstance( instance );
	if ( !g_VerifyOutput.empty() )
	{
		CreateDirectoryW( g_VerifyOutput.c_str(), NULL );
		g_VerifyLog = CreateFileW( ( g_VerifyOutput + L"\\launcher_debug.log" ).c_str(), FILE_APPEND_DATA,
			FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		SpewOutputFunc( VerificationSpew );
	}
	CommandLine()->CreateCmdLine( GetCommandLineA() );
	CommandLine()->RemoveParm( "-game" );
	CommandLine()->AppendParm( "-game", ToUTF8( g_GameDirectory + L"\\gammasource" ).c_str() );
	CommandLine()->AppendParm( "-width", "800" ); CommandLine()->AppendParm( "-height", "580" );
	CGammaLauncherApp app;
	CSteamApplication bootstrap( &app );
	return bootstrap.Run();
}
