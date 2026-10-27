# Stage a self-contained, Steam-free GammaSource build into <Dest>\<yyyy-MM-dd_HH-mm-ss>.
#
#   GammaSource FULL BUILD\   the 64-bit build (engine + gammasource game dir + every content layer it mounts)
#   GammaSource_2003\   (with -Include2003) a copy of the current 2003 build
#
# Nothing in the result refers to Steam: the retail HL2/TF2/Portal VPKs and loose files the
# GammaSource build mounts are staged under content\, and gameinfo.txt only uses |gameinfo_path| paths.
# The retail content comes from plain folders under -RetailRoot (or the GAMMA_RETAIL_ROOT environment
# variable): <RetailRoot>\Half-Life 2, <RetailRoot>\Team Fortress 2 and <RetailRoot>\Portal, each a
# copy of that game's install folder. Pass -RetailHL2 / -RetailTF2 / -RetailPortal to use other folders.
# Large content files are NTFS hard links (no extra disk space) when source and destination are
# on the same volume, otherwise copies. Binaries are always real copies.
param(
    [string]$Dest = 'C:\Users\doge6\Documents\GammaSourceTest',
    [string]$Install = 'C:\Users\doge6\Downloads\source2018_install',
    [string]$Repo = 'C:\Users\doge6\Videos\GammaSource-main',
    [string]$OriginalContent = '',
    [string]$Assets = 'C:\Users\doge6\Downloads\source2018_assets',
    [string]$Beta2003 = 'C:\Users\doge6\Documents\HL2BetaMegaCuck\hl2_2003',
    [string]$RetailRoot = $(if ($env:GAMMA_RETAIL_ROOT) { $env:GAMMA_RETAIL_ROOT } else { 'C:\GammaSource\retail' }),
    [string]$RetailHL2 = (Join-Path $RetailRoot 'Half-Life 2'),
    [string]$RetailTF2 = (Join-Path $RetailRoot 'Team Fortress 2'),
    [string]$RetailPortal = (Join-Path $RetailRoot 'Portal'),
    [string]$Build2003 = 'C:\Users\doge6\Videos\GammaSource_Build',
    [switch]$Include2003
)
$ErrorActionPreference = 'Stop'
foreach ($retailDir in @($RetailHL2, $RetailTF2, $RetailPortal)) {
    if (-not (Test-Path -LiteralPath $retailDir -PathType Container)) {
        throw "Retail content folder is missing: $retailDir. Copy that game's install folder there, or pass -RetailRoot / -RetailHL2 / -RetailTF2 / -RetailPortal."
    }
}
if (-not $OriginalContent) { $OriginalContent = Join-Path $Repo 'gammasource_content' }
if (-not (Test-Path -LiteralPath $OriginalContent -PathType Container)) {
    throw "Original beta content is missing. Pass -OriginalContent with the actual gammasource_content directory: $OriginalContent"
}
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class HL { [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern bool CreateHardLink(string newFile, string existing, IntPtr sa); }
"@

$script:linked = 0; $script:copied = 0
function Put-File([string]$src, [string]$dst, [switch]$Copy) {
    $dir = Split-Path $dst
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    if (-not $Copy -and [HL]::CreateHardLink($dst, $src, [IntPtr]::Zero)) { $script:linked++; return }
    Copy-Item -LiteralPath $src -Destination $dst -Force; $script:copied++
}
function Put-Tree([string]$src, [string]$dst, [string[]]$Exclude = @(), [switch]$Copy) {
    if (-not (Test-Path -LiteralPath $src)) { Write-Warning "missing $src"; return }
    $root = (Resolve-Path -LiteralPath $src).Path.TrimEnd('\')
    Get-ChildItem -LiteralPath $root -Recurse -File -Force | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length + 1)
        foreach ($x in $Exclude) { if ($rel -like $x) { return } }
        Put-File $_.FullName (Join-Path $dst $rel) -Copy:$Copy
    }
}

$stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$out = Join-Path $Dest $stamp
$buildRoot = Join-Path $out 'GammaSource FULL BUILD'
$content = Join-Path $buildRoot 'content'
Write-Host "staging $out"

# engine and launcher (real copies)
Put-File "$Install\hl2_launcher.exe" "$buildRoot\hl2_launcher.exe" -Copy
Put-Tree "$Install\bin" "$buildRoot\bin" -Exclude @('*.lib', '*.exp') -Copy
Put-Tree "$Install\platform" "$buildRoot\platform" -Copy
Put-File "$RetailHL2\platform\platform_misc_dir.vpk" "$buildRoot\platform\platform_misc_dir.vpk"
Get-ChildItem "$RetailHL2\platform" -Filter 'platform_misc_0*.vpk' | ForEach-Object { Put-File $_.FullName "$buildRoot\platform\$($_.Name)" }

# the game directory
$game = Join-Path $buildRoot 'gammasource'
Put-Tree "$Install\hl2\bin" "$game\bin" -Exclude @('*.lib', '*.exp') -Copy
New-Item -ItemType Directory -Force "$game\cfg" | Out-Null

# content layers, in mount order
Put-Tree "$Assets\gammashaders" "$content\gammashaders"
Put-Tree "$Assets\gmodshaders" "$content\gmodshaders"
Put-Tree "$Install\hl2\converted" "$content\converted"
Put-Tree "$Repo\gammasource_overlay" "$content\gammasource_overlay" -Copy
Put-Tree "$Assets\gamma" "$content\gamma"
Put-Tree $OriginalContent "$content\gammasource_content" -Exclude @('*.bak', '*.before-*')
Put-Tree $Beta2003 "$content\hl2_2003"
foreach ($d in 'materials', 'sound', 'models', 'maps', 'scripts', 'resource') {
    if (Test-Path "$Assets\$d") { Put-Tree "$Assets\$d" "$content\assets\$d" }
}
# retail HL2 (and TF2's hl2_misc, whose shaders match this engine), staged from the retail folders
foreach ($v in 'hl2_misc', 'hl2_textures', 'hl2_sound_misc', 'hl2_sound_vo_english', 'hl2_pak') {
    Get-ChildItem "$RetailHL2\hl2" -Filter "${v}_*.vpk" | ForEach-Object { Put-File $_.FullName "$content\retail_hl2\$($_.Name)" }
}
Get-ChildItem "$RetailTF2\hl2" -Filter 'hl2_misc_*.vpk' | ForEach-Object { Put-File $_.FullName "$content\retail_tf2\$($_.Name)" }
foreach ($d in 'resource', 'scripts', 'media', 'maps', 'cfg', 'gamepadui') {
    if (Test-Path "$RetailHL2\hl2\$d") { Put-Tree "$RetailHL2\hl2\$d" "$content\retail_hl2_loose\$d" }
}
# Portal's content, for the portal gun (models, particles, sounds)
Get-ChildItem "$RetailPortal\portal" -Filter 'portal_pak_*.vpk' | ForEach-Object { Put-File $_.FullName "$content\retail_portal\$($_.Name)" }

@"
"GameInfo"
{
	game	"GammaSource"
	type	singleplayer_only
	FileSystem
	{
		SearchPaths
		{
			game+mod+mod_write+default_write_path	|gameinfo_path|.
			gamebin	|gameinfo_path|bin
			game	|gameinfo_path|..\content\gammashaders
			game	|gameinfo_path|..\content\gmodshaders
			game	|gameinfo_path|..\content\converted
			game	|gameinfo_path|..\content\gammasource_overlay
			game	|gameinfo_path|..\content\gamma
			game	|gameinfo_path|..\content\gammasource_content
			game	|gameinfo_path|..\content\hl2_2003
			game	|gameinfo_path|..\content\assets
			game	|gameinfo_path|..\content\retail_tf2\hl2_misc.vpk
			game	|gameinfo_path|..\content\retail_hl2\hl2_misc.vpk
			game	|gameinfo_path|..\content\retail_hl2\hl2_textures.vpk
			game	|gameinfo_path|..\content\retail_hl2\hl2_sound_misc.vpk
			game	|gameinfo_path|..\content\retail_hl2\hl2_sound_vo_english.vpk
			game	|gameinfo_path|..\content\retail_hl2\hl2_pak.vpk
			game	|gameinfo_path|..\content\retail_hl2_loose
			game	|gameinfo_path|..\content\retail_portal\portal_pak.vpk
			platform	|gameinfo_path|..\platform
			platform	|gameinfo_path|..\platform\platform_misc.vpk
		}
	}
}
"@ | Set-Content -Encoding ascii "$game\gameinfo.txt"

@"
@echo off
rem GammaSource (64-bit). Needs no Steam.
cd /d "%~dp0"
start "" hl2_launcher.exe -game gammasource -multirun -dxlevel 95 -novid %*
"@ | Set-Content -Encoding ascii "$buildRoot\play.cmd"

if ($Include2003) {
    Put-Tree $Build2003 "$out\GammaSource_2003" -Exclude @('t_*\*', '*.log', 'demoheader.tmp')
}

Write-Host "done: $out ($script:linked hard links, $script:copied copies)"
$out
