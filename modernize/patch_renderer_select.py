#!/usr/bin/env python3
"""Add truthful renderer DLL selection to the existing Source shader API loader.

This does not implement Vulkan/GL/DX11/DX12 rendering. Those backend DLLs must
implement the original shader API interface and supply all required methods.
Unsupported DLLs fail explicitly or fall back to the original DX9 loader with
an unmistakable warning.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'materialsystem'/'cmaterialsystem.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_RENDER_BACKEND_SELECTION' not in s:
    anchor='//-----------------------------------------------------------------------------\n// Initialization + shutdown of the material system\n//-----------------------------------------------------------------------------\nInitReturnVal_t CMaterialSystem::Init()'
    assert s.count(anchor)==1
    selection='''// HL2MODERN_RENDER_BACKEND_SELECTION
// Requested renderer names map to shader API modules. There are NO modern
// renderer implementations in the overlay. A module is only usable if it
// exports the complete shader API interface and successfully initializes.
static const char *HL2Modern_ShaderAPIModule( const char *name )
{
    if ( !name || !name[0] ) return NULL;
    if ( !Q_stricmp( name, "dx9" ) || !Q_stricmp( name, "d3d9" ) ) return "shaderapidx9";
    if ( !Q_stricmp( name, "dx8" ) || !Q_stricmp( name, "d3d8" ) ) return "shaderapidx8";
    if ( !Q_stricmp( name, "vulkan" ) || !Q_stricmp( name, "vk" ) ) return "shaderapivulkan";
    if ( !Q_stricmp( name, "opengl" ) || !Q_stricmp( name, "gl" ) ) return "shaderapiogl";
    if ( !Q_stricmp( name, "dx11" ) || !Q_stricmp( name, "d3d11" ) ) return "shaderapidx11";
    if ( !Q_stricmp( name, "dx12" ) || !Q_stricmp( name, "d3d12" ) ) return "shaderapidx12";
    return NULL;
}

'''
    s=s.replace(anchor,selection+anchor)
    old='\tCreateInterfaceFn clientFactory = CreateShaderAPI( m_pShaderDLL ? m_pShaderDLL : "shaderapidx9" );\n\tif (!clientFactory)'
    assert s.count(old)==1
    new='''    // -renderapi dx9|dx8|vulkan|opengl|dx11|dx12
    // Modern options require separately implemented shaderapi*.dll backends.
    // -renderapi-strict disables fallback for predictable capability testing.
    const char *requested = CommandLine()->ParmValue( "-renderapi" );
    const char *module = m_pShaderDLL ? m_pShaderDLL : "shaderapidx9";
    if ( requested && requested[0] )
    {
        const char *selected = HL2Modern_ShaderAPIModule( requested );
        if ( !selected )
        {
            Warning( "Unknown -renderapi '%s'; valid: dx9 dx8 vulkan opengl dx11 dx12\\n", requested );
            if ( CommandLine()->FindParm( "-renderapi-strict" ) )
                return INIT_FAILED;
        }
        else
            module = selected;
    }
    CreateInterfaceFn clientFactory = CreateShaderAPI( module );
    if ( !clientFactory && requested && requested[0] && !CommandLine()->FindParm( "-renderapi-strict" ) )
    {
        Warning( "Shader API module '%s' not installed; reverting to the legacy shaderapidx9 renderer.\\n", module );
        module = "shaderapidx9";
        clientFactory = CreateShaderAPI( module );
    }
    if (!clientFactory)'''
    s=s.replace(old,new)
    # no persistent active backend name currently, diagnostic warn before trying
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Renderer DLL selection wired',p)
