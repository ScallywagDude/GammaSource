//========= GammaSource ============//
//
// Player-chosen portal colours.
//
// cl_portal_color1/2 hold each portal's colour ("r g b") and cl_portal_color_random picks a new
// random colour for every shot; the server chooses (CWeaponPortalgun::ChoosePortalColor) and
// networks the result on the portal gun. Everything Portal draws in blue/orange follows it:
//   - UTIL_Portal_Color (crosshair, gun lights and beam) asks GammaPortalColor
//   - the portal surfaces: their PortalRefract overlays get a generated colour ramp in place of
//     portal-blue-color / portal-orange-color
//   - the portal_1_* / portal_2_* particle systems are recoloured after each simulation step
//
//=============================================================================//
#include "cbase.h"
#include "c_weapon_portalgun.h"
#include "particles_new.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "pixelwriter.h"
#include "bitmap/imageformat.h"
#include "vtf/vtf.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_portal_color1( "cl_portal_color1", "64 160 255", FCVAR_ARCHIVE | FCVAR_USERINFO, "Colour of portal 1 (r g b)" );
ConVar cl_portal_color2( "cl_portal_color2", "255 160 32", FCVAR_ARCHIVE | FCVAR_USERINFO, "Colour of portal 2 (r g b)" );
ConVar cl_portal_color_random( "cl_portal_color_random", "0", FCVAR_ARCHIVE | FCVAR_USERINFO, "Give every portal you fire a random colour" );

static const Color s_DefaultColors[2] = { Color( 64, 160, 255, 255 ), Color( 255, 160, 32, 255 ) };

// Last colour of each portal, normalised so the brightest channel is 1 (read by particle threads)
static Vector s_vecTint[2] = { Vector( 64.0f / 255.0f, 160.0f / 255.0f, 1.0f ), Vector( 1.0f, 160.0f / 255.0f, 32.0f / 255.0f ) };

static C_WeaponPortalgun *LocalPortalgun()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		C_WeaponPortalgun *pGun = dynamic_cast< C_WeaponPortalgun * >( pPlayer->GetWeapon( i ) );
		if ( pGun )
			return pGun;
	}
	return NULL;
}

static bool ParseColor( const char *psz, Color &color )
{
	int r, g, b;
	if ( !psz || sscanf( psz, "%d %d %d", &r, &g, &b ) != 3 )
		return false;
	color.SetColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
	return true;
}

//-----------------------------------------------------------------------------
// The colour of portal 1 or 2: what the gun last fired, else the player's setting
//-----------------------------------------------------------------------------
bool GammaPortalColor( int iPortal, Color &color )
{
	if ( iPortal != 1 && iPortal != 2 )
		return false;
	C_WeaponPortalgun *pGun = LocalPortalgun();
	int nPacked = pGun ? pGun->GetPortalColor( iPortal ) : 0;
	if ( nPacked )
	{
		color.SetColor( ( nPacked >> 16 ) & 0xFF, ( nPacked >> 8 ) & 0xFF, nPacked & 0xFF, 255 );
		return true;
	}
	if ( !cl_portal_color_random.GetBool() && ParseColor( ( iPortal == 1 ? cl_portal_color1 : cl_portal_color2 ).GetString(), color ) )
		return true;
	color = s_DefaultColors[iPortal - 1];
	return true;
}

//-----------------------------------------------------------------------------
// Particles: keep each particle's brightness, take the portal's hue. Idempotent, since the
// tint's brightest channel is 1, so particles that are only coloured at birth stay right.
//-----------------------------------------------------------------------------
static void RecolorCollection( CParticleCollection *pCollection, const Vector &tint )
{
	size_t nStride;
	fltx4 *pRGB = pCollection->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_TINT_RGB, &nStride );
	if ( pRGB )
	{
		int nBlocks = ( pCollection->m_nActiveParticles + 3 ) / 4;
		fltx4 tr = ReplicateX4( tint.x ), tg = ReplicateX4( tint.y ), tb = ReplicateX4( tint.z );
		for ( int i = 0; i < nBlocks; ++i, pRGB += nStride )
		{
			fltx4 peak = MaxSIMD( pRGB[0], MaxSIMD( pRGB[1], pRGB[2] ) );
			pRGB[0] = MulSIMD( peak, tr );
			pRGB[1] = MulSIMD( peak, tg );
			pRGB[2] = MulSIMD( peak, tb );
		}
	}
	for ( CParticleCollection *pChild = pCollection->m_Children.m_pHead; pChild; pChild = pChild->m_pNext )
		RecolorCollection( pChild, tint );
}

void GammaRecolorPortalParticles( CNewParticleEffect *pEffect )
{
	int iPortal = pEffect->m_nGammaPortalTint;
	if ( iPortal != 1 && iPortal != 2 )
		return;
	Vector tint = s_vecTint[iPortal - 1];
	// The stock colours need no work (and keep their white-hot cores)
	Color stock = s_DefaultColors[iPortal - 1];
	if ( fabsf( tint.x * 255.0f - stock.r() * ( 255.0f / MAX( MAX( stock.r(), stock.g() ), stock.b() ) ) ) < 1.0f &&
		 fabsf( tint.y * 255.0f - stock.g() * ( 255.0f / MAX( MAX( stock.r(), stock.g() ), stock.b() ) ) ) < 1.0f &&
		 fabsf( tint.z * 255.0f - stock.b() * ( 255.0f / MAX( MAX( stock.r(), stock.g() ), stock.b() ) ) ) < 1.0f )
		return;
	RecolorCollection( pEffect, tint );
}

//-----------------------------------------------------------------------------
// Portal surfaces: a colour ramp like portal-blue-color (dark, then the colour, then a hot
// white tip), sampled by the PortalRefract shader
//-----------------------------------------------------------------------------
class CGammaPortalRampRegen : public ITextureRegenerator
{
public:
	Color m_Color;
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		CPixelWriter writer;
		writer.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );
		int nWidth = pVTFTexture->Width(), nHeight = pVTFTexture->Height();
		for ( int y = 0; y < nHeight; ++y )
		{
			writer.Seek( 0, y );
			for ( int x = 0; x < nWidth; ++x )
			{
				float t = (float)x / ( nWidth - 1 );
				float body = clamp( t / 0.6f, 0.0f, 1.0f );
				float hot = clamp( ( t - 0.8f ) / 0.2f, 0.0f, 1.0f ) * 0.6f;
				int r = (int)clamp( ( m_Color.r() * body ) + ( 255 - m_Color.r() * body ) * hot, 0.0f, 255.0f );
				int g = (int)clamp( ( m_Color.g() * body ) + ( 255 - m_Color.g() * body ) * hot, 0.0f, 255.0f );
				int b = (int)clamp( ( m_Color.b() * body ) + ( 255 - m_Color.b() * body ) * hot, 0.0f, 255.0f );
				writer.WritePixel( r, g, b, 255 );
			}
		}
	}
	virtual void Release() {}
};

class CGammaPortalColorSystem : public CAutoGameSystemPerFrame
{
public:
	CGammaPortalColorSystem() : CAutoGameSystemPerFrame( "CGammaPortalColorSystem" ) {}

	virtual void LevelInitPostEntity()
	{
		for ( int i = 0; i < 2; ++i )
			m_Applied[i].SetColor( 0, 0, 0, 0 );
	}

	virtual void Update( float frametime )
	{
		for ( int i = 0; i < 2; ++i )
		{
			Color color;
			GammaPortalColor( i + 1, color );
			int nPeak = MAX( MAX( color.r(), color.g() ), MAX( color.b(), 1 ) );
			s_vecTint[i].Init( color.r() / (float)nPeak, color.g() / (float)nPeak, color.b() / (float)nPeak );
			if ( color == m_Applied[i] )
				continue;
			m_Applied[i] = color;
			ApplySurface( i, color );
		}
	}

	virtual void Shutdown()
	{
		for ( int i = 0; i < 2; ++i )
		{
			if ( m_Ramp[i] )
			{
				m_Ramp[i]->SetTextureRegenerator( NULL );
				m_Ramp[i].Shutdown();
			}
		}
	}

private:
	void ApplySurface( int i, const Color &color )
	{
		static const char *s_Overlays[2] = { "models/portals/portalstaticoverlay_1", "models/portals/portalstaticoverlay_2" };
		if ( !m_Ramp[i] )
		{
			char name[64];
			Q_snprintf( name, sizeof( name ), "_rt_GammaPortalColor%d", i + 1 );
			m_Ramp[i].InitProceduralTexture( name, TEXTURE_GROUP_CLIENT_EFFECTS, 256, 1, IMAGE_FORMAT_RGBA8888,
				TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY );
			m_Ramp[i]->SetTextureRegenerator( &m_Regen[i] );
		}
		m_Regen[i].m_Color = color;
		m_Ramp[i]->Download();

		IMaterial *pMaterial = materials->FindMaterial( s_Overlays[i], TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !pMaterial || pMaterial->IsErrorMaterial() )
			return;
		bool bFound = false;
		IMaterialVar *pVar = pMaterial->FindVar( "$PortalColorTexture", &bFound, false );
		if ( !bFound || !pVar )
			return;
		if ( !m_Original[i] && pVar->IsTexture() )
			m_Original[i].Init( pVar->GetTextureValue() );
		bool bStock = color == s_DefaultColors[i];
		pVar->SetTextureValue( bStock && m_Original[i] ? (ITexture *)m_Original[i] : (ITexture *)m_Ramp[i] );
		// the stock ramps are pre-scaled for colour scale 4; ours is a plain 0..1 ramp
		IMaterialVar *pScale = pMaterial->FindVar( "$PortalColorScale", &bFound, false );
		if ( bFound && pScale )
			pScale->SetFloatValue( bStock ? 4.0f : 2.0f );
	}

	CTextureReference m_Ramp[2];
	CTextureReference m_Original[2];
	CGammaPortalRampRegen m_Regen[2];
	Color m_Applied[2];
};
static CGammaPortalColorSystem s_GammaPortalColorSystem;
