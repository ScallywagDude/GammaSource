// GammaSource: native Direct3D 11 shader API, started from shaderapiempty; dx11device.cpp drives the device.
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "utlvector.h"
#include "materialsystem/imaterialsystem.h"
#include "IHardwareConfigInternal.h"
#include "shadersystem.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "mathlib/vmatrix.h"
#include "tier0/dbg.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/deformations.h"
#include "dx11device.h"
#include "dx11render.h"
#include "bitmap/imageformat.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "shaderapi/commandbuffer.h"
#include "imaterialinternal.h"
#include "materialsystem/imesh.h"

// GammaSource DX11: the vertex layout every mesh is written in (each attribute in its own
// slot of one interleaved vertex, so any vertex format the mesh builder asks for fits)
enum
{
	DX11V_POSITION = 0,
	DX11V_NORMAL = 12,
	DX11V_COLOR = 24,
	DX11V_SPECULAR = 28,
	DX11V_TEXCOORD = 32,	// 8 x float4
	DX11V_TANGENT_S = 160,
	DX11V_TANGENT_T = 172,
	DX11V_BONEWEIGHT = 184,	// float4
	DX11V_BONEINDEX = 200,	// 4 bytes
	DX11V_USERDATA = 204,	// float4
	DX11V_STRIDE = 220,
	DX11_MAX_VERTS = 65536,
	DX11_MAX_INDICES = 196608,
};


//-----------------------------------------------------------------------------
// The empty mesh
//-----------------------------------------------------------------------------
void MarkLitPass();

class CEmptyMesh : public IMesh
{
public:
	CEmptyMesh( bool bIsDynamic );
	virtual ~CEmptyMesh();

	// FIXME: Make this work! Unsupported methods of IIndexBuffer + IVertexBuffer
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc );
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc );
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc );
	virtual void ModifyEnd( IndexDesc_t& desc );
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc );
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc );
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc );
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc );
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc );
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc );
	virtual bool IsDynamic() const { return m_bIsDynamic; }
	virtual void BeginCastBuffer( VertexFormat_t format ) {}
	virtual void BeginCastBuffer( MaterialIndexFormat_t format ) {}
	virtual void EndCastBuffer( ) {}
	virtual int GetRoomRemaining() const { return 0; }
	virtual MaterialIndexFormat_t IndexFormat() const { return MATERIAL_INDEX_FORMAT_UNKNOWN; }

	void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc );
	void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc );

	void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyEnd( MeshDesc_t& desc );

	// returns the # of vertices (static meshes only)
	int  VertexCount() const;

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	 
	// Draws the entire mesh
	void Draw(int firstIndex, int numIndices);

	void Draw(CPrimList *pPrims, int nPrims);

	// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
	virtual void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder );

	// Spews the mesh data
	void Spew( int numVerts, int numIndices, const MeshDesc_t & desc );

	void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc );

	// gets the associated material
	IMaterial* GetMaterial();

	// static prop lighting: per-vertex colours from another mesh's specular slot
	// (nVertexOffset is in bytes: pooled colour meshes share one buffer)
	void SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
	{
		m_pColorMesh = static_cast< CEmptyMesh * >( pColorMesh );
		m_nColorMeshOffset = pColorMesh ? nVertexOffset : 0;
	}
	CEmptyMesh *m_pColorMesh;
	int m_nColorMeshOffset;
	int m_nStride;		// DX11V_STRIDE, or 4 for colour meshes (specular only, packed)


	virtual int IndexCount() const
	{
		return m_nIndexCount;
	}

	virtual void SetFlexMesh( IMesh *pMesh, int nVertexOffset ) {}

	virtual void DisableFlexMesh() {}

	virtual void MarkAsDrawn() {}

	virtual unsigned ComputeMemoryUsed() { return 0; }

	virtual VertexFormat_t GetVertexFormat() const { return VERTEX_POSITION; }

	virtual IMesh *GetMesh()
	{
		return this;
	}

	// GammaSource DX11: the mesh's data, kept in memory and handed to the device per pass
	unsigned char *m_pVertexMemory;
	unsigned short *m_pIndexMemory;
	int m_nVertexCapacity;
	int m_nIndexCapacity;
	// GetDynamicMesh( ..., pVertexOverride, pIndexOverride ): draw with another mesh's data
	CEmptyMesh *m_pVertexSource;
	CEmptyMesh *m_pIndexSource;
	void EnsureVertices( int nCount );
	void EnsureIndices( int nCount );
	int m_nVertexCount;
	int m_nIndexCount;
	int m_nDrawFirstIndex;
	int m_nDrawIndexCount;
	MaterialPrimitiveType_t m_PrimitiveType;
	void RenderPass();

private:
	bool m_bIsDynamic;
public:
	// static meshes from CreateStaticMesh: vertices stay on the GPU between draws
	bool m_bCacheable;
	int m_nVersion;
};


//-----------------------------------------------------------------------------
// The empty shader shadow
//-----------------------------------------------------------------------------
class CShaderShadowEmpty : public IShaderShadow
{
public:
	CShaderShadowEmpty();
	virtual ~CShaderShadowEmpty();

	// Sets the default *shadow* state
	void SetDefaultState();

	// Methods related to depth buffering
	void DepthFunc( ShaderDepthFunc_t depthFunc );
	void EnableDepthWrites( bool bEnable );
	void EnableDepthTest( bool bEnable );
	void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode );

	// Suppresses/activates color writing 
	void EnableColorWrites( bool bEnable );
	void EnableAlphaWrites( bool bEnable );

	// Methods related to alpha blending
	void EnableBlending( bool bEnable );
	void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );

	// Alpha testing
	void EnableAlphaTest( bool bEnable );
	void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ );

	// Wireframe/filled polygons
	void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode );

	// Back face culling
	void EnableCulling( bool bEnable );
	
	// constant color + transparency
	void EnableConstantColor( bool bEnable );

	// Indicates the vertex format for use with a vertex shader
	// The flags to pass in here come from the VertexFormatFlags_t enum
	// If pTexCoordDimensions is *not* specified, we assume all coordinates
	// are 2-dimensional
	void VertexShaderVertexFormat( unsigned int nFlags, 
		int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize );
	
	// Indicates we're going to light the model
	void EnableLighting( bool bEnable );
	void EnableSpecular( bool bEnable );

	// vertex blending
	void EnableVertexBlend( bool bEnable );

	// per texture unit stuff
	void OverbrightValue( TextureStage_t stage, float value );
	void EnableTexture( Sampler_t stage, bool bEnable );
	void EnableTexGen( TextureStage_t stage, bool bEnable );
	void TexGen( TextureStage_t stage, ShaderTexGenParam_t param );

	// alternate method of specifying per-texture unit stuff, more flexible and more complicated
	// Can be used to specify different operation per channel (alpha/color)...
	void EnableCustomPixelPipe( bool bEnable );
	void CustomTextureStages( int stageCount );
	void CustomTextureOperation( TextureStage_t stage, ShaderTexChannel_t channel, 
		ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 );

	// indicates what per-vertex data we're providing
	void DrawFlags( unsigned int drawFlags );

	// A simpler method of dealing with alpha modulation
	void EnableAlphaPipe( bool bEnable );
	void EnableConstantAlpha( bool bEnable );
	void EnableVertexAlpha( bool bEnable );
	void EnableTextureAlpha( TextureStage_t stage, bool bEnable );

	// GR - Separate alpha blending
	void EnableBlendingSeparateAlpha( bool bEnable );
	void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );

	// Sets the vertex and pixel shaders
	void SetVertexShader( const char *pFileName, int vshIndex );
	void SetPixelShader( const char *pFileName, int pshIndex );

	// Convert from linear to gamma color space on writes to frame buffer.
	void EnableSRGBWrite( bool bEnable )
	{
	}

	void EnableSRGBRead( Sampler_t stage, bool bEnable )
	{
	}

	virtual void FogMode( ShaderFogMode_t fogMode )
	{
		m_State.m_nFogMode = fogMode;
	}

	virtual void DisableFogGammaCorrection( bool bDisable )
	{
	}

	virtual void SetDiffuseMaterialSource( ShaderMaterialSource_t materialSource )
	{
	}

	virtual void SetMorphFormat( MorphFormat_t flags )
	{
	}

	virtual void EnableStencil( bool bEnable )
	{
	}
	virtual void StencilFunc( ShaderStencilFunc_t stencilFunc )
	{
	}
	virtual void StencilPassOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilFailOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilDepthFailOp( ShaderStencilOp_t stencilOp )
	{
	}
	virtual void StencilReference( int nReference )
	{
	}
	virtual void StencilMask( int nMask )
	{
	}
	virtual void StencilWriteMask( int nMask )
	{
	}

	virtual void ExecuteCommandBuffer( uint8 *pBuf ) 
	{
	}
	// Alpha to coverage
	void EnableAlphaToCoverage( bool bEnable );
	
	virtual void SetShadowDepthFiltering( Sampler_t stage )
	{
	}

	virtual void BlendOp( ShaderBlendOp_t blendOp ) {}
	virtual void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp ) {}

	bool m_IsTranslucent;
	bool m_IsAlphaTested;
	bool m_bIsDepthWriteEnabled;
	bool m_bUsesVertexAndPixelShaders;
	VertexFormat_t m_VertexUsage;	// what the pass's shader reads, as the DX9 backend reports it
	Dx11State_t m_State;	// GammaSource DX11: what the snapshot applies
};


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceEmpty : public IShaderDevice
{
public:
	CShaderDeviceEmpty() : m_DynamicMesh( true ), m_Mesh( false ) {}

	// Methods of IShaderDevice
	virtual int GetCurrentAdapter() const { return 0; }
	virtual bool IsUsingGraphics() const { return Dx11_IsActive(); }
	virtual void SpewDriverInfo() const;
	virtual ImageFormat GetBackBufferFormat() const { return IMAGE_FORMAT_BGRA8888; }
	virtual void GetBackBufferDimensions( int& width, int& height ) const;
	virtual int  StencilBufferBits() const { return 0; }
	virtual bool IsAAEnabled() const { return false; }
	virtual void Present( ) { Dx11_Present(); }
	virtual void GetWindowSize( int &width, int &height ) const;
	virtual bool AddView( void* hwnd );
	virtual void RemoveView( void* hwnd );
	virtual void SetView( void* hwnd );
	virtual void ReleaseResources();
	virtual void ReacquireResources();
	virtual IMesh* CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial = NULL );
	virtual void DestroyStaticMesh( IMesh* mesh );
	virtual IShaderBuffer* CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) { return NULL; }
	virtual VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer ) { return VERTEX_SHADER_HANDLE_INVALID; }
	virtual void DestroyVertexShader( VertexShaderHandle_t hShader ) {}
	virtual GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer ) { return GEOMETRY_SHADER_HANDLE_INVALID; }
	virtual void DestroyGeometryShader( GeometryShaderHandle_t hShader ) {}
	virtual PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) { return PIXEL_SHADER_HANDLE_INVALID; }
	virtual void DestroyPixelShader( PixelShaderHandle_t hShader ) {}
	virtual IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	virtual void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer );
	virtual IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	virtual void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer );
	virtual IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered );
	virtual IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered );
	virtual void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled ) {}
	virtual void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo ) {}
	virtual void RefreshFrontBufferNonInteractive( ) {}
	virtual void HandleThreadEvent( uint32 threadEvent ) {}

#ifdef DX_TO_GL_ABSTRACTION
	virtual void DoStartupShaderPreloading( void ) {}
#endif

	virtual char *GetDisplayDeviceName() OVERRIDE { return (char *)Dx11_AdapterName(); }

private:
	CEmptyMesh m_Mesh;
	CEmptyMesh m_DynamicMesh;
};

static CShaderDeviceEmpty s_ShaderDeviceEmpty;

// FIXME: Remove; it's for backward compat with the materialsystem only for now
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceEmpty, IShaderDevice, 
								  SHADER_DEVICE_INTERFACE_VERSION, s_ShaderDeviceEmpty )


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrEmpty : public IShaderDeviceMgr
{
public:
	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

public:
	// Methods of IShaderDeviceMgr
	virtual int	 GetAdapterCount() const;
	virtual void GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;
	virtual bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pKeyValues );
	virtual int	 GetModeCount( int adapter ) const;
	virtual void GetModeInfo( ShaderDisplayMode_t *pInfo, int nAdapter, int mode ) const;
	virtual void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;
	virtual bool SetAdapter( int nAdapter, int nFlags );
	virtual CreateInterfaceFn SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );
	virtual void AddModeChangeCallback( ShaderModeChangeCallbackFunc_t func ) {}
	virtual void RemoveModeChangeCallback( ShaderModeChangeCallbackFunc_t func ) {}
};

static CShaderDeviceMgrEmpty s_ShaderDeviceMgrEmpty;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceMgrEmpty, IShaderDeviceMgr, 
								  SHADER_DEVICE_MGR_INTERFACE_VERSION, s_ShaderDeviceMgrEmpty )


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader API
//-----------------------------------------------------------------------------
class CShaderAPIEmpty : public IShaderAPI, public IHardwareConfigInternal, public IDebugTextureInfo
{
public:
	// constructor, destructor
	CShaderAPIEmpty( );
	virtual ~CShaderAPIEmpty();

	// IDebugTextureInfo implementation.
public:

	virtual bool IsDebugTextureListFresh( int numFramesAllowed = 1 ) { return false; }
	virtual bool SetDebugTextureRendering( bool bEnable ) { return false; }
	virtual void EnableDebugTextureList( bool bEnable ) {}
	virtual void EnableGetAllTextures( bool bEnable ) {}
	virtual KeyValues* GetDebugTextureList() { return NULL; }
	virtual int GetTextureMemoryUsed( TextureMemoryType eTextureMemory ) { return 0; }

	// Methods of IShaderDynamicAPI
	virtual void GetBackBufferDimensions( int& width, int& height ) const
	{
		s_ShaderDeviceEmpty.GetBackBufferDimensions( width, height );
	}
	virtual void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
	{
		pInfo->m_bIsEnabled = false;
		pInfo->m_nLookupCount = 0;
		pInfo->m_flDefaultWeight = 0.0f;
	}


	// Methods of IShaderAPI
public:
	virtual void SetViewports( int nCount, const ShaderViewport_t* pViewports );
	virtual int GetViewports( ShaderViewport_t* pViewports, int nMax ) const;
	virtual void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	virtual void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	virtual void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	virtual void BindVertexShader( VertexShaderHandle_t hVertexShader ) {}
	virtual void BindGeometryShader( GeometryShaderHandle_t hGeometryShader ) {}
	virtual void BindPixelShader( PixelShaderHandle_t hPixelShader ) {}
	virtual void SetRasterState( const ShaderRasterState_t& state ) {}
	virtual void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) {}
	virtual bool OwnGPUResources( bool bEnable ) { return false; }

	virtual bool DoRenderTargetsNeedSeparateDepthBuffer() const;

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots();

	ShaderAPITextureHandle_t m_StdTextureHandles[TEXTURE_MAX_STD_TEXTURES];

	// Sets the mode...
	// the material system creates the device through here
	bool SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info )
	{
		return Dx11_Init( hwnd, info.m_DisplayMode.m_nWidth, info.m_DisplayMode.m_nHeight ) && Dx11R_Init();
	}

	void ChangeVideoMode( const ShaderDeviceInfo_t &info )
	{
		Dx11_Resize( info.m_DisplayMode.m_nWidth, info.m_DisplayMode.m_nHeight );
	}

	// Called when the dx support level has changed
	virtual void DXSupportLevelChanged() {}

	virtual void EnableUserClipTransformOverride( bool bEnable ) {}
	virtual void UserClipTransform( const VMatrix &worldToView ) {}

	// Sets the default *dynamic* state
	void SetDefaultState( );

	// Returns the snapshot id for the shader state
	StateSnapshot_t	 TakeSnapshot( );

	// Returns true if the state snapshot is transparent
	bool IsTranslucent( StateSnapshot_t id ) const;
	bool IsAlphaTested( StateSnapshot_t id ) const;
	bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const;
	virtual bool IsDepthWriteEnabled( StateSnapshot_t id ) const;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Begins a rendering pass that uses a state snapshot
	void BeginPass( StateSnapshot_t snapshot  );

	// Uses a state snapshot
	void UseSnapshot( StateSnapshot_t snapshot );

	// Use this to get the mesh builder that allows us to modify vertex data
	CMeshBuilder* GetVertexModifyBuilder();

	// Sets the color to modulate by
	void Color3f( float r, float g, float b );
	void Color3fv( float const* pColor );
	void Color4f( float r, float g, float b, float a );
	void Color4fv( float const* pColor );

	// Faster versions of color
	void Color3ub( unsigned char r, unsigned char g, unsigned char b );
	void Color3ubv( unsigned char const* rgb );
	void Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	void Color4ubv( unsigned char const* rgba );

	// Sets the lights
	void SetLight( int lightNum, const LightDesc_t& desc );
	void SetLightingOrigin( Vector vLightingOrigin );
	void SetAmbientLight( float r, float g, float b );
	void SetAmbientLightCube( Vector4D cube[6] );

	// Get the lights
	int GetMaxLights( void ) const;
	const LightDesc_t& GetLight( int lightNum ) const;

	// Render state for the ambient light cube (vertex shaders)
	void SetVertexShaderStateAmbientLightCube();
	void SetPixelShaderStateAmbientLightCube( int pshReg, bool bForceToBlack = false )
	{
		MarkLitPass();
	}

	float GetAmbientLightCubeLuminance(void)
	{
		return 0.0f;
	}

	void SetSkinningMatrices();

	// Lightmap texture binding
	void BindLightmap( TextureStage_t stage );
	void BindLightmapAlpha( TextureStage_t stage )
	{
	}
	void BindBumpLightmap( TextureStage_t stage );
	void BindFullbrightLightmap( TextureStage_t stage );
	void BindWhite( TextureStage_t stage );
	void BindBlack( TextureStage_t stage );
	void BindGrey( TextureStage_t stage );
	void BindFBTexture( TextureStage_t stage, int textureIdex );
	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t texID )
	{
		CopyRenderTargetToTextureEx( texID, 0, NULL, NULL );
	}

	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t texID, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
	{
		int w, h;
		Dx11R_GetTextureSize( (int)texID, w, h );
		Rect_t src = { 0, 0, w, h }, dst = { 0, 0, w, h };
		if ( pSrcRect ) src = *pSrcRect;
		if ( pDstRect ) dst = *pDstRect;
		Dx11R_CopyRenderTargetToTexture( (int)texID, src.x, src.y, src.width, src.height, dst.x, dst.y );
	}

	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect, Rect_t *pDstRect )
	{
	}

	// Special system flat normal map binding.
	void BindFlatNormalMap( TextureStage_t stage );
	void BindNormalizationCubeMap( TextureStage_t stage );
	void BindSignedNormalizationCubeMap( TextureStage_t stage );

	// Set the number of bone weights
	void SetNumBoneWeights( int numBones );
	void EnableHWMorphing( bool bEnable );

	// Flushes any primitives that are buffered
	void FlushBufferedPrimitives();

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride );

	IMesh* GetFlexMesh();

	// Renders a single pass of a material
	void RenderPass( int nPass, int nPassCount );

	// stuff related to matrix stacks
	void MatrixMode( MaterialMatrixMode_t matrixMode );
	void PushMatrix();
	void PopMatrix();
	void LoadMatrix( float *m );
	void LoadBoneMatrix( int boneIndex, const float *m );
	void MultMatrix( float *m );
	void MultMatrixLocal( float *m );
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );
	void LoadIdentity( void );
	void LoadCameraToWorld( void );
	void Ortho( double left, double top, double right, double bottom, double zNear, double zFar );
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );
	void PickMatrix( int x, int y, int width, int height );
	void Rotate( float angle, float x, float y, float z );
	void Translate( float x, float y, float z );
	void Scale( float x, float y, float z );
	void ScaleXY( float x, float y );

	// Fog methods...
	void FogMode( MaterialFogMode_t fogMode );
	void FogStart( float fStart );
	void FogEnd( float fEnd );
	void SetFogZ( float fogZ );
	void FogMaxDensity( float flMaxDensity );
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );
	void FogColor3f( float r, float g, float b );
	void FogColor3fv( float const* rgb );
	void FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void FogColor3ubv( unsigned char const* rgb );

	virtual void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	virtual void SceneFogMode( MaterialFogMode_t fogMode );
	virtual void GetSceneFogColor( unsigned char *rgb );
	virtual MaterialFogMode_t GetSceneFogMode( );
	virtual int GetPixelFogCombo( );

	void SetHeightClipZ( float z ); 
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ); 

	void SetClipPlane( int index, const float *pPlane );
	void EnableClipPlane( int index, bool bEnable );

	void SetFastClipPlane( const float *pPlane );
	void EnableFastClip( bool bEnable );
	
	// We use smaller dynamic VBs during level transitions, to free up memory
	virtual int  GetCurrentDynamicVBSize( void );
	virtual void DestroyVertexBuffers( bool bExitingLevel = false );

	// Sets the vertex and pixel shaders
	void SetVertexShaderIndex( int vshIndex );
	void SetPixelShaderIndex( int pshIndex );

	// Sets the constant register for vertex and pixel shaders
	void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numConst = 1, bool bForce = false );
	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numConst = 1, bool bForce = false );
	void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );

	void InvalidateDelayedShaderConstants( void );

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const;
	float LinearToGamma_HardwareSpecific( float fLinear ) const;

	//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
	void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );

	// Cull mode
	void CullMode( MaterialCullMode_t cullMode );

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable );

	// Forces Z buffering on or off
	void OverrideDepthEnable( bool bEnable, bool bDepthEnable );
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	// Sets the shade mode
	void ShadeMode( ShaderShadeMode_t mode );

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial );

	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const;
 	ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt ) const;

	// Sets the texture state
	void BindTexture( Sampler_t stage, ShaderAPITextureHandle_t textureHandle );

	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
	{
		Dx11R_SetRenderTarget( (int)colorTextureHandle );
	}

	void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
	{
		if ( nRenderTargetID == 0 )
			Dx11R_SetRenderTarget( (int)colorTextureHandle );
	}

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	void ModifyTexture( ShaderAPITextureHandle_t textureHandle );

	// Texture management methods
	void TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
							 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData );
	void TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
							 ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData );

	void TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame );

	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
									int width, int height, CPixelWriter& writer );
	void TexUnlock( );
	
	// These are bound to the texture, not the texture environment
	void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );
	void TexSetPriority( int priority );	

	ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName );
	// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
	void CreateTextures( 
		ShaderAPITextureHandle_t *pHandles,
		int count,
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName );
	ShaderAPITextureHandle_t CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture );
	void DeleteTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );

	// stuff that isn't to be used from within a shader
	void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );
	void PerformFullScreenStencilOperation( void );
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat );
	virtual void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );

	// Selection mode methods
	int SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();

	void FlushHardware();
	void ResetRenderState( bool bFullReset = true );

	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );

	// Can we download textures?
	virtual bool CanDownloadTextures() const;

	// Board-independent calls, here to unify how shaders set state
	// Implementations should chain back to IShaderUtil->BindTexture(), etc.

	// Use this to begin and end the frame
	void BeginFrame();
	void EndFrame();

	// returns current time
	double CurrentTime() const;

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float * pPos ) const;

	// Members of IMaterialSystemHardwareConfig
	bool HasDestAlphaBuffer() const;
	bool HasStencilBuffer() const;
	virtual int  MaxViewports() const;
	virtual void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport ) {}
	virtual int  GetShadowFilterMode() const;
	int  StencilBufferBits() const;
	int	 GetFrameBufferColorDepth() const;
	int  GetSamplerCount() const;
	bool HasSetDeviceGammaRamp() const;
	bool SupportsCompressedTextures() const;
	VertexCompressionType_t SupportsCompressedVertices() const;
	bool SupportsVertexAndPixelShaders() const;
	bool SupportsPixelShaders_1_4() const;
	bool SupportsPixelShaders_2_0() const;
	bool SupportsPixelShaders_2_b() const;
	bool ActuallySupportsPixelShaders_2_b() const;
	bool SupportsStaticControlFlow() const;
	bool SupportsVertexShaders_2_0() const;
	bool SupportsShaderModel_3_0() const;
	int  MaximumAnisotropicLevel() const;
	int  MaxTextureWidth() const;
	int  MaxTextureHeight() const;
	int  MaxTextureAspectRatio() const;
	int  GetDXSupportLevel() const;
	const char *GetShaderDLLName() const
	{
		return "UNKNOWN";
	}
	int	 TextureMemorySize() const;
	bool SupportsOverbright() const;
	bool SupportsCubeMaps() const;
	bool SupportsMipmappedCubemaps() const;
	bool SupportsNonPow2Textures() const;
	int  GetTextureStageCount() const;
	int	 NumVertexShaderConstants() const;
	int	 NumBooleanVertexShaderConstants() const;
	int	 NumIntegerVertexShaderConstants() const;
	int	 NumPixelShaderConstants() const;
	int	 MaxNumLights() const;
	bool SupportsHardwareLighting() const;
	int	 MaxBlendMatrices() const;
	int	 MaxBlendMatrixIndices() const;
	int	 MaxVertexShaderBlendMatrices() const;
	int	 MaxUserClipPlanes() const;
	bool UseFastClipping() const
	{
		return false;
	}
	bool SpecifiesFogColorInLinearSpace() const;
	virtual bool SupportsSRGB() const;
	virtual bool FakeSRGBWrite() const;
	virtual bool CanDoSRGBReadFromRTs() const;
	virtual bool SupportsGLMixedSizeTargets() const;

	const char *GetHWSpecificShaderDLLName() const;
	bool NeedsAAClamp() const
	{
		return false;
	}
	bool SupportsSpheremapping() const;
	virtual int MaxHWMorphBatchCount() const { return 0; }

	// This is the max dx support level supported by the card
	virtual int	 GetMaxDXSupportLevel() const;

	bool ReadPixelsFromFrontBuffer() const;
	bool PreferDynamicTextures() const;
	virtual bool PreferReducedFillrate() const;
	bool HasProjectedBumpEnv() const;
	void ForceHardwareSync( void );
	
	int GetCurrentNumBones( void ) const;
	bool IsHWMorphingEnabled( void ) const;
	int GetCurrentLightCombo( void ) const;
	void GetDX9LightState( LightState_t *state ) const;
	MaterialFogMode_t GetCurrentFogType( void ) const;

	void RecordString( const char *pStr );

	void EvictManagedResources();

	void SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected );
	void DisableTextureTransform( TextureStage_t textureStage )
	{
	}
	void SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 );

	// Gets the lightmap dimensions
	virtual void GetLightmapDimensions( int *w, int *h );

	virtual void SyncToken( const char *pToken );

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	virtual void SetStandardVertexShaderConstants( float fOverbright )
	{
	}
	
	// Level of anisotropic filtering
	virtual void SetAnisotropicLevel( int nAnisotropyLevel );

	bool SupportsHDR() const
	{
		return false;
	}
	HDRType_t GetHDRType() const
	{
		return HDR_TYPE_NONE;
	}
	HDRType_t GetHardwareHDRType() const
	{
		return HDR_TYPE_NONE;
	}
	virtual bool NeedsATICentroidHack() const
	{
		return false;
	}
	// static props draw with vrad's per-vertex lighting from a colour mesh, plus dynamic lights
	virtual bool SupportsColorOnSecondStream() const
	{
		return true;
	}
	virtual bool SupportsStaticPlusDynamicLighting() const
	{
		return true;
	}
	virtual bool SupportsStreamOffset() const
	{
		return false;
	}
	void SetDefaultDynamicState()
	{
	}
	virtual void CommitPixelShaderLighting( int pshReg )
	{
		MarkLitPass();
	}

	// sprite glows and other proxies ask how much of them was visible last frame
	ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void )
	{
		int hQuery = Dx11R_CreateQuery();
		return hQuery ? (ShaderAPIOcclusionQuery_t)(intp)hQuery : INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
	}

	void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t handle )
	{
		Dx11R_DestroyQuery( (int)(intp)handle );
	}

	void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
	{
		Dx11R_BeginQuery( (int)(intp)handle );
	}

	void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
	{
		Dx11R_EndQuery( (int)(intp)handle );
	}

	int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t handle, bool bFlush )
	{
		return Dx11R_GetQueryResult( (int)(intp)handle, bFlush );
	}

	virtual void AcquireThreadOwnership() {}
	virtual void ReleaseThreadOwnership() {}

	virtual bool SupportsBorderColor() const { return false; }
	virtual bool SupportsFetch4() const { return false; }
	virtual bool CanStretchRectFromTextures( void ) const { return false; }
	virtual void EnableBuffer2FramesAhead( bool bEnable ) {}

	virtual void SetPSNearAndFarZ( int pshReg ) { }

	virtual void SetDepthFeatheringPixelShaderConstant( int iConstant, float fDepthBlendScale ) {}

	void SetPixelShaderFogParams( int reg )
	{
	}

	virtual bool InFlashlightMode() const
	{
		return false;
	}

	virtual bool InEditorMode() const
	{
		return false;
	}

	// What fields in the morph do we actually use?
	virtual MorphFormat_t ComputeMorphFormat( int numSnapshots, StateSnapshot_t* pIds ) const
	{
		return 0;
	}

	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	virtual MorphFormat_t GetBoundMorphFormat()
	{
		return 0;
	}

	// Binds a standard texture
	virtual void BindStandardTexture( Sampler_t stage, StandardTextureId_t id );

	virtual void BindStandardVertexTexture( VertexTextureSampler_t stage, StandardTextureId_t id )
	{
	}

	virtual void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
	{
		*pWidth = *pHeight = 0;
	}


	virtual void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
	{
	}

	virtual void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
	{
	}

	virtual const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const 
	{
		static FlashlightState_t  blah;
		return blah;
	}

	virtual const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const 
	{
		static FlashlightState_t  blah;
		return blah;
	}

	virtual void ClearVertexAndPixelShaderRefCounts()
	{
	}

	virtual void PurgeUnusedVertexAndPixelShaders()
	{
	}

	virtual bool IsAAEnabled() const
	{
		return false;
	}

	virtual int GetVertexTextureCount() const
	{
		return 0;
	}

	virtual int GetMaxVertexTextureDimension() const
	{
		return 0;
	}

	virtual int  MaxTextureDepth() const
	{
		return 0;
	}

	// Binds a vertex texture to a particular texture stage in the vertex pipe
	virtual void BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t hTexture )
	{
	}

	// Sets morph target factors
	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
	{
	}

	// NOTE: Stuff after this is added after shipping HL2.
	ITexture *GetRenderTargetEx( int nRenderTargetID )
	{
		return NULL;
	}

	void SetToneMappingScaleLinear( const Vector &scale )
	{
	}

	const Vector &GetToneMappingScaleLinear( void ) const
	{
		static Vector dummy;
		return dummy;
	}

	virtual float GetLightMapScaleFactor( void ) const
	{
		return 1.0;
	}


	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	virtual void HandleDeviceLost()
	{
	}

	virtual void EnableLinearColorSpaceFrameBuffer( bool bEnable )
	{
	}

	// Lets the shader know about the full-screen texture so it can 
	virtual void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
	{
	}

	void SetFloatRenderingParameter(int parm_number, float value)
	{
	}

	void SetIntRenderingParameter(int parm_number, int value)
	{
	}
	void SetVectorRenderingParameter(int parm_number, Vector const &value)
	{
	}

	float GetFloatRenderingParameter(int parm_number) const
	{
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const
	{
		return 0;
	}

	Vector GetVectorRenderingParameter(int parm_number) const
	{
		return Vector(0,0,0);
	}

	// Methods related to stencil
	void SetStencilEnable(bool onoff)
	{
	}

	void SetStencilFailOperation(StencilOperation_t op)
	{
	}

	void SetStencilZFailOperation(StencilOperation_t op)
	{
	}

	void SetStencilPassOperation(StencilOperation_t op)
	{
	}

	void SetStencilCompareFunction(StencilComparisonFunction_t cmpfn)
	{
	}

	void SetStencilReferenceValue(int ref)
	{
	}

	void SetStencilTestMask(uint32 msk)
	{
	}

	void SetStencilWriteMask(uint32 msk)
	{
	}

	void ClearStencilBufferRectangle( int xmin, int ymin, int xmax, int ymax,int value)
	{
	}

	virtual void GetDXLevelDefaults(uint &max_dxlevel,uint &recommended_dxlevel)
	{
		max_dxlevel=recommended_dxlevel=90;
	}

	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
	{
		*pMaxVerts = 32768;
		*pMaxIndices = 32768;
	}

	// Returns the max possible vertices + indices to render in a single draw call
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial )
	{
		return 32768;
	}

	virtual int GetMaxIndicesToRender( )
	{
		return 32768;
	}
	virtual int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 ) { return 0; }

	virtual void DisableAllLocalLights();

	virtual bool SupportsMSAAMode( int nMSAAMode ) { return false; }

	virtual bool SupportsCSAAMode( int nNumSamples, int nQualityLevel ) { return false; }

	// Hooks for firing PIX events from outside the Material System...
	virtual void BeginPIXEvent( unsigned long color, const char *szName ) {}
	virtual void EndPIXEvent() {}
	virtual void SetPIXMarker( unsigned long color, const char *szName ) {}

	virtual void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const {}

	virtual bool SupportsShadowDepthTextures() { return false; }

	virtual bool SupportsFetch4() { return false; }

	virtual int NeedsShaderSRGBConversion(void) const { return 0; }
	virtual bool UsesSRGBCorrectBlending() const { return false; }

	virtual bool HasFastVertexTextures() const { return false; }

	virtual void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) {}

	virtual void SetDisallowAccess( bool ) {}
	virtual void EnableShaderShaderMutex( bool ) {}
	virtual void ShaderLock() {}
	virtual void ShaderUnlock() {}

// ------------ New Vertex/Index Buffer interface ----------------------------
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions1 )
	{
	}
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
	{
	}
	void Draw( MaterialPrimitiveType_t primitiveType, int firstIndex, int numIndices )
	{
	}
// ------------ End ----------------------------

	virtual int  GetVertexBufferCompression( void ) const { return 0; };

	virtual bool ShouldWriteDepthToDestAlpha( void ) const { return false; };
	virtual bool SupportsHDRMode( HDRType_t nHDRMode ) const { return false; };
	virtual bool IsDX10Card() const { return false; };

	void PushDeformation( const DeformationBase_t *pDeformation )
	{
	}

	virtual void PopDeformation( )
	{
	}

	int GetNumActiveDeformations( ) const
	{
		return 0;
	}

	// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
	int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
										 float *pConstantValuesOut,
										 int nBufferSize,
										 int nMaximumDeformations,
										 int *pNumDefsOut ) const
	{
		*pNumDefsOut = 0;
		return 0;
	}

	void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t hTexture )
	{
		if ( nId >= 0 && nId < TEXTURE_MAX_STD_TEXTURES )
			m_StdTextureHandles[nId] = hTexture;
	}

	// The shaders' dynamic state (texture binds, constants) comes through here
	virtual void ExecuteCommandBuffer( uint8 *pData );
	// the fixed shaders are LDR: let the engine pick LDR skies and lightmaps
	virtual bool GetHDREnabled( void ) const { return false; }
	virtual void SetHDREnabled( bool bEnable ) {}

	virtual void CopyRenderTargetToScratchTexture( ShaderAPITextureHandle_t srcRt, ShaderAPITextureHandle_t dstTex, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) 
	{
	}

	// Allows locking and unlocking of very specific surface types.
	virtual void LockRect( void** pOutBits, int* pOutPitch, ShaderAPITextureHandle_t texHandle, int mipmap, int x, int y, int w, int h, bool bWrite, bool bRead ) 
	{
	}

	virtual void UnlockRect( ShaderAPITextureHandle_t texHandle, int mipmap )
	{
	}

	virtual void TexLodClamp( int finest ) {}

	virtual void TexLodBias( float bias ) {}

	virtual void CopyTextureToTexture( ShaderAPITextureHandle_t srcTex, ShaderAPITextureHandle_t dstTex ) {}
	
	void PrintfVA( char *fmt, va_list vargs ) {}
	void Printf( const char *fmt, ... ) {}
	float Knob( char *knobname, float *setvalue = NULL ) { return 0.0f; };

private:
	enum
	{
		TRANSLUCENT = 0x1,
		ALPHATESTED = 0x2,
		VERTEX_AND_PIXEL_SHADERS = 0x4,
		DEPTHWRITE = 0x8,
	};

	CEmptyMesh m_Mesh;
	ShaderViewport_t m_Viewport;

	void EnableAlphaToCoverage() {} ;
	void DisableAlphaToCoverage() {} ;

	ImageFormat GetShadowDepthTextureFormat() { return IMAGE_FORMAT_UNKNOWN; };
	ImageFormat GetNullTextureFormat() { return IMAGE_FORMAT_UNKNOWN; };
};


//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------

static CShaderAPIEmpty g_ShaderAPIEmpty;
static CShaderShadowEmpty g_ShaderShadow;

// FIXME: Remove; it's for backward compat with the materialsystem only for now
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIEmpty, IShaderAPI, 
									SHADERAPI_INTERFACE_VERSION, g_ShaderAPIEmpty )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderShadowEmpty, IShaderShadow, 
								SHADERSHADOW_INTERFACE_VERSION, g_ShaderShadow )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIEmpty, IMaterialSystemHardwareConfig, 
				MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, g_ShaderAPIEmpty )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIEmpty, IDebugTextureInfo, 
				DEBUG_TEXTURE_INFO_VERSION, g_ShaderAPIEmpty )


//-----------------------------------------------------------------------------
// The main GL Shader util interface
//-----------------------------------------------------------------------------
IShaderUtil* g_pShaderUtil;


//-----------------------------------------------------------------------------
// Factory to return from SetMode
//-----------------------------------------------------------------------------
static void* ShaderInterfaceFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_INTERFACE_VERSION ) )
		return static_cast< IShaderDevice* >( &s_ShaderDeviceEmpty );
	if ( !Q_stricmp( pInterfaceName, SHADERAPI_INTERFACE_VERSION ) )
		return static_cast< IShaderAPI* >( &g_ShaderAPIEmpty );
	if ( !Q_stricmp( pInterfaceName, SHADERSHADOW_INTERFACE_VERSION ) )
		return static_cast< IShaderShadow* >( &g_ShaderShadow );

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//
// CShaderDeviceMgrEmpty
//
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrEmpty::Connect( CreateInterfaceFn factory )
{
	// So others can access it
	g_pShaderUtil = (IShaderUtil*)factory( SHADER_UTIL_INTERFACE_VERSION, NULL );

	return true;
}

void CShaderDeviceMgrEmpty::Disconnect()
{
	g_pShaderUtil = NULL;
}

void *CShaderDeviceMgrEmpty::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION ) )
		return static_cast< IShaderDeviceMgr* >( this );
	if ( !Q_stricmp( pInterfaceName, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION ) )
		return static_cast< IMaterialSystemHardwareConfig* >( &g_ShaderAPIEmpty );
	return NULL;
}

InitReturnVal_t CShaderDeviceMgrEmpty::Init()
{
	return INIT_OK;
}

void CShaderDeviceMgrEmpty::Shutdown()
{
	Dx11R_Shutdown();
	Dx11_Shutdown();

}

// Sets the adapter
bool CShaderDeviceMgrEmpty::SetAdapter( int nAdapter, int nFlags )
{
	return true;
}

// FIXME: Is this a public interface? Might only need to be private to shaderapi
CreateInterfaceFn CShaderDeviceMgrEmpty::SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode )
{
	if ( !Dx11_Init( hWnd, mode.m_DisplayMode.m_nWidth, mode.m_DisplayMode.m_nHeight ) || !Dx11R_Init() )
	{
		Warning( "shaderapidx11: could not create a Direct3D 11 device\n" );
		return NULL;
	}
	return ShaderInterfaceFactory;
}

// Gets the number of adapters...
int	 CShaderDeviceMgrEmpty::GetAdapterCount() const
{
	return 1;
}

bool CShaderDeviceMgrEmpty::GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pKeyValues ) 
{
	return true;
}

// Returns info about each adapter
void CShaderDeviceMgrEmpty::GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
{
	memset( &info, 0, sizeof( info ) );
	Q_strncpy( info.m_pDriverName, "Direct3D 11", sizeof( info.m_pDriverName ) );
	info.m_nDXSupportLevel = 95;
	info.m_nMaxDXSupportLevel = 95;
}

// Returns the number of modes
int	 CShaderDeviceMgrEmpty::GetModeCount( int nAdapter ) const
{
	return 1;	// the desktop mode; windowed and borderless follow the window
}

// Returns mode information..
static void DesktopMode( ShaderDisplayMode_t *pInfo )
{
	Dx11_GetDesktopSize( pInfo->m_nWidth, pInfo->m_nHeight );
	pInfo->m_Format = IMAGE_FORMAT_BGRA8888;
	pInfo->m_nRefreshRateNumerator = 60;
	pInfo->m_nRefreshRateDenominator = 1;
}

void CShaderDeviceMgrEmpty::GetModeInfo( ShaderDisplayMode_t *pInfo, int nAdapter, int nMode ) const
{
	DesktopMode( pInfo );
}

void CShaderDeviceMgrEmpty::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const
{
	DesktopMode( pInfo );
}


//-----------------------------------------------------------------------------
//
// Shader device empty
//
//-----------------------------------------------------------------------------
void CShaderDeviceEmpty::GetWindowSize( int &width, int &height ) const
{
	Dx11_GetWindowSize( width, height );
}

void CShaderDeviceEmpty::GetBackBufferDimensions( int& width, int& height ) const
{
	Dx11_GetBackBufferSize( width, height );
	if ( width <= 0 || height <= 0 )
	{
		width = 1024;
		height = 768;
	}
}

// Use this to spew information about the 3D layer 
void CShaderDeviceEmpty::SpewDriverInfo() const
{
	Warning("Empty shader\n");
}

// Creates/ destroys a child window
bool CShaderDeviceEmpty::AddView( void* hwnd )
{
	return true;
}

void CShaderDeviceEmpty::RemoveView( void* hwnd )
{
}

// Activates a view
void CShaderDeviceEmpty::SetView( void* hwnd )
{
}

void CShaderDeviceEmpty::ReleaseResources()
{
}

void CShaderDeviceEmpty::ReacquireResources()
{
}

// Creates/destroys Mesh
IMesh* CShaderDeviceEmpty::CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	CEmptyMesh *pMesh = new CEmptyMesh( false );
	pMesh->m_bCacheable = true;
	// static prop colour meshes: the engine writes 4 byte colours back to back
	if ( fmt == VERTEX_SPECULAR )
		pMesh->m_nStride = 4;
	return pMesh;
}

void CShaderDeviceEmpty::DestroyStaticMesh( IMesh* mesh )
{
	if ( mesh && mesh != &m_Mesh && mesh != &m_DynamicMesh )
		delete static_cast< CEmptyMesh * >( mesh );
}

// Creates/destroys static vertex + index buffers
IVertexBuffer *CShaderDeviceEmpty::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup )
{
	return ( type == SHADER_BUFFER_TYPE_STATIC || type == SHADER_BUFFER_TYPE_STATIC_TEMP ) ? &m_Mesh : &m_DynamicMesh;
}

void CShaderDeviceEmpty::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{

}

IIndexBuffer *CShaderDeviceEmpty::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup )
{
	switch( bufferType )
	{
	case SHADER_BUFFER_TYPE_STATIC:
	case SHADER_BUFFER_TYPE_STATIC_TEMP:
		return &m_Mesh;
	default:
		Assert( 0 );
	case SHADER_BUFFER_TYPE_DYNAMIC:
	case SHADER_BUFFER_TYPE_DYNAMIC_TEMP:
		return &m_DynamicMesh;
	}
}

void CShaderDeviceEmpty::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{

}

IVertexBuffer *CShaderDeviceEmpty::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	return &m_DynamicMesh;
}

IIndexBuffer *CShaderDeviceEmpty::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	return &m_Mesh;
}



//-----------------------------------------------------------------------------
//
// The empty mesh...
//
//-----------------------------------------------------------------------------
CEmptyMesh::CEmptyMesh( bool bIsDynamic ) : m_bIsDynamic( bIsDynamic )
{
	// static meshes get their memory when they are first locked, sized to their data
	m_pVertexMemory = NULL;
	m_pIndexMemory = NULL;
	m_nVertexCapacity = m_nIndexCapacity = 0;
	m_pVertexSource = m_pIndexSource = NULL;
	m_bCacheable = false;
	m_nVersion = 0;
	m_pColorMesh = NULL;
	m_nColorMeshOffset = 0;
	m_nStride = DX11V_STRIDE;
	if ( bIsDynamic )
	{
		EnsureVertices( DX11_MAX_VERTS );
		EnsureIndices( DX11_MAX_INDICES );
	}
	m_nVertexCount = m_nIndexCount = 0;
	m_nDrawFirstIndex = 0;
	m_nDrawIndexCount = -1;
	m_PrimitiveType = MATERIAL_TRIANGLES;
}

CEmptyMesh::~CEmptyMesh()
{
	Dx11R_ReleaseCachedVertices( this );
	delete[] m_pVertexMemory;
	delete[] m_pIndexMemory;
}

void CEmptyMesh::EnsureVertices( int nCount )
{
	if ( nCount <= m_nVertexCapacity )
		return;
	unsigned char *pNew = new unsigned char[m_nStride * nCount];
	if ( m_pVertexMemory )
	{
		memcpy( pNew, m_pVertexMemory, m_nStride * m_nVertexCapacity );
		delete[] m_pVertexMemory;
	}
	m_pVertexMemory = pNew;
	m_nVertexCapacity = nCount;
}

void CEmptyMesh::EnsureIndices( int nCount )
{
	if ( nCount <= m_nIndexCapacity )
		return;
	unsigned short *pNew = new unsigned short[nCount];
	if ( m_pIndexMemory )
	{
		memcpy( pNew, m_pIndexMemory, sizeof( unsigned short ) * m_nIndexCapacity );
		delete[] m_pIndexMemory;
	}
	m_pIndexMemory = pNew;
	m_nIndexCapacity = nCount;
}

bool CEmptyMesh::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )
{
	EnsureIndices( max( nMaxIndexCount, 1 ) );
	desc.m_pIndices = m_pIndexMemory;
	desc.m_nIndexSize = 1;	// in shorts
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CEmptyMesh::Unlock( int nWrittenIndexCount, IndexDesc_t& desc )
{
	m_nIndexCount = clamp( nWrittenIndexCount, 0, m_nIndexCapacity );
}

void CEmptyMesh::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	nFirstIndex = max( nFirstIndex, 0 );
	EnsureIndices( max( nFirstIndex + nIndexCount, 1 ) );
	desc.m_pIndices = m_pIndexMemory + nFirstIndex;
	desc.m_nIndexSize = 1;	// in shorts
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
}

void CEmptyMesh::ModifyEnd( IndexDesc_t& desc )
{
}

void CEmptyMesh::Spew( int nIndexCount, const IndexDesc_t & desc )
{
}

void CEmptyMesh::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
}

// Points a vertex description at interleaved vertices starting at pBase
static void SetVertexDesc( unsigned char *pBase, VertexDesc_t &desc, int nStride = DX11V_STRIDE )
{
	if ( nStride == 4 )
	{
		// a colour mesh: only the specular colour is real; the rest write to scratch
		static unsigned char s_Scratch[DX11V_STRIDE];
		SetVertexDesc( s_Scratch, desc );
		desc.m_VertexSize_Position = desc.m_VertexSize_BoneWeight = desc.m_VertexSize_BoneMatrixIndex = 0;
		desc.m_VertexSize_Normal = desc.m_VertexSize_Color = desc.m_VertexSize_TangentS = 0;
		desc.m_VertexSize_TangentT = desc.m_VertexSize_UserData = 0;
		for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
			desc.m_VertexSize_TexCoord[i] = 0;
		desc.m_pSpecular = pBase;
		desc.m_VertexSize_Specular = 4;
		desc.m_ActualVertexSize = 4;
		return;
	}
	desc.m_pPosition = (float*)( pBase + DX11V_POSITION );
	desc.m_pNormal = (float*)( pBase + DX11V_NORMAL );
	desc.m_pColor = pBase + DX11V_COLOR;
	desc.m_pSpecular = pBase + DX11V_SPECULAR;

	int i;
	for ( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i)
	{
		desc.m_pTexCoord[i] = (float*)( pBase + DX11V_TEXCOORD + i * 16 );
		desc.m_VertexSize_TexCoord[i] = DX11V_STRIDE;
	}

	desc.m_pBoneWeight = (float*)( pBase + DX11V_BONEWEIGHT );
	desc.m_pBoneMatrixIndex = pBase + DX11V_BONEINDEX;
	desc.m_pTangentS = (float*)( pBase + DX11V_TANGENT_S );
	desc.m_pTangentT = (float*)( pBase + DX11V_TANGENT_T );
	desc.m_pUserData = (float*)( pBase + DX11V_USERDATA );
	desc.m_NumBoneWeights = 3;

	desc.m_VertexSize_Position = DX11V_STRIDE;
	desc.m_VertexSize_BoneWeight = DX11V_STRIDE;
	desc.m_VertexSize_BoneMatrixIndex = DX11V_STRIDE;
	desc.m_VertexSize_Normal = DX11V_STRIDE;
	desc.m_VertexSize_Color = DX11V_STRIDE;
	desc.m_VertexSize_Specular = DX11V_STRIDE;
	desc.m_VertexSize_TangentS = DX11V_STRIDE;
	desc.m_VertexSize_TangentT = DX11V_STRIDE;
	desc.m_VertexSize_UserData = DX11V_STRIDE;
	desc.m_ActualVertexSize = DX11V_STRIDE;

}

bool CEmptyMesh::Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
{
	EnsureVertices( max( nVertexCount, 1 ) );
	nVertexCount = clamp( nVertexCount, 0, m_nVertexCapacity );
	// Formats without colour or texture coordinates leave those slots alone; make them neutral
	for ( int v = 0; m_nStride == DX11V_STRIDE && v < nVertexCount; ++v )
	{
		unsigned char *pVert = m_pVertexMemory + v * DX11V_STRIDE;
		*(unsigned int *)( pVert + DX11V_COLOR ) = 0xFFFFFFFF;
		memset( pVert + DX11V_TEXCOORD, 0, 8 );
		memset( pVert + DX11V_TEXCOORD + 32, 0, 8 );		// texcoord 2: the bumped lightmap offset
		memset( pVert + DX11V_TANGENT_S, 0, 24 );
		memset( pVert + DX11V_USERDATA, 0, 16 );
	}

	SetVertexDesc( m_pVertexMemory, desc, m_nStride );
	desc.m_nFirstVertex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CEmptyMesh::Unlock( int nVertexCount, VertexDesc_t &desc )
{
	m_nVertexCount = clamp( nVertexCount, 0, m_nVertexCapacity );
	++m_nVersion;
}

void CEmptyMesh::Spew( int nVertexCount, const VertexDesc_t &desc ) 
{
}

void CEmptyMesh::ValidateData( int nVertexCount, const VertexDesc_t & desc )
{
}

void CEmptyMesh::LockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
	Lock( numVerts, false, *static_cast<VertexDesc_t*>( &desc ) );
	Lock( numIndices, false, *static_cast<IndexDesc_t*>( &desc ) );
}

void CEmptyMesh::UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
	Unlock( numVerts, *static_cast<VertexDesc_t*>( &desc ) );
	Unlock( numIndices, *static_cast<IndexDesc_t*>( &desc ) );
}

// Modify edits existing data in place: point at the requested range and keep what is there
void CEmptyMesh::ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	firstVertex = max( firstVertex, 0 );
	EnsureVertices( max( firstVertex + numVerts, 1 ) );
	SetVertexDesc( m_pVertexMemory + firstVertex * m_nStride, *static_cast<VertexDesc_t*>( &desc ), m_nStride );
	desc.m_nFirstVertex = firstVertex;
	static_cast<VertexDesc_t&>( desc ).m_nOffset = 0;
	ModifyBegin( bReadOnly, firstIndex, numIndices, *static_cast<IndexDesc_t*>( &desc ) );
}

void CEmptyMesh::ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	ModifyBeginEx( false, firstVertex, numVerts, firstIndex, numIndices, desc );
}

void CEmptyMesh::ModifyEnd( MeshDesc_t& desc )
{
	++m_nVersion;
}

// returns the # of vertices (static meshes only)
int CEmptyMesh::VertexCount() const
{
	return m_nVertexCount;
}

// Sets the primitive type
void CEmptyMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	m_PrimitiveType = type;
}

extern void Dx11_DrawMesh( CEmptyMesh *pMesh );

// Draws the entire mesh
void CEmptyMesh::Draw( int firstIndex, int numIndices )
{
	m_nDrawFirstIndex = firstIndex;
	m_nDrawIndexCount = numIndices;
	Dx11_DrawMesh( this );
}

// One pass of the bound material: convert to the device's vertex and draw
void CEmptyMesh::RenderPass()
{
	extern int g_nDx11EmptyMeshes;
	const CEmptyMesh *pVB = m_pVertexSource ? m_pVertexSource : this;
	const CEmptyMesh *pIB = m_pIndexSource ? m_pIndexSource : this;
	const unsigned char *pVertexMemory = pVB->m_pVertexMemory;
	const unsigned short *pIndexMemory = pIB->m_pIndexMemory;
	int nVertexCount = pVB->m_nVertexCount;
	int nIndexCount = pIB->m_nIndexCount;
	if ( nVertexCount <= 0 )
	{
		++g_nDx11EmptyMeshes;
		return;
	}

	static Dx11Vertex_t s_Verts[DX11_MAX_VERTS];
	static unsigned short s_Indices[DX11_MAX_INDICES];
	// 16 bit indices reach 65536 vertices
	int nVerts = min( nVertexCount, (int)DX11_MAX_VERTS );
	// a colour mesh gives each instance its own colours, so those draws aren't cached
	const CEmptyMesh *pColors = ( m_pColorMesh && m_pColorMesh->m_pVertexMemory ) ? m_pColorMesh : NULL;
	const void *pCacheKey = ( pVB->m_bCacheable && !pColors ) ? pVB : NULL;
	int nVersion = pVB->m_nVersion;
	bool bCached = pCacheKey && Dx11R_HasCachedVertices( pCacheKey, nVersion );
	for ( int v = 0; !bCached && v < nVerts; ++v )
	{
		const unsigned char *pSrc = pVertexMemory + v * DX11V_STRIDE;
		Dx11Vertex_t &dst = s_Verts[v];
		memcpy( dst.m_Pos, pSrc + DX11V_POSITION, 12 );
		// the mesh builder stores colours as B G R A; the device reads R G B A
		const unsigned char *c = pSrc + DX11V_COLOR;
		dst.m_Color = c[2] | ( c[1] << 8 ) | ( c[0] << 16 ) | ( c[3] << 24 );
		memcpy( dst.m_UV, pSrc + DX11V_TEXCOORD, 8 );
		memcpy( dst.m_UV2, pSrc + DX11V_TEXCOORD + 16, 8 );
		memcpy( dst.m_Normal, pSrc + DX11V_NORMAL, 12 );
		memcpy( dst.m_BoneIndex, pSrc + DX11V_BONEINDEX, 4 );
		memcpy( dst.m_BoneWeight, pSrc + DX11V_BONEWEIGHT, 8 );
		// the tangent frame: brushes and displacements write S and T, models S and T's sign in userdata
		const float *pUser = (const float *)( pSrc + DX11V_USERDATA );
		if ( pUser[3] != 0.0f )
		{
			const float *n = dst.m_Normal;
			memcpy( dst.m_TangentS, pUser, 12 );
			dst.m_TangentT[0] = ( n[1] * pUser[2] - n[2] * pUser[1] ) * pUser[3];
			dst.m_TangentT[1] = ( n[2] * pUser[0] - n[0] * pUser[2] ) * pUser[3];
			dst.m_TangentT[2] = ( n[0] * pUser[1] - n[1] * pUser[0] ) * pUser[3];
		}
		else
		{
			memcpy( dst.m_TangentS, pSrc + DX11V_TANGENT_S, 12 );
			memcpy( dst.m_TangentT, pSrc + DX11V_TANGENT_T, 12 );
		}
		memcpy( &dst.m_LightmapOffset, pSrc + DX11V_TEXCOORD + 32, 4 );
		int nColorByte = pColors ? m_nColorMeshOffset + v * pColors->m_nStride : 0;
		if ( pColors && nColorByte + pColors->m_nStride <= pColors->m_nVertexCapacity * pColors->m_nStride )
		{
			const unsigned char *s = pColors->m_pVertexMemory + nColorByte + ( pColors->m_nStride == 4 ? 0 : DX11V_SPECULAR );
			dst.m_Color = s[2] | ( s[1] << 8 ) | ( s[0] << 16 ) | ( 255u << 24 );
		}
	}

	// Draw( -1, 0 ) (the default) means the whole mesh
	bool bAll = m_nDrawFirstIndex < 0 || m_nDrawIndexCount <= 0;
	int nFirst = bAll ? 0 : m_nDrawFirstIndex;
	int nCount = bAll ? nIndexCount : m_nDrawIndexCount;
	if ( nFirst + nCount > nIndexCount )
		nCount = nIndexCount - nFirst;

	switch ( m_PrimitiveType )
	{
	case MATERIAL_QUADS:
	case MATERIAL_INSTANCED_QUADS:
		{
			int nQuads = nVerts / 4, n = 0;
			for ( int q = 0; q < nQuads && n + 6 <= DX11_MAX_INDICES; ++q )
			{
				unsigned short b = (unsigned short)( q * 4 );
				s_Indices[n++] = b; s_Indices[n++] = b + 1; s_Indices[n++] = b + 2;
				s_Indices[n++] = b; s_Indices[n++] = b + 2; s_Indices[n++] = b + 3;
			}
			Dx11R_Draw( DX11_TRIANGLES, bCached ? NULL : s_Verts, nVerts, s_Indices, n, pCacheKey, nVersion );
		}
		break;
	case MATERIAL_POLYGON:
		{
			int n = 0;
			for ( int i = 1; i + 1 < nVerts && n + 3 <= DX11_MAX_INDICES; ++i )
			{
				s_Indices[n++] = 0; s_Indices[n++] = (unsigned short)i; s_Indices[n++] = (unsigned short)( i + 1 );
			}
			Dx11R_Draw( DX11_TRIANGLES, bCached ? NULL : s_Verts, nVerts, s_Indices, n, pCacheKey, nVersion );
		}
		break;
	case MATERIAL_LINE_LOOP:
		{
			int n = 0;
			for ( int i = 0; i < nVerts && n + 2 <= DX11_MAX_INDICES; ++i )
			{
				s_Indices[n++] = (unsigned short)i; s_Indices[n++] = (unsigned short)( ( i + 1 ) % nVerts );
			}
			Dx11R_Draw( DX11_LINES, bCached ? NULL : s_Verts, nVerts, s_Indices, n, pCacheKey, nVersion );
		}
		break;
	default:
		{
			Dx11Primitive_t type = DX11_TRIANGLES;
			if ( m_PrimitiveType == MATERIAL_POINTS ) type = DX11_POINTS;
			else if ( m_PrimitiveType == MATERIAL_LINES ) type = DX11_LINES;
			else if ( m_PrimitiveType == MATERIAL_TRIANGLE_STRIP ) type = DX11_TRIANGLE_STRIP;
			else if ( m_PrimitiveType == MATERIAL_LINE_STRIP ) type = DX11_LINE_STRIP;
			if ( nCount > 0 )
				Dx11R_Draw( type, bCached ? NULL : s_Verts, nVerts, pIndexMemory + nFirst, nCount, pCacheKey, nVersion );
			else if ( nIndexCount == 0 )
				Dx11R_Draw( type, bCached ? NULL : s_Verts, nVerts, NULL, 0, pCacheKey, nVersion );
		}
		break;
	}
}

void CEmptyMesh::Draw(CPrimList *pPrims, int nPrims)
{
	for ( int i = 0; i < nPrims; ++i )
		Draw( pPrims[i].m_FirstIndex, pPrims[i].m_NumIndices );
}

// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
void CEmptyMesh::CopyToMeshBuilder( 
	int iStartVert,		// Which vertices to copy.
	int nVerts, 
	int iStartIndex,	// Which indices to copy.
	int nIndices, 
	int indexOffset,	// This is added to each index.
	CMeshBuilder &builder )
{
}

// Spews the mesh data
void CEmptyMesh::Spew( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

void CEmptyMesh::ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

// gets the associated material
IMaterial* CEmptyMesh::GetMaterial()
{
	// umm. this don't work none
	Assert(0);
	return 0;
}

//-----------------------------------------------------------------------------
// The shader shadow interface
//-----------------------------------------------------------------------------
CShaderShadowEmpty::CShaderShadowEmpty()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
}

CShaderShadowEmpty::~CShaderShadowEmpty()
{
}

// Sets the default *shadow* state
void CShaderShadowEmpty::SetDefaultState()
{
	m_IsTranslucent = false;
	m_IsAlphaTested = false;
	m_bIsDepthWriteEnabled = true;
	m_bUsesVertexAndPixelShaders = false;
	m_VertexUsage = 0;
	memset( &m_State, 0, sizeof( m_State ) );
	m_State.m_nSrcBlend = SHADER_BLEND_ONE;
	m_State.m_nDstBlend = SHADER_BLEND_ZERO;
	m_State.m_bDepthTest = true;
	m_State.m_bDepthWrite = true;
	m_State.m_nDepthFunc = SHADER_DEPTHFUNC_NEAREROREQUAL;
	m_State.m_bCull = true;
	m_State.m_flAlphaRef = 0.5f;
}

// Methods related to depth buffering
void CShaderShadowEmpty::DepthFunc( ShaderDepthFunc_t depthFunc )
{
	m_State.m_nDepthFunc = depthFunc;
}

void CShaderShadowEmpty::EnableDepthWrites( bool bEnable )
{
	m_bIsDepthWriteEnabled = bEnable;
	m_State.m_bDepthWrite = bEnable;
}

void CShaderShadowEmpty::EnableDepthTest( bool bEnable )
{
	m_State.m_bDepthTest = bEnable;
}

void CShaderShadowEmpty::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
	m_State.m_bDecalOffset = nOffsetMode != SHADER_POLYOFFSET_DISABLE;
}

// Suppresses/activates color writing 
void CShaderShadowEmpty::EnableColorWrites( bool bEnable )
{
	m_State.m_bNoColorWrites = !bEnable;
}

// Suppresses/activates alpha writing 
void CShaderShadowEmpty::EnableAlphaWrites( bool bEnable )
{
	m_State.m_bNoAlphaWrites = !bEnable;
}

// Methods related to alpha blending
void CShaderShadowEmpty::EnableBlending( bool bEnable )
{
	m_IsTranslucent = bEnable;
	m_State.m_bBlend = bEnable;
}

void CShaderShadowEmpty::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
	m_State.m_nSrcBlend = srcFactor;
	m_State.m_nDstBlend = dstFactor;
}

// A simpler method of dealing with alpha modulation
void CShaderShadowEmpty::EnableAlphaPipe( bool bEnable )
{
}

void CShaderShadowEmpty::EnableConstantAlpha( bool bEnable )
{
}

void CShaderShadowEmpty::EnableVertexAlpha( bool bEnable )
{
}

void CShaderShadowEmpty::EnableTextureAlpha( TextureStage_t stage, bool bEnable )
{
}


// Alpha testing
void CShaderShadowEmpty::EnableAlphaTest( bool bEnable )
{
	m_IsAlphaTested = bEnable;
	m_State.m_bAlphaTest = bEnable;
}

void CShaderShadowEmpty::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
	m_State.m_flAlphaRef = alphaRef;
}


// Wireframe/filled polygons
void CShaderShadowEmpty::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
}


// Back face culling
void CShaderShadowEmpty::EnableCulling( bool bEnable )
{
	m_State.m_bCull = bEnable;
}


// Alpha to coverage
void CShaderShadowEmpty::EnableAlphaToCoverage( bool bEnable )
{
}


// constant color + transparency
void CShaderShadowEmpty::EnableConstantColor( bool bEnable )
{
}

// Indicates the vertex format for use with a vertex shader
// The flags to pass in here come from the VertexFormatFlags_t enum
// If pTexCoordDimensions is *not* specified, we assume all coordinates
// are 2-dimensional
void CShaderShadowEmpty::VertexShaderVertexFormat( unsigned int nFlags, 
												   int nTexCoordCount,
												   int* pTexCoordDimensions,
												   int nUserDataSize )
{
	// shaders that light their vertices ask for normals; unlit ones (UnlitGeneric, sprites) don't
	m_State.m_bNormals = ( nFlags & VERTEX_NORMAL ) != 0;

	// the vertex format the material reports (built like the DX9 backend's CMeshMgr::ComputeVertexFormat)
	VertexFormat_t fmt = nFlags & ~( VERTEX_FORMAT_USE_EXACT_FORMAT | VERTEX_FORMAT_COMPRESSED );
	if ( NumBoneWeights( nFlags ) > 0 )
		fmt = ( fmt & ~VERTEX_BONE_WEIGHT_MASK ) | VERTEX_BONEWEIGHT( 2 );
	fmt |= VERTEX_USERDATA_SIZE( nUserDataSize );
	nTexCoordCount = min( nTexCoordCount, (int)VERTEX_MAX_TEXTURE_COORDINATES );
	for ( int i = 0; i < nTexCoordCount; ++i )
		fmt |= VERTEX_TEXCOORD_SIZE( i, pTexCoordDimensions ? pTexCoordDimensions[i] : 2 );
	m_VertexUsage = fmt;
	m_State.m_nBoneWeights = ( nFlags & VERTEX_BONE_INDEX ) ? NumBoneWeights( nFlags ) : 0;
	m_State.m_bVertexColor = ( nFlags & VERTEX_COLOR ) != 0;
}

// Indicates we're going to light the model
void CShaderShadowEmpty::EnableLighting( bool bEnable )
{
}

void CShaderShadowEmpty::EnableSpecular( bool bEnable )
{
}

// Activate/deactivate skinning
void CShaderShadowEmpty::EnableVertexBlend( bool bEnable )
{
}

// per texture unit stuff
void CShaderShadowEmpty::OverbrightValue( TextureStage_t stage, float value )
{
}

void CShaderShadowEmpty::EnableTexture( Sampler_t stage, bool bEnable )
{
}

void CShaderShadowEmpty::EnableCustomPixelPipe( bool bEnable )
{
}

void CShaderShadowEmpty::CustomTextureStages( int stageCount )
{
}

void CShaderShadowEmpty::CustomTextureOperation( TextureStage_t stage, ShaderTexChannel_t channel, 
	ShaderTexOp_t op, ShaderTexArg_t arg1, ShaderTexArg_t arg2 )
{
}

void CShaderShadowEmpty::EnableTexGen( TextureStage_t stage, bool bEnable )
{
}

void CShaderShadowEmpty::TexGen( TextureStage_t stage, ShaderTexGenParam_t param )
{
}

// Sets the vertex and pixel shaders
void CShaderShadowEmpty::SetVertexShader( const char *pShaderName, int vshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowEmpty::EnableBlendingSeparateAlpha( bool bEnable )
{
}
void CShaderShadowEmpty::SetPixelShader( const char *pShaderName, int pshIndex )
{
	m_bUsesVertexAndPixelShaders = ( pShaderName != NULL );
}

void CShaderShadowEmpty::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
}
// indicates what per-vertex data we're providing
void CShaderShadowEmpty::DrawFlags( unsigned int drawFlags )
{
}



//-----------------------------------------------------------------------------
//
// Shader API Empty
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

CShaderAPIEmpty::CShaderAPIEmpty()  : m_Mesh( false )
{
	for ( int i = 0; i < TEXTURE_MAX_STD_TEXTURES; ++i )
		m_StdTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
}

CShaderAPIEmpty::~CShaderAPIEmpty()
{
}


bool CShaderAPIEmpty::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	return false;
}

// Can we download textures?
bool CShaderAPIEmpty::CanDownloadTextures() const
{
	return Dx11_IsActive();
}

// Used to clear the transition table when we know it's become invalid.
void CShaderAPIEmpty::ClearSnapshots()
{
}

// Members of IMaterialSystemHardwareConfig
bool CShaderAPIEmpty::HasDestAlphaBuffer() const
{
	return false;
}

bool CShaderAPIEmpty::HasStencilBuffer() const
{
	return false;
}

int CShaderAPIEmpty::MaxViewports() const
{
	return 1;
}

int CShaderAPIEmpty::GetShadowFilterMode() const
{
	return 0;
}

int CShaderAPIEmpty::StencilBufferBits() const
{
	return 0;
}

int	 CShaderAPIEmpty::GetFrameBufferColorDepth() const
{
	return 0;
}

int  CShaderAPIEmpty::GetSamplerCount() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 60))
		return 1;
	if (( ShaderUtil()->GetConfig().dxSupportLevel >= 60 ) && ( ShaderUtil()->GetConfig().dxSupportLevel < 80 ))
		return 2;
	return 4;
}

bool CShaderAPIEmpty::HasSetDeviceGammaRamp() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsCompressedTextures() const
{
	return false;
}

VertexCompressionType_t CShaderAPIEmpty::SupportsCompressedVertices() const
{
	return VERTEX_COMPRESSION_NONE;
}

bool CShaderAPIEmpty::SupportsVertexAndPixelShaders() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 80))
		return false;

	return true;
}

bool CShaderAPIEmpty::SupportsPixelShaders_1_4() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 81))
		return false;

	return true;
}

bool CShaderAPIEmpty::SupportsPixelShaders_2_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

bool CShaderAPIEmpty::SupportsPixelShaders_2_b() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

bool CShaderAPIEmpty::ActuallySupportsPixelShaders_2_b() const
{
	return true;
}

bool CShaderAPIEmpty::SupportsShaderModel_3_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
		(ShaderUtil()->GetConfig().dxSupportLevel < 95))
		return false;

	return true;
}

bool CShaderAPIEmpty::SupportsStaticControlFlow() const
{
	if ( IsOpenGL() )
		return false;

	return SupportsVertexShaders_2_0();
}

bool CShaderAPIEmpty::SupportsVertexShaders_2_0() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 90))
		return false;

	return true;
}

int  CShaderAPIEmpty::MaximumAnisotropicLevel() const
{
	return 0;
}

void CShaderAPIEmpty::SetAnisotropicLevel( int nAnisotropyLevel )
{
}

int  CShaderAPIEmpty::MaxTextureWidth() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIEmpty::MaxTextureHeight() const
{
	// Should be big enough to cover all cases
	return 16384;
}

int  CShaderAPIEmpty::MaxTextureAspectRatio() const
{
	// Should be big enough to cover all cases
	return 16384;
}


int	 CShaderAPIEmpty::TextureMemorySize() const
{
	// fake it
	return 64 * 1024 * 1024;
}

int  CShaderAPIEmpty::GetDXSupportLevel() const
{
	return 95;
}

bool CShaderAPIEmpty::SupportsOverbright() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsCubeMaps() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

bool CShaderAPIEmpty::SupportsNonPow2Textures() const
{
	return true;
}

bool CShaderAPIEmpty::SupportsMipmappedCubemaps() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

int  CShaderAPIEmpty::GetTextureStageCount() const
{
	return 4;
}

int	 CShaderAPIEmpty::NumVertexShaderConstants() const
{
	return 128;
}

int	 CShaderAPIEmpty::NumBooleanVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIEmpty::NumIntegerVertexShaderConstants() const
{
	return 0;
}

int	 CShaderAPIEmpty::NumPixelShaderConstants() const
{
	return 8;
}

int	 CShaderAPIEmpty::MaxNumLights() const
{
	return 4;
}

bool CShaderAPIEmpty::SupportsSpheremapping() const
{
	return false;
}


// This is the max dx support level supported by the card
int	CShaderAPIEmpty::GetMaxDXSupportLevel() const
{
	return 95;
}

bool CShaderAPIEmpty::SupportsHardwareLighting() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
		return false;

	return true;
}

int	 CShaderAPIEmpty::MaxBlendMatrices() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
	{
		return 1;
	}

	return 0;
}

int	 CShaderAPIEmpty::MaxBlendMatrixIndices() const
{
	if ((ShaderUtil()->GetConfig().dxSupportLevel > 0) &&
	    (ShaderUtil()->GetConfig().dxSupportLevel < 70))
	{
		return 1;
	}

	return 0;
}

int	 CShaderAPIEmpty::MaxVertexShaderBlendMatrices() const
{
	return 0;
}

int	CShaderAPIEmpty::MaxUserClipPlanes() const
{
	return 2;
}

bool CShaderAPIEmpty::SpecifiesFogColorInLinearSpace() const
{
	return false;
}

bool CShaderAPIEmpty::SupportsSRGB() const
{
	return false;
}

bool CShaderAPIEmpty::FakeSRGBWrite() const
{
	return false;
}

bool CShaderAPIEmpty::CanDoSRGBReadFromRTs() const
{
	return true;
}

bool CShaderAPIEmpty::SupportsGLMixedSizeTargets() const
{
	return false;
}

const char *CShaderAPIEmpty::GetHWSpecificShaderDLLName() const
{
	return 0;
}

// Sets the default *dynamic* state
void ResetPassLighting();

// a new material draw: nothing carries over from the previous one
void CShaderAPIEmpty::SetDefaultState()
{
	ResetPassLighting();
	Dx11R_BindLightmap( 0 );
}


// Returns the snapshot id for the shader state
// GammaSource DX11: snapshots are indices into this table; the low bits keep the old flags
struct Dx11Snapshot_t
{
	int m_nFlags;
	Dx11State_t m_State;
	VertexFormat_t m_VertexUsage;
};
static CUtlVector< Dx11Snapshot_t > s_Snapshots;

static const Dx11Snapshot_t *GetSnapshot( StateSnapshot_t id )
{
	return ( id >= 0 && id < s_Snapshots.Count() ) ? &s_Snapshots[(int)id] : NULL;
}

StateSnapshot_t	 CShaderAPIEmpty::TakeSnapshot( )
{
	Dx11Snapshot_t snap;
	memset( &snap, 0, sizeof( snap ) );
	if (g_ShaderShadow.m_IsTranslucent)
		snap.m_nFlags |= TRANSLUCENT;
	if (g_ShaderShadow.m_IsAlphaTested)
		snap.m_nFlags |= ALPHATESTED;
	if (g_ShaderShadow.m_bUsesVertexAndPixelShaders)
		snap.m_nFlags |= VERTEX_AND_PIXEL_SHADERS;
	if (g_ShaderShadow.m_bIsDepthWriteEnabled)
		snap.m_nFlags |= DEPTHWRITE;
	snap.m_State = g_ShaderShadow.m_State;
	snap.m_VertexUsage = g_ShaderShadow.m_VertexUsage;
	for ( int i = 0; i < s_Snapshots.Count(); ++i )
	{
		if ( !memcmp( &s_Snapshots[i], &snap, sizeof( snap ) ) )
			return i;
	}
	return s_Snapshots.AddToTail( snap );
}

// Returns true if the state snapshot is transparent
bool CShaderAPIEmpty::IsTranslucent( StateSnapshot_t id ) const
{
	const Dx11Snapshot_t *pSnap = GetSnapshot( id );
	return pSnap && ( pSnap->m_nFlags & TRANSLUCENT ) != 0; 
}

bool CShaderAPIEmpty::IsAlphaTested( StateSnapshot_t id ) const
{
	const Dx11Snapshot_t *pSnap = GetSnapshot( id );
	return pSnap && ( pSnap->m_nFlags & ALPHATESTED ) != 0; 
}

bool CShaderAPIEmpty::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	const Dx11Snapshot_t *pSnap = GetSnapshot( id );
	return pSnap && ( pSnap->m_nFlags & DEPTHWRITE ) != 0; 
}

bool CShaderAPIEmpty::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	const Dx11Snapshot_t *pSnap = GetSnapshot( id );
	return pSnap && ( pSnap->m_nFlags & VERTEX_AND_PIXEL_SHADERS ) != 0; 
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIEmpty::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return ComputeVertexUsage( numSnapshots, pIds );
}

// Gets the vertex format for a set of snapshot ids: every pass's needs combined
VertexFormat_t CShaderAPIEmpty::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const
{
	int nFlags = 0, nUserData = 0, nBones = 0;
	int texCoordSize[VERTEX_MAX_TEXTURE_COORDINATES] = {};
	for ( int i = 0; i < numSnapshots; ++i )
	{
		const Dx11Snapshot_t *pSnap = GetSnapshot( pIds[i] );
		if ( !pSnap )
			continue;
		VertexFormat_t fmt = pSnap->m_VertexUsage;
		nFlags |= VertexFlags( fmt );
		nBones = max( nBones, NumBoneWeights( fmt ) );
		nUserData = max( nUserData, UserDataSize( fmt ) );
		for ( int t = 0; t < VERTEX_MAX_TEXTURE_COORDINATES; ++t )
			texCoordSize[t] = max( texCoordSize[t], TexCoordSize( t, fmt ) );
	}
	VertexFormat_t fmt = nFlags | VERTEX_USERDATA_SIZE( nUserData );
	if ( nBones > 0 )
		fmt |= VERTEX_BONEWEIGHT( nBones );
	for ( int t = 0; t < VERTEX_MAX_TEXTURE_COORDINATES; ++t )
		if ( texCoordSize[t] )
			fmt |= VERTEX_TEXCOORD_SIZE( t, texCoordSize[t] );
	return fmt;
}

// Uses a state snapshot
static const Dx11Snapshot_t *s_pCurrentSnapshot;

void CShaderAPIEmpty::UseSnapshot( StateSnapshot_t snapshot )
{
	const Dx11Snapshot_t *pSnap = GetSnapshot( snapshot );
	s_pCurrentSnapshot = pSnap;
	if ( pSnap )
		Dx11R_SetState( pSnap->m_State );
}

// Sets the color to modulate by
void CShaderAPIEmpty::Color3f( float r, float g, float b )
{
}

void CShaderAPIEmpty::Color3fv( float const* pColor )
{
}

void CShaderAPIEmpty::Color4f( float r, float g, float b, float a )
{
}

void CShaderAPIEmpty::Color4fv( float const* pColor )
{
}

// Faster versions of color
void CShaderAPIEmpty::Color3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIEmpty::Color3ubv( unsigned char const* rgb )
{
}

void CShaderAPIEmpty::Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
}

void CShaderAPIEmpty::Color4ubv( unsigned char const* rgba )
{
}

// The shade mode
void CShaderAPIEmpty::ShadeMode( ShaderShadeMode_t mode )
{
}

// Binds a particular material to render with
void UpdateMatrices();
static IMaterialInternal *s_pBoundMaterial;
static float *CurMatrix( int nMode );
// kept for the few passes the fixed shader reproduces (the shadow colour)
static float s_PixelConstants[32][4];
// the shader specific vertex constants (VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 onwards)
static float s_VertexConstants[10][4];

// Eyes, EyeRefract and Teeth: the eye's projection and the teeth's darkening come from
// their vertex constants; the iris and glint from the stages the shader bound
static void ApplyCharacterShader( const char *pszShader )
{
	int nMode = 0;
	if ( pszShader && !Q_stricmp( pszShader, "EyeRefract" ) )
		nMode = 2;
	else if ( pszShader && !Q_strnicmp( pszShader, "Eyes", 4 ) )
		nMode = 1;
	else if ( pszShader && !Q_stricmp( pszShader, "Teeth" ) )
		nMode = 3;
	if ( !nMode )
	{
		Dx11R_SetCharacterPass( 0, NULL, 0.0f, 0, 0 );
		return;
	}
	Dx11R_SetCharacterPass( nMode, s_VertexConstants, nMode == 1 ? s_PixelConstants[0][1] : 0.0f,
		nMode != 3 ? Dx11R_GetBoundTexture( 1 ) : 0, nMode == 1 ? Dx11R_GetBoundTexture( 2 ) : 0 );
}

// the generic shaders use vertex colour only with $vertexcolor/$vertexalpha and modulate by
// $color/$alpha; other shaders (sprites, cables, particles, vgui) take vertex colour as is
static void ApplyVertexColorUse( IMaterialInternal *pMaterial, bool bColorMesh )
{
	const char *pszShader = pMaterial ? pMaterial->GetShaderName() : NULL;
	bool bGeneric = pszShader && ( Q_stristr( pszShader, "UnlitGeneric" ) || Q_stristr( pszShader, "VertexLitGeneric" ) ||
		Q_stristr( pszShader, "LightmappedGeneric" ) || Q_stristr( pszShader, "WorldVertexTransition" ) ||
		Q_stristr( pszShader, "TwoTexture" ) );
	if ( !bGeneric )
	{
		// sprites take their colour from pixel constant 0 ($color/rendercolor * alpha) and use
		// vertex colour only when their format has it ($ignorevertexcolors 0)
		if ( pszShader && Q_stristr( pszShader, "Sprite" ) && !Q_stristr( pszShader, "SpriteCard" ) )
		{
			bool bVertexColor = s_pCurrentSnapshot && s_pCurrentSnapshot->m_State.m_bVertexColor;
			Dx11R_SetVertexColorUse( bVertexColor, bVertexColor, s_PixelConstants[0] );
			return;
		}
		Dx11R_SetVertexColorUse( true, true, NULL );
		return;
	}
	float color[4];
	pMaterial->GetColorModulation( &color[0], &color[1], &color[2] );
	color[3] = pMaterial->GetAlphaModulation();
	// a static prop's colour mesh is its lighting, not a vertex colour
	bool bColor = bColorMesh || pMaterial->GetMaterialVarFlag( MATERIAL_VAR_VERTEXCOLOR );
	Dx11R_SetVertexColorUse( bColor, pMaterial->GetMaterialVarFlag( MATERIAL_VAR_VERTEXALPHA ), color );
}

static float VarFloat( IMaterialInternal *pMaterial, const char *pszName, float flDefault );

// VertexLitGeneric draws with the skin shader when $phong is on and there is a bump map or
// $basemapalphaphongmask (vertexlitgeneric_dx9_helper.cpp WantsSkinShader)
static bool IsSkinPass( IMaterialInternal *pMaterial, const char *pszShader )
{
	if ( !pMaterial || !pszShader || !Q_stristr( pszShader, "VertexLitGeneric" ) )
		return false;
	bool bFound = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$phong", &bFound, false );
	if ( !bFound || !pVar || !pVar->IsDefined() || !pVar->GetIntValue() )
		return false;
	pVar = pMaterial->FindVar( "$diffusewarp", &bFound, false );
	if ( bFound && pVar && pVar->IsDefined() && pVar->IsTexture() )
		return true;
	pVar = pMaterial->FindVar( "$basemapalphaphongmask", &bFound, false );
	if ( bFound && pVar && pVar->IsDefined() && pVar->GetIntValue() == 1 )
		return true;
	pVar = pMaterial->FindVar( "$bumpmap", &bFound, false );
	return bFound && pVar && pVar->IsDefined() && pVar->IsTexture();
}

// $envmap on the generic shaders: they bind the cube on stage 1 (VertexLitGeneric), 2
// (LightmappedGeneric) or 8 (the skin shader); the tint and mask come from the material
static bool ApplyEnvmap( IMaterialInternal *pMaterial, const char *pszShader )
{
	bool bGeneric = pszShader && ( Q_stristr( pszShader, "VertexLitGeneric" ) || Q_stristr( pszShader, "LightmappedGeneric" ) ||
		Q_stristr( pszShader, "WorldVertexTransition" ) );
	// the material must have an envmap: stages keep whatever an earlier material bound there
	bool bHasEnvmap = false;
	if ( bGeneric )
	{
		IMaterialVar *pEnvmap = pMaterial->FindVar( "$envmap", &bHasEnvmap, false );
		bHasEnvmap = bHasEnvmap && pEnvmap && pEnvmap->IsDefined() && pEnvmap->IsTexture();
	}
	bool bSkin = bHasEnvmap && IsSkinPass( pMaterial, pszShader );
	int hCube = 0;
	if ( bSkin )
		hCube = Dx11R_GetBoundTexture( 8 );
	else if ( bHasEnvmap )
		hCube = Dx11R_IsCubeTexture( Dx11R_GetBoundTexture( 1 ) ) ? Dx11R_GetBoundTexture( 1 ) : Dx11R_GetBoundTexture( 2 );
	if ( !bHasEnvmap || !Dx11R_IsCubeTexture( hCube ) )
	{
		Dx11R_SetEnvmap( false, NULL, 0, NULL );
		return false;
	}
	float tint[3] = { 1.0f, 1.0f, 1.0f };
	bool bFound = false;
	IMaterialVar *pTint = pMaterial->FindVar( "$envmaptint", &bFound, false );
	if ( bFound && pTint )
		pTint->GetVecValue( tint, 3 );
	int nMask = pMaterial->GetMaterialVarFlag( MATERIAL_VAR_BASEALPHAENVMAPMASK ) ? 1 : 0;
	// the mask's stage: VertexLitGeneric binds $envmapmask on 4 and $bumpmap on 3,
	// LightmappedGeneric / WorldVertexTransition $envmapmask on 5 and $bumpmap on 4
	bool bVertexLit = Q_stristr( pszShader, "VertexLitGeneric" ) != NULL;
	int hMask = 0;
	IMaterialVar *pEnvMask = pMaterial->FindVar( "$envmapmask", &bFound, false );
	if ( bFound && pEnvMask && pEnvMask->IsTexture() && pEnvMask->GetTextureValue() && !nMask )
	{
		hMask = Dx11R_GetBoundTexture( bVertexLit ? 4 : 5 );
		nMask = hMask ? 2 : 0;
	}
	if ( !nMask && pMaterial->GetMaterialVarFlag( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK ) )
	{
		hMask = Dx11R_GetBoundTexture( bVertexLit ? 3 : 4 );
		nMask = hMask ? 3 : 0;
	}
	// the skin shader masks by base alpha (not inverted), or with $normalmapalphaenvmapmask by the phong mask
	if ( bSkin )
	{
		hMask = 0;
		nMask = pMaterial->GetMaterialVarFlag( MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK ) ? 5 : 4;
	}

	// the eye: the inverse of the view matrix's translation (row vectors)
	float *v = CurMatrix( MATERIAL_VIEW );
	float eye[3];
	for ( int i = 0; i < 3; ++i )
		eye[i] = -( v[12] * v[i * 4 + 0] + v[13] * v[i * 4 + 1] + v[14] * v[i * 4 + 2] );
	Dx11R_SetEnvmap( true, tint, nMask, eye, hMask );
	float flContrast = 0.0f, flSaturation = 1.0f;
	IMaterialVar *pVar = pMaterial->FindVar( "$envmapcontrast", &bFound, false );
	if ( bFound && pVar && pVar->IsDefined() )
		flContrast = pVar->GetFloatValue();
	pVar = pMaterial->FindVar( "$envmapsaturation", &bFound, false );
	if ( bFound && pVar && pVar->IsDefined() )
		flSaturation = pVar->GetFloatValue();
	Dx11R_SetEnvmapParams( flContrast, flSaturation );
	Dx11R_SetEnvmapCube( hCube, bSkin, bSkin ? VarFloat( pMaterial, "$envmapfresnel", 0.0f ) : 0.0f );
	return nMask == 2 || nMask == 3;
}

// $selfillum on the generic shaders (after ApplyEnvmap, which may claim t3 for its mask)
static void ApplySelfIllum( IMaterialInternal *pMaterial, const char *pszShader, bool bEnvmapMask )
{
	bool bGeneric = pszShader && ( Q_stristr( pszShader, "VertexLitGeneric" ) || Q_stristr( pszShader, "LightmappedGeneric" ) );
	if ( !bGeneric || !pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ) )
	{
		Dx11R_SetSelfIllum( 0, NULL, 0 );
		return;
	}
	float tint[3] = { 1.0f, 1.0f, 1.0f };
	bool bFound = false;
	IMaterialVar *pTint = pMaterial->FindVar( "$selfillumtint", &bFound, false );
	if ( bFound && pTint && pTint->IsDefined() )
		pTint->GetVecValue( tint, 3 );
	// VertexLitGeneric binds $selfillummask on stage 11, the skin shader on 14
	int nMask = 1, hMask = 0;
	IMaterialVar *pMaskVar = pMaterial->FindVar( "$selfillummask", &bFound, false );
	if ( bFound && pMaskVar && pMaskVar->IsDefined() && pMaskVar->IsTexture() && !bEnvmapMask && Q_stristr( pszShader, "VertexLitGeneric" ) )
	{
		hMask = Dx11R_GetBoundTexture( IsSkinPass( pMaterial, pszShader ) ? 14 : 11 );
		if ( hMask )
			nMask = 2;
	}
	Dx11R_SetSelfIllum( nMask, tint, hMask );
}

// material variables, with defaults for the ones a material leaves out
static IMaterialVar *DefinedVar( IMaterialInternal *pMaterial, const char *pszName )
{
	bool bFound = false;
	IMaterialVar *pVar = pMaterial->FindVar( pszName, &bFound, false );
	return ( bFound && pVar && pVar->IsDefined() ) ? pVar : NULL;
}

static float VarFloat( IMaterialInternal *pMaterial, const char *pszName, float flDefault )
{
	IMaterialVar *pVar = DefinedVar( pMaterial, pszName );
	return pVar ? pVar->GetFloatValue() : flDefault;
}

static void VarVec( IMaterialInternal *pMaterial, const char *pszName, float *pOut, int nComps )
{
	IMaterialVar *pVar = DefinedVar( pMaterial, pszName );
	if ( !pVar )
		return;
	if ( pVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
		pVar->GetVecValue( pOut, nComps );
	else
		for ( int i = 0; i < nComps; ++i )
			pOut[i] = pVar->GetFloatValue();
}

static bool VarTexture( IMaterialInternal *pMaterial, const char *pszName )
{
	IMaterialVar *pVar = DefinedVar( pMaterial, pszName );
	return pVar && pVar->IsTexture() && pVar->GetTextureValue();
}

// the generic shaders' bump map, detail texture, $basetexture2, texture transform and phong, and
// expensive water's normal map; every other pass gets the default surface (all off)
static void ApplySurface( IMaterialInternal *pMaterial, const char *pszShader, bool bWater )
{
	Dx11R_Surface_t surf;
	memset( &surf, 0, sizeof( surf ) );
	surf.m_BaseTransform[0][0] = surf.m_BaseTransform[1][1] = 1.0f;
	surf.m_flDetailScale[0] = surf.m_flDetailScale[1] = 4.0f;
	surf.m_flDetailFactor = 1.0f;
	if ( pMaterial && pszShader && bWater )
	{
		// the water shader binds its normal map on stage 4 and its $bumptransform as vertex constants 1-2
		surf.m_nBump = 3;
		surf.m_hBump = Dx11R_GetBoundTexture( 4 );
		memcpy( surf.m_BaseTransform, s_VertexConstants[1], sizeof( surf.m_BaseTransform ) );
		surf.m_flWater[0] = VarFloat( pMaterial, "$refractamount", 0.0f );
		surf.m_flWater[1] = VarFloat( pMaterial, "$reflectamount", 0.8f );
	}
	else if ( pMaterial && pszShader )
	{
		bool bVertexLit = Q_stristr( pszShader, "VertexLitGeneric" ) != NULL;
		bool bUnlit = Q_stristr( pszShader, "UnlitGeneric" ) != NULL;
		bool bWorld = Q_stristr( pszShader, "LightmappedGeneric" ) || Q_stristr( pszShader, "WorldVertexTransition" );
		if ( bVertexLit || bUnlit || bWorld )
		{
			IMaterialVar *pTransform = DefinedVar( pMaterial, "$basetexturetransform" );
			if ( pTransform && pTransform->GetType() == MATERIAL_VAR_TYPE_MATRIX )
			{
				const VMatrix &m = pTransform->GetMatrixValue();
				for ( int j = 0; j < 4; ++j )
				{
					surf.m_BaseTransform[0][j] = m[0][j];
					surf.m_BaseTransform[1][j] = m[1][j];
				}
			}
			// VertexLitGeneric binds $bumpmap on stage 3, the world shaders on 4
			bool bPhong = IsSkinPass( pMaterial, pszShader );
			if ( ( ( bVertexLit || bWorld ) && VarTexture( pMaterial, "$bumpmap" ) ) || bPhong )
			{
				surf.m_nBump = bVertexLit ? 2 : 1;
				surf.m_hBump = Dx11R_GetBoundTexture( bVertexLit ? 3 : 4 );
				surf.m_bSSBump = bWorld && VarFloat( pMaterial, "$ssbump", 0.0f ) != 0.0f;
			}
			// $detail: stage 12 on the world shaders, 13 with phong (the skin shader), else 2
			if ( VarTexture( pMaterial, "$detail" ) )
			{
				surf.m_nDetail = (int)VarFloat( pMaterial, "$detailblendmode", 0.0f ) + 1;
				surf.m_hDetail = Dx11R_GetBoundTexture( bWorld ? 12 : ( bPhong ? 13 : 2 ) );
				VarVec( pMaterial, "$detailscale", surf.m_flDetailScale, 2 );
				surf.m_flDetailFactor = VarFloat( pMaterial, "$detailblendfactor", 1.0f );
			}
			if ( Q_stristr( pszShader, "WorldVertexTransition" ) && VarTexture( pMaterial, "$basetexture2" ) )
				surf.m_hBase2 = Dx11R_GetBoundTexture( 7 );
			if ( bPhong && surf.m_nBump == 2 )
			{
				surf.m_Phong[0] = VarFloat( pMaterial, "$phongexponent", VarTexture( pMaterial, "$phongexponenttexture" ) ? 20.0f : 5.0f );
				surf.m_Phong[1] = VarFloat( pMaterial, "$phongboost", 1.0f );
				surf.m_Phong[2] = VarFloat( pMaterial, "$basemapalphaphongmask", 0.0f ) != 0.0f ? 1.0f : 0.0f;
				surf.m_PhongFresnel[0] = 0.0f; surf.m_PhongFresnel[1] = 0.5f; surf.m_PhongFresnel[2] = 1.0f;
				VarVec( pMaterial, "$phongfresnelranges", surf.m_PhongFresnel, 3 );
				surf.m_PhongTint[0] = surf.m_PhongTint[1] = surf.m_PhongTint[2] = 1.0f;
				VarVec( pMaterial, "$phongtint", surf.m_PhongTint, 3 );
			}
		}
	}
	Dx11R_SetSurface( surf );
}

static bool IsVertexLitMaterial( IMaterialInternal *pMaterial )
{
	if ( !pMaterial )
		return false;
	const char *pszShader = pMaterial->GetShaderName();
	return pszShader && ( Q_stristr( pszShader, "VertexLit" ) || Q_stristr( pszShader, "Eye" ) ||
		Q_stristr( pszShader, "Teeth" ) || Q_stristr( pszShader, "Skin" ) );
}
static CEmptyMesh *s_pRenderMesh;

void CShaderAPIEmpty::Bind( IMaterial* pMaterial )
{
	s_pBoundMaterial = static_cast< IMaterialInternal * >( pMaterial );
}

void Dx11_DrawMesh( CEmptyMesh *pMesh )
{
	if ( !s_pBoundMaterial || !Dx11_IsActive() )
		return;
	if ( g_pShaderUtil && g_pShaderUtil->GetConfig().m_bSuppressRendering )
		return;
	extern int g_nDx11Meshes;
	++g_nDx11Meshes;
	s_pRenderMesh = pMesh;
	s_pBoundMaterial->DrawMesh( VERTEX_COMPRESSION_NONE );
	s_pRenderMesh = NULL;
}

// Cull mode
void CShaderAPIEmpty::CullMode( MaterialCullMode_t cullMode )
{
}

void CShaderAPIEmpty::ForceDepthFuncEquals( bool bEnable )
{
}

// Forces Z buffering on or off
void CShaderAPIEmpty::OverrideDepthEnable( bool bEnable, bool bDepthEnable )
{
}

void CShaderAPIEmpty::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
}

void CShaderAPIEmpty::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
}

//legacy fast clipping linkage
void CShaderAPIEmpty::SetHeightClipZ( float z )
{
}

void CShaderAPIEmpty::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
}

// GammaSource: lighting for vertex lit passes, handed to the device when such a pass draws
static LightDesc_t s_LightDescs[4];
static float s_AmbientCube[6][3];
static bool s_bLitPass;

void CShaderAPIEmpty::SetLight( int lightNum, const LightDesc_t& desc )
{
	if ( lightNum >= 0 && lightNum < 4 )
		memcpy( &s_LightDescs[lightNum], &desc, sizeof( desc ) );
}

// Sets lighting origin for the current model
void CShaderAPIEmpty::SetLightingOrigin( Vector vLightingOrigin )
{
}

void CShaderAPIEmpty::DisableAllLocalLights()
{
	for ( int i = 0; i < 4; ++i )
		s_LightDescs[i].m_Type = MATERIAL_LIGHT_DISABLE;
}

// the fixed-function ambient term; vertex lit shaders use the cube
void CShaderAPIEmpty::SetAmbientLight( float r, float g, float b )
{
}

void CShaderAPIEmpty::SetAmbientLightCube( Vector4D cube[6] )
{
	for ( int i = 0; i < 6; ++i )
	{
		s_AmbientCube[i][0] = cube[i].x; s_AmbientCube[i][1] = cube[i].y; s_AmbientCube[i][2] = cube[i].z;
	}
}

// Get lights
int CShaderAPIEmpty::GetMaxLights( void ) const
{
	return 4;
}

const LightDesc_t& CShaderAPIEmpty::GetLight( int lightNum ) const
{
	return s_LightDescs[( lightNum >= 0 && lightNum < 4 ) ? lightNum : 0];
}

// the pass lights its vertices (the shader asked for the ambient cube / lights)
void MarkLitPass()
{
	s_bLitPass = true;
}

void ResetPassLighting()
{
	s_bLitPass = false;
}

static void ApplyLighting()
{
	if ( !s_bLitPass )
	{
		Dx11R_SetLighting( false, NULL, 0, NULL );
		return;
	}
	Dx11Light_t lights[4];
	int nLights = 0;
	for ( int i = 0; i < 4; ++i )
	{
		const LightDesc_t &d = s_LightDescs[i];
		if ( d.m_Type == MATERIAL_LIGHT_DISABLE )
			continue;
		Dx11Light_t &l = lights[nLights++];
		l.m_nType = d.m_Type;
		l.m_Color[0] = d.m_Color.x; l.m_Color[1] = d.m_Color.y; l.m_Color[2] = d.m_Color.z;
		l.m_Pos[0] = d.m_Position.x; l.m_Pos[1] = d.m_Position.y; l.m_Pos[2] = d.m_Position.z;
		l.m_Dir[0] = d.m_Direction.x; l.m_Dir[1] = d.m_Direction.y; l.m_Dir[2] = d.m_Direction.z;
		l.m_flRange = d.m_Range;
		l.m_flAtten[0] = d.m_Attenuation0; l.m_flAtten[1] = d.m_Attenuation1; l.m_flAtten[2] = d.m_Attenuation2;
		l.m_flThetaDot = d.m_ThetaDot; l.m_flPhiDot = d.m_PhiDot; l.m_flFalloff = d.m_Falloff;
	}
	Dx11R_SetLighting( true, s_AmbientCube, nLights, lights );
}

// Render state for the ambient light cube (vertex shaders)
void CShaderAPIEmpty::SetVertexShaderStateAmbientLightCube()
{
	MarkLitPass();
}

void CShaderAPIEmpty::SetSkinningMatrices()
{
}

// Lightmap texture binding
void CShaderAPIEmpty::BindLightmap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindBumpLightmap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindFullbrightLightmap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindWhite( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindBlack( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindGrey( TextureStage_t stage )
{
}

// Gets the lightmap dimensions
void CShaderAPIEmpty::GetLightmapDimensions( int *w, int *h )
{
	g_pShaderUtil->GetLightmapDimensions( w, h );
}

// Special system flat normal map binding.
void CShaderAPIEmpty::BindFlatNormalMap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindSignedNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIEmpty::BindFBTexture( TextureStage_t stage, int textureIndex )
{
}

// Flushes any primitives that are buffered
void CShaderAPIEmpty::FlushBufferedPrimitives()
{
}

// Gets the dynamic mesh; note that you've got to render the mesh
// before calling this function a second time. Clients should *not*
// call DestroyStaticMesh on the mesh returned by this call.
IMesh* CShaderAPIEmpty::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	m_Mesh.m_pVertexSource = static_cast< CEmptyMesh * >( pVertexOverride );
	m_Mesh.m_pIndexSource = static_cast< CEmptyMesh * >( pIndexOverride );
	return &m_Mesh;
}

IMesh* CShaderAPIEmpty::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	return GetDynamicMesh( pMaterial, nHWSkinBoneCount, buffered, pVertexOverride, pIndexOverride );
}

IMesh* CShaderAPIEmpty::GetFlexMesh()
{
	return &m_Mesh;
}

// Begins a rendering pass that uses a state snapshot
void CShaderAPIEmpty::BeginPass( StateSnapshot_t snapshot  )
{
	UseSnapshot( snapshot );
}

// set while IShaderUtil binds the lightmap page for us
static bool s_bBindingLightmap;

// Renders a single pass of a material
void CShaderAPIEmpty::RenderPass( int nPass, int nPassCount )
{
	extern int g_nDx11Passes;
	++g_nDx11Passes;
	if ( s_pRenderMesh )
	{
		UpdateMatrices();
		// only vertex lit shaders light per vertex (world shaders use lightmaps, unlit ones
		// nothing); static props add vrad's baked lighting from their colour mesh
		bool bVertexLit = IsVertexLitMaterial( s_pBoundMaterial );
		if ( !bVertexLit )
			ResetPassLighting();
		ApplyLighting();
		Dx11R_SetStaticLight( bVertexLit && s_pRenderMesh->m_pColorMesh != NULL );
		const char *pszShader = s_pBoundMaterial ? s_pBoundMaterial->GetShaderName() : NULL;
		Dx11R_SetShadowPass( pszShader && !Q_stricmp( pszShader, "Shadow" ), s_PixelConstants[1] );
		ApplySelfIllum( s_pBoundMaterial, pszShader, ApplyEnvmap( s_pBoundMaterial, pszShader ) );
		ApplyCharacterShader( pszShader );
		// expensive water: the shader bound the refraction (0) and reflection (2) targets and set
		// the refract (c1) and reflect (c4) tints; cheap water (envmap) draws as before
		bool bWater = pszShader && !Q_strnicmp( pszShader, "Water", 5 ) && Dx11R_IsRenderTargetTexture( Dx11R_GetBoundTexture( 0 ) );
		Dx11R_SetWaterPass( bWater, Dx11R_GetBoundTexture( 2 ), s_PixelConstants[1], s_PixelConstants[4] );
		ApplySurface( s_pBoundMaterial, pszShader, bWater );
		// bloom: the downsample and blur shaders' taps, from their pixel constants
		int nPost = 0;
		float postParams[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		if ( pszShader && !Q_stricmp( pszShader, "Downsample_nohdr" ) )
		{
			nPost = 1;
			memcpy( postParams, s_PixelConstants[0], sizeof( postParams ) );
		}
		else if ( pszShader && !Q_stricmp( pszShader, "BlurFilterX" ) )
		{
			nPost = 2;
		}
		else if ( pszShader && !Q_stricmp( pszShader, "BlurFilterY" ) )
		{
			nPost = 3;
			postParams[0] = s_PixelConstants[3][0];
		}
		int nPostW = 0, nPostH = 0;
		if ( nPost )
			Dx11R_GetTextureSize( Dx11R_GetBoundTexture( 0 ), nPostW, nPostH );
		Dx11R_SetPostPass( nPost, postParams, nPostW, nPostH );
		ApplyVertexColorUse( s_pBoundMaterial, false );
		s_pRenderMesh->RenderPass();
		// the next pass binds its own lightmap / lighting if it has them
		Dx11R_BindLightmap( 0 );
		s_bLitPass = false;
	}
}

// stuff related to matrix stacks
// GammaSource DX11: D3D-style row-vector matrices, one stack per mode
static float s_Matrices[NUM_MATRIX_MODES][8][16];
static int s_nMatrixDepth[NUM_MATRIX_MODES];
static int s_nMatrixMode = MATERIAL_VIEW;
static bool s_bMatricesInit;

static float *CurMatrix( int nMode = -1 )
{
	if ( nMode < 0 ) nMode = s_nMatrixMode;
	return s_Matrices[nMode][s_nMatrixDepth[nMode]];
}

static void Identity( float *m )
{
	for ( int i = 0; i < 16; ++i ) m[i] = ( i % 5 ) ? 0.0f : 1.0f;
}

static void MatMul( const float *a, const float *b, float *out )
{
	float r[16];
	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 4; ++j )
			r[i * 4 + j] = a[i * 4 + 0] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
	memcpy( out, r, sizeof( r ) );
}

static void InitMatrices()
{
	if ( s_bMatricesInit ) return;
	s_bMatricesInit = true;
	for ( int m = 0; m < NUM_MATRIX_MODES; ++m )
		Identity( s_Matrices[m][0] );
}

void UpdateMatrices()
{
	InitMatrices();
	Dx11R_SetMatrices( CurMatrix( MATERIAL_MODEL ), CurMatrix( MATERIAL_VIEW ), CurMatrix( MATERIAL_PROJECTION ) );
}

// bone 0 is also the model matrix (as in the DX9 backend); skinned passes blend them all
static float s_BoneMatrices[DX11_MAX_BONES][12];
static int s_nBones;

void CShaderAPIEmpty::LoadBoneMatrix( int boneIndex, const float *m )
{
	if ( boneIndex < 0 || boneIndex >= DX11_MAX_BONES )
		return;
	memcpy( s_BoneMatrices[boneIndex], m, sizeof( s_BoneMatrices[0] ) );
	if ( boneIndex >= s_nBones )
		s_nBones = boneIndex + 1;
	if ( boneIndex == 0 )
	{
		MatrixMode( MATERIAL_MODEL );
		VMatrix transposeMatrix;
		transposeMatrix.Init( *(matrix3x4_t *)m );
		MatrixTranspose( transposeMatrix, transposeMatrix );
		LoadMatrix( (float *)transposeMatrix.m );
	}
	Dx11R_SetBoneMatrices( s_BoneMatrices, s_nBones );
}

void CShaderAPIEmpty::MatrixMode( MaterialMatrixMode_t matrixMode )
{
	InitMatrices();
	if ( matrixMode >= 0 && matrixMode < NUM_MATRIX_MODES )
		s_nMatrixMode = matrixMode;
}

void CShaderAPIEmpty::PushMatrix()
{
	InitMatrices();
	int &nDepth = s_nMatrixDepth[s_nMatrixMode];
	if ( nDepth < 7 )
	{
		memcpy( s_Matrices[s_nMatrixMode][nDepth + 1], s_Matrices[s_nMatrixMode][nDepth], sizeof( float ) * 16 );
		++nDepth;
	}
}

void CShaderAPIEmpty::PopMatrix()
{
	if ( s_nMatrixDepth[s_nMatrixMode] > 0 )
		--s_nMatrixDepth[s_nMatrixMode];
}

void CShaderAPIEmpty::LoadMatrix( float *m )
{
	InitMatrices();
	memcpy( CurMatrix(), m, sizeof( float ) * 16 );
}

void CShaderAPIEmpty::MultMatrix( float *m )
{
	InitMatrices();
	MatMul( CurMatrix(), m, CurMatrix() );
}

void CShaderAPIEmpty::MultMatrixLocal( float *m )
{
	InitMatrices();
	MatMul( m, CurMatrix(), CurMatrix() );
}

void CShaderAPIEmpty::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
	InitMatrices();
	if ( matrixMode >= 0 && matrixMode < NUM_MATRIX_MODES )
		memcpy( dst, CurMatrix( matrixMode ), sizeof( float ) * 16 );
}

void CShaderAPIEmpty::LoadIdentity( void )
{
	InitMatrices();
	Identity( CurMatrix() );
}

void CShaderAPIEmpty::LoadCameraToWorld( void )
{
}

void CShaderAPIEmpty::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
}

void CShaderAPIEmpty::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
}

void CShaderAPIEmpty::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
}

void CShaderAPIEmpty::PickMatrix( int x, int y, int width, int height )
{
}

void CShaderAPIEmpty::Rotate( float angle, float x, float y, float z )
{
}

void CShaderAPIEmpty::Translate( float x, float y, float z )
{
}

void CShaderAPIEmpty::Scale( float x, float y, float z )
{
}

void CShaderAPIEmpty::ScaleXY( float x, float y )
{
}

// Fog methods... GammaSource: the scene fog, handed to the renderer (linear; height fog
// below fog Z is drawn as linear fog)
static MaterialFogMode_t s_FogMode = MATERIAL_FOG_NONE;
static float s_flFogStart, s_flFogEnd, s_flFogZ, s_flFogMaxDensity = 1.0f;
static unsigned char s_FogColor[3];

static void UpdateSceneFog()
{
	Dx11R_SetSceneFog( s_FogMode != MATERIAL_FOG_NONE, s_flFogStart, s_flFogEnd, s_flFogMaxDensity, s_FogColor );
}

void CShaderAPIEmpty::FogMode( MaterialFogMode_t fogMode )
{
	s_FogMode = fogMode;
	UpdateSceneFog();
}

void CShaderAPIEmpty::FogStart( float fStart )
{
	s_flFogStart = fStart;
	UpdateSceneFog();
}

void CShaderAPIEmpty::FogEnd( float fEnd )
{
	s_flFogEnd = fEnd;
	UpdateSceneFog();
}

void CShaderAPIEmpty::SetFogZ( float fogZ )
{
	s_flFogZ = fogZ;
}

void CShaderAPIEmpty::FogMaxDensity( float flMaxDensity )
{
	s_flFogMaxDensity = flMaxDensity;
	UpdateSceneFog();
}

void CShaderAPIEmpty::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	if ( fStart ) *fStart = s_flFogStart;
	if ( fEnd ) *fEnd = s_flFogEnd;
	if ( fFogZ ) *fFogZ = s_flFogZ;
}


void CShaderAPIEmpty::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	s_FogColor[0] = r; s_FogColor[1] = g; s_FogColor[2] = b;
	UpdateSceneFog();
}


void CShaderAPIEmpty::SceneFogMode( MaterialFogMode_t fogMode )
{
	s_FogMode = fogMode;
	UpdateSceneFog();
}

void CShaderAPIEmpty::GetSceneFogColor( unsigned char *rgb )
{
	if ( rgb ) { rgb[0] = s_FogColor[0]; rgb[1] = s_FogColor[1]; rgb[2] = s_FogColor[2]; }
}

MaterialFogMode_t CShaderAPIEmpty::GetSceneFogMode( )
{
	return s_FogMode;
}

int CShaderAPIEmpty::GetPixelFogCombo( )
{
	return 0;
}

void CShaderAPIEmpty::FogColor3f( float r, float g, float b )
{
	FogColor3ub( (unsigned char)clamp( r * 255.0f, 0.0f, 255.0f ), (unsigned char)clamp( g * 255.0f, 0.0f, 255.0f ), (unsigned char)clamp( b * 255.0f, 0.0f, 255.0f ) );
}

void CShaderAPIEmpty::FogColor3fv( float const* rgb )
{
	FogColor3f( rgb[0], rgb[1], rgb[2] );
}

void CShaderAPIEmpty::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	SceneFogColor3ub( r, g, b );
}

void CShaderAPIEmpty::FogColor3ubv( unsigned char const* rgb )
{
	FogColor3ub( rgb[0], rgb[1], rgb[2] );
}

void CShaderAPIEmpty::SetViewports( int nCount, const ShaderViewport_t* pViewports )
{
	if ( nCount > 0 && pViewports )
	{
		m_Viewport = pViewports[0];
		Dx11R_SetViewport( m_Viewport.m_nTopLeftX, m_Viewport.m_nTopLeftY, m_Viewport.m_nWidth, m_Viewport.m_nHeight,
			m_Viewport.m_flMinZ, m_Viewport.m_flMaxZ );
	}
}

int CShaderAPIEmpty::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	if ( pViewports && nMax > 0 )
		pViewports[0] = m_Viewport;
	return 1;
}

// Sets the vertex and pixel shaders
void CShaderAPIEmpty::SetVertexShaderIndex( int vshIndex )
{
}

void CShaderAPIEmpty::SetPixelShaderIndex( int pshIndex )
{
}

// Sets the constant registers for vertex and pixel shaders
void CShaderAPIEmpty::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
	for ( int i = 0; i < numConst; ++i )
	{
		int nReg = var + i - VERTEX_SHADER_SHADER_SPECIFIC_CONST_0;
		if ( nReg >= 0 && nReg < 10 )
			memcpy( s_VertexConstants[nReg], pVec + i * 4, sizeof( s_VertexConstants[0] ) );
	}
}

void CShaderAPIEmpty::SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIEmpty::SetIntegerVertexShaderConstant( int var, int const* pVec, int numConst, bool bForce )
{
}


void CShaderAPIEmpty::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
	for ( int i = 0; i < numConst && var + i < 32; ++i )
		if ( var + i >= 0 )
			memcpy( s_PixelConstants[var + i], pVec + i * 4, sizeof( s_PixelConstants[0] ) );
}

void CShaderAPIEmpty::SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools, bool bForce )
{
}

void CShaderAPIEmpty::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
}

void CShaderAPIEmpty::InvalidateDelayedShaderConstants( void )
{
}

float CShaderAPIEmpty::GammaToLinear_HardwareSpecific( float fGamma ) const
{
	return 0.0f;
}

float CShaderAPIEmpty::LinearToGamma_HardwareSpecific( float fLinear ) const
{
	return 0.0f;
}

void CShaderAPIEmpty::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
}


// Returns the nearest supported format
ImageFormat CShaderAPIEmpty::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return fmt;
}

ImageFormat CShaderAPIEmpty::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return fmt;
}

// Sets the texture state
void CShaderAPIEmpty::BindTexture( Sampler_t stage, ShaderAPITextureHandle_t textureHandle )
{
	if ( s_bBindingLightmap )
	{
		Dx11R_BindLightmap( (int)textureHandle );
		return;
	}
	Dx11R_BindTexture( stage, (int)textureHandle );
}

void CShaderAPIEmpty::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	Dx11_SetClearColor( r / 255.0f, g / 255.0f, b / 255.0f, 1.0f );
}

void CShaderAPIEmpty::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	Dx11_SetClearColor( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );
}

// Indicates we're going to be modifying this texture
// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
// all use the texture specified by this function.
static int s_hModifyTexture;

void CShaderAPIEmpty::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
	s_hModifyTexture = (int)textureHandle;
}

// GammaSource DX11: every texture is held as BGRA8888 for now
static unsigned char *ToBGRA( const void *pData, ImageFormat srcFormat, int nWidth, int nHeight, int nSrcStride = 0 )
{
	static CUtlMemory< unsigned char > s_Buffer;
	int nBytes = nWidth * nHeight * 4;
	if ( s_Buffer.Count() < nBytes )
		s_Buffer.Grow( nBytes - s_Buffer.Count() );
	if ( srcFormat == IMAGE_FORMAT_BGRA8888 && ( nSrcStride == 0 || nSrcStride == nWidth * 4 ) )
		return (unsigned char *)pData;
	if ( !ImageLoader::ConvertImageFormat( (const unsigned char *)pData, srcFormat, s_Buffer.Base(), IMAGE_FORMAT_BGRA8888,
		nWidth, nHeight, nSrcStride, 0 ) )
		return NULL;
	return s_Buffer.Base();
}

// Texture management methods
void CShaderAPIEmpty::TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height,
						 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{
	if ( cubeFace != 0 || zOffset != 0 || !imageData )
		return;
	unsigned char *pBGRA = ToBGRA( imageData, srcFormat, width, height );
	if ( pBGRA )
		Dx11R_TexImage( s_hModifyTexture, level, width, height, pBGRA );
}

void CShaderAPIEmpty::TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
						 ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{
	if ( cubeFace != 0 || zOffset != 0 || !imageData )
		return;
	unsigned char *pBGRA = ToBGRA( imageData, srcFormat, width, height, srcStride );
	if ( pBGRA )
		Dx11R_TexSubImage( s_hModifyTexture, level, xOffset, yOffset, width, height, pBGRA );
}

// File textures arrive here: upload every mip level of the frame (first face only for now)
void CShaderAPIEmpty::TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame )
{
	if ( !pVTF || pVTF->Depth() > 1 )
		return;
	ImageFormat fmt = pVTF->Format();
	bool bCompressed = ImageLoader::IsCompressed( fmt );
	if ( pVTF->IsCubeMap() && Dx11R_IsCubeTexture( s_hModifyTexture ) )
	{
		// all six faces; mips under a compressed block are skipped (rarely sampled on envmaps)
		for ( int nFace = 0; nFace < 6; ++nFace )
			for ( int nMip = 0; nMip < pVTF->MipCount(); ++nMip )
			{
				int w, h, d;
				pVTF->ComputeMipLevelDimensions( nMip, &w, &h, &d );
				if ( bCompressed && ( w < 4 || h < 4 ) )
					continue;
				unsigned char *pSrc = pVTF->ImageData( iVTFFrame, nFace, nMip );
				unsigned char *pBGRA = pSrc ? ToBGRA( pSrc, fmt, w, h ) : NULL;
				if ( pBGRA )
					Dx11R_TexImageFace( s_hModifyTexture, nFace, nMip, w, h, pBGRA );
			}
		return;
	}
	static CUtlMemory< unsigned char > s_Padded;
	for ( int nMip = 0; nMip < pVTF->MipCount(); ++nMip )
	{
		int w, h, d;
		pVTF->ComputeMipLevelDimensions( nMip, &w, &h, &d );
		unsigned char *pSrc = pVTF->ImageData( iVTFFrame, 0, nMip );
		if ( !pSrc )
			continue;
		if ( bCompressed && ( w < 4 || h < 4 ) )
		{
			// compressed blocks are 4x4: decode a whole block, then keep the corner
			unsigned char *pBlock = ToBGRA( pSrc, fmt, 4, 4 );
			if ( !pBlock )
				continue;
			if ( s_Padded.Count() < w * h * 4 )
				s_Padded.Grow( w * h * 4 - s_Padded.Count() );
			for ( int y = 0; y < h; ++y )
				memcpy( s_Padded.Base() + y * w * 4, pBlock + y * 16, w * 4 );
			Dx11R_TexImage( s_hModifyTexture, nMip, w, h, s_Padded.Base() );
			continue;
		}
		unsigned char *pBGRA = ToBGRA( pSrc, fmt, w, h );
		if ( pBGRA )
			Dx11R_TexImage( s_hModifyTexture, nMip, w, h, pBGRA );
	}
}

// Locks hand out a CPU copy of the rect (lightmaps are written this way); unlock uploads it
static CUtlMemory< unsigned char > s_LockBits;
static int s_nLockLevel, s_nLockX, s_nLockY, s_nLockW, s_nLockH, s_hLockTexture;
static bool s_bLocked;
static unsigned char *s_pLockShadow;
static int s_nLockPitch;

bool CShaderAPIEmpty::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
								int width, int height, CPixelWriter& writer )
{
	if ( s_bLocked )
		TexUnlock();
	if ( cubeFaceID != 0 || width <= 0 || height <= 0 || !Dx11R_IsTexture( s_hModifyTexture ) )
	{
		Dx11_DebugLog( "texlock refused, handle", s_hModifyTexture );
		return false;
	}
	// mip 0 locks write into the texture's CPU copy so the rest of the page survives
	int nPitch, nTexW, nTexH;
	unsigned char *pShadow = ( level == 0 ) ? Dx11R_LockShadow( s_hModifyTexture, nPitch, nTexW, nTexH ) : NULL;
	if ( pShadow && xOffset >= 0 && yOffset >= 0 && xOffset + width <= nTexW && yOffset + height <= nTexH )
	{
		s_pLockShadow = pShadow + yOffset * nPitch + xOffset * 4;
		s_nLockPitch = nPitch;
		writer.SetPixelMemory( IMAGE_FORMAT_BGRA8888, s_pLockShadow, nPitch );
	}
	else
	{
		if ( s_LockBits.Count() < width * height * 4 )
			s_LockBits.Grow( width * height * 4 - s_LockBits.Count() );
		memset( s_LockBits.Base(), 0, width * height * 4 );
		s_pLockShadow = NULL;
		writer.SetPixelMemory( IMAGE_FORMAT_BGRA8888, s_LockBits.Base(), width * 4 );
	}
	s_nLockLevel = level; s_nLockX = xOffset; s_nLockY = yOffset; s_nLockW = width; s_nLockH = height;
	s_hLockTexture = s_hModifyTexture;
	s_bLocked = true;
	return true;
}

void CShaderAPIEmpty::TexUnlock( )
{
	if ( !s_bLocked )
		return;
	s_bLocked = false;
	if ( s_pLockShadow )
		Dx11R_UploadRect( s_hLockTexture, s_nLockX, s_nLockY, s_nLockW, s_nLockH, s_pLockShadow, s_nLockPitch );
	else
		Dx11R_TexSubImage( s_hLockTexture, s_nLockLevel, s_nLockX, s_nLockY, s_nLockW, s_nLockH, s_LockBits.Base() );
}


// These are bound to the texture, not the texture environment
void CShaderAPIEmpty::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIEmpty::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIEmpty::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
	Dx11R_TexWrap( s_hModifyTexture, coord, wrapMode != SHADER_TEXWRAPMODE_REPEAT );
}

void CShaderAPIEmpty::TexSetPriority( int priority )
{
}

ShaderAPITextureHandle_t CShaderAPIEmpty::CreateTexture( 
	int width, 
	int height,
	int depth,
	ImageFormat dstImageFormat, 
	int numMipLevels, 
	int numCopies, 
	int flags, 
	const char *pDebugName,
	const char *pTextureGroupName )
{
	return Dx11R_CreateTexture( width, height, numMipLevels, ( flags & TEXTURE_CREATE_RENDERTARGET ) != 0, ( flags & TEXTURE_CREATE_CUBEMAP ) != 0 );
}

// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
void CShaderAPIEmpty::CreateTextures( 
							ShaderAPITextureHandle_t *pHandles,
							int count,
							int width, 
							int height,
							int depth,
							ImageFormat dstImageFormat, 
							int numMipLevels, 
							int numCopies, 
							int flags, 
							const char *pDebugName,
							const char *pTextureGroupName )
{
	for ( int k = 0; k < count; ++ k )
		pHandles[ k ] = Dx11R_CreateTexture( width, height, numMipLevels, ( flags & TEXTURE_CREATE_RENDERTARGET ) != 0, ( flags & TEXTURE_CREATE_CUBEMAP ) != 0 );
}


ShaderAPITextureHandle_t CShaderAPIEmpty::CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture )
{
	return 0;
}

void CShaderAPIEmpty::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
	Dx11R_DeleteTexture( (int)textureHandle );
}

bool CShaderAPIEmpty::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	return Dx11R_IsTexture( (int)textureHandle );
}

bool CShaderAPIEmpty::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return false;
}

// stuff that isn't to be used from within a shader
void CShaderAPIEmpty::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
	Dx11R_Clear( bClearColor, bClearDepth || bClearStencil );
}

void CShaderAPIEmpty::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
}

void CShaderAPIEmpty::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
}

void CShaderAPIEmpty::PerformFullScreenStencilOperation( void )
{
}

void CShaderAPIEmpty::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
}

void CShaderAPIEmpty::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
{
}

void CShaderAPIEmpty::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{
}

void CShaderAPIEmpty::FlushHardware()
{
}

void CShaderAPIEmpty::ResetRenderState( bool bFullReset )
{
}

// Set the number of bone weights
void CShaderAPIEmpty::SetNumBoneWeights( int numBones )
{
}

void CShaderAPIEmpty::EnableHWMorphing( bool bEnable )
{
}

// Selection mode methods
int CShaderAPIEmpty::SelectionMode( bool selectionMode )
{
	return 0;
}

void CShaderAPIEmpty::SelectionBuffer( unsigned int* pBuffer, int size )
{
}

void CShaderAPIEmpty::ClearSelectionNames( )
{
}

void CShaderAPIEmpty::LoadSelectionName( int name )
{
}

void CShaderAPIEmpty::PushSelectionName( int name )
{
}

void CShaderAPIEmpty::PopSelectionName()
{
}


// Use this to get the mesh builder that allows us to modify vertex data
CMeshBuilder* CShaderAPIEmpty::GetVertexModifyBuilder()
{
	return 0;
}

// Board-independent calls, here to unify how shaders set state
// Implementations should chain back to IShaderUtil->BindTexture(), etc.

// Use this to begin and end the frame
void CShaderAPIEmpty::BeginFrame()
{
}

void CShaderAPIEmpty::EndFrame()
{
}

// returns the current time in seconds....
double CShaderAPIEmpty::CurrentTime() const
{
	return Sys_FloatTime();
}

// Get the current camera position in world space.
void CShaderAPIEmpty::GetWorldSpaceCameraPosition( float * pPos ) const
{
}

void CShaderAPIEmpty::ForceHardwareSync( void )
{
}

// user clip planes are world space ( nx, ny, nz, dist ), as in the DX9 backend
static float s_UserClipPlanes[2][4];
static bool s_bUserClipEnabled[2];

void CShaderAPIEmpty::SetClipPlane( int index, const float *pPlane )
{
	if ( index < 0 || index >= 2 || !pPlane )
		return;
	memcpy( s_UserClipPlanes[index], pPlane, sizeof( s_UserClipPlanes[index] ) );
	Dx11R_SetClipPlane( index, s_UserClipPlanes[index], s_bUserClipEnabled[index] );
}

void CShaderAPIEmpty::EnableClipPlane( int index, bool bEnable )
{
	if ( index < 0 || index >= 2 )
		return;
	s_bUserClipEnabled[index] = bEnable;
	Dx11R_SetClipPlane( index, s_UserClipPlanes[index], bEnable );
}

void CShaderAPIEmpty::SetFastClipPlane( const float *pPlane )
{
}

void CShaderAPIEmpty::EnableFastClip( bool bEnable )
{
}

int CShaderAPIEmpty::GetCurrentNumBones( void ) const
{
	return 0;
}

bool CShaderAPIEmpty::IsHWMorphingEnabled( void ) const
{
	return false;
}

int CShaderAPIEmpty::GetCurrentLightCombo( void ) const
{
	return 0;
}

void CShaderAPIEmpty::GetDX9LightState( LightState_t *state ) const
{
	state->m_nNumLights = 0;
	state->m_bAmbientLight = false;
	state->m_bStaticLightVertex = false;
	state->m_bStaticLightTexel = false;
}

MaterialFogMode_t CShaderAPIEmpty::GetCurrentFogType( void ) const
{
	return s_FogMode;
}

void CShaderAPIEmpty::RecordString( const char *pStr )
{
}

bool CShaderAPIEmpty::ReadPixelsFromFrontBuffer() const
{
	return true;
}

bool CShaderAPIEmpty::PreferDynamicTextures() const
{
	return false;
}

bool CShaderAPIEmpty::PreferReducedFillrate() const
{ 
	return false; 
}

bool CShaderAPIEmpty::HasProjectedBumpEnv() const
{
	return true;
}

int  CShaderAPIEmpty::GetCurrentDynamicVBSize( void )
{
	return 0;
}

void CShaderAPIEmpty::DestroyVertexBuffers( bool bExitingLevel )
{
}

void CShaderAPIEmpty::EvictManagedResources()
{
}

void CShaderAPIEmpty::SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected )
{
}

void CShaderAPIEmpty::SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 )
{
}

void CShaderAPIEmpty::SyncToken( const char *pToken )
{
}


//-----------------------------------------------------------------------------
// GammaSource DX11: run the shaders' command buffers. Constants are not used by the
// fixed DX11 shader yet; texture binds are.
//-----------------------------------------------------------------------------
template < class T > static inline T CmdData( const uint8 *p ) { return *(const T *)p; }

void CShaderAPIEmpty::ExecuteCommandBuffer( uint8 *pCmdBuf )
{
	for ( ;; )
	{
		int nCmd = CmdData< int >( pCmdBuf );
		switch ( nCmd )
		{
		case CBCMD_END:
			return;
		case CBCMD_JUMP:
			pCmdBuf = CmdData< uint8 * >( pCmdBuf + sizeof( int ) );
			break;
		case CBCMD_JSR:
			ExecuteCommandBuffer( CmdData< uint8 * >( pCmdBuf + sizeof( int ) ) );
			pCmdBuf += sizeof( int ) + sizeof( uint8 * );
			break;
		case CBCMD_SET_PIXEL_SHADER_FLOAT_CONST:
			SetPixelShaderConstant( CmdData< int >( pCmdBuf + sizeof( int ) ), (const float *)( pCmdBuf + 3 * sizeof( int ) ),
				CmdData< int >( pCmdBuf + 2 * sizeof( int ) ) );
			pCmdBuf += CmdData< int >( pCmdBuf + 2 * sizeof( int ) ) * 4 * sizeof( float ) + 3 * sizeof( int );
			break;
		case CBCMD_SET_VERTEX_SHADER_FLOAT_CONST:
			SetVertexShaderConstant( CmdData< int >( pCmdBuf + sizeof( int ) ), (const float *)( pCmdBuf + 3 * sizeof( int ) ),
				CmdData< int >( pCmdBuf + 2 * sizeof( int ) ) );
			pCmdBuf += CmdData< int >( pCmdBuf + 2 * sizeof( int ) ) * 4 * sizeof( float ) + 3 * sizeof( int );
			break;
		case CBCMD_SETPIXELSHADERFOGPARAMS:
		case CBCMD_STORE_EYE_POS_IN_PSCONST:
		case CBCMD_COMMITPIXELSHADERLIGHTING:
		case CBCMD_SETPIXELSHADERSTATEAMBIENTLIGHTCUBE:
			MarkLitPass();
			pCmdBuf += 2 * sizeof( int );
			break;
		case CBCMD_SET_PSHINDEX:
		case CBCMD_SET_VSHINDEX:
			pCmdBuf += 2 * sizeof( int );
			break;
		case CBCMD_SETAMBIENTCUBEDYNAMICSTATEVERTEXSHADER:
			MarkLitPass();
			pCmdBuf += sizeof( int );
			break;
		case CBCMD_SET_DEPTH_FEATHERING_CONST:
			pCmdBuf += 2 * sizeof( int ) + sizeof( float );
			break;
		case CBCMD_BIND_STANDARD_TEXTURE:
			{
				int nSampler = CmdData< int >( pCmdBuf + sizeof( int ) );
				int nTextureID = CmdData< int >( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 3 * sizeof( int );
				BindStandardTexture( (Sampler_t)nSampler, (StandardTextureId_t)nTextureID );
			}
			break;
		case CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE:
			{
				int nSampler = CmdData< int >( pCmdBuf + sizeof( int ) );
				ShaderAPITextureHandle_t hTexture = CmdData< ShaderAPITextureHandle_t >( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( ShaderAPITextureHandle_t );
				BindTexture( (Sampler_t)nSampler, hTexture );
			}
			break;
		default:
			// a command this backend does not know the size of: stop here
			return;
		}
	}
}

void CShaderAPIEmpty::BindStandardTexture( Sampler_t stage, StandardTextureId_t id )
{
	bool bLightmap = ( id == TEXTURE_LIGHTMAP || id == TEXTURE_LIGHTMAP_BUMPED || id == TEXTURE_LIGHTMAP_FULLBRIGHT || id == TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT );
	s_bBindingLightmap = bLightmap;
	if ( id >= 0 && id < TEXTURE_MAX_STD_TEXTURES && m_StdTextureHandles[id] != INVALID_SHADERAPI_TEXTURE_HANDLE )
		BindTexture( stage, m_StdTextureHandles[id] );
	else if ( g_pShaderUtil )
		g_pShaderUtil->BindStandardTexture( stage, id );
	s_bBindingLightmap = false;
}
