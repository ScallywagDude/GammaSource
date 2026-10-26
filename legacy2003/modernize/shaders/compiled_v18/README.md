# Experimental compiled postprocess stages

These four files were compiled from the project's `../hlsl/fullscreen_tonemap.hlsl` using installed Windows SDK 10.0.28000.0 x64 FXC/DXC. `provenance.json` records source/compiler/output SHA-256 values, exact arguments, stages and zero compiler exit codes. Compiler binaries are not included.

The DXBC files target D3D11 Shader Model 5.0; DXIL files target D3D12 Shader Model 6.0. This prepares shader assets for future renderer integration. The engine does not load or execute these files. No modern backend, GPU rendering or visual correctness is claimed.

Rebuild available stages with `python modernize/build_modern_shaders.py --compile --out-dir modernize/shaders/compiled_v18`. Four GLSL/Vulkan jobs still require glslangValidator; unavailable tools cause a nonzero overall result even when the four HLSL stages succeed. See `modernize/reports/SHADER_VALIDATION_V18.md` for actual validation results.
