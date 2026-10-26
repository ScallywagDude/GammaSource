from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\engine\host_cmd.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b"\tif ( Cmd_Argc() != 2 )" + nl + b"\t{" + nl + b"\t\treturn;" + nl + b"\t}" + nl + nl +
       b"\tif ( !stricmp( Cmd_Argv(1), \"windowed\" ) )" + nl + b"\t{" + nl + b"\t\tfullscreen = false;" + nl + b"\t}" + nl + nl +
       b"\tVideoMode_ChangeFullscreenMode( fullscreen );")
new = (b"\t// GameUI sends \"_setrenderer <renderer> <mode>\"; older callers send only the mode." + nl +
       b"\tif ( Cmd_Argc() != 2 && Cmd_Argc() != 3 )" + nl + b"\t{" + nl + b"\t\treturn;" + nl + b"\t}" + nl + nl +
       b"\tconst char *mode = Cmd_Argv( Cmd_Argc() - 1 );" + nl +
       b"\tbool borderless = !stricmp( mode, \"borderless\" );" + nl +
       b"\tif ( borderless || !stricmp( mode, \"windowed\" ) )" + nl + b"\t{" + nl + b"\t\tfullscreen = false;" + nl + b"\t}" + nl + nl +
       b"\tregistry->WriteInt( \"ScreenBorderless\", borderless ? 1 : 0 );" + nl +
       b"\tVideoMode_ChangeFullscreenMode( fullscreen );")
assert d.count(old) == 1
d = d.replace(old, new)
p.write_bytes(d)
print("ok")
