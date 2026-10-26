"""Stunstick: land the hit when the swing connects instead of on the first frame, and
stop restarting the 0.67s swing every 0.6s. Guard gun: charge-up lasers (were disabled
behind a FIXME), the viewmodel fire animation and a visible blast beam."""
from pathlib import Path
ROOT = Path(r"C:\Users\doge6\Videos\GammaSource-main")

def patch(rel, edits):
    p = ROOT / rel; d = p.read_bytes()
    nl = b"\r\n" if b"\r\n" in d else b"\n"
    for old, new in edits:
        old = old.replace(b"\r\n", b"\n").replace(b"\n", nl); new = new.replace(b"\r\n", b"\n").replace(b"\n", nl)
        assert d.count(old) == 1, (rel, old[:70])
        d = d.replace(old, new)
    p.write_bytes(d)

# Subclasses need the hit helpers to defer the impact.
patch('dlls/basebludgeonweapon.h', [
(b"""private:
	virtual	void	ImpactEffect( trace_t &trace );
	virtual	void	ImpactSound( bool isWorld );
	void			Swing( int bIsSecondary );
	void			Hit( trace_t &traceHit, Activity nHitActivity );
	Activity		ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );""",
b"""protected:
	virtual	void	ImpactEffect( trace_t &trace );
	virtual	void	ImpactSound( bool isWorld );
	void			Swing( int bIsSecondary );
	void			Hit( trace_t &traceHit, Activity nHitActivity );
	Activity		ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );"""),
])

patch('dlls/hl2_dll/weapon_stunstick.cpp', [
(b"""#define	STUNSTICK_RANGE		48
#define	STUNSTICK_REFIRE	0.6f
""",
b"""#define	STUNSTICK_RANGE		48
#define	STUNSTICK_REFIRE	0.7f	// v_stunbaton attack sequences last 0.67s
#define	STUNSTICK_HIT_DELAY	0.25f	// the baton connects about a third into the swing
"""),
(b"""	void		SecondaryAttack( void )	{}
	DECLARE_ACTTABLE();
""",
b"""	void		SecondaryAttack( void )	{}
	void		PrimaryAttack( void );
	void		ItemPostFrame( void );
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
"""),
(b"""	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
};
""",
b"""	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	bool		TraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity );

	float		m_flDelayedHitTime;	// 0 when no swing is waiting to connect
};
"""),
(b"""LINK_ENTITY_TO_CLASS( weapon_stunstick, CWeaponStunStick );
PRECACHE_WEAPON_REGISTER( weapon_stunstick );
""",
b"""LINK_ENTITY_TO_CLASS( weapon_stunstick, CWeaponStunStick );
PRECACHE_WEAPON_REGISTER( weapon_stunstick );

BEGIN_DATADESC( CWeaponStunStick )
	DEFINE_FIELD( CWeaponStunStick, m_flDelayedHitTime, FIELD_TIME ),
END_DATADESC()
"""),
(b"""CWeaponStunStick::CWeaponStunStick( void )
{
}
""",
b"""CWeaponStunStick::CWeaponStunStick( void )
{
	m_flDelayedHitTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Same reach test as the base swing; true when something is in range
//-----------------------------------------------------------------------------
bool CWeaponStunStick::TraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity )
{
	static const Vector mins( -16, -16, -16 ), maxs( 16, 16, 16 );
	Vector swingStart = pOwner->Weapon_ShootPosition();
	Vector forward;
	pOwner->EyeVectors( &forward, NULL, NULL );

	hitActivity = ACT_VM_HITCENTER;
	UTIL_TraceLine( swingStart, swingStart + forward * GetRange(), MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	if ( traceHit.fraction == 1.0f )
	{
		UTIL_TraceHull( swingStart, swingStart + forward * GetRange(), mins, maxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
		if ( traceHit.fraction < 1.0f )
		{
			hitActivity = ChooseIntersectionPointAndActivity( traceHit, mins, maxs, pOwner );
		}
	}
	return traceHit.fraction < 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Start the swing now; the impact is applied when the baton connects
//-----------------------------------------------------------------------------
void CWeaponStunStick::PrimaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	trace_t traceHit;
	Activity hitActivity;
	SendWeaponAnim( TraceSwing( pOwner, traceHit, hitActivity ) ? hitActivity : ACT_VM_MISSCENTER );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
	m_flDelayedHitTime = gpGlobals->curtime + STUNSTICK_HIT_DELAY;

	WeaponSound( SINGLE );
}

//-----------------------------------------------------------------------------
// Purpose: Apply a pending swing's impact at the right moment
//-----------------------------------------------------------------------------
void CWeaponStunStick::ItemPostFrame( void )
{
	if ( m_flDelayedHitTime > 0.0f && gpGlobals->curtime >= m_flDelayedHitTime )
	{
		m_flDelayedHitTime = 0.0f;

		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		trace_t traceHit;
		Activity hitActivity;
		if ( pOwner && TraceSwing( pOwner, traceHit, hitActivity ) )
		{
			Hit( traceHit, hitActivity );
		}
	}

	BaseClass::ItemPostFrame();
}
"""),
])

patch('dlls/hl2_dll/weapon_cguard.cpp', [
(b"""	//FIXME: UpdateLasers();
""",
b"""	// Charge-up laser fan while the blast builds.
	UpdateLasers();
"""),
(b"""	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	pPlayer->m_fEffects |= EF_MUZZLEFLASH;
""",
b"""	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	pPlayer->m_fEffects |= EF_MUZZLEFLASH;
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
"""),
(b"""	UTIL_TraceHull( vecSrc, impactPoint, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	CreateConcussiveBlast( tr.endpos, tr.plane.normal, this, 1.0 );
""",
b"""	UTIL_TraceHull( vecSrc, impactPoint, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	// Visible blast path from the gun to the impact.
	Vector vForward, vRight, vUp;
	pPlayer->GetVectors( &vForward, &vRight, &vUp );
	Vector vecMuzzle = vecSrc + ( vForward * 8.0f ) + ( vRight * 3.0f ) + ( vUp * -2.0f );
	UTIL_Beam( vecMuzzle, tr.endpos, m_beamIndex, 0, 0, 2.0f, 0.2f, 12, 2, 0, 0, 255, 255, 255, 200, 100 );
	UTIL_Beam( vecMuzzle, tr.endpos, m_haloIndex, 0, 0, 2.0f, 0.3f, 24, 8, 0, 0, 255, 255, 255, 96, 100 );

	CreateConcussiveBlast( tr.endpos, tr.plane.normal, this, 1.0 );
"""),
])
print("ok")
