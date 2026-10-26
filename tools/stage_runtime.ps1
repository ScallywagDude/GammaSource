# Assemble an isolated runtime from fresh Release builds. The installed game is only read.
param(
    [string]$Src = 'C:\Users\doge6\Videos\GammaSource-main',
    [string]$Install = "$env:USERPROFILE\Documents\HL2BetaMegaCuck",
    [string]$Out = 'C:\Users\doge6\Videos\GammaRuntime'
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force "$Out\bin", "$Out\gamma\bin", "$Out\gamma\cfg" | Out-Null

# Third-party runtime files (Miles, configs) come from the installed bin; fresh builds overwrite engine modules.
Copy-Item "$Install\bin\*" "$Out\bin" -Recurse -Force

$bin = @{
    'tier0\build\Release\tier0.dll' = 'tier0.dll'
    'vstdlib\build\Release\vstdlib.dll' = 'vstdlib.dll'
    'unitlib\build\Release\unitlib.dll' = 'unitlib.dll'
    'launcher\build\Release\launcher.dll' = 'launcher.dll'
    'engine\build\Release\engine.dll' = 'engine.dll'
    'filesystem\filesystem_stdio\build\Release\filesystem_stdio.dll' = 'FileSystem_Stdio.dll'
    'filesystem\filesystem_stdio\build\Release STEAM\filesystem_steam.dll' = 'FileSystem_Steam.dll'
    'materialsystem\build\Release\materialsystem.dll' = 'MaterialSystem.dll'
    'materialsystem\shaderdx8\build\Release\shaderapidx8.dll' = 'shaderapidx9.dll'
    'materialsystem\shaderempty\build\Release\shaderapiempty.dll' = 'shaderapiempty.dll'
    'studiorender\build\Release\studiorender.dll' = 'studiorender.dll'
    'vphysics\build\Release\vphysics.dll' = 'vphysics.dll'
    'vgui2\src\build\Release\vgui2.dll' = 'vgui2.dll'
    'vguimatsurface\build\Release\vguimatsurface.dll' = 'vguimatsurface.dll'
    'utils\bugreporter\build\Release\bugreporter.dll' = 'bugreporter.dll'
}
foreach ($s in 'dbg','dx6','dx7','dx8','dx9','hdr_dx9') {
    $bin["materialsystem\stdshaders\build\Release\stdshader_$s.dll"] = "stdshader_$s.dll"
}
$bin['materialsystem\stdshaders\build\Release\shader_nvfx.dll'] = 'shader_nvfx.dll'
$bin['materialsystem\stdshaders\build\Release ps20\shader_nvfx.dll'] = 'shader_nvfx_ps20.dll'

$missing = @()
foreach ($k in $bin.Keys) {
    $p = Join-Path $Src $k
    if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p "$Out\bin\$($bin[$k])" -Force } else { $missing += $k }
}
Copy-Item "$Src\launcher_main\build\Release\launcher_main.exe" "$Out\hl2.exe" -Force

$game = @{
    'cl_dll\build\Release HL2\client.dll' = 'client.dll'
    'dlls\build\Release HL2\server.dll' = 'server.dll'
    'gameui\build\Release\GameUI.dll' = 'GameUI.dll'
    'Tracker\ServerBrowser\build\Release\ServerBrowser.dll' = 'ServerBrowser.dll'
}
foreach ($k in $game.Keys) { Copy-Item -LiteralPath (Join-Path $Src $k) "$Out\gamma\bin\$($game[$k])" -Force }

# Base game content is linked read-only by convention; the mod dir (gamma) receives configs/saves.
foreach ($d in 'hl2','platform') {
    if (-not (Test-Path "$Out\$d")) { New-Item -ItemType Junction -Path "$Out\$d" -Target "$Install\$d" | Out-Null }
}
# GammaSource's always-mounted content layer.
robocopy "$Src\gammasource_content" "$Out\gammasource_content" /E /NFL /NDL /NJH /NJS /XF *.ilk *.pdb launcher_main.exe launcher.dll | Out-Null
# The content layer sits ahead of every game dir; drop maps this BSP18 engine cannot load so 2003 maps are not shadowed.
Get-ChildItem "$Out\gammasource_content\maps" -Filter *.bsp | ForEach-Object {
    $fs = [IO.File]::OpenRead($_.FullName); $b = New-Object byte[] 8; $fs.Read($b, 0, 8) | Out-Null; $fs.Close()
    if ([BitConverter]::ToInt32($b, 4) -ne 18) { Remove-Item -LiteralPath $_.FullName }
}
# 2003-era game dir: fresh game DLLs plus links to the installed hl2_2003 content; cfg is copied so writes stay local.
$era = "$Install\hl2_2003"
New-Item -ItemType Directory -Force "$Out\hl2_2003\bin" | Out-Null
Copy-Item "$Out\gamma\bin\*" "$Out\hl2_2003\bin" -Force
foreach ($d in 'maps','materials','models','scenes','scripts','sound') {
    if (-not (Test-Path "$Out\hl2_2003\$d")) { New-Item -ItemType Junction -Path "$Out\hl2_2003\$d" -Target "$era\$d" | Out-Null }
}
if (-not (Test-Path "$Out\hl2_2003\cfg")) { Copy-Item "$era\cfg" "$Out\hl2_2003\cfg" -Recurse }
"Staged $Out; not yet fresh: $($missing -join ', ')"
