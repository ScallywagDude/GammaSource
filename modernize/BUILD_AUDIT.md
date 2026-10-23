# HL2 2003 Windows build: missing input audit

Only references listed in the original Debug/Release HL2 build batch files are examined.
Checks use Windows-style case-insensitive file matching. This does **not** verify successful compilation or linking.

| Configuration | Build steps | Projects lacking compiled source files | Missing compiled inputs | Missing shader source | Missing include directories |
|---|---:|---:|---:|---:|---:|
| Debug | 61 | 0 | 0 | 0 | 20 |
| Release | 61 | 0 | 0 | 0 | 17 |

## Repair queue (active missing compilation inputs)

## How to use

Run `python modernize/build_audit.py` after extracting and overlaying the source.
Review `build_audit.json` for complete per-project inputs. Restore legitimately available source and SDKs before attempting each build.
Do not replace missing inputs with stub implementations merely to make the build appear successful.
