"""Extinguisher: play the viewmodel fire/idle animations, and make the client jet draw
at the muzzle (it was parented to the weapon yet given world coordinates as a local
offset, and had zero render bounds so it was culled)."""
from pathlib import Path
import re

def patch(path, edits):
    p = Path(path); d = p.read_bytes()
    nl = b"\r\n" if b"\r\n" in d else b"\n"
    for old, new in edits:
        old = old.replace(b"\n", nl); new = new.replace(b"\n", nl)
        assert d.count(old) == 1, (path, old[:80])
        d = d.replace(old, new)
    p.write_bytes(d)

# ---- server: animations
patch(r"C:\Users\doge6\Videos\GammaSource-main\dlls\hl2_dll\weapon_extinguisher.cpp", [
(b"""	//Only shoot if we have ammo
	if ( pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 )
	{
		StopJet();
		return;
	}
""",
b"""	//Only shoot if we have ammo
	if ( pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 )
	{
		StopJet();
		StopSprayAnim();
		return;
	}
"""),
(b"""		//If we're just run out...
		if ( pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 )
		{
			StopJet();
			return;
		}

		//Turn the jet on
		StartJet();
""",
b"""		//If we're just run out...
		if ( pOwner->GetAmmoCount(m_iSecondaryAmmoType) <= 0 )
		{
			StopJet();
			StopSprayAnim();
			return;
		}

		//Turn the jet on
		StartJet();

		// Loop the viewmodel spray animation while the trigger is held.
		if ( GetActivity() != ACT_VM_PRIMARYATTACK || HasWeaponIdleTimeElapsed() )
		{
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			SetWeaponIdleTime( gpGlobals->curtime + EXTINGUISHER_SPRAY_LOOP );
		}
"""),
(b"""	else
	{
		StopJet();
	}
}
""",
b"""	else
	{
		StopJet();
		StopSprayAnim();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the viewmodel to idle once spraying stops
//-----------------------------------------------------------------------------
void CWeaponExtinguisher::StopSprayAnim( void )
{
	if ( GetActivity() == ACT_VM_PRIMARYATTACK )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
}
"""),
(b"""	void	StartJet( void );
	void	StopJet( void );
""",
b"""	void	StartJet( void );
	void	StopJet( void );
	void	StopSprayAnim( void );
"""),
(b"""#define	EXTINGUISHER_AMMO_RATE	0.2
""",
b"""#define	EXTINGUISHER_AMMO_RATE	0.2
#define	EXTINGUISHER_SPRAY_LOOP	0.5	// restart the (non-looping) fire sequence this often
"""),
])

# ---- client: draw position and bounds
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\cl_dll\hl2_hud\c_extinguisher.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
start = d.index(b"void C_ExtinguisherJet::Update( float fTimeDelta )")
end = d.index(b"int C_ExtinguisherJet::DrawModel( int flags )")
body = d[start:end]
old_block = (b"""	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( m_bUseMuzzlePoint )
	{
		C_BaseViewModel *vm = player ? player->GetViewModel( 0 ) : NULL;

		if ( vm )
		{
			int iAttachment = vm->LookupAttachment( "muzzle" );
			Vector origin;
			QAngle angles;
			vm->GetAttachment( iAttachment, origin, angles );

			Assert( !GetMoveParent() );
			SetLocalOrigin( origin );
			SetLocalAngles( angles );
		}
	}
""").replace(b"\n", nl)
new_block = (b"""	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	// The jet is parented to the weapon, so never write world coordinates into its
	// local origin; work from the viewmodel muzzle (or our own position) directly.
	Vector vecOrigin = GetAbsOrigin();
	QAngle angJet = GetAbsAngles();
	if ( m_bUseMuzzlePoint )
	{
		C_BaseViewModel *vm = player ? player->GetViewModel( 0 ) : NULL;

		if ( vm )
		{
			int iAttachment = vm->LookupAttachment( "muzzle" );
			if ( iAttachment > 0 )
			{
				vm->GetAttachment( iAttachment, vecOrigin, angJet );
			}
		}
	}
""").replace(b"\n", nl)
assert body.count(old_block) == 1
body = body.replace(old_block, new_block)
head, rest = body.split(new_block)
rest = rest.replace(b"GetAbsOrigin()", b"vecOrigin").replace(b"GetAbsAngles()", b"angJet")
body = head + new_block + rest
d = d[:start] + body + d[end:]
# render bounds so the particle jet is not culled (the entity has no model)
old_decl = b"\tbool\tShouldDraw( void ) { return m_bEmit; }"
assert d.count(old_decl) == 1
d = d.replace(old_decl, old_decl + nl +
    b"\t// No model: give the jet room so the leaf system does not cull it." + nl +
    b"\tvirtual void\tGetRenderBounds( Vector& mins, Vector& maxs ) { mins.Init( -256, -256, -256 ); maxs.Init( 256, 256, 256 ); }")
p.write_bytes(d)
print("ok")
