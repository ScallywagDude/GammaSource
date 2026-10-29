#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include <stdio.h>
#include "gamma_launcher_commandline.h"

int main()
{
	const std::vector<std::wstring> arguments = {
		L"C:\\Game folder\\hl2_launcher.exe", L"-game", L"gammasource", L"",
		L"plain", L"with spaces", L"C:\\trailing slash\\", L"quote\"inside",
		L"backslash\\\"quote", L"\\\"", L"two\\\\\"slashes", L"tabs\tand\nlines",
		L"\u0418\u0433\u0440\u0430", L"literal & | ; $(nothing)", L"+gamma_launcher"
	};
	const std::wstring command = GammaJoinArguments( arguments );
	int count = 0;
	wchar_t **decoded = CommandLineToArgvW( command.c_str(), &count );
	assert( decoded && count == static_cast<int>( arguments.size() ) );
	for ( int i = 0; i < count; ++i ) assert( arguments[i] == decoded[i] );
	LocalFree( decoded );
	puts( "PASS: Unicode launch arguments preserve quotes, spaces, empty values and trailing backslashes" );
	return 0;
}
