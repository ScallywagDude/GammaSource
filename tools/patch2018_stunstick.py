"""2018 stunstick: bring over the GammaSource fix where the hit lands when the baton connects
(about a third into the 0.67 s v_stunbaton swing), not at the start of the animation."""
from pathlib import Path
R = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game\server")

def edit(rel, old, new):
    p = R / rel
    t = p.read_text(encoding="latin-1"); nl = "\r\n" if "\r\n" in t else "\n"; t = t.replace("\r\n", "\n")
    if new in t:
        return
    assert t.count(old) == 1, (rel, old[:50], t.count(old))
    p.write_text(t.replace(old, new).replace("\n", nl), encoding="latin-1")

# subclasses need the swing helpers (same change as the 2003 basebludgeonweapon.h)
edit("basebludgeonweapon.h",
     "private:\n\tbool\t\t\tImpactWater( const Vector &start, const Vector &end );\n\tvoid\t\t\tSwing( int bIsSecondary );\n\tvoid\t\t\tHit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary );\n\tActivity\t\tChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );",
     "protected:\t// GammaSource: was private; the stunstick times its own hit\n\tbool\t\t\tImpactWater( const Vector &start, const Vector &end );\n\tvoid\t\t\tSwing( int bIsSecondary );\n\tvoid\t\t\tHit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary );\n\tActivity\t\tChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );")

edit("hl2/weapon_stunstick.h", "#define\tSTUNSTICK_RANGE\t\t75.0f\n#define\tSTUNSTICK_REFIRE\t0.6f",
     "#define\tSTUNSTICK_RANGE\t\t48.0f\t// GammaSource: 2003 reach\n#define\tSTUNSTICK_REFIRE\t0.7f\t// v_stunbaton attack sequences last 0.67s\n#define\tSTUNSTICK_HIT_DELAY\t0.25f\t// the baton connects about a third into the swing")
edit("hl2/weapon_stunstick.h", "\tvoid\t\tSecondaryAttack( void )\t{}\n",
     "\tvoid\t\tSecondaryAttack( void )\t{}\n\tvoid\t\tPrimaryAttack( void );\n\tvoid\t\tItemPostFrame( void );\n")
edit("hl2/weapon_stunstick.h", "\tCNetworkVar( bool, m_bActive );",
     "\tCNetworkVar( bool, m_bActive );\n\nprivate:\n\tbool\t\tTraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity );\n\tfloat\t\tm_flDelayedHitTime;\t// 0 when no swing is waiting to connect\npublic:")

edit("hl2/weapon_stunstick.cpp", "\tDEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),\n",
     "\tDEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),\n\tDEFINE_FIELD( m_flDelayedHitTime, FIELD_TIME ),\n")

impl = '''
//-----------------------------------------------------------------------------
// GammaSource: same reach test as the base swing; true when something is in range
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
			hitActivity = ChooseIntersectionPointAndActivity( traceHit, mins, maxs, pOwner );
	}
	return traceHit.fraction < 1.0f;
}

//-----------------------------------------------------------------------------
// GammaSource: start the swing now; the impact is applied when the baton connects
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
// GammaSource: apply a pending swing's impact at the right moment
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
			Hit( traceHit, hitActivity, false );
	}

	BaseClass::ItemPostFrame();
}
'''
p = R / "hl2/weapon_stunstick.cpp"
t = p.read_text(encoding="latin-1")
if "CWeaponStunStick::TraceSwing" not in t:
    nl = "\r\n" if "\r\n" in t else "\n"
    p.write_text(t.rstrip() + nl + impl.replace("\n", nl), encoding="latin-1")
# init the new member in the constructor
edit("hl2/weapon_stunstick.cpp", "CWeaponStunStick::CWeaponStunStick( void )\n{\n", "CWeaponStunStick::CWeaponStunStick( void )\n{\n\tm_flDelayedHitTime = 0.0f;\n")
print("ok")
