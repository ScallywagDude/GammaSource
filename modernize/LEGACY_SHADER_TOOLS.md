# Building the original shader libraries

These tools rebuild the existing DirectX shader profiles. They do not implement
the Phase 3 renderer work. A successful shader DLL build does not prove that its
shaders render correctly in the game.

The bundled 2003 Perl/compiler executables fail to load on the tested Windows
host. Generated projects therefore run the original preparation scripts through
`run_shader_tool.py`, using Python 3 and a configurable Perl interpreter. Set
`HL2_MODERN_PERL`, pass `/p:HL2ModernPerl=...` to MSBuild, or put `perl` on PATH.
No bundled executables are replaced.

The tested portable interpreter is [Strawberry Perl 5.42.3.1](https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54231_64bit/strawberry-perl-5.42.3.1-64bit-portable.zip),
listed in the [official release manifest](https://strawberryperl.com/releases.json).
Its ZIP SHA-256 is
`6a081a811781c30aca51dbc036afd93092af91e3297901f02c17043795a10690`.
Extract it outside the checkout; point the override at `perl/bin/perl.exe`.

The Windows SDK supplies FXC and D3DCompiler for HLSL compilation and legacy
shader assembly. `HL2_MODERN_FXC` and `HL2_MODERN_D3DCOMPILER` can override their
discovered paths. D3DCompiler's architecture must match the Python process.
Generated headers contain DWORD tokens, as the original shader registration code
requires, and retain each source's declared shader model.

Some original `vs_2_0` lighting permutations exceed the modern FXC allocator's
12 temporary registers. The helper first retries only register overflow errors
with IEEE strictness, retaining the profile and full precision. An optional
`HL2_MODERN_D3DX9` path selects the earlier D3DX compiler if that retry still
reports exclusively X4505 errors. Syntax and other errors remain build failures;
the fallback rejects options it cannot preserve. Partial precision is never
introduced.

The tested fallback is Microsoft's signed `d3dx9_31.dll`, extracted from
`OCT2006_d3dx9_31_x64.cab` in the official
[DirectX End-User Runtimes (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109).
The downloaded `directx_Jun2010_redist.exe` SHA-256 was
`053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b`;
the extracted x64 DLL SHA-256 was
`84167b7df7e97c63f0cc15c1d272cdc7bba942f6f36a8801aec9840825891d0d`.
Keep this dependency outside the checkout. The redist's `/Q /T:ABSOLUTE_DIRECTORY`
switches extract its CABs; `expand.exe -F:d3dx9_31.dll CAB OUTPUT_DIRECTORY` extracts
the chosen DLL. No system installation or `DXSETUP.exe` execution is needed.
Use the x86 CAB instead if Python is 32-bit; the DLL architecture follows Python,
independently of the Win32 game target.

From a Visual Studio developer command prompt, with the prerequisite libraries
built for the same configuration:

```bat
set "HL2_MODERN_PERL=C:\task-tools\strawberry\perl\bin\perl.exe"
set "HL2_MODERN_D3DX9=C:\task-tools\d3dx9_31_x64\d3dx9_31.dll"
msbuild materialsystem\stdshaders\stdshader_dbg.vcxproj /m:1 /p:Configuration=Release /p:Platform=Win32
msbuild materialsystem\stdshaders\stdshader_dx9.vcxproj /m:1 /p:Configuration=Release /p:Platform=Win32
py -3 -m unittest discover -s modernize\tests -p "test_legacy_shader*.py" -v
```

Run shader custom builds serially. The original preparation scripts share
temporary files in `materialsystem/stdshaders`. Generated headers, downloaded
dependencies, and built DLLs are local build outputs and must not be committed.

The local verification compiled all 352 `teeth_vs20.fxc` permutations through
D3DX9_31 with their original `vs_2_0` profile and full precision, checking version
and end tokens. The optional real-compiler test runs when `HL2_MODERN_D3DX9` is
set; other focused tests cover routing, exact bytecode packing, and error
propagation.
