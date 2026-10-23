# Cross-API filmic postprocessing shader experiments (v7)

These independent shaders are provided for integration into **future** Vulkan,
OpenGL 3.3+, D3D11, and D3D12 rendering backends. They do not currently run
in Half-Life 2. A full backend must supply a scene-color texture, an actual
fullscreen draw/graphics pipeline, descriptors, state transitions, and a
presentation target. The original DX9 shader API remains active.

Inputs: `SceneColor` (linear HDR RGBA) plus float4 `controls`:
- `x`: exposure multiplier, >=0; 1.0 preserves exposure.
- `y`: filmic ACES-fitted operator enabled when >0.5; otherwise clamp linear.
- `z`: manual linear-to-sRGB encoding enabled when >0.5. Set to 0 when using
  hardware sRGB render-target conversion to avoid applying the curve twice.
- `w`: reserved.

Resources: HLSL uses t0/s0/b0; Vulkan GLSL uses set=0 binding=0 sampled
image and set=0 binding=1 UBO. OpenGL 3.3 uses sampler and vec4 uniforms.
Full-screen triangle uses three vertices without a vertex buffer. Texture
vertical orientation differs by backend, so flip V or adjust rendering
viewport as needed during integration.

Compile helpers: `python modernize/build_modern_shaders.py --plan` shows the
required tools. `--compile` uses `fxc` for D3D11 DXBC, `dxc` for D3D12 DXIL,
and `glslangValidator` for SPIR-V/GLSL validation if installed. This project
does not redistribute compiler binaries or claim these shaders have been
GPU-tested. OpenGL 3.3 uses `glslangValidator` front-end syntax validation
without a SPIR-V output.

The shared reference curve is tested on the CPU under
`modernize/tests/test_lighting_reference.py`.
