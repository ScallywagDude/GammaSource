"""2018 HL2 game rules: add the 2003 ammo types (and their skill cvars) the GammaSource weapons use."""
import re
from pathlib import Path

R = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")
G = Path(r"C:\Users\doge6\Videos\GammaSource-main")
NAMES = ["SmallRound", "MediumRound", "LargeRound", "FlareRound", "ML_Grenade", "AR2_Grenade", "Slam", "Tripwire", "Molotov", "Brickbat", "Rock"]

# 1) room for more ammo types
p = R / "game/shared/shareddefs.h"
t = p.read_text(encoding="latin-1")
t = re.sub(r"#define\tMAX_AMMO_TYPES\t32\t\t// \?\?\?", "#define\tMAX_AMMO_TYPES\t64\t\t// GammaSource: 2018 HL2 + the 2003 ammo types", t)
t = re.sub(r"#define MAX_AMMO_SLOTS  32\t\t// not really slots", "#define MAX_AMMO_SLOTS  64\t\t// not really slots (GammaSource: was 32)", t)
p.write_text(t, encoding="latin-1")

g = (G / "game_shared/hl2/hl2_gamerules.cpp").read_text(encoding="latin-1").replace("\r\n", "\n")
p = R / "game/shared/hl2/hl2_gamerules.cpp"
t = p.read_text(encoding="latin-1")
nl = "\r\n" if "\r\n" in t else "\n"
t = t.replace("\r\n", "\n")

if "GammaSource: 2003 ammo" not in t:
    lines = [l for l in g.split("\n") if re.search(r'AddAmmoType\(\s*"(%s)"' % "|".join(NAMES), l)]
    cvars = set()
    for l in lines:
        cvars.update(re.findall(r'"(sk_\w+)"', l))
    cvar_defs = []
    for c in sorted(cvars):
        if re.search(r'ConVar\s+%s\b' % c, t):
            continue
        m = re.search(r'^ConVar\s+%s\b[^\n]*$' % c, g, re.M)
        cvar_defs.append(m.group(0) if m else 'ConVar\t%s( "%s", "0", FCVAR_REPLICATED );' % (c, c))
    # ammo entries get the 2018 trailing flags argument
    new_lines = [re.sub(r'\);\s*$', ', 0 );', l) for l in lines]

    anchor = "CAmmoDef *GetAmmoDef()\n"
    t = t.replace(anchor, "// GammaSource: 2003 ammo skill cvars\n" + "\n".join(cvar_defs) + "\n\n" + anchor, 1)
    # add after the last AddAmmoType of the function
    i = t.index(anchor)
    j = t.rindex("def.AddAmmoType(", i)
    j = t.index("\n", j) + 1
    t = t[:j] + "\t\t// GammaSource: 2003 ammo types used by the beta weapons and their scripts\n" + "\n".join(new_lines) + "\n" + t[j:]
p.write_text(t.replace("\n", nl), encoding="latin-1")
print("ok")
