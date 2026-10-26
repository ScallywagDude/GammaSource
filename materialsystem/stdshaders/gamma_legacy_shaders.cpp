//========= GammaSource ============//
//
// The 2002-2003 Predator (refraction) and JellyFish (hydra glow) shaders, ported to
// the DX9 HLSL path. Shaders are built by gamma_build_shaders.py.
//
//=================================//

#include "BaseVSShader.h"

#include "gamma_predator_vs20.inc"
#include "gamma_predator_ps20.inc"
#include "gamma_predator_ps20b.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	GAMMA_MODE_REFRACT = 0,
	GAMMA_MODE_ENVMAP = 1,
	GAMMA_MODE_ENVMAP_MASK = 2,
	GAMMA_MODE_JELLYFISH = 3,
};

static void GammaSetShaders( IShaderShadow *pShaderShadow, int nMode )
{
	unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
	pShaderShadow->VertexShaderVertexFormat( flags, 1, NULL, 0 );

	gamma_predator_vs20_Static_Index vsh;
	pShaderShadow->SetVertexShader( "gamma_predator_vs20", vsh.GetIndex() );
	if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		gamma_predator_ps20b_Static_Index psh;
		psh.SetMODE( nMode );
		pShaderShadow->SetPixelShader( "gamma_predator_ps20b", psh.GetIndex() );
	}
	else
	{
		gamma_predator_ps20_Static_Index psh;
		psh.SetMODE( nMode );
		pShaderShadow->SetPixelShader( "gamma_predator_ps20", psh.GetIndex() );
	}
}

static void GammaSetDynamic( IShaderDynamicAPI *pShaderAPI, VertexCompressionType_t vertexCompression, const float *pParams4 )
{
	gamma_predator_vs20_Dynamic_Index vsh;
	vsh.SetCOMPRESSED_VERTS( (int)vertexCompression );
	vsh.SetSKINNING( pShaderAPI->GetCurrentNumBones() > 0 );
	pShaderAPI->SetVertexShaderIndex( vsh.GetIndex() );
	pShaderAPI->SetPixelShaderIndex( 0 );
	pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, pParams4, 1 );
}

BEGIN_VS_SHADER( Predator, "2003 screen refraction" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( REFRACTIONAMOUNT, SHADER_PARAM_TYPE_VEC2, "[0 0]", "refraction amount" )
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "", "envmap" )
		SHADER_PARAM( ENVMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "", "envmap frame number" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "", "envmap mask" )
		SHADER_PARAM( ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmap tint" )
		SHADER_PARAM( REFRACTTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "refraction tint" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "unused in this port" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		if ( !params[ENVMAPTINT]->IsDefined() )
			params[ENVMAPTINT]->SetVecValue( 1.0f, 1.0f, 1.0f );
		if ( !params[REFRACTTINT]->IsDefined() )
			params[REFRACTTINT]->SetVecValue( 1.0f, 1.0f, 1.0f );
	}

	SHADER_FALLBACK
	{
		if ( g_pHardwareConfig->GetDXSupportLevel() < 90 )
			return "VertexLitGeneric";
		return 0;
	}

	SHADER_INIT
	{
		if ( params[ENVMAP]->IsDefined() )
			LoadCubeMap( ENVMAP );
		if ( params[ENVMAPMASK]->IsDefined() )
			LoadTexture( ENVMAPMASK );
	}

	SHADER_DRAW
	{
		float refract[2] = { 0, 0 };
		params[REFRACTIONAMOUNT]->GetVecValue( refract, 2 );
		float c0[4] = { refract[0], refract[1], 0, 0 };

		// refractive pass
		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			GammaSetShaders( pShaderShadow, GAMMA_MODE_REFRACT );
			DefaultFog();
		}
		DYNAMIC_STATE
		{
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			SetPixelShaderConstant( 2, REFRACTTINT );
			GammaSetDynamic( pShaderAPI, vertexCompression, c0 );
		}
		Draw();

		if ( params[ENVMAP]->IsDefined() )
		{
			bool bMask = params[ENVMAPMASK]->IsTexture();
			SHADOW_STATE
			{
				SetInitialShadowState();
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				if ( bMask )
					pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				pShaderShadow->EnableDepthWrites( false );
				GammaSetShaders( pShaderShadow, bMask ? GAMMA_MODE_ENVMAP_MASK : GAMMA_MODE_ENVMAP );
				FogToBlack();
			}
			DYNAMIC_STATE
			{
				BindTexture( SHADER_SAMPLER0, ENVMAP, ENVMAPFRAME );
				if ( bMask )
					BindTexture( SHADER_SAMPLER1, ENVMAPMASK, ENVMAPMASKFRAME );
				SetPixelShaderConstant( 2, ENVMAPTINT );
				GammaSetDynamic( pShaderAPI, vertexCompression, c0 );
			}
			Draw();
		}
	}
END_SHADER

BEGIN_VS_SHADER( JellyFish, "2003 hydra silhouette glow" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( GRADIENTTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "1D texture for silhouette glowy bits" )
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "", "envmap" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmaptint" )
		SHADER_PARAM( PULSERATE, SHADER_PARAM_TYPE_FLOAT, "0", "gradient scroll rate" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		if ( !params[ENVMAPTINT]->IsDefined() )
			params[ENVMAPTINT]->SetVecValue( 1.0f, 1.0f, 1.0f );
		if ( !params[PULSERATE]->IsDefined() )
			params[PULSERATE]->SetFloatValue( 0.0f );
	}

	SHADER_FALLBACK
	{
		if ( g_pHardwareConfig->GetDXSupportLevel() < 90 )
			return "UnlitGeneric";
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( GRADIENTTEXTURE );
		LoadTexture( BASETEXTURE );
		if ( params[ENVMAP]->IsDefined() )
			LoadCubeMap( ENVMAP );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			GammaSetShaders( pShaderShadow, GAMMA_MODE_JELLYFISH );
			FogToBlack();
		}
		DYNAMIC_STATE
		{
			float c0[4] = { 0, 0, (float)pShaderAPI->CurrentTime() * params[PULSERATE]->GetFloatValue(), 0 };
			BindTexture( SHADER_SAMPLER0, GRADIENTTEXTURE );
			BindTexture( SHADER_SAMPLER1, BASETEXTURE );
			GammaSetDynamic( pShaderAPI, vertexCompression, c0 );
		}
		Draw();

		if ( params[ENVMAP]->IsDefined() )
		{
			SHADOW_STATE
			{
				SetInitialShadowState();
				pShaderShadow->EnableDepthWrites( false );
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
				GammaSetShaders( pShaderShadow, GAMMA_MODE_ENVMAP );
				FogToFogColor();
			}
			DYNAMIC_STATE
			{
				float c0[4] = { 0, 0, 0, 0 };
				BindTexture( SHADER_SAMPLER0, ENVMAP );
				SetPixelShaderConstant( 2, ENVMAPTINT );
				GammaSetDynamic( pShaderAPI, vertexCompression, c0 );
			}
			Draw();
		}
	}
END_SHADER
