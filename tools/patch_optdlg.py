from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\gameui\OptionsDialog.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b'\tif (HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()))' + nl +
       b'\t\tAddPage(new COptionsSubHUD(this), "HUD");')
new = (b'\t// Show the HUD tab whenever the HL2 client registered its HUD color cvars, so it' + nl +
       b'\t// also appears for game folders not named "hl2" (e.g. hl2_2003).' + nl +
       b'\tif (cvar->FindVar("hud_color_custom") || HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()))' + nl +
       b'\t\tAddPage(new COptionsSubHUD(this), "HUD");')
assert d.count(old) == 1
p.write_bytes(d.replace(old, new))
print("ok")
