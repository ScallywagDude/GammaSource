from pathlib import Path
p = Path(r'C:\Users\doge6\Videos\GammaSource-main\dlls\hl2_dll\npc_Manhack.cpp')
data = p.read_bytes()
nl = b'\r\n' if b'\r\n' in data else b'\n'
edits = [
    (b"\tVector pos, posOther;" + nl + b"\t// Put the force on the line",
     b"\tVector pos, posOther;" + nl +
     b"\t// Models without a collision model leave either object without physics." + nl +
     b"\tif ( !pOtherPhysics || !VPhysicsGetObject() )" + nl + b"\t\treturn;" + nl +
     b"\t// Put the force on the line"),
    (b"void CNPC_Manhack::Bump( CBaseEntity *pHitEntity, float flInterval, trace_t &tr )" + nl + b"{" + nl,
     b"void CNPC_Manhack::Bump( CBaseEntity *pHitEntity, float flInterval, trace_t &tr )" + nl + b"{" + nl +
     b"\t// Without a physics object (model lacks a collision model) the manhack cannot move or bounce." + nl +
     b"\tif ( !VPhysicsGetObject() )" + nl + b"\t\treturn;" + nl + nl),
    (b"\t// FIXME: move this" + nl + b"\tVPhysicsGetObject()->Wake();",
     b"\t// FIXME: move this" + nl +
     b"\t// Installed manhack models without a collision model never get a physics object." + nl +
     b"\tIPhysicsObject *pPhysics = VPhysicsGetObject();" + nl +
     b"\tif ( !pPhysics )" + nl + b"\t\treturn;" + nl +
     b"\tpPhysics->Wake();"),
]
for old, new in edits:
    assert data.count(old) == 1, old
    data = data.replace(old, new)
p.write_bytes(data)
print('patched')
