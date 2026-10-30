//========= GammaSource ============//
//
// Native OpenGL 4.5 (core profile) renderer for shaderapigl (kept free of Source headers):
// context, textures, render targets and the textured, vertex-coloured, lightmapped draw
// path the materials' passes go through. It implements the same functions as the DX11,
// DX12 and Vulkan backends (gldevice.h, glrender.h) and runs the same shader, rewritten
// as GLSL by make_gl.py (glshaders.h).
//
// Direct3D's conventions are kept so the shared code needs no changes: render targets
// hold rows top-down (the shader flips y into clip space and the back buffer is flipped
// when it is shown), depth runs 0..1 (glClipControl) and D3D's clockwise front faces
// become GL's counter-clockwise ones.
//
// Frames: the game draws into an offscreen back buffer (a framebuffer object) that is
// blitted to the window at Present. Per-draw data comes from persistently mapped
// per-frame buffers, recycled behind a fence.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include "gldevice.h"
#include "glrender.h"
#include "glshaders.h"

#pragma comment( lib, "opengl32.lib" )
#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "user32.lib" )

//-----------------------------------------------------------------------------
// What GL/gl.h (1.1) lacks
//-----------------------------------------------------------------------------
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char GLchar;
typedef unsigned __int64 GLuint64;
typedef struct __GLsync *GLsync;
typedef void ( APIENTRY *GLDEBUGPROC )( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam );

#define GL_ARRAY_BUFFER						0x8892
#define GL_ELEMENT_ARRAY_BUFFER				0x8893
#define GL_UNIFORM_BUFFER					0x8A11
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT	0x8A34
#define GL_COPY_WRITE_BUFFER				0x8F37
#define GL_MAP_WRITE_BIT					0x0002
#define GL_MAP_PERSISTENT_BIT				0x0040
#define GL_MAP_COHERENT_BIT					0x0080
#define GL_VERTEX_SHADER					0x8B31
#define GL_FRAGMENT_SHADER					0x8B30
#define GL_COMPILE_STATUS					0x8B81
#define GL_LINK_STATUS						0x8B82
#define GL_INFO_LOG_LENGTH					0x8B84
#define GL_TEXTURE0							0x84C0
#define GL_TEXTURE_CUBE_MAP					0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X		0x8515
#define GL_TEXTURE_CUBE_MAP_SEAMLESS		0x884F
#define GL_BGRA								0x80E1
#define GL_CLAMP_TO_EDGE					0x812F
#define GL_TEXTURE_WRAP_R					0x8072
#define GL_TEXTURE_BASE_LEVEL				0x813C
#define GL_TEXTURE_MAX_LEVEL				0x813D
#define GL_FRAMEBUFFER						0x8D40
#define GL_READ_FRAMEBUFFER					0x8CA8
#define GL_DRAW_FRAMEBUFFER					0x8CA9
#define GL_RENDERBUFFER						0x8D41
#define GL_COLOR_ATTACHMENT0				0x8CE0
#define GL_DEPTH_STENCIL_ATTACHMENT			0x821A
#define GL_DEPTH24_STENCIL8					0x88F0
#define GL_FRAMEBUFFER_COMPLETE				0x8CD5
#define GL_SAMPLES_PASSED					0x8914
#define GL_QUERY_RESULT						0x8866
#define GL_QUERY_RESULT_AVAILABLE			0x8867
#define GL_SYNC_GPU_COMMANDS_COMPLETE		0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT			0x00000001
#define GL_ALREADY_SIGNALED					0x911A
#define GL_CONDITION_SATISFIED				0x911C
#define GL_WAIT_FAILED						0x911D
#define GL_LOWER_LEFT						0x8CA1
#define GL_ZERO_TO_ONE						0x935F
#define GL_CLIP_DISTANCE0					0x3000
#define GL_CLIP_DISTANCE1					0x3001
#define GL_UNPACK_ROW_LENGTH_				0x0CF2
#define GL_DEBUG_OUTPUT_SYNCHRONOUS			0x8242
#define GL_DEBUG_OUTPUT						0x92E0
#define GL_DEBUG_SEVERITY_NOTIFICATION		0x826B

#define WGL_CONTEXT_MAJOR_VERSION_ARB		0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB		0x2092
#define WGL_CONTEXT_FLAGS_ARB				0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB		0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB	0x0001
#define WGL_CONTEXT_DEBUG_BIT_ARB			0x0001

//-----------------------------------------------------------------------------
// Loader: everything past GL 1.1 comes from wglGetProcAddress
//-----------------------------------------------------------------------------
#define GL_FUNCS( X ) \
	X( void, glBlendFuncSeparate, ( GLenum, GLenum, GLenum, GLenum ) ) \
	X( void, glGenBuffers, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteBuffers, ( GLsizei, const GLuint * ) ) \
	X( void, glBindBuffer, ( GLenum, GLuint ) ) \
	X( void, glBufferStorage, ( GLenum, GLsizeiptr, const void *, GLbitfield ) ) \
	X( void *, glMapBufferRange, ( GLenum, GLintptr, GLsizeiptr, GLbitfield ) ) \
	X( void, glBindBufferRange, ( GLenum, GLuint, GLuint, GLintptr, GLsizeiptr ) ) \
	X( void, glGenVertexArrays, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteVertexArrays, ( GLsizei, const GLuint * ) ) \
	X( void, glBindVertexArray, ( GLuint ) ) \
	X( void, glEnableVertexAttribArray, ( GLuint ) ) \
	X( void, glVertexAttribFormat, ( GLuint, GLint, GLenum, GLboolean, GLuint ) ) \
	X( void, glVertexAttribIFormat, ( GLuint, GLint, GLenum, GLuint ) ) \
	X( void, glVertexAttribBinding, ( GLuint, GLuint ) ) \
	X( void, glBindVertexBuffer, ( GLuint, GLuint, GLintptr, GLsizei ) ) \
	X( GLuint, glCreateShader, ( GLenum ) ) \
	X( void, glShaderSource, ( GLuint, GLsizei, const GLchar *const *, const GLint * ) ) \
	X( void, glCompileShader, ( GLuint ) ) \
	X( void, glGetShaderiv, ( GLuint, GLenum, GLint * ) ) \
	X( void, glGetShaderInfoLog, ( GLuint, GLsizei, GLsizei *, GLchar * ) ) \
	X( void, glDeleteShader, ( GLuint ) ) \
	X( GLuint, glCreateProgram, ( void ) ) \
	X( void, glAttachShader, ( GLuint, GLuint ) ) \
	X( void, glLinkProgram, ( GLuint ) ) \
	X( void, glGetProgramiv, ( GLuint, GLenum, GLint * ) ) \
	X( void, glGetProgramInfoLog, ( GLuint, GLsizei, GLsizei *, GLchar * ) ) \
	X( void, glUseProgram, ( GLuint ) ) \
	X( void, glDeleteProgram, ( GLuint ) ) \
	X( void, glGenSamplers, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteSamplers, ( GLsizei, const GLuint * ) ) \
	X( void, glSamplerParameteri, ( GLuint, GLenum, GLint ) ) \
	X( void, glBindSampler, ( GLuint, GLuint ) ) \
	X( void, glActiveTexture, ( GLenum ) ) \
	X( void, glTexStorage2D, ( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) ) \
	X( void, glGenFramebuffers, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteFramebuffers, ( GLsizei, const GLuint * ) ) \
	X( void, glBindFramebuffer, ( GLenum, GLuint ) ) \
	X( void, glFramebufferTexture2D, ( GLenum, GLenum, GLenum, GLuint, GLint ) ) \
	X( void, glGenRenderbuffers, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteRenderbuffers, ( GLsizei, const GLuint * ) ) \
	X( void, glBindRenderbuffer, ( GLenum, GLuint ) ) \
	X( void, glRenderbufferStorage, ( GLenum, GLenum, GLsizei, GLsizei ) ) \
	X( void, glFramebufferRenderbuffer, ( GLenum, GLenum, GLenum, GLuint ) ) \
	X( GLenum, glCheckFramebufferStatus, ( GLenum ) ) \
	X( void, glBlitFramebuffer, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) ) \
	X( void, glCopyImageSubData, ( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei ) ) \
	X( void, glGenQueries, ( GLsizei, GLuint * ) ) \
	X( void, glDeleteQueries, ( GLsizei, const GLuint * ) ) \
	X( void, glBeginQuery, ( GLenum, GLuint ) ) \
	X( void, glEndQuery, ( GLenum ) ) \
	X( void, glGetQueryObjectiv, ( GLuint, GLenum, GLint * ) ) \
	X( void, glGetQueryObjectui64v, ( GLuint, GLenum, GLuint64 * ) ) \
	X( GLsync, glFenceSync, ( GLenum, GLbitfield ) ) \
	X( GLenum, glClientWaitSync, ( GLsync, GLbitfield, GLuint64 ) ) \
	X( void, glDeleteSync, ( GLsync ) ) \
	X( void, glClipControl, ( GLenum, GLenum ) )
#define GL_DECLARE( ret, name, args ) typedef ret ( APIENTRY *PFN_##name ) args; static PFN_##name name##_;
GL_FUNCS( GL_DECLARE )
typedef HGLRC ( WINAPI *PFN_wglCreateContextAttribsARB )( HDC, HGLRC, const int * );
typedef BOOL ( WINAPI *PFN_wglSwapIntervalEXT )( int );
typedef void ( APIENTRY *PFN_glDebugMessageCallback )( GLDEBUGPROC, const void * );

//-----------------------------------------------------------------------------
// Device state
//-----------------------------------------------------------------------------
enum { FRAMES = 2, STREAM_SIZE = 16 * 1024 * 1024 };

static HWND s_hWnd;
static HDC s_hDC;
static HGLRC s_hGLRC;
static float s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static char s_szAdapterName[256] = "OpenGL";
static GLsync s_Fences[FRAMES];
static int s_nFrame;					// current slot
static unsigned int s_nSerial = 1;		// bumped whenever streaming memory is recycled
static GLuint s_nVAO;
static GLint s_nUniformAlign = 256;

void Gl_DebugLog( const char *pszMsg, int n )
{
	static char s_szFile[MAX_PATH];
	static const char *s_pszFile = GetEnvironmentVariableA( "GAMMA_GL_LOG", s_szFile, MAX_PATH ) ? s_szFile : NULL;
	if ( !s_pszFile )
		return;
	FILE *f = fopen( s_pszFile, "a" );
	if ( f ) { fprintf( f, "%s %d\n", pszMsg, n ); fclose( f ); }
}

static void APIENTRY DebugCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam )
{
	if ( severity != GL_DEBUG_SEVERITY_NOTIFICATION )
		Gl_DebugLog( message, (int)id );
}

static void WaitFence( GLsync &fence )
{
	if ( !fence )
		return;
	for ( ;; )
	{
		GLenum res = glClientWaitSync_( fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull );
		if ( res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED || res == GL_WAIT_FAILED )
			break;
	}
	glDeleteSync_( fence );
	fence = NULL;
}

//-----------------------------------------------------------------------------
// Per-frame streaming memory (vertices/indices used once, constants), persistently mapped
//-----------------------------------------------------------------------------
struct StreamBuffer_t
{
	GLuint m_nBuffer;
	unsigned char *m_pMapped;
	size_t m_nSize, m_nUsed;
};
static std::vector< StreamBuffer_t > s_Stream[FRAMES];

static unsigned char *StreamAlloc( size_t nBytes, size_t nAlign, GLuint &nBuffer, size_t &nOffset )
{
	std::vector< StreamBuffer_t > &stream = s_Stream[s_nFrame];
	for ( size_t i = 0; i < stream.size(); ++i )
	{
		StreamBuffer_t &sb = stream[i];
		size_t nStart = ( sb.m_nUsed + nAlign - 1 ) / nAlign * nAlign;
		if ( nStart + nBytes <= sb.m_nSize )
		{
			sb.m_nUsed = nStart + nBytes;
			nBuffer = sb.m_nBuffer;
			nOffset = nStart;
			return sb.m_pMapped + nStart;
		}
	}
	StreamBuffer_t sb = {};
	sb.m_nSize = nBytes > STREAM_SIZE ? ( nBytes + 255 ) & ~(size_t)255 : STREAM_SIZE;
	glGenBuffers_( 1, &sb.m_nBuffer );
	glBindBuffer_( GL_COPY_WRITE_BUFFER, sb.m_nBuffer );
	GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	glBufferStorage_( GL_COPY_WRITE_BUFFER, sb.m_nSize, NULL, flags );
	sb.m_pMapped = (unsigned char *)glMapBufferRange_( GL_COPY_WRITE_BUFFER, 0, sb.m_nSize, flags );
	glBindBuffer_( GL_COPY_WRITE_BUFFER, 0 );
	if ( !sb.m_pMapped )
	{
		glDeleteBuffers_( 1, &sb.m_nBuffer );
		return NULL;
	}
	sb.m_nUsed = nBytes;
	stream.push_back( sb );
	nBuffer = sb.m_nBuffer;
	nOffset = 0;
	return sb.m_pMapped;
}

static size_t StreamUsed()
{
	size_t n = 0;
	for ( size_t i = 0; i < s_Stream[s_nFrame].size(); ++i )
		n += s_Stream[s_nFrame][i].m_nUsed;
	return n;
}

static void ResetStream( int nSlot )
{
	for ( size_t i = 0; i < s_Stream[nSlot].size(); ++i )
		s_Stream[nSlot][i].m_nUsed = 0;
	++s_nSerial;
}

//-----------------------------------------------------------------------------
// Textures and render targets
//-----------------------------------------------------------------------------
struct Texture_t
{
	GLuint m_nTexture;
	int m_nWidth, m_nHeight, m_nMips;
	bool m_bClampS, m_bClampT;
	bool m_bUsed;
	bool m_bHasData;
	bool m_bRenderTarget;
	bool m_bCube;
	unsigned char *m_pPending[16];	// mip levels given before the context existed
	unsigned char *m_pShadow;		// see GlR_LockShadow
	// render targets
	GLuint m_nFBO, m_nDepth;
};
static std::vector< Texture_t > s_Textures;	// handle = index + 1
static Texture_t s_BackBuffer;			// what the game draws into; blitted to the window
static int s_hWhite;
static int s_hWhiteCube;				// bound when a pass has no envmap
static int s_hRenderTarget;				// 0: the back buffer
static GLuint s_nBoundFBO = (GLuint)-1;
static unsigned int s_nAppliedState = 0xffffffff;	// render state key last given to GL

static Texture_t *GetTexture( int hTexture )
{
	if ( hTexture <= 0 || hTexture > (int)s_Textures.size() || !s_Textures[hTexture - 1].m_bUsed )
		return NULL;
	return &s_Textures[hTexture - 1];
}

static int MipSize( int n, int nLevel )
{
	n >>= nLevel;
	return n > 0 ? n : 1;
}

static void BindFBO( GLuint nFBO )
{
	if ( s_nBoundFBO != nFBO )
	{
		glBindFramebuffer_( GL_FRAMEBUFFER, nFBO );
		s_nBoundFBO = nFBO;
	}
}

static void UploadRegion( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch, int nLayer = 0 )
{
	if ( nWidth <= 0 || nHeight <= 0 || !pTex->m_nTexture )
		return;
	GLenum target = pTex->m_bCube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
	glActiveTexture_( GL_TEXTURE0 );
	glBindTexture( target, pTex->m_nTexture );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	glPixelStorei( GL_UNPACK_ROW_LENGTH_, nPitch / 4 );
	glTexSubImage2D( pTex->m_bCube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + nLayer : GL_TEXTURE_2D, nLevel, x, y, nWidth, nHeight,
		GL_BGRA, GL_UNSIGNED_BYTE, pBGRA );
	glPixelStorei( GL_UNPACK_ROW_LENGTH_, 0 );
	pTex->m_bHasData = true;
}

// creates the GL texture (and for render targets the depth buffer and framebuffer)
static bool EnsureResource( Texture_t *pTex )
{
	if ( pTex->m_nTexture )
		return true;
	if ( !s_hGLRC )
		return false;
	GLenum target = pTex->m_bCube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
	glGenTextures( 1, &pTex->m_nTexture );
	glActiveTexture_( GL_TEXTURE0 );
	glBindTexture( target, pTex->m_nTexture );
	glTexStorage2D_( target, pTex->m_nMips, GL_RGBA8, pTex->m_nWidth, pTex->m_nHeight );
	glTexParameteri( target, GL_TEXTURE_BASE_LEVEL, 0 );
	glTexParameteri( target, GL_TEXTURE_MAX_LEVEL, pTex->m_nMips - 1 );
	if ( pTex->m_bRenderTarget )
	{
		glGenRenderbuffers_( 1, &pTex->m_nDepth );
		glBindRenderbuffer_( GL_RENDERBUFFER, pTex->m_nDepth );
		glRenderbufferStorage_( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, pTex->m_nWidth, pTex->m_nHeight );
		glGenFramebuffers_( 1, &pTex->m_nFBO );
		BindFBO( pTex->m_nFBO );
		glFramebufferTexture2D_( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pTex->m_nTexture, 0 );
		glFramebufferRenderbuffer_( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pTex->m_nDepth );
		if ( glCheckFramebufferStatus_( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
			Gl_DebugLog( "incomplete framebuffer", pTex->m_nWidth );
		// start out black with a cleared depth buffer
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glDepthMask( GL_TRUE );
		s_nAppliedState = 0xffffffff;
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		pTex->m_bHasData = true;
	}
	// data that arrived before the context existed
	for ( int i = 0; i < 16 && i < pTex->m_nMips; ++i )
	{
		if ( !pTex->m_pPending[i] )
			continue;
		UploadRegion( pTex, i, 0, 0, MipSize( pTex->m_nWidth, i ), MipSize( pTex->m_nHeight, i ), pTex->m_pPending[i], MipSize( pTex->m_nWidth, i ) * 4 );
		free( pTex->m_pPending[i] );
		pTex->m_pPending[i] = NULL;
	}
	return true;
}

// GL keeps objects alive while queued commands still use them, so nothing is deferred
static void DestroyTexture( Texture_t *pTex )
{
	if ( s_hGLRC )
	{
		if ( pTex->m_nFBO && s_nBoundFBO == pTex->m_nFBO )
			BindFBO( 0 );
		if ( pTex->m_nFBO ) glDeleteFramebuffers_( 1, &pTex->m_nFBO );
		if ( pTex->m_nDepth ) glDeleteRenderbuffers_( 1, &pTex->m_nDepth );
		if ( pTex->m_nTexture ) glDeleteTextures( 1, &pTex->m_nTexture );
	}
	for ( int i = 0; i < 16; ++i ) free( pTex->m_pPending[i] );
	free( pTex->m_pShadow );
	memset( pTex, 0, sizeof( *pTex ) );
}

//-----------------------------------------------------------------------------
// Render state
//-----------------------------------------------------------------------------
static GLuint s_nProgram;
static GLuint s_Samplers[4];		// stage 0: bit 0 clamp S, bit 1 clamp T
static GLuint s_nClampSampler;		// lightmap / reflection and envmap
int g_nGlRBinds, g_nGlRDraws, g_nGlRMeshes, g_nGlRPasses, g_nGlREmptyMeshes;
static int s_Viewport[4];
static float s_flMinZ, s_flMaxZ = 1.0f;
static bool s_bViewportSet;

#include "rendershared.inl"

static const GLenum s_BlendMap[] =
{
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR
};
static const GLenum s_DepthMap[] =
{
	GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};
static const GLenum s_Topology[] =
{
	GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_LINE_STRIP
};

static void ApplyState()
{
	int nSrc = ( s_State.m_nSrcBlend >= 0 && s_State.m_nSrcBlend <= 10 ) ? s_State.m_nSrcBlend : 1;
	int nDst = ( s_State.m_nDstBlend >= 0 && s_State.m_nDstBlend <= 10 ) ? s_State.m_nDstBlend : 0;
	int nDepthFunc = ( s_State.m_nDepthFunc >= 0 && s_State.m_nDepthFunc <= 7 ) ? s_State.m_nDepthFunc : 3;
	unsigned int nKey = ( s_State.m_bBlend ? 1 : 0 ) | ( nSrc << 1 ) | ( nDst << 5 ) | ( s_State.m_bDepthTest ? 1 << 9 : 0 ) |
		( s_State.m_bDepthWrite ? 1 << 10 : 0 ) | ( nDepthFunc << 11 ) | ( s_State.m_bCull ? 1 << 14 : 0 ) |
		( s_State.m_bDecalOffset ? 1 << 18 : 0 ) | ( s_State.m_bNoColorWrites ? 1 << 19 : 0 ) | ( s_State.m_bNoAlphaWrites ? 1 << 20 : 0 );
	if ( nKey == s_nAppliedState )
		return;
	s_nAppliedState = nKey;
	if ( s_State.m_bBlend )
	{
		glEnable( GL_BLEND );
		glBlendFuncSeparate_( s_BlendMap[nSrc], s_BlendMap[nDst], GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		glDisable( GL_BLEND );
	}
	// GL writes depth only with the test on: a write without a test is an ALWAYS test
	if ( s_State.m_bDepthTest || s_State.m_bDepthWrite )
	{
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( s_State.m_bDepthTest ? s_DepthMap[nDepthFunc] : GL_ALWAYS );
	}
	else
	{
		glDisable( GL_DEPTH_TEST );
	}
	glDepthMask( s_State.m_bDepthWrite ? GL_TRUE : GL_FALSE );
	if ( s_State.m_bCull ) glEnable( GL_CULL_FACE ); else glDisable( GL_CULL_FACE );
	if ( s_State.m_bDecalOffset )
	{
		// as D3D9's decal bias (mat_depthbias_decal / mat_slopescaledepthbias_decal)
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -0.5f, -84.0f );
	}
	else
	{
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
	GLboolean bColor = s_State.m_bNoColorWrites ? GL_FALSE : GL_TRUE;
	glColorMask( bColor, bColor, bColor, s_State.m_bNoAlphaWrites ? GL_FALSE : GL_TRUE );
}

//-----------------------------------------------------------------------------
// Device
//-----------------------------------------------------------------------------
static bool CreateBackBuffer( int nWidth, int nHeight )
{
	if ( s_BackBuffer.m_nTexture )
		DestroyTexture( &s_BackBuffer );
	s_BackBuffer.m_nWidth = nWidth > 0 ? nWidth : 1;
	s_BackBuffer.m_nHeight = nHeight > 0 ? nHeight : 1;
	s_BackBuffer.m_nMips = 1;
	s_BackBuffer.m_bRenderTarget = true;
	s_BackBuffer.m_bUsed = true;
	return EnsureResource( &s_BackBuffer );
}

static PROC GetGLProc( const char *pszName )
{
	PROC p = wglGetProcAddress( pszName );
	if ( p == NULL || p == (PROC)1 || p == (PROC)2 || p == (PROC)3 || p == (PROC)-1 )
		return NULL;
	return p;
}

bool Gl_Init( void *hWnd, int nWidth, int nHeight )
{
	if ( s_hGLRC && s_hWnd == (HWND)hWnd )
		return true;
	Gl_Shutdown();
	s_hWnd = (HWND)hWnd;
	s_hDC = GetDC( s_hWnd );
	if ( !s_hDC )
		return false;
	// a window's pixel format can be set once; a window that had one keeps it
	if ( !GetPixelFormat( s_hDC ) )
	{
		PIXELFORMATDESCRIPTOR pfd = {};
		pfd.nSize = sizeof( pfd );
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cAlphaBits = 8;
		pfd.iLayerType = PFD_MAIN_PLANE;
		int nFormat = ChoosePixelFormat( s_hDC, &pfd );
		if ( !nFormat || !SetPixelFormat( s_hDC, nFormat, &pfd ) )
			return false;
	}
	HGLRC hLegacy = wglCreateContext( s_hDC );
	if ( !hLegacy || !wglMakeCurrent( s_hDC, hLegacy ) )
	{
		if ( hLegacy ) wglDeleteContext( hLegacy );
		return false;
	}
	PFN_wglCreateContextAttribsARB pfnCreateContextAttribs = (PFN_wglCreateContextAttribsARB)GetGLProc( "wglCreateContextAttribsARB" );
	char szDebug[8];
	bool bDebug = GetEnvironmentVariableA( "GAMMA_GL_DEBUG", szDebug, sizeof( szDebug ) ) != 0;
	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4, WGL_CONTEXT_MINOR_VERSION_ARB, 5,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB, bDebug ? WGL_CONTEXT_DEBUG_BIT_ARB : 0, 0
	};
	s_hGLRC = pfnCreateContextAttribs ? pfnCreateContextAttribs( s_hDC, NULL, attribs ) : NULL;
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hLegacy );
	if ( !s_hGLRC || !wglMakeCurrent( s_hDC, s_hGLRC ) )
	{
		Gl_Shutdown();
		return false;
	}
#define GL_LOADPROC( ret, name, args ) name##_ = (PFN_##name)GetGLProc( #name ); if ( !name##_ ) { Gl_DebugLog( "missing " #name, 0 ); Gl_Shutdown(); return false; }
	GL_FUNCS( GL_LOADPROC )
	PFN_wglSwapIntervalEXT pfnSwapInterval = (PFN_wglSwapIntervalEXT)GetGLProc( "wglSwapIntervalEXT" );
	if ( pfnSwapInterval )
		pfnSwapInterval( 1 );
	if ( bDebug )
	{
		PFN_glDebugMessageCallback pfnDebugCallback = (PFN_glDebugMessageCallback)GetGLProc( "glDebugMessageCallback" );
		if ( pfnDebugCallback )
		{
			glEnable( GL_DEBUG_OUTPUT );
			glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
			pfnDebugCallback( DebugCallback, NULL );
		}
	}
	const char *pszRenderer = (const char *)glGetString( GL_RENDERER );
	if ( pszRenderer )
	{
		strncpy( s_szAdapterName, pszRenderer, sizeof( s_szAdapterName ) - 1 );
		s_szAdapterName[sizeof( s_szAdapterName ) - 1] = 0;
	}
	glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_nUniformAlign );
	if ( s_nUniformAlign < 16 ) s_nUniformAlign = 256;

	// Direct3D conventions: depth 0..1, clockwise (in D3D's y-down screen) front faces
	glClipControl_( GL_LOWER_LEFT, GL_ZERO_TO_ONE );
	glFrontFace( GL_CCW );
	glCullFace( GL_BACK );
	glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
	glEnable( GL_CLIP_DISTANCE0 );
	glEnable( GL_CLIP_DISTANCE1 );
	glGenVertexArrays_( 1, &s_nVAO );
	glBindVertexArray_( s_nVAO );
	s_nBoundFBO = (GLuint)-1;
	s_nAppliedState = 0xffffffff;

	RECT rc; GetClientRect( s_hWnd, &rc );
	if ( nWidth <= 0 || nHeight <= 0 ) { nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top; }
	return CreateBackBuffer( nWidth, nHeight );
}

bool Gl_Resize( int nWidth, int nHeight )
{
	if ( !s_hGLRC )
		return false;
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
	}
	if ( nWidth <= 0 || nHeight <= 0 || ( nWidth == s_BackBuffer.m_nWidth && nHeight == s_BackBuffer.m_nHeight ) )
		return true;
	return CreateBackBuffer( nWidth, nHeight );
}

void Gl_Shutdown()
{
	GlR_Shutdown();
	if ( s_hGLRC )
	{
		for ( int i = 0; i < FRAMES; ++i )
		{
			if ( s_Fences[i] ) { glDeleteSync_( s_Fences[i] ); s_Fences[i] = NULL; }
			for ( size_t j = 0; j < s_Stream[i].size(); ++j )
				glDeleteBuffers_( 1, &s_Stream[i][j].m_nBuffer );	// unmaps
			s_Stream[i].clear();
		}
		if ( s_BackBuffer.m_nTexture ) DestroyTexture( &s_BackBuffer );
		if ( s_nVAO ) glDeleteVertexArrays_( 1, &s_nVAO );
		s_nVAO = 0;
		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( s_hGLRC );
	}
	if ( s_hDC ) ReleaseDC( s_hWnd, s_hDC );
	s_hGLRC = NULL;
	s_hDC = NULL;
	s_hWnd = NULL;
	memset( &s_BackBuffer, 0, sizeof( s_BackBuffer ) );
}

bool Gl_IsActive()
{
	return s_hGLRC != NULL;
}

void Gl_SetClearColor( float r, float g, float b, float a )
{
	s_ClearColor[0] = r; s_ClearColor[1] = g; s_ClearColor[2] = b; s_ClearColor[3] = a;
}

void Gl_Present()
{
	if ( !s_hGLRC )
		return;
	Gl_Resize( 0, 0 );
	RECT rc; GetClientRect( s_hWnd, &rc );
	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;
	if ( nWidth > 0 && nHeight > 0 && s_BackBuffer.m_nFBO )
	{
		// rows are top-down in the back buffer: flip them onto the window
		glBindFramebuffer_( GL_READ_FRAMEBUFFER, s_BackBuffer.m_nFBO );
		glBindFramebuffer_( GL_DRAW_FRAMEBUFFER, 0 );
		glBlitFramebuffer_( 0, 0, s_BackBuffer.m_nWidth, s_BackBuffer.m_nHeight, 0, nHeight, nWidth, 0, GL_COLOR_BUFFER_BIT,
			( nWidth == s_BackBuffer.m_nWidth && nHeight == s_BackBuffer.m_nHeight ) ? GL_NEAREST : GL_LINEAR );
		s_nBoundFBO = (GLuint)-1;
	}
	SwapBuffers( s_hDC );
	// this frame's streaming memory is recycled once the GPU is through with it
	s_Fences[s_nFrame] = glFenceSync_( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	s_nFrame = ( s_nFrame + 1 ) % FRAMES;
	WaitFence( s_Fences[s_nFrame] );
	ResetStream( s_nFrame );
}

void Gl_GetBackBufferSize( int &nWidth, int &nHeight )
{
	nWidth = s_BackBuffer.m_nWidth; nHeight = s_BackBuffer.m_nHeight;
}

void Gl_GetWindowSize( int &nWidth, int &nHeight )
{
	RECT rc = {};
	if ( s_hWnd ) GetClientRect( s_hWnd, &rc );
	nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
}

void Gl_GetDesktopSize( int &nWidth, int &nHeight )
{
	nWidth = GetSystemMetrics( SM_CXSCREEN ); nHeight = GetSystemMetrics( SM_CYSCREEN );
}

const char *Gl_AdapterName()
{
	return s_szAdapterName;
}

//-----------------------------------------------------------------------------
// Renderer
//-----------------------------------------------------------------------------
static GLuint CompileShader( GLenum type, const char *pszDefine )
{
	GLuint nShader = glCreateShader_( type );
	const GLchar *sources[3] = { "#version 450 core\n", pszDefine, s_szGLShader };
	glShaderSource_( nShader, 3, sources, NULL );
	glCompileShader_( nShader );
	GLint nOK = 0;
	glGetShaderiv_( nShader, GL_COMPILE_STATUS, &nOK );
	if ( !nOK )
	{
		char szLog[4096] = "";
		glGetShaderInfoLog_( nShader, sizeof( szLog ), NULL, szLog );
		OutputDebugStringA( szLog );
		Gl_DebugLog( szLog, (int)type );
		glDeleteShader_( nShader );
		return 0;
	}
	return nShader;
}

bool GlR_Init()
{
	if ( s_nProgram )
		return true;
	if ( !s_hGLRC )
		return false;
	GLuint nVS = CompileShader( GL_VERTEX_SHADER, "#define GAMMA_VS\n" );
	GLuint nPS = CompileShader( GL_FRAGMENT_SHADER, "#define GAMMA_PS\n" );
	if ( !nVS || !nPS )
		return false;
	s_nProgram = glCreateProgram_();
	glAttachShader_( s_nProgram, nVS );
	glAttachShader_( s_nProgram, nPS );
	glLinkProgram_( s_nProgram );
	glDeleteShader_( nVS );
	glDeleteShader_( nPS );
	GLint nOK = 0;
	glGetProgramiv_( s_nProgram, GL_LINK_STATUS, &nOK );
	if ( !nOK )
	{
		char szLog[4096] = "";
		glGetProgramInfoLog_( s_nProgram, sizeof( szLog ), NULL, szLog );
		OutputDebugStringA( szLog );
		Gl_DebugLog( szLog, 0 );
		glDeleteProgram_( s_nProgram );
		s_nProgram = 0;
		return false;
	}
	glUseProgram_( s_nProgram );

	// the vertex layout (GlRVertex_t), read from binding 0
	glEnableVertexAttribArray_( 0 ); glVertexAttribFormat_( 0, 3, GL_FLOAT, GL_FALSE, 0 );			// position
	glEnableVertexAttribArray_( 1 ); glVertexAttribFormat_( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 12 );	// colour
	glEnableVertexAttribArray_( 2 ); glVertexAttribFormat_( 2, 2, GL_FLOAT, GL_FALSE, 16 );			// uv
	glEnableVertexAttribArray_( 3 ); glVertexAttribFormat_( 3, 2, GL_FLOAT, GL_FALSE, 24 );			// lightmap uv
	glEnableVertexAttribArray_( 4 ); glVertexAttribFormat_( 4, 3, GL_FLOAT, GL_FALSE, 32 );			// normal
	glEnableVertexAttribArray_( 5 ); glVertexAttribIFormat_( 5, 4, GL_UNSIGNED_BYTE, 44 );			// bone indices
	glEnableVertexAttribArray_( 6 ); glVertexAttribFormat_( 6, 2, GL_FLOAT, GL_FALSE, 48 );			// bone weights
	for ( GLuint i = 0; i < 7; ++i )
		glVertexAttribBinding_( i, 0 );

	glGenSamplers_( 4, s_Samplers );
	glGenSamplers_( 1, &s_nClampSampler );
	for ( int i = 0; i < 5; ++i )
	{
		GLuint nSampler = i < 4 ? s_Samplers[i] : s_nClampSampler;
		int nClamp = i < 4 ? i : 3;
		glSamplerParameteri_( nSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glSamplerParameteri_( nSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glSamplerParameteri_( nSampler, GL_TEXTURE_WRAP_S, ( nClamp & 1 ) ? GL_CLAMP_TO_EDGE : GL_REPEAT );
		glSamplerParameteri_( nSampler, GL_TEXTURE_WRAP_T, ( nClamp & 2 ) ? GL_CLAMP_TO_EDGE : GL_REPEAT );
		glSamplerParameteri_( nSampler, GL_TEXTURE_WRAP_R, GL_REPEAT );
	}
	glBindSampler_( 1, s_nClampSampler );
	glBindSampler_( 2, s_nClampSampler );

	static const unsigned char s_White[4] = { 255, 255, 255, 255 };
	s_hWhite = GlR_CreateTexture( 1, 1, 1 );
	GlR_TexImage( s_hWhite, 0, 1, 1, s_White );
	s_hWhiteCube = GlR_CreateTexture( 1, 1, 1, false, true );
	for ( int nFace = 0; nFace < 6; ++nFace )
		GlR_TexImageFace( s_hWhiteCube, nFace, 0, 1, 1, s_White );

	memset( &s_State, 0, sizeof( s_State ) );
	s_State.m_bDepthTest = true;
	s_State.m_bDepthWrite = true;
	s_State.m_nDepthFunc = 3;	// near or equal
	for ( int i = 0; i < 16; ++i )
		s_WVP[i] = ( i % 5 ) ? 0.0f : 1.0f;
	return true;
}

void GlR_Shutdown()
{
	if ( !s_hGLRC )
		return;
	for ( size_t i = 0; i < s_Textures.size(); ++i )
		if ( s_Textures[i].m_bUsed )
			DestroyTexture( &s_Textures[i] );
	s_Textures.clear();
	GlR_ReleaseCachedVertices( NULL );
	extern void GlR_DestroyAllQueries();
	GlR_DestroyAllQueries();
	if ( s_Samplers[0] ) glDeleteSamplers_( 4, s_Samplers );
	if ( s_nClampSampler ) glDeleteSamplers_( 1, &s_nClampSampler );
	memset( s_Samplers, 0, sizeof( s_Samplers ) );
	s_nClampSampler = 0;
	if ( s_nProgram ) glDeleteProgram_( s_nProgram );
	s_nProgram = 0;
	s_hWhite = s_hWhiteCube = s_hBound = s_hLightmap = s_hRenderTarget = 0;
}

int GlR_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget, bool bCube )
{
	Texture_t tex = {};
	tex.m_nWidth = nWidth > 0 ? nWidth : 1;
	tex.m_nHeight = nHeight > 0 ? nHeight : 1;
	tex.m_nMips = ( nMips > 0 && !bRenderTarget ) ? nMips : 1;
	tex.m_bRenderTarget = bRenderTarget;
	tex.m_bCube = bCube && !bRenderTarget;
	tex.m_bUsed = true;
	for ( size_t i = 0; i < s_Textures.size(); ++i )
	{
		if ( !s_Textures[i].m_bUsed )
		{
			s_Textures[i] = tex;
			return (int)i + 1;
		}
	}
	s_Textures.push_back( tex );
	return (int)s_Textures.size();
}

void GlR_DeleteTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( hTexture == s_hRenderTarget )
		GlR_SetRenderTarget( 0 );
	DestroyTexture( pTex );
}

bool GlR_IsTexture( int hTexture )
{
	return GetTexture( hTexture ) != NULL;
}

static void KeepPending( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	if ( nLevel >= 16 )
		return;
	int nLevelW = MipSize( pTex->m_nWidth, nLevel ), nLevelH = MipSize( pTex->m_nHeight, nLevel );
	if ( !pTex->m_pPending[nLevel] )
		pTex->m_pPending[nLevel] = (unsigned char *)calloc( nLevelW * nLevelH, 4 );
	for ( int row = 0; row < nHeight && y + row < nLevelH; ++row )
	{
		int nCopy = nWidth;
		if ( x + nCopy > nLevelW ) nCopy = nLevelW - x;
		if ( nCopy > 0 )
			memcpy( pTex->m_pPending[nLevel] + ( ( y + row ) * nLevelW + x ) * 4, pBGRA + row * nWidth * 4, nCopy * 4 );
	}
}

void GlR_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( nLevel == 0 && ( nWidth != pTex->m_nWidth || nHeight != pTex->m_nHeight ) && !pTex->m_nTexture )
	{
		pTex->m_nWidth = nWidth;
		pTex->m_nHeight = nHeight;
	}
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA, nWidth * 4 );
}

void GlR_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, x, y, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, x, y, nWidth, nHeight, pBGRA, nWidth * 4 );
}

// one face of a cube texture (cubes only get data once the context exists)
void GlR_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || !pTex->m_bCube || nFace < 0 || nFace >= 6 || nLevel >= pTex->m_nMips || !EnsureResource( pTex ) )
		return;
	UploadRegion( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA, nWidth * 4, nFace );
}

bool GlR_IsCubeTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bCube;
}

bool GlR_IsRenderTargetTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bRenderTarget;
}

unsigned char *GlR_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return NULL;
	if ( !pTex->m_pShadow )
		pTex->m_pShadow = (unsigned char *)calloc( pTex->m_nWidth * pTex->m_nHeight, 4 );
	nPitch = pTex->m_nWidth * 4;
	nWidth = pTex->m_nWidth;
	nHeight = pTex->m_nHeight;
	return pTex->m_pShadow;
}

void GlR_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nWidth <= 0 || nHeight <= 0 )
		return;
	if ( !EnsureResource( pTex ) )
	{
		for ( int row = 0; row < nHeight; ++row )
			KeepPending( pTex, 0, x, y + row, nWidth, 1, pBGRA + row * nPitch );
		return;
	}
	UploadRegion( pTex, 0, x, y, nWidth, nHeight, pBGRA, nPitch );
}

void GlR_TexWrap( int hTexture, int nCoord, bool bClamp )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( nCoord == 0 )
		pTex->m_bClampS = bClamp;
	else if ( nCoord == 1 )
		pTex->m_bClampT = bClamp;
}

void GlR_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ )
{
	s_Viewport[0] = x; s_Viewport[1] = y; s_Viewport[2] = w; s_Viewport[3] = h;
	s_flMinZ = flMinZ; s_flMaxZ = flMaxZ;
	s_bViewportSet = w > 0 && h > 0;
}

void GlR_GetTextureSize( int hTexture, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	nWidth = pTex ? pTex->m_nWidth : 0;
	nHeight = pTex ? pTex->m_nHeight : 0;
}

void GlR_SetRenderTarget( int hColor )
{
	Texture_t *pTex = GetTexture( hColor );
	if ( !pTex || !pTex->m_bRenderTarget || !EnsureResource( pTex ) )
		hColor = 0;
	s_hRenderTarget = hColor;
}

static Texture_t *CurrentTarget()
{
	Texture_t *pTex = GetTexture( s_hRenderTarget );
	return ( pTex && pTex->m_nFBO ) ? pTex : &s_BackBuffer;
}

void GlR_Clear( bool bColor, bool bDepth )
{
	if ( !( bColor || bDepth ) || !s_hGLRC )
		return;
	Texture_t *pTarget = CurrentTarget();
	if ( !pTarget->m_nFBO )
		return;
	BindFBO( pTarget->m_nFBO );
	GLbitfield mask = 0;
	if ( bColor )
	{
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glClearColor( s_ClearColor[0], s_ClearColor[1], s_ClearColor[2], s_ClearColor[3] );
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if ( bDepth )
	{
		glDepthMask( GL_TRUE );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	}
	s_nAppliedState = 0xffffffff;	// the masks changed
	glClear( mask );
}

void GlR_ClearDepth()
{
	GlR_Clear( false, true );
}

void GlR_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy )
{
	Texture_t *pDst = GetTexture( hTexture );
	if ( !pDst || !s_hGLRC || !EnsureResource( pDst ) || hTexture == s_hRenderTarget || pDst->m_bCube )
		return;
	Texture_t *pSrc = CurrentTarget();
	if ( !pSrc->m_nTexture )
		return;
	// clip the copy to both surfaces (no scaling here)
	if ( sx < 0 ) { dx -= sx; sw += sx; sx = 0; }
	if ( sy < 0 ) { dy -= sy; sh += sy; sy = 0; }
	if ( dx < 0 ) { sx -= dx; sw += dx; dx = 0; }
	if ( dy < 0 ) { sy -= dy; sh += dy; dy = 0; }
	if ( sx + sw > pSrc->m_nWidth ) sw = pSrc->m_nWidth - sx;
	if ( sy + sh > pSrc->m_nHeight ) sh = pSrc->m_nHeight - sy;
	if ( dx + sw > pDst->m_nWidth ) sw = pDst->m_nWidth - dx;
	if ( dy + sh > pDst->m_nHeight ) sh = pDst->m_nHeight - dy;
	if ( sw <= 0 || sh <= 0 )
		return;
	glCopyImageSubData_( pSrc->m_nTexture, GL_TEXTURE_2D, 0, sx, sy, 0, pDst->m_nTexture, GL_TEXTURE_2D, 0, dx, dy, 0, sw, sh, 1 );
	pDst->m_bHasData = true;
}

//-----------------------------------------------------------------------------
// Occlusion queries (handle = index + 1)
//-----------------------------------------------------------------------------
struct Query_t
{
	GLuint m_nQuery;
	bool m_bUsed, m_bIssued;
};
static std::vector< Query_t > s_Queries;
static int s_nOpenQuery;

int GlR_CreateQuery()
{
	if ( !s_hGLRC )
		return 0;
	size_t i = 0;
	while ( i < s_Queries.size() && s_Queries[i].m_bUsed )
		++i;
	if ( i == s_Queries.size() )
	{
		Query_t q = {};
		s_Queries.push_back( q );
	}
	Query_t &q = s_Queries[i];
	if ( !q.m_nQuery )
		glGenQueries_( 1, &q.m_nQuery );
	q.m_bUsed = true;
	q.m_bIssued = false;
	return (int)i + 1;
}

void GlR_DestroyQuery( int hQuery )
{
	if ( hQuery == s_nOpenQuery )
		GlR_EndQuery( hQuery );
	if ( hQuery > 0 && hQuery <= (int)s_Queries.size() )
		s_Queries[hQuery - 1].m_bUsed = false;	// the GL object is reused
}

void GlR_DestroyAllQueries()
{
	if ( s_nOpenQuery )
		GlR_EndQuery( s_nOpenQuery );
	for ( size_t i = 0; i < s_Queries.size(); ++i )
		if ( s_Queries[i].m_nQuery )
			glDeleteQueries_( 1, &s_Queries[i].m_nQuery );
	s_Queries.clear();
}

void GlR_BeginQuery( int hQuery )
{
	if ( hQuery <= 0 || hQuery > (int)s_Queries.size() || !s_hGLRC )
		return;
	if ( s_nOpenQuery )
		GlR_EndQuery( s_nOpenQuery );
	glBeginQuery_( GL_SAMPLES_PASSED, s_Queries[hQuery - 1].m_nQuery );
	s_Queries[hQuery - 1].m_bIssued = true;
	s_nOpenQuery = hQuery;
}

void GlR_EndQuery( int hQuery )
{
	if ( hQuery != s_nOpenQuery || !s_hGLRC )
		return;
	glEndQuery_( GL_SAMPLES_PASSED );
	s_nOpenQuery = 0;
}

int GlR_GetQueryResult( int hQuery, bool bFlush )
{
	if ( hQuery <= 0 || hQuery > (int)s_Queries.size() || !s_Queries[hQuery - 1].m_bIssued || hQuery == s_nOpenQuery )
		return -2;	// OCCLUSION_QUERY_RESULT_ERROR
	GLint nAvailable = 0;
	glGetQueryObjectiv_( s_Queries[hQuery - 1].m_nQuery, GL_QUERY_RESULT_AVAILABLE, &nAvailable );
	if ( !nAvailable )
	{
		if ( bFlush )
			glFlush();
		return -1;	// OCCLUSION_QUERY_RESULT_PENDING
	}
	GLuint64 nPixels = 0;
	glGetQueryObjectui64v_( s_Queries[hQuery - 1].m_nQuery, GL_QUERY_RESULT, &nPixels );
	return nPixels > 0x7fffffff ? 0x7fffffff : (int)nPixels;
}

//-----------------------------------------------------------------------------
// Static mesh vertex cache
//-----------------------------------------------------------------------------
struct CachedVertices_t
{
	const void *m_pKey;
	int m_nVersion;
	GLuint m_nBuffer;
};
static std::vector< CachedVertices_t > s_VertexCache;

static CachedVertices_t *FindCached( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
		if ( s_VertexCache[i].m_pKey == pCacheKey )
			return &s_VertexCache[i];
	return NULL;
}

bool GlR_HasCachedVertices( const void *pCacheKey, int nVersion )
{
	CachedVertices_t *pCached = pCacheKey ? FindCached( pCacheKey ) : NULL;
	return pCached && pCached->m_nVersion == nVersion && pCached->m_nBuffer;
}

// NULL releases everything
void GlR_ReleaseCachedVertices( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); )
	{
		if ( pCacheKey && s_VertexCache[i].m_pKey != pCacheKey )
		{
			++i;
			continue;
		}
		if ( s_VertexCache[i].m_nBuffer && s_hGLRC )
			glDeleteBuffers_( 1, &s_VertexCache[i].m_nBuffer );
		s_VertexCache.erase( s_VertexCache.begin() + i );
		if ( pCacheKey )
			return;
	}
}

static GLuint CacheVertices( const void *pCacheKey, int nVersion, const GlRVertex_t *pVerts, int nVerts )
{
	CachedVertices_t *pCached = FindCached( pCacheKey );
	if ( pCached && pCached->m_nVersion == nVersion && pCached->m_nBuffer )
		return pCached->m_nBuffer;
	if ( !pVerts )
		return 0;
	if ( !pCached )
	{
		CachedVertices_t entry = {};
		entry.m_pKey = pCacheKey;
		s_VertexCache.push_back( entry );
		pCached = &s_VertexCache.back();
	}
	if ( pCached->m_nBuffer )
		glDeleteBuffers_( 1, &pCached->m_nBuffer );
	glGenBuffers_( 1, &pCached->m_nBuffer );
	glBindBuffer_( GL_COPY_WRITE_BUFFER, pCached->m_nBuffer );
	glBufferStorage_( GL_COPY_WRITE_BUFFER, (GLsizeiptr)nVerts * sizeof( GlRVertex_t ), pVerts, 0 );
	glBindBuffer_( GL_COPY_WRITE_BUFFER, 0 );
	pCached->m_nVersion = nVersion;
	return pCached->m_nBuffer;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------
static unsigned int s_nBoneSerial;	// the bones in streaming memory belong to this serial
static GLuint s_nBoneBuffer;
static size_t s_nBoneOffset;

static Texture_t *SampleableTexture( int hTexture, const Texture_t *pTarget )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( pTex && !pTex->m_nTexture )
		EnsureResource( pTex );
	if ( !pTex || !pTex->m_nTexture || !pTex->m_bHasData || pTex->m_nTexture == pTarget->m_nTexture )
		return NULL;
	return pTex;
}

void GlR_Draw( GlRPrimitive_t type, const GlRVertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey, int nVersion )
{
	if ( !s_nProgram || nVerts <= 0 )
		return;
	// big frames (level load) would grow the stream without bound: past this, wait for the GPU
	if ( StreamUsed() > 256 * 1024 * 1024 )
	{
		glFinish();
		ResetStream( s_nFrame );
	}
	Texture_t *pTarget = CurrentTarget();
	if ( !pTarget->m_nFBO )
		return;

	// textures; a texture without data (or the current target) samples white
	// (GL won't bind a cube where the shader samples a 2D texture)
	Texture_t *pTex = SampleableTexture( s_hBound, pTarget );
	if ( !pTex || pTex->m_bCube ) pTex = SampleableTexture( s_hWhite, pTarget );
	Texture_t *pLightmap = SampleableTexture( s_hLightmap, pTarget );
	Texture_t *pSecond = SampleableTexture( s_bWaterPass ? s_hWaterReflection : s_hLightmap, pTarget );
	if ( !pSecond || pSecond->m_bCube ) pSecond = SampleableTexture( s_hWhite, pTarget );
	Texture_t *pEnvmap = NULL;
	if ( s_bEnvmap )
		pEnvmap = SampleableTexture( GlR_IsCubeTexture( s_hStage1 ) ? s_hStage1 : s_hStage2, pTarget );
	if ( !pEnvmap || !pEnvmap->m_bCube ) pEnvmap = SampleableTexture( s_hWhiteCube, pTarget );
	if ( !pTex || !pSecond || !pEnvmap )
		return;

	// vertex data: cached for static meshes, streamed otherwise
	GLuint nVB = 0;
	size_t nVBOffset = 0;
	if ( pCacheKey )
	{
		nVB = CacheVertices( pCacheKey, nVersion, pVerts, nVerts );
	}
	else if ( pVerts )
	{
		unsigned char *p = StreamAlloc( nVerts * sizeof( GlRVertex_t ), 16, nVB, nVBOffset );
		if ( p ) memcpy( p, pVerts, nVerts * sizeof( GlRVertex_t ) );
		else nVB = 0;
	}
	if ( !nVB )
		return;
	GLuint nIB = 0;
	size_t nIBOffset = 0;
	if ( pIndices && nIndices > 0 )
	{
		unsigned char *p = StreamAlloc( nIndices * sizeof( unsigned short ), 16, nIB, nIBOffset );
		if ( !p )
			return;
		memcpy( p, pIndices, nIndices * sizeof( unsigned short ) );
	}

	// per-draw constants and the bone matrices
	GLuint nCB;
	size_t nCBOffset;
	Constants_t *pConst = (Constants_t *)StreamAlloc( sizeof( Constants_t ), s_nUniformAlign, nCB, nCBOffset );
	if ( !pConst )
		return;
	memset( pConst, 0, sizeof( Constants_t ) );
	FillConstants( pConst, pLightmap != NULL, pTarget->m_nWidth, pTarget->m_nHeight );
	if ( s_bBonesDirty || s_nBoneSerial != s_nSerial || !s_nBoneBuffer )
	{
		unsigned char *pBones = StreamAlloc( sizeof( s_Bones ), s_nUniformAlign, s_nBoneBuffer, s_nBoneOffset );
		if ( !pBones )
		{
			s_nBoneBuffer = 0;
			return;
		}
		memcpy( pBones, s_Bones, sizeof( s_Bones ) );
		s_bBonesDirty = false;
		s_nBoneSerial = s_nSerial;
	}

	BindFBO( pTarget->m_nFBO );
	ApplyState();
	glActiveTexture_( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, pTex->m_nTexture );
	glBindSampler_( 0, s_Samplers[( pTex->m_bClampS ? 1 : 0 ) | ( pTex->m_bClampT ? 2 : 0 )] );
	glActiveTexture_( GL_TEXTURE0 + 1 );
	glBindTexture( GL_TEXTURE_2D, pSecond->m_nTexture );
	glActiveTexture_( GL_TEXTURE0 + 2 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, pEnvmap->m_nTexture );
	glBindBufferRange_( GL_UNIFORM_BUFFER, 0, nCB, nCBOffset, sizeof( Constants_t ) );
	glBindBufferRange_( GL_UNIFORM_BUFFER, 1, s_nBoneBuffer, s_nBoneOffset, sizeof( s_Bones ) );

	if ( s_bViewportSet )
	{
		glViewport( s_Viewport[0], s_Viewport[1], s_Viewport[2], s_Viewport[3] );
		glDepthRange( s_flMinZ, s_flMaxZ );
	}
	else
	{
		glViewport( 0, 0, pTarget->m_nWidth, pTarget->m_nHeight );
		glDepthRange( 0.0, 1.0 );
	}

	++g_nGlRDraws;
	glBindVertexBuffer_( 0, nVB, nVBOffset, sizeof( GlRVertex_t ) );
	if ( nIB )
	{
		glBindBuffer_( GL_ELEMENT_ARRAY_BUFFER, nIB );
		glDrawElements( s_Topology[type], nIndices, GL_UNSIGNED_SHORT, (const void *)nIBOffset );
	}
	else
	{
		glDrawArrays( s_Topology[type], 0, nVerts );
	}
}
