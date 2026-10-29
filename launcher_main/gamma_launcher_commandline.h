#ifndef GAMMA_LAUNCHER_COMMANDLINE_H
#define GAMMA_LAUNCHER_COMMANDLINE_H

#include <string>
#include <vector>

// Quote one argument for CreateProcess/CommandLineToArgvW. No command shell is
// involved, and trailing backslashes or embedded quotes remain literal data.
inline std::wstring GammaQuoteArgument( const std::wstring &argument )
{
	std::wstring result( L"\"" );
	size_t slashes = 0;
	for ( size_t i = 0; i < argument.size(); ++i )
	{
		const wchar_t character = argument[i];
		if ( character == L'\\' )
		{
			++slashes;
			continue;
		}
		if ( character == L'"' )
			result.append( slashes * 2 + 1, L'\\' );
		else
			result.append( slashes, L'\\' );
		result.push_back( character );
		slashes = 0;
	}
	result.append( slashes * 2, L'\\' );
	result.push_back( L'"' );
	return result;
}

inline std::wstring GammaJoinArguments( const std::vector<std::wstring> &arguments )
{
	std::wstring commandLine;
	for ( size_t i = 0; i < arguments.size(); ++i )
	{
		if ( i ) commandLine.push_back( L' ' );
		commandLine += GammaQuoteArgument( arguments[i] );
	}
	return commandLine;
}

#endif
