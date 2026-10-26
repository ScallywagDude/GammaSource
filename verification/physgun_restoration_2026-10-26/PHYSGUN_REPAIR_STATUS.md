# Physics gun repair status — 26 October 2026

The native baseline on `zoo_item` attached to a 250 kg original beta crate, but the client beam endpoint stayed at (0,0,0) even while active, registered and drawing with a valid material. This establishes the beam replication failure; baseline screenshots alone did not establish server physics movement.

The controller's target/world vectors were ordinary fields inside an `IMotionEvent`, so writes did not participate in modern entity delta change tracking. Explicit `CNetworkVector` fields on the weapon now copy both endpoint positions every weapon frame. The original 2003 Bezier draw path, texture, gameplay force/velocity settings, weapon values and hands remain in use.

Modern prop pickup callbacks now precede controller attachment, allowing an authored `SF_PHYSPROP_ENABLE_ON_PHYSCANNON` prop to enable its physics before the motion controller checks its core. Pickup permission is respected. Controller, pellet and owner lifetimes are guarded on deletion/drop/holster; leaving distance-control mode restores player movement.

`gamma_physgun_info` prints actual server-held object, controller, VPhysics mobility, positions and hit trace; `gamma_physgun_beaminfo` prints received client positions and render state. These diagnostics require normal native weapon input; they do not force a grab or bypass the simulation.

Native pickup/lift/release and beam verification passed after the successful combined build on both the 250 kg original beta crate and the independent 30 kg direct-modern MDL v44 control. Their origins rose 82.353 and 80.314 units respectively. Both runs had an active controller and movable physics, valid material/draw calls, moving nonzero beam endpoints, and controller/beam removal after release. See `physgun_native_qa.json` and the two native logs. The control is a byte-exact MDL v44/VVD/VTX/PHY extracted from the user's installed HL2 VPK under the isolated profile's `models/gamma_validation_retail/` directory; it must not be delivered as production beta art. Its provenance and hashes are recorded in `private_retail_probe_model.json`.

The beta physics gun's secondary attack places glue pellets. Native pickup/lift/release/beam tests passed on both model generations. Pellet welding and save/load need separate native regression checks before claiming those features verified.
