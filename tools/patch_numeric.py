from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\cl_dll\hud_numericdisplay.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
edits = [
    (b'        !Q_stricmp(name,"HudAmmo") || !Q_stricmp(name,"HudAmmoSecondary"));',
     b'        !Q_stricmp(name,"HudAmmo") || !Q_stricmp(name,"HudAmmoSecondary") ||' + nl +
     b'        !Q_stricmp(name,"HudSuitPower"));'),
    (b'        const bool isSecondary=!Q_stricmp(name,"HudAmmoSecondary");' + nl +
     b'        const int kind=isHealth?0:isSuit?1:isSecondary?3:2;',
     b'        const bool isSecondary=!Q_stricmp(name,"HudAmmoSecondary");' + nl +
     b'        const bool isEnergy=!Q_stricmp(name,"HudSuitPower");' + nl +
     b'        const int kind=isHealth?0:isSuit?1:isSecondary?3:isEnergy?4:2;'),
    (b'            PaintBetaLabel(isHealth?"health_label":isSuit?"battery_label":"ammo_label",' + nl +
     b'                isHealth?L"HEALTH":isSuit?L"HEV":L"AMMO",0,0,tint);',
     b'            PaintBetaLabel(isHealth?"health_label":isSuit?"battery_label":isEnergy?"energy_label":"ammo_label",' + nl +
     b'                isHealth?L"HEALTH":isSuit?L"HEV":isEnergy?L"ENERGY":L"AMMO",0,0,tint);'),
    (b'            const int columns=isSecondary?2:isHealth||isSuit?3:2;',
     b'            const int columns=isSecondary?2:isHealth||isSuit||isEnergy?3:2;'),
]
for old, new in edits:
    assert d.count(old) == 1, old
    d = d.replace(old, new)
p.write_bytes(d)
print("ok")
