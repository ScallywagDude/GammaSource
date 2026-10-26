from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\gameui\OptionsSubVideo.h")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
edits = [
    (b"\tvgui::CheckButton *m_pWindowed;", b"\tvgui::ComboBox *m_pWindowMode;"),
    (b"class CCvarSlider;",
     b"class CCvarSlider;" + nl + nl +
     b"// CVidSettings::windowed values; also the WindowMode combo box rows." + nl +
     b"enum { WINDOWMODE_FULLSCREEN = 0, WINDOWMODE_WINDOWED = 1, WINDOWMODE_BORDERLESS = 2 };"),
]
for old, new in edits:
    assert d.count(old) == 1, old
    d = d.replace(old, new)
p.write_bytes(d)
print("ok")
