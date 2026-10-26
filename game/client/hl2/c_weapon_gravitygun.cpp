//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "beamdraw.h"
#include "c_weapon__stubs.h"
#include "clienteffectprecachesystem.h"
#include "c_baseplayer.h"
#include "materialsystem/MaterialSystemUtil.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectGravityGun )
CLIENTEFFECT_MATERIAL( "sprites/gamma_physbeam" )
CLIENTEFFECT_REGISTER_END()

class C_BeamQuadratic : public CDefaultClientRenderable
{
public:
	C_BeamQuadratic();
	~C_BeamQuadratic();
	void			Update( C_BaseEntity *pOwner );
	void			Shutdown();
	bool			GetBeamStart( Vector &start );
	void			DumpState();

	// IClientRenderable
	virtual const Vector&			GetRenderOrigin( void ) { return m_worldPosition; }
	virtual const QAngle&			GetRenderAngles( void ) { return vec3_angle; }
	virtual bool					ShouldDraw( void ) { return true; }
	virtual bool					IsTransparent( void ) { return true; }
	virtual bool					ShouldReceiveProjectedTextures( int flags ) { return false; }
	virtual int						DrawModel( int flags );
	virtual const matrix3x4_t &RenderableToWorldTransform()	// GammaSource: required by the 2018 IClientRenderable
	{
		static matrix3x4_t mat;
		SetIdentityMatrix( mat );
		PositionMatrix( GetRenderOrigin(), mat );
		return mat;
	}

	// Returns the bounds relative to the origin (render bounds)
	virtual void	GetRenderBounds( Vector& mins, Vector& maxs )
	{
		Vector start;
		GetBeamStart( start );
		// The entire curve must remain visible when either endpoint is outside
		// the view. Its control point encloses the quadratic Bezier curve.
		Vector control = 0.5f * ( m_targetPosition + start );
		VectorMin( start, control, mins );
		VectorMax( start, control, maxs );
		VectorMin( mins, m_worldPosition, mins );
		VectorMax( maxs, m_worldPosition, maxs );
		mins -= m_worldPosition + Vector( 16, 16, 16 );
		maxs -= m_worldPosition - Vector( 16, 16, 16 );
	}

	C_BaseEntity			*m_pOwner;
	Vector					m_targetPosition;
	Vector					m_worldPosition;
	int						m_active;
	int						m_glueTouching;
	int						m_viewModelIndex;
	int						m_nLastDrawFrame;
	CMaterialReference		m_BeamMaterial;
};


class C_WeaponGravityGun : public C_BaseCombatWeapon
{
	DECLARE_CLASS( C_WeaponGravityGun, C_BaseCombatWeapon );
public:
	C_WeaponGravityGun() {}

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	int KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
	{
		if ( gHUD.m_iKeyBits & IN_ATTACK )
		{
			switch ( keynum )
			{
			case MOUSE_WHEEL_UP:
				gHUD.m_iKeyBits |= IN_WEAPON1;
				return 0;

			case MOUSE_WHEEL_DOWN:
				gHUD.m_iKeyBits |= IN_WEAPON2;
				return 0;
			}
		}

		// Allow engine to process
		return BaseClass::KeyInput( down, keynum, pszCurrentBinding );
	}

	void OnDataChanged( DataUpdateType_t updateType )
	{
		BaseClass::OnDataChanged( updateType );
		m_beam.Update( this );
		SetNextClientThink( m_beam.m_active ? CLIENT_THINK_ALWAYS : CLIENT_THINK_NEVER );
	}
	void ClientThink() { m_beam.Update( this ); }
	void UpdateOnRemove()
	{
		m_beam.Shutdown();
		BaseClass::UpdateOnRemove();
	}
	void DumpBeamState() { m_beam.DumpState(); }

private:
	C_WeaponGravityGun( const C_WeaponGravityGun & );

	C_BeamQuadratic	m_beam;
};

STUB_WEAPON_CLASS_IMPLEMENT( weapon_physgun, C_WeaponGravityGun );

IMPLEMENT_CLIENTCLASS_DT( C_WeaponGravityGun, DT_WeaponGravityGun, CWeaponGravityGun )
	RecvPropVector( RECVINFO_NAME(m_beam.m_targetPosition,m_targetPosition) ),
	RecvPropVector( RECVINFO_NAME(m_beam.m_worldPosition, m_worldPosition) ),
	RecvPropInt( RECVINFO_NAME(m_beam.m_active, m_active) ),
	RecvPropInt( RECVINFO_NAME(m_beam.m_glueTouching, m_glueTouching) ),
	RecvPropInt( RECVINFO_NAME(m_beam.m_viewModelIndex, m_viewModelIndex) ),
END_RECV_TABLE()


C_BeamQuadratic::C_BeamQuadratic()
{
	m_pOwner = NULL;
	m_targetPosition.Init();
	m_worldPosition.Init();
	m_active = m_glueTouching = m_viewModelIndex = 0;
	m_nLastDrawFrame = -1;
}

C_BeamQuadratic::~C_BeamQuadratic()
{
	Shutdown();
}

void C_BeamQuadratic::Shutdown()
{
	if ( m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE )
		ClientLeafSystem()->RemoveRenderable( m_hRenderHandle );
	m_active = 0;
	m_pOwner = NULL;
}

bool C_BeamQuadratic::GetBeamStart( Vector &start )
{
	C_BaseEntity *pEmitter = cl_entitylist->GetEnt( m_viewModelIndex );
	QAngle angles;
	if ( pEmitter && pEmitter->GetAttachment( 1, start, angles ) )
		return true;
	C_BaseCombatWeapon *pWeapon = static_cast<C_BaseCombatWeapon *>( m_pOwner );
	C_BasePlayer *pOwner = pWeapon ? ToBasePlayer( pWeapon->GetOwner() ) : NULL;
	start = pOwner ? pOwner->Weapon_ShootPosition() : m_worldPosition;
	return false;
}

void C_BeamQuadratic::Update( C_BaseEntity *pOwner )
{
	m_pOwner = pOwner;
	if ( m_active )
	{
		if ( !m_BeamMaterial.IsValid() )
			m_BeamMaterial.Init( "sprites/gamma_physbeam", TEXTURE_GROUP_CLIENT_EFFECTS );
		if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		{
			ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_TRANSLUCENT_ENTITY );
		}
		else
		{
			ClientLeafSystem()->RenderableChanged( m_hRenderHandle );
		}
	}
	else if ( !m_active && m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->RemoveRenderable( m_hRenderHandle );
	}
}


int	C_BeamQuadratic::DrawModel( int )
{
	Vector points[3];

	if ( !m_active )
		return 0;

	GetBeamStart( points[0] );

	points[1] = 0.5 * (m_targetPosition + points[0]);
	
	// a little noise 11t & 13t should be somewhat non-periodic looking
	//points[1].z += 4*sin( gpGlobals->curtime*11 ) + 5*cos( gpGlobals->curtime*13 );
	points[2] = m_worldPosition;

	IMaterial *pMat = m_BeamMaterial;
	if ( !pMat || IsErrorMaterial( pMat ) )
		return 0;
	Vector color;
	if ( m_glueTouching )
	{
		color.Init(1,0,0);
	}
	else
	{
		color.Init(1,1,1);
	}

	float scrollOffset = gpGlobals->curtime - (int)gpGlobals->curtime;
	CMatRenderContextPtr( materials )->Bind( pMat );
	DrawBeamQuadratic( points[0], points[1], points[2], 13, color, scrollOffset );
	m_nLastDrawFrame = gpGlobals->framecount;
	return 1;
}

void C_BeamQuadratic::DumpState()
{
	Vector start, mins, maxs;
	bool attachment = GetBeamStart( start );
	GetRenderBounds( mins, maxs );
	IMaterial *pMat = m_BeamMaterial;
	Msg( "GAMMA_PHYSGUN_BEAM active=%d registered=%d emitter=%d attachment=%d material=%s error=%d draw_age=%d start=(%.1f %.1f %.1f) end=(%.1f %.1f %.1f) bounds=(%.1f %.1f %.1f)/(%.1f %.1f %.1f)\n",
		m_active, m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE, m_viewModelIndex, attachment,
		pMat ? pMat->GetName() : "none", !pMat || IsErrorMaterial( pMat ), m_nLastDrawFrame < 0 ? -1 : gpGlobals->framecount - m_nLastDrawFrame,
		start.x, start.y, start.z, m_worldPosition.x, m_worldPosition.y, m_worldPosition.z,
		mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z );
}

CON_COMMAND( gamma_physgun_beaminfo, "Print local physgun beam state, material and attachment" )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( pWeapon && FClassnameIs( pWeapon, "weapon_physgun" ) )
		static_cast<C_WeaponGravityGun *>( pWeapon )->DumpBeamState();
	else
		Msg( "GAMMA_PHYSGUN_BEAM no active physgun\n" );
}

