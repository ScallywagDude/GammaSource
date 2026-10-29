# Mega Build immolator integration

The active `game/server/gamma/weapon_immolator.cpp` already descended from the
Mega Build's `dlls/hl2_dll/weapon_immolator.cpp`. This change retains its committed
green beam, growing burn radius (50 units per second to 256), direct-hit damage,
wall-impact burn field, and two-second cooldown.

The converted Mega Build `v_cremator` and `w_immolator` assets are now in
`gammasource_overlay/models/weapons`, with their material, texture and sound
dependencies. The first-person model has its actual Idle, Draw and looping Fire
sequences mapped to `ACT_VM_IDLE`, `ACT_VM_DRAW`, and `ACT_VM_PRIMARYATTACK`.
The original v37 files cannot be loaded directly by the v44-v48 engine. The
staging manifest records the original and converted hashes rather than claiming
that a converted model is byte-identical to its old-format source.

Fixes in the weapon:

- Start the supplied looping Fire animation and matching third-person attack
  gesture; keep the existing idle/draw model activities.
- Use the original start, run and off sounds with explicit stop cleanup.
- Consume five Gasoline units per second and stop cleanly when empty.
- Do not reschedule `UpdateThink` after `Update` has stopped the charge. The old
  unconditional scheduling revived the stopped charge on the next think.
- Stop damage, sounds and thinking on holster, drop, owner death, submersion,
  entity removal, or loss of the active weapon. Restore a live saved charge's
  run sound and think state.
- Enforce the cooldown even when repeated mouse presses cause base weapon code
  to reset `m_flNextPrimaryAttack`.
- Use the configured 4096-unit range, ignore the real owner in the beam trace,
  and keep world solids between the expanding burn field and nearby actors.

The original behavior is a committed charge: releasing primary fire does not
cancel it. Holstering cancels it. This is deliberate parity with the supplied
Mega Build source, rather than a new flamethrower replacement.

Native test fixtures under `verification/immolator_tests` exercise a complete
charge after button release, empty fuel, and holstering. They do not rebind any
keys. Run each in a separate isolated game session after giving the weapon a
clear aim direction; `check_immolator_log.py` validates the captured engine log.
`gamma_immolator_status` reports the owned weapon while active or holstered.
`gamma_immolator_fuel` is a cheat-only fuel setup command for the fixtures.

The first native server build passed. The coordinating task owns the final
combined build and in-game execution of the fixtures; absent captured runtime
logs, the presence of a fixture is not proof that it passed.
