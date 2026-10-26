# Shader compilation evidence — v18

Actual installed Windows SDK compilers were found beyond PATH. The source helper now discovers installed SDK binaries and accepts repeated `--tool-dir` overrides. No compiler binaries were downloaded, installed or bundled.

Actual compilation exposed a source error: `linear` is an HLSL reserved modifier, so the prior local variable prevented both FXC and DXC from compiling either entry point. Renaming it to `linearColor` preserves the formula and matches the existing GLSL naming. All four HLSL stages then compiled successfully.

| Job | Result | Artifact bytes |
|---|---|---:|
| Direct3D 11 VS | compiled_artifact_verified | 772 |
| Direct3D 11 PS | compiled_artifact_verified | 1744 |
| Direct3D 12 VS | compiled_artifact_verified | 3024 |
| Direct3D 12 PS | compiled_artifact_verified | 4952 |
| OpenGL 3.3 vert | blocked_missing_compiler | — |
| Vulkan vert | blocked_missing_compiler | — |
| OpenGL 3.3 frag | blocked_missing_compiler | — |
| Vulkan frag | blocked_missing_compiler | — |

Succeeded: 4; blocked: 4; failed: 0.

D3D11 VSMain/vs_5_0 and PSMain/ps_5_0 were compiled with SDK 10.0.28000.0 x64 fxc. D3D12 VSMain/vs_6_0 and PSMain/ps_6_0 were compiled with the same SDK x64 dxc. Every successful process returned zero and produced a nonempty DXBC container; exact commands, compiler/source/output hashes, sizes and diagnostics are recorded in SHADER_VALIDATION_V18.json.

The four generated DXBC/DXIL artifacts reside in `modernize/shaders/compiled_v18` with a source/compiler/command/hash provenance manifest. These are compiled project-owned experimental shaders, not redistributed compiler binaries. Separate QA copies remain in `../shader_asset_validation_v18`. Existing SDK versions 10.0.22621.0, 10.0.26100.0 and 10.0.28000.0 contain fxc/dxc binaries. No glslangValidator executable was found in PATH, the inspected Windows Kits or Visual Studio installations; the standard VulkanSDK locations are absent. Consequently OpenGL stage syntax checks and Vulkan SPIR-V compilation remain blocked.

Three shader tool tests passed, including actual optional HLSL stage compilation with installed compilers. Shader compilation validates these existing experimental fullscreen postprocess stages only. No GPU execution, renderer/pipeline integration, Windows engine build, modern backend operation or visual correctness is claimed. Original DX9 shader permutations still require their legacy build pipeline.

Reproduce all available jobs (the command returns nonzero while four glslang jobs remain blocked):

```text
python modernize/build_modern_shaders.py --plan --out-dir ../shader_asset_validation_v18
python modernize/build_modern_shaders.py --compile --out-dir ../shader_asset_validation_v18
python -m unittest modernize.tests.test_shader_tool_discovery_v18 -v
```
