@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
set "BUILD_MODE=%~1"
if /I not "%BUILD_MODE%"=="Debug" if /I not "%BUILD_MODE%"=="Release" (echo Usage: build_hl2_vs2026.cmd Debug^|Release & exit /b 2)
where msbuild >nul 2>&1
if errorlevel 1 (echo MSBuild not found. Run in an x86 Native Tools Command Prompt for VS 2026. & exit /b 2)
echo Sequential HL2 2003 build - %BUILD_MODE%
if /I "%BUILD_MODE%"=="Debug" goto debug
if /I "%BUILD_MODE%"=="Release" goto release
:debug
echo [build] tier0\tier0.vcxproj [Debug]
msbuild "%ROOT%tier0\tier0.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vstdlib\vstdlib.vcxproj [Debug]
msbuild "%ROOT%vstdlib\vstdlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vtf\vtf.vcxproj [Debug]
msbuild "%ROOT%vtf\vtf.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] shaderlib\shaderlib.vcxproj [Debug]
msbuild "%ROOT%shaderlib\shaderlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dbg.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dbg.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx6.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx6.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx7.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx7.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx8.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx8.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx9.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx9.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_hdr_dx9.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_hdr_dx9.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\shader_nvfx.vcxproj [Debug]
msbuild "%ROOT%materialsystem\stdshaders\shader_nvfx.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\shader_nvfx.vcxproj [Debug ps20]
msbuild "%ROOT%materialsystem\stdshaders\shader_nvfx.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug ps20" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] unitlib\unitlib.vcxproj [Debug]
msbuild "%ROOT%unitlib\unitlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderempty\shaderempty.vcxproj [Debug]
msbuild "%ROOT%materialsystem\shaderempty\shaderempty.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderdx8\shaderdx8.vcxproj [Debug]
msbuild "%ROOT%materialsystem\shaderdx8\shaderdx8.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderdx8\playback\playback.vcxproj [Debug DX9]
msbuild "%ROOT%materialsystem\shaderdx8\playback\playback.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug DX9" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\materialsystem.vcxproj [Debug]
msbuild "%ROOT%materialsystem\materialsystem.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\mxtk\msvc\mxtoolkitwin32.vcxproj [Debug]
msbuild "%ROOT%utils\mxtk\msvc\mxtoolkitwin32.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\vgui_surfacelib\vgui_surfacelib.vcxproj [Debug]
msbuild "%ROOT%vgui2\vgui_surfacelib\vgui_surfacelib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\src\vgui_dll.vcxproj [Debug]
msbuild "%ROOT%vgui2\src\vgui_dll.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\controls\vgui_controls.vcxproj [Debug]
msbuild "%ROOT%vgui2\controls\vgui_controls.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vphysics\vphysics.vcxproj [Debug]
msbuild "%ROOT%vphysics\vphysics.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] studiorender\studiorender.vcxproj [Debug]
msbuild "%ROOT%studiorender\studiorender.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] filesystem\filesystem_stdio\filesystem_stdio.vcxproj [Debug]
msbuild "%ROOT%filesystem\filesystem_stdio\filesystem_stdio.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] filesystem\filesystem_stdio\filesystem_stdio.vcxproj [Debug STEAM]
msbuild "%ROOT%filesystem\filesystem_stdio\filesystem_stdio.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug STEAM" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] appframework\appframework.vcxproj [Debug]
msbuild "%ROOT%appframework\appframework.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vguimatsurface\vguimatsurface.vcxproj [Debug]
msbuild "%ROOT%vguimatsurface\vguimatsurface.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] gameui\GameUI.vcxproj [Debug]
msbuild "%ROOT%gameui\GameUI.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] engine\voice_codecs\miles\miles.vcxproj [Debug]
msbuild "%ROOT%engine\voice_codecs\miles\miles.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] gameui\GameUI.vcxproj [Debug]
msbuild "%ROOT%gameui\GameUI.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] Tracker\ServerBrowser\ServerBrowser.vcxproj [Debug]
msbuild "%ROOT%Tracker\ServerBrowser\ServerBrowser.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vmpi\vmpi.vcxproj [Debug]
msbuild "%ROOT%utils\vmpi\vmpi.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vbspinfo\vbspinfo.vcxproj [Debug]
msbuild "%ROOT%utils\vbspinfo\vbspinfo.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vbsp\vbsp.vcxproj [Debug]
msbuild "%ROOT%utils\vbsp\vbsp.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vrad\vrad.vcxproj [Debug]
msbuild "%ROOT%utils\vrad\vrad.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vrad_launcher\vrad_launcher.vcxproj [Debug]
msbuild "%ROOT%utils\vrad_launcher\vrad_launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vvis\vvis.vcxproj [Debug]
msbuild "%ROOT%utils\vvis\vvis.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vvis_launcher\vvis_launcher.vcxproj [Debug]
msbuild "%ROOT%utils\vvis_launcher\vvis_launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\nvtristriplib\nvtristriplib.vcxproj [Debug]
msbuild "%ROOT%utils\nvtristriplib\nvtristriplib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\studiomdl\studiomdl.vcxproj [Debug]
msbuild "%ROOT%utils\studiomdl\studiomdl.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\hlmviewer\src\hlmv.vcxproj [Debug]
msbuild "%ROOT%utils\hlmviewer\src\hlmv.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\hlfaceposer\hlfaceposer.vcxproj [Debug]
msbuild "%ROOT%utils\hlfaceposer\hlfaceposer.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\scenemanager\scenemanager.vcxproj [Debug]
msbuild "%ROOT%utils\scenemanager\scenemanager.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\phonemeextractor\phonemeextractor.vcxproj [Debug]
msbuild "%ROOT%utils\phonemeextractor\phonemeextractor.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\phonemeextractor\phonemeextractor.vcxproj [Debug IMS]
msbuild "%ROOT%utils\phonemeextractor\phonemeextractor.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug IMS" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\newdat\newdat.vcxproj [Debug]
msbuild "%ROOT%utils\newdat\newdat.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtex\vtex.vcxproj [Debug]
msbuild "%ROOT%utils\vtex\vtex.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtex\vtex_dll.vcxproj [Debug]
msbuild "%ROOT%utils\vtex\vtex_dll.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\unittest\unittest.vcxproj [Debug]
msbuild "%ROOT%utils\unittest\unittest.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bsppack\bsppack.vcxproj [Debug]
msbuild "%ROOT%utils\bsppack\bsppack.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bspzip\bspzip.vcxproj [Debug]
msbuild "%ROOT%utils\bspzip\bspzip.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\height2normal\height2normal.vcxproj [Debug]
msbuild "%ROOT%utils\height2normal\height2normal.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtf2tga\vtf2tga.vcxproj [Debug]
msbuild "%ROOT%utils\vtf2tga\vtf2tga.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bugreporter\bugreporter.vcxproj [Debug]
msbuild "%ROOT%utils\bugreporter\bugreporter.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] engine\engine.vcxproj [Debug]
msbuild "%ROOT%engine\engine.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] dedicated\dedicated.vcxproj [Debug]
msbuild "%ROOT%dedicated\dedicated.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] worldcraft\worldcraft.vcxproj [Debug]
msbuild "%ROOT%worldcraft\worldcraft.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] dlls\hl.vcxproj [Debug HL2]
msbuild "%ROOT%dlls\hl.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug HL2" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] cl_dll\client.vcxproj [Debug HL2]
msbuild "%ROOT%cl_dll\client.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug HL2" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] launcher\launcher.vcxproj [Debug]
msbuild "%ROOT%launcher\launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] launcher_main\launcher_main.vcxproj [Debug]
msbuild "%ROOT%launcher_main\launcher_main.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Debug" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo Finished Debug in original batch-file order.
exit /b 0
:release
echo [build] tier0\tier0.vcxproj [Release]
msbuild "%ROOT%tier0\tier0.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vstdlib\vstdlib.vcxproj [Release]
msbuild "%ROOT%vstdlib\vstdlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] unitlib\unitlib.vcxproj [Release]
msbuild "%ROOT%unitlib\unitlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vtf\vtf.vcxproj [Release]
msbuild "%ROOT%vtf\vtf.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] shaderlib\shaderlib.vcxproj [Release]
msbuild "%ROOT%shaderlib\shaderlib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dbg.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dbg.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx6.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx6.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx7.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx7.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx8.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx8.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_dx9.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_dx9.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\stdshader_hdr_dx9.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\stdshader_hdr_dx9.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\shader_nvfx.vcxproj [Release]
msbuild "%ROOT%materialsystem\stdshaders\shader_nvfx.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\stdshaders\shader_nvfx.vcxproj [Release ps20]
msbuild "%ROOT%materialsystem\stdshaders\shader_nvfx.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release ps20" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderempty\shaderempty.vcxproj [Release]
msbuild "%ROOT%materialsystem\shaderempty\shaderempty.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderdx8\shaderdx8.vcxproj [Release]
msbuild "%ROOT%materialsystem\shaderdx8\shaderdx8.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\shaderdx8\playback\playback.vcxproj [Release DX9]
msbuild "%ROOT%materialsystem\shaderdx8\playback\playback.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release DX9" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] materialsystem\materialsystem.vcxproj [Release]
msbuild "%ROOT%materialsystem\materialsystem.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\mxtk\msvc\mxtoolkitwin32.vcxproj [Release]
msbuild "%ROOT%utils\mxtk\msvc\mxtoolkitwin32.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\vgui_surfacelib\vgui_surfacelib.vcxproj [Release]
msbuild "%ROOT%vgui2\vgui_surfacelib\vgui_surfacelib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\src\vgui_dll.vcxproj [Release]
msbuild "%ROOT%vgui2\src\vgui_dll.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vgui2\controls\vgui_controls.vcxproj [Release]
msbuild "%ROOT%vgui2\controls\vgui_controls.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vphysics\vphysics.vcxproj [Release]
msbuild "%ROOT%vphysics\vphysics.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] studiorender\studiorender.vcxproj [Release]
msbuild "%ROOT%studiorender\studiorender.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] filesystem\filesystem_stdio\filesystem_stdio.vcxproj [Release]
msbuild "%ROOT%filesystem\filesystem_stdio\filesystem_stdio.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] filesystem\filesystem_stdio\filesystem_stdio.vcxproj [Release STEAM]
msbuild "%ROOT%filesystem\filesystem_stdio\filesystem_stdio.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release STEAM" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] appframework\appframework.vcxproj [Release]
msbuild "%ROOT%appframework\appframework.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] vguimatsurface\vguimatsurface.vcxproj [Release]
msbuild "%ROOT%vguimatsurface\vguimatsurface.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] gameui\GameUI.vcxproj [Release]
msbuild "%ROOT%gameui\GameUI.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] engine\voice_codecs\miles\miles.vcxproj [Release]
msbuild "%ROOT%engine\voice_codecs\miles\miles.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] Tracker\ServerBrowser\ServerBrowser.vcxproj [Release]
msbuild "%ROOT%Tracker\ServerBrowser\ServerBrowser.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] gameui\GameUI.vcxproj [Release]
msbuild "%ROOT%gameui\GameUI.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vmpi\vmpi.vcxproj [Release]
msbuild "%ROOT%utils\vmpi\vmpi.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vbspinfo\vbspinfo.vcxproj [Release]
msbuild "%ROOT%utils\vbspinfo\vbspinfo.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vbsp\vbsp.vcxproj [Release]
msbuild "%ROOT%utils\vbsp\vbsp.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vrad\vrad.vcxproj [Release]
msbuild "%ROOT%utils\vrad\vrad.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vrad_launcher\vrad_launcher.vcxproj [Release]
msbuild "%ROOT%utils\vrad_launcher\vrad_launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vvis\vvis.vcxproj [Release]
msbuild "%ROOT%utils\vvis\vvis.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vvis_launcher\vvis_launcher.vcxproj [Release]
msbuild "%ROOT%utils\vvis_launcher\vvis_launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\nvtristriplib\nvtristriplib.vcxproj [Release]
msbuild "%ROOT%utils\nvtristriplib\nvtristriplib.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\studiomdl\studiomdl.vcxproj [Release]
msbuild "%ROOT%utils\studiomdl\studiomdl.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\hlmviewer\src\hlmv.vcxproj [Release]
msbuild "%ROOT%utils\hlmviewer\src\hlmv.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\hlfaceposer\hlfaceposer.vcxproj [Release]
msbuild "%ROOT%utils\hlfaceposer\hlfaceposer.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\scenemanager\scenemanager.vcxproj [Release]
msbuild "%ROOT%utils\scenemanager\scenemanager.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\phonemeextractor\phonemeextractor.vcxproj [Release]
msbuild "%ROOT%utils\phonemeextractor\phonemeextractor.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\phonemeextractor\phonemeextractor.vcxproj [Release IMS]
msbuild "%ROOT%utils\phonemeextractor\phonemeextractor.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release IMS" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\newdat\newdat.vcxproj [Release]
msbuild "%ROOT%utils\newdat\newdat.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtex\vtex.vcxproj [Release]
msbuild "%ROOT%utils\vtex\vtex.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtex\vtex_dll.vcxproj [Release]
msbuild "%ROOT%utils\vtex\vtex_dll.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\unittest\unittest.vcxproj [Release]
msbuild "%ROOT%utils\unittest\unittest.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bsppack\bsppack.vcxproj [Release]
msbuild "%ROOT%utils\bsppack\bsppack.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bspzip\bspzip.vcxproj [Release]
msbuild "%ROOT%utils\bspzip\bspzip.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\height2normal\height2normal.vcxproj [Release]
msbuild "%ROOT%utils\height2normal\height2normal.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\vtf2tga\vtf2tga.vcxproj [Release]
msbuild "%ROOT%utils\vtf2tga\vtf2tga.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] utils\bugreporter\bugreporter.vcxproj [Release]
msbuild "%ROOT%utils\bugreporter\bugreporter.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] engine\engine.vcxproj [Release]
msbuild "%ROOT%engine\engine.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] dedicated\dedicated.vcxproj [Release]
msbuild "%ROOT%dedicated\dedicated.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] worldcraft\worldcraft.vcxproj [Release]
msbuild "%ROOT%worldcraft\worldcraft.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] dlls\hl.vcxproj [Release HL2]
msbuild "%ROOT%dlls\hl.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release HL2" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] cl_dll\client.vcxproj [Release HL2]
msbuild "%ROOT%cl_dll\client.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release HL2" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] launcher\launcher.vcxproj [Release]
msbuild "%ROOT%launcher\launcher.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo [build] launcher_main\launcher_main.vcxproj [Release]
msbuild "%ROOT%launcher_main\launcher_main.vcxproj" /nologo /m:1 /t:Build /p:Configuration="Release" /p:Platform=Win32 /v:minimal
if errorlevel 1 exit /b 1
echo Finished Release in original batch-file order.
exit /b 0
