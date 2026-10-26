from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\dlls\BaseCombatCharacter.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
old = b"\tOnKilledNPC( inputdata.pActivator->MyCombatCharacterPointer() );"
new = (b"\t// Map I/O can fire KilledNPC with no activator (e.g. from a logic entity)." + nl +
       b"\tif ( !inputdata.pActivator )" + nl + b"\t\treturn;" + nl + old)
assert d.count(old) == 1
p.write_bytes(d.replace(old, new))
print("ok")
