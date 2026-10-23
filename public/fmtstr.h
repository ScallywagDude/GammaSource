//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ==========
//
// Purpose: A simple class for performing safe and in-expression sprintf-style
//			string formatting
//
// $NoKeywords: $
//=============================================================================

#ifndef FMTSTR_H
#define FMTSTR_H

#include <stdarg.h>
#include <stdio.h>

#if defined( _WIN32 )
#pragma once
#endif

//=============================================================================

// using macro to be compatable with GCC
#define FmtStrVSNPrintf( szBuf, nBufSize, formatValue, lastNamed ) \
	do \
	{ \
		int     result; \
		va_list arg_ptr; \
	\
		va_start(arg_ptr, lastNamed); \
		result = Q_vsnprintf((szBuf), (nBufSize)-1, (formatValue), arg_ptr); \
		(void)result; \
		va_end(arg_ptr); \
	\
		(szBuf)[(nBufSize)-1] = 0; \
	} \
	while (0)

//-----------------------------------------------------------------------------
//
// Purpose: String formatter with specified size
//

template <int SIZE_BUF>
class CFmtStrN
{
public:
	CFmtStrN()									{ m_szBuf[0] = 0; }
	
	// Standard C formatting
	CFmtStrN(const char *pszFormat, ...)		{ FmtStrVSNPrintf(m_szBuf, SIZE_BUF, pszFormat, pszFormat); }

	// Pointer-to-format compatibility form; arguments belong to this call.
	CFmtStrN(const char ** ppszFormat, ...)		{ FmtStrVSNPrintf(m_szBuf, SIZE_BUF, *ppszFormat, ppszFormat); }

	// Explicit reformat
	void sprintf(const char *pszFormat, ...)	{ FmtStrVSNPrintf(m_szBuf, SIZE_BUF, pszFormat, pszFormat); }

	// Pointer-to-format compatibility form; arguments belong to this call.
	void VSprintf(const char **ppszFormat, ...)	{ FmtStrVSNPrintf(m_szBuf, SIZE_BUF, *ppszFormat, ppszFormat); }

	// Forward an existing argument list explicitly; no caller-stack assumptions.
	void VSprintf(const char *pszFormat, va_list args)
	{
		Q_vsnprintf(m_szBuf, SIZE_BUF-1, pszFormat, args);
		m_szBuf[SIZE_BUF-1]=0;
	}

	// Use for access
	operator const char *() const				{ return m_szBuf; }
	char *Access()								{ return m_szBuf; }

	void Clear()								{ m_szBuf[0] = 0; }

private:
	char m_szBuf[SIZE_BUF];
};

//-----------------------------------------------------------------------------
//
// Purpose: Default-sized string formatter
//

#define FMTSTR_STD_LEN 256

typedef CFmtStrN<FMTSTR_STD_LEN> CFmtStr;

//=============================================================================

#endif // FMTSTR_H
