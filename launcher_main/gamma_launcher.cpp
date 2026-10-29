// The independent launcher uses the Source tool renderer, not the game engine.
// Loading the UI module from bin also resolves its toolkit DLLs beside it.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "gamma_launcher_commandline.h"

static int LauncherError( const std::wstring &message )
{
	MessageBoxW( NULL, message.c_str(), L"GammaSource Launcher", MB_OK | MB_ICONERROR );
	return 1;
}

static std::wstring WindowsError( DWORD error )
{
	wchar_t *description = NULL;
	FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, error, 0, reinterpret_cast<wchar_t *>( &description ), 0, NULL );
	std::wstring result = description ? description : L"Windows could not start the game.";
	if ( description ) LocalFree( description );
	return result;
}

static bool FileExists( const std::wstring &path )
{
	const DWORD attributes = GetFileAttributesW( path.c_str() );
	return attributes != INVALID_FILE_ATTRIBUTES && !( attributes & FILE_ATTRIBUTE_DIRECTORY );
}

static std::wstring ExecutableDirectory()
{
	std::vector<wchar_t> path( 512, L'\0' );
	while ( path.size() <= 32768 )
	{
		const DWORD size = static_cast<DWORD>( path.size() );
		const DWORD length = GetModuleFileNameW( NULL, path.data(), size );
		if ( !length ) return std::wstring();
		if ( length < size )
		{
			const std::wstring executable( path.data(), length );
			const size_t separator = executable.find_last_of( L"\\/" );
			return separator == std::wstring::npos ? std::wstring() : executable.substr( 0, separator );
		}
		path.resize( path.size() * 2, L'\0' );
	}
	return std::wstring();
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int )
{
	const std::wstring directory = ExecutableDirectory();
	if ( directory.empty() )
		return LauncherError( L"The launcher could not find its game folder." );

	const std::wstring engine = directory + L"\\hl2_launcher.exe";
	const std::wstring module = directory + L"\\bin\\gamma_launcher_ui.dll";
	if ( !FileExists( engine ) || !FileExists( directory + L"\\gammasource\\gameinfo.txt" ) )
		return LauncherError( L"Put GammaSource Launcher.exe in your GammaSource game folder, next to hl2_launcher.exe.\n\nThe gammasource folder must be present too." );
	if ( !FileExists( module ) )
		return LauncherError( L"The launcher UI is missing. Restore bin\\gamma_launcher_ui.dll from the GammaSource build." );
	SetCurrentDirectoryW( directory.c_str() );
	SetDllDirectoryW( ( directory + L"\\bin" ).c_str() );
	HMODULE library = LoadLibraryExW( module.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
	if ( !library ) return LauncherError( L"The launcher could not load its renderer.\n\n" + WindowsError( GetLastError() ) );
	typedef int ( *LauncherMain )( HINSTANCE );
	LauncherMain main = reinterpret_cast<LauncherMain>( GetProcAddress( library, "GammaLauncherMain" ) );
	if ( !main ) return LauncherError( L"The launcher UI is from a different build. Restore bin\\gamma_launcher_ui.dll." );
	const int result = main( instance );
	// Toolkit worker threads and interfaces have already been shut down by the
	// application framework. Keep the DLL mapped until process exit so static
	// destructors cannot refer to unloaded Source modules.
	return result;
}
