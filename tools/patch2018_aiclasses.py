"""2018 HL2: AI classes and hint types the 2003 NPCs use, with their 2003 relationships."""
import re
from pathlib import Path

R = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game")
G = Path(r"C:\Users\doge6\Videos\GammaSource-main")
NEW = ["CLASS_MORTAR_SYNTH", "CLASS_WASTE_SCANNER", "CLASS_CREMATOR"]

def rw(p):
    t = p.read_text(encoding="latin-1")
    return t.replace("\r\n", "\n"), ("\r\n" if "\r\n" in t else "\n")

# Class_T (HL2 branch only: the first enum in baseentity.h)
p = R / "server/baseentity.h"
t, nl = rw(p)
if "CLASS_MORTAR_SYNTH" not in t:
    t = t.replace("\tCLASS_COMBINE_HUNTER,\n\n\tNUM_AI_CLASSES", "\tCLASS_COMBINE_HUNTER,\n\t// GammaSource: 2003 NPC classes\n" + "".join("\t%s,\n" % c for c in NEW) + "\n\tNUM_AI_CLASSES", 1)
    p.write_text(t.replace("\n", nl), encoding="latin-1")

# hint types (same numbers the 2018 enum keeps as NOT_USED)
p = R / "server/ai_hint.h"
t, nl = rw(p)
if "HINT_ROLLER_PATROL_POINT " not in t:
    t = t.replace("\tHINT_NOT_USED_ROLLER_PATROL_POINT = 500,\n\tHINT_NOT_USED_ROLLER_CLEANUP_POINT,",
                  "\tHINT_ROLLER_PATROL_POINT = 500,\t// GammaSource: the 2003 rollers use these again\n\tHINT_ROLLER_CLEANUP_POINT,")
    p.write_text(t.replace("\n", nl), encoding="latin-1")

# relationships
g, _ = rw(G / "game_shared/hl2/hl2_gamerules.cpp")
p = R / "shared/hl2/hl2_gamerules.cpp"
t, nl = rw(p)
if "GammaSource: 2003 NPC classes" not in t:
    enum, _ = rw(R / "server/baseentity.h")
    known = set(re.findall(r"\b(CLASS_\w+)\b", enum.split("#elif")[0]))
    rows = []
    for l in g.split("\n"):
        m = re.search(r"SetDefaultRelationship\(\s*(CLASS_\w+)\s*,\s*(CLASS_\w+)", l)
        if m and (m.group(1) in NEW or m.group(2) in NEW) and m.group(1) in known and m.group(2) in known:
            rows.append("\t\t" + l.strip())
    i = t.index("void CHalfLife2::InitDefaultAIRelationships")
    # last SetDefaultRelationship in the function
    j = t.index("\n", t.rindex("SetDefaultRelationship(", i, t.index("\n}\n", i))) + 1
    t = t[:j] + "\n\t\t// GammaSource: 2003 NPC classes (from the 2003 hl2_gamerules.cpp)\n" + "\n".join(rows) + "\n" + t[j:]
    p.write_text(t.replace("\n", nl), encoding="latin-1")
    print("relationship rows:", len(rows))
print("ok")
