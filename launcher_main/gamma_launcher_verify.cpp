#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sstream>
#include <cmath>
#include "gamma_launcher_verify.h"
#include "../game/shared/gamma/gamma_player_characters.h"
#include "materialsystem/imaterialsystem.h"
#include "tier1/strtools.h"
#include "vgui/IVGui.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Panel.h"
#include "tier3/tier3.h"

static std::string VerifyUTF8( const std::wstring &text )
{
	if ( text.empty() ) return std::string();
	int bytes = WideCharToMultiByte( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), NULL, 0, NULL, NULL );
	std::string result( bytes, 0 );
	WideCharToMultiByte( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), &result[0], bytes, NULL, NULL );
	return result;
}

static std::string JSON( const std::string &text )
{
	std::string result( "\"" );
	for ( size_t i = 0; i < text.size(); ++i )
	{
		const unsigned char c = text[i];
		if ( c == '"' || c == '\\' ) { result.push_back( '\\' ); result.push_back( c ); }
		else if ( c < 32 ) { char escaped[7]; Q_snprintf( escaped, sizeof( escaped ), "\\u%04x", c ); result += escaped; }
		else result.push_back( c );
	}
	result.push_back( '"' ); return result;
}

static bool WriteVerificationFile( const std::wstring &path, const void *data, size_t bytes )
{
	HANDLE file = CreateFileW( path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( file == INVALID_HANDLE_VALUE ) return false;
	DWORD written = 0;
	bool result = bytes <= MAXDWORD && WriteFile( file, data, static_cast<DWORD>( bytes ), &written, NULL ) && written == bytes;
	CloseHandle( file ); return result;
}

static bool CreateVerificationDirectory( const std::wstring &path )
{
	const bool drive = path.size() > 2 && path[1] == L':' && ( path[2] == L'\\' || path[2] == L'/' );
	const bool unc = path.size() > 2 && path[0] == L'\\' && path[1] == L'\\';
	if ( !drive && !unc ) return false;
	for ( size_t i = drive ? 3 : 2; i <= path.size(); ++i )
	{
		if ( i != path.size() && path[i] != L'\\' && path[i] != L'/' ) continue;
		std::wstring part = path.substr( 0, i );
		if ( !CreateDirectoryW( part.c_str(), NULL ) && GetLastError() != ERROR_ALREADY_EXISTS )
		{
			DWORD attributes = GetFileAttributesW( part.c_str() );
			if ( attributes == INVALID_FILE_ATTRIBUTES || !( attributes & FILE_ATTRIBUTE_DIRECTORY ) ) return false;
		}
	}
	return true;
}

static std::string ModelName( const char *input )
{
	std::string name( input );
	for ( size_t i = 0; i < name.size(); ++i )
	{
		if ( name[i] == '\\' ) name[i] = '/';
		else if ( name[i] >= 'A' && name[i] <= 'Z' ) name[i] += 'a' - 'A';
	}
	if ( name.compare( 0, 7, "models/" ) == 0 ) name.erase( 0, 7 );
	return name;
}

CGammaLauncherVerification::CGammaLauncherVerification( const std::wstring &output, bool play, int wide, int tall ) :
	m_Output( output ), m_Play( play ), m_Passed( true ), m_Completed( false ),
	m_EngineAbsent( true ), m_ClientAbsent( true ), m_ServerAbsent( true ),
	m_PlayInvoked( false ), m_PlayFinished( false ), m_CanvasWide( wide ), m_CanvasTall( tall ),
	m_Index( 0 ), m_Phase( 0 ), m_ChangedAt( 0 ), m_FirstAt( 0 ), m_Process( 0 )
{
	memset( &m_LaunchProfile, 0, sizeof( m_LaunchProfile ) );
}

void CGammaLauncherVerification::CheckModules()
{
	m_EngineAbsent &= GetModuleHandleW( L"engine.dll" ) == NULL;
	m_ClientAbsent &= GetModuleHandleW( L"client.dll" ) == NULL;
	m_ServerAbsent &= GetModuleHandleW( L"server.dll" ) == NULL;
	if ( !m_EngineAbsent || !m_ClientAbsent || !m_ServerAbsent ) m_Passed = false;
}

bool CGammaLauncherVerification::Begin( vgui::Panel *panel, double now )
{
	if ( !Enabled() ) return true;
	if ( !CreateVerificationDirectory( m_Output ) )
	{
		OutputDebugStringA( "Launcher verification output must be an absolute writable directory.\n" );
		m_Passed = false; return false;
	}
	CheckModules();
	GammaLauncherProfile profile = {};
	Q_strncpy( profile.playerName, "Gamma Launcher Test", sizeof( profile.playerName ) );
	Q_strncpy( profile.playerModel, g_GammaPlayerCharacters[0].alias, sizeof( profile.playerModel ) );
	profile.thirdPerson = true;
	if ( !GammaLauncherVerifyProfileSet( panel, profile ) )
	{
		m_Errors.push_back( "Could not set the profile through the launcher controls." ); m_Passed = false;
	}
	m_ChangedAt = now; SaveReport(); return true;
}

bool CGammaLauncherVerification::Capture( IMatRenderContext *context, vgui::Panel *panel, const std::string &name, Sample &sample )
{
	memset( &sample.state, 0, sizeof( sample.state ) );
	if ( !GammaLauncherVerifyReport( panel, sample.state ) ) return false;
	context->GetViewport( sample.viewportX, sample.viewportY, sample.viewportWide, sample.viewportTall );
	vgui::VPANEL root = g_pVGuiSurface->GetEmbeddedPanel();
	g_pVGuiPanel->GetSize( root, sample.rootWide, sample.rootTall );
	sample.rootVisible = g_pVGuiPanel->IsVisible( root );
	int panelX, panelY, panelWide, panelTall, clipLeft, clipTop, clipRight, clipBottom;
	panel->GetBounds( panelX, panelY, panelWide, panelTall );
	panel->GetClipRect( clipLeft, clipTop, clipRight, clipBottom );
	char diagnostic[512];
	Q_snprintf( diagnostic, sizeof( diagnostic ),
		"panel visible=%d fullyVisible=%d alpha=%d popup=%d popups=%d bounds=%d,%d,%d,%d clip=%d,%d,%d,%d rootChildren=%d root=%dx%d viewport=%dx%d\n",
		panel->IsVisible(), g_pVGuiPanel->IsFullyVisible( panel->GetVPanel() ), panel->GetAlpha(),
		g_pVGuiPanel->IsPopup( panel->GetVPanel() ), g_pVGuiSurface->GetPopupCount(), panelX, panelY, panelWide, panelTall,
		clipLeft, clipTop, clipRight, clipBottom, g_pVGuiPanel->GetChildCount( root ), sample.rootWide, sample.rootTall,
		sample.viewportWide, sample.viewportTall );
	WriteVerificationFile( m_Output + L"\\panel_diagnostic.txt", diagnostic, strlen( diagnostic ) );
	HMODULE surfaceModule = NULL;
	wchar_t surfacePath[MAX_PATH] = {};
	GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>( *reinterpret_cast<void ***>( g_pVGuiSurface ) ), &surfaceModule );
	GetModuleFileNameW( surfaceModule, surfacePath, ARRAYSIZE( surfacePath ) );
	CMatRenderContextPtr currentContext( g_pMaterialSystem );
	std::ostringstream renderer;
	renderer << "surface=" << VerifyUTF8( surfacePath ) << " threadMode=" << g_pMaterialSystem->GetThreadMode()
		<< " context=" << context << " currentContext=" << static_cast<IMatRenderContext *>( currentContext ) << "\n";
	const std::string rendererText = renderer.str();
	WriteVerificationFile( m_Output + L"\\renderer_diagnostic.txt", rendererText.data(), rendererText.size() );
	g_pMaterialSystem->GetBackBufferDimensions( sample.wide, sample.tall );
	if ( sample.wide < 1 || sample.tall < 1 || sample.wide > 8192 || sample.tall > 8192 ) return false;
	std::vector<unsigned char> image( 18 + static_cast<size_t>( sample.wide ) * sample.tall * 4, 0 );
	image[2] = 2; image[12] = sample.wide & 255; image[13] = ( sample.wide >> 8 ) & 255;
	image[14] = sample.tall & 255; image[15] = ( sample.tall >> 8 ) & 255; image[16] = 32; image[17] = 0x28;
	unsigned char *pixels = image.data() + 18;
	// This reads only our application's own completed render target, not a
	// desktop/window capture. Opaque alpha makes BGRX backbuffers valid TGAs.
	context->ReadPixels( 0, 0, sample.wide, sample.tall, pixels, IMAGE_FORMAT_BGRA8888 );
	for ( size_t i = 3; i < image.size() - 18; i += 4 ) pixels[i] = 255;
	sample.file = name + ".tga";
	const std::wstring file( sample.file.begin(), sample.file.end() );
	if ( !WriteVerificationFile( m_Output + L"\\" + file, image.data(), image.size() ) ) return false;
	int left = sample.state.previewX * sample.wide / m_CanvasWide;
	int top = sample.state.previewY * sample.tall / m_CanvasTall;
	int right = ( sample.state.previewX + sample.state.previewWide ) * sample.wide / m_CanvasWide;
	int bottom = ( sample.state.previewY + sample.state.previewTall ) * sample.tall / m_CanvasTall;
	left = clamp( left, 0, sample.wide ); right = clamp( right, 0, sample.wide );
	top = clamp( top, 0, sample.tall ); bottom = clamp( bottom, 0, sample.tall );
	if ( right <= left || bottom <= top ) return false;
	unsigned long long hash = 14695981039346656037ULL;
	for ( int y = top; y < bottom; ++y )
		for ( int x = left; x < right; ++x )
			for ( int component = 0; component < 3; ++component )
			{
				hash ^= pixels[( static_cast<size_t>( y ) * sample.wide + x ) * 4 + component];
				hash *= 1099511628211ULL;
			}
	char text[32]; Q_snprintf( text, sizeof( text ), "%016llx", hash ); sample.pixelHash = text;
	return true;
}

void CGammaLauncherVerification::AfterPaint( IMatRenderContext *context, vgui::Panel *panel, double now )
{
	if ( !Enabled() || m_Completed ) return;
	CheckModules();
	if ( m_Index >= ARRAYSIZE( g_GammaPlayerCharacters ) ) { Complete( context, panel, now ); return; }
	if ( now - m_ChangedAt < 0.9 ) return;
	if ( m_Phase == 1 && now - m_FirstAt < 0.6 ) return;
	const GammaPlayerCharacter &character = g_GammaPlayerCharacters[m_Index];
	char base[128]; Q_snprintf( base, sizeof( base ), "launcher_%02d_%s", m_Index, character.alias );
	if ( m_Phase == 0 )
	{
		if ( !Capture( context, panel, std::string( base ) + "_a", m_First ) )
		{
			m_Errors.push_back( std::string( "Render readback failed for " ) + character.alias ); m_Passed = false;
		}
		m_FirstAt = now; m_Phase = 1; return;
	}
	Record record; record.alias = character.alias; record.first = m_First;
	record.passed = Capture( context, panel, std::string( base ) + "_b", record.second );
	const GammaLauncherVerificationState &a = record.first.state, &b = record.second.state;
	if ( !a.previewAvailable || !b.previewAvailable ) record.error += "Preview asset or animation did not load. ";
	if ( ModelName( b.loadedModel ) != ModelName( character.model ) || Q_stricmp( b.profile.playerModel, character.alias ) ) record.error += "Dropdown/model mismatch. ";
	if ( Q_stricmp( b.sequence, "gamma_neutral_idle_subtle" ) ) record.error += "Neutral sequence not active. ";
	if ( b.boneCount <= 0 ) record.error += "Model has no bones. ";
	if ( !( std::fabs( b.animationTime - a.animationTime ) > 0.05f ) ) record.error += "Animation clock did not advance. ";
	if ( record.first.pixelHash == record.second.pixelHash ) record.error += "Preview pixels did not animate between frames. ";
	record.passed &= record.error.empty(); m_Passed &= record.passed;
	m_Records.push_back( record );
	++m_Index; m_Phase = 0; m_ChangedAt = now;
	if ( m_Index < ARRAYSIZE( g_GammaPlayerCharacters ) && !GammaLauncherVerifySelect( panel, g_GammaPlayerCharacters[m_Index].alias ) )
	{
		m_Errors.push_back( "Dropdown could not select the next character." ); m_Passed = false;
	}
	SaveReport();
}

void CGammaLauncherVerification::Complete( IMatRenderContext *context, vgui::Panel *panel, double now )
{
	if ( !m_Play )
	{
		m_Completed = true; SaveReport(); g_pVGui->Stop(); return;
	}
	if ( !m_PlayInvoked )
	{
		GammaLauncherProfile profile = {};
		Q_strncpy( profile.playerName, "Gamma Launcher Test", sizeof( profile.playerName ) );
		Q_strncpy( profile.playerModel, "chell_2010", sizeof( profile.playerModel ) ); profile.thirdPerson = true;
		if ( !GammaLauncherVerifyProfileSet( panel, profile ) )
		{
			m_Errors.push_back( "Could not set the Play test profile." ); m_Passed = false;
		}
		m_ChangedAt = now; m_PlayInvoked = true; SaveReport(); return;
	}
	if ( now - m_ChangedAt < 0.9 ) return;
	Sample sample; if ( !Capture( context, panel, "launcher_play_profile", sample ) )
		m_Errors.push_back( "Play profile frame could not be captured." );
	CheckModules();
	GammaLauncherVerifyPlay( panel );
	m_PlayFinished = true; m_Completed = true;
	if ( !m_Process ) { m_Errors.push_back( "Play did not create the game process." ); m_Passed = false; }
	SaveReport(); g_pVGui->Stop();
}

void CGammaLauncherVerification::RecordLaunch( unsigned long process, const std::wstring &command, const GammaLauncherProfile &profile )
{
	if ( !Enabled() ) return;
	m_Process = process; m_Command = command; m_LaunchProfile = profile; SaveReport();
}

void CGammaLauncherVerification::RecordLaunchError( const char *error )
{
	if ( Enabled() ) { m_Errors.push_back( error ); m_Passed = false; SaveReport(); }
}

void CGammaLauncherVerification::Closed()
{
	if ( !Enabled() ) return;
	if ( !m_Completed ) { m_Errors.push_back( "Launcher closed before verification completed." ); m_Passed = false; }
	SaveReport();
}

int CGammaLauncherVerification::ExitCode() const { return Enabled() && ( !m_Completed || !m_Passed ) ? 2 : 0; }

void CGammaLauncherVerification::SaveReport()
{
	if ( !Enabled() ) return;
	std::ostringstream json;
	json << "{\n  \"mode\":\"own_renderer_component_integration\",\n  \"completed\":" << ( m_Completed ? "true" : "false" )
		<< ",\n  \"passed\":" << ( m_Completed && m_Passed ? "true" : "false" )
		<< ",\n  \"engine_absent_before_play\":" << ( m_EngineAbsent ? "true" : "false" )
		<< ",\n  \"client_absent_before_play\":" << ( m_ClientAbsent ? "true" : "false" )
		<< ",\n  \"server_absent_before_play\":" << ( m_ServerAbsent ? "true" : "false" )
		<< ",\n  \"play_requested\":" << ( m_Play ? "true" : "false" )
		<< ",\n  \"play_invoked\":" << ( m_PlayFinished ? "true" : "false" )
		<< ",\n  \"game_process_id\":" << m_Process << ",\n  \"game_command_line\":" << JSON( VerifyUTF8( m_Command ) )
		<< ",\n  \"launch_name\":" << JSON( m_LaunchProfile.playerName ) << ",\n  \"launch_model\":" << JSON( m_LaunchProfile.playerModel )
		<< ",\n  \"launch_third_person\":" << ( m_LaunchProfile.thirdPerson ? "true" : "false" ) << ",\n  \"models\":[\n";
	for ( size_t i = 0; i < m_Records.size(); ++i )
	{
		const Record &r = m_Records[i];
		json << ( i ? ",\n" : "" ) << "    {\"alias\":" << JSON( r.alias ) << ",\"passed\":" << ( r.passed ? "true" : "false" )
			<< ",\"error\":" << JSON( r.error ) << ",\"model\":" << JSON( r.second.state.loadedModel )
			<< ",\"sequence\":" << JSON( r.second.state.sequence ) << ",\"bones\":" << r.second.state.boneCount
			<< ",\"dropdown_index\":" << r.second.state.dropdownIndex << ",\"selected_alias\":" << JSON( r.second.state.profile.playerModel )
			<< ",\"entered_name\":" << JSON( r.second.state.profile.playerName ) << ",\"third_person\":" << ( r.second.state.profile.thirdPerson ? "true" : "false" )
			<< ",\"preview_available\":" << ( r.second.state.previewAvailable ? "true" : "false" )
			<< ",\"animation_time_a\":" << r.first.state.animationTime << ",\"animation_time_b\":" << r.second.state.animationTime
			<< ",\"preview_hash_a\":" << JSON( r.first.pixelHash ) << ",\"preview_hash_b\":" << JSON( r.second.pixelHash )
			<< ",\"image_a\":" << JSON( r.first.file ) << ",\"image_b\":" << JSON( r.second.file )
			<< ",\"width\":" << r.second.wide << ",\"height\":" << r.second.tall
			<< ",\"viewport_x\":" << r.second.viewportX << ",\"viewport_y\":" << r.second.viewportY
			<< ",\"viewport_width\":" << r.second.viewportWide << ",\"viewport_height\":" << r.second.viewportTall
			<< ",\"root_width\":" << r.second.rootWide << ",\"root_height\":" << r.second.rootTall
			<< ",\"root_visible\":" << ( r.second.rootVisible ? "true" : "false" )
			<< ",\"frame_paints\":" << r.second.state.framePaintCount << ",\"preview_paints\":" << r.second.state.previewPaintCount
			<< ",\"preview_visible\":" << r.second.state.previewVisible << ",\"preview_fully_visible\":" << r.second.state.previewFullyVisible
			<< ",\"preview_alpha\":" << r.second.state.previewAlpha << ",\"frame_bg_alpha\":" << r.second.state.frameBgAlpha
			<< ",\"surface_alpha\":" << r.second.state.surfaceAlpha << ",\"scheme_handle\":" << r.second.state.schemeHandle
			<< ",\"preview_clip\":[" << r.second.state.previewClipLeft << "," << r.second.state.previewClipTop << ","
			<< r.second.state.previewClipRight << "," << r.second.state.previewClipBottom << "]}";
	}
	json << "\n  ],\n  \"errors\":[";
	for ( size_t i = 0; i < m_Errors.size(); ++i ) json << ( i ? "," : "" ) << JSON( m_Errors[i] );
	json << "]\n}\n";
	const std::string text = json.str();
	WriteVerificationFile( m_Output + L"\\launcher_verification.json", text.data(), text.size() );
}
