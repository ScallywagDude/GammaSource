#ifndef HOST_JMP_H
#define HOST_JMP_H
#pragma once

#ifndef _INC_SETJMP
#include <setjmp.h>
#endif

extern jmp_buf 		host_abortserver;
extern jmp_buf     host_enddemo;

#if defined(_WIN32) && defined(_M_IX86)
extern bool g_bAbortServerSet;
#endif

#endif

