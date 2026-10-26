# Stage a self-contained, Steam-free GammaSource build into <Dest>\<yyyy-MM-dd_HH-mm-ss>.
#
#   GammaSource_2018\   the 64-bit build (engine + gammasource game dir + every content layer it mounts)
#   GammaSource_2003\   (with -Include2003) a copy of the current 2003 build
#
# Nothing in the result points at the Steam folder: the retail HL2/TF2 VPKs and loose files the
# 2018 build mounts are staged under content\, and gameinfo.txt only uses |gameinfo_path| paths.
# Large content files are NTFS hard links (no extra disk space) when source and destination are
# on the same volume, otherwise copies. Binaries are always real copies.
param(
    [string]$Dest = 'C:\Users\doge6\Documents\GammaSourceTest',
    [string]$Install = 'C:\Users\doge6\Downloads\source2018_install',
    [string]$Repo = 'C:\Users\doge6\Videos\GammaSource-main',
    [string]$Assets = 'C:\Users\doge6\Downloads\source2018_assets',
    [string]$Beta2003 = 'C:\Users\doge6\Documents\HL2BetaMegaCuck\hl2_2003',
    [string]$SteamHL2 = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life 2',
    [string]$SteamTF2 = 'C:\Program Files (x86)\Steam\steamapps\common\Team Fortress 2',
    [string]$SteamPortal = 'C:\Program Files (x86)\Steam\steamapps\common\Portal',
    [string]$Build2003 = 'C:\Users\doge6\Videos\GammaSource_Build',
    [switch]$Include2003
)
$ErrorActionPreference = 'Stop'
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
$g18 = Join-Path $out 'GammaSource_2018'
$content = Join-Path $g18 'content'
Write-Host "staging $out"

# engine and launcher (real copies)
Put-File "$Install\hl2_launcher.exe" "$g18\hl2_launcher.exe" -Copy
Put-Tree "$Install\bin" "$g18\bin" -Exclude @('*.lib', '*.exp') -Copy
Put-Tree "$Install\platform" "$g18\platform" -Copy
Put-File "$SteamHL2\platform\platform_misc_dir.vpk" "$g18\platform\platform_misc_dir.vpk"
Get-ChildItem "$SteamHL2\platform" -Filter 'platform_misc_0*.vpk' | ForEach-Object { Put-File $_.FullName "$g18\platform\$($_.Name)" }

# the game directory
$game = Join-Path $g18 'gammasource'
Put-Tree "$Install\hl2\bin" "$game\bin" -Exclude @('*.lib', '*.exp') -Copy
New-Item -ItemType Directory -Force "$game\cfg" | Out-Null

# content layers, in mount order
Put-Tree "$Assets\gammashaders" "$content\gammashaders"
Put-Tree "$Assets\gmodshaders" "$content\gmodshaders"
Put-Tree "$Install\hl2\converted" "$content\converted"
Put-Tree "$Repo\gammasource_content_2018" "$content\gammasource_content_2018" -Copy
Put-Tree "$Assets\gamma" "$content\gamma"
Put-Tree "$Repo\gammasource_content" "$content\gammasource_content" -Exclude @('*.bak', '*.before-*')
Put-Tree $Beta2003 "$content\hl2_2003"
foreach ($d in 'materials', 'sound', 'models', 'maps', 'scripts', 'resource') {
    if (Test-Path "$Assets\$d") { Put-Tree "$Assets\$d" "$content\assets\$d" }
}
# retail HL2 (and TF2's hl2_misc, whose shaders match this engine) that used to come from Steam
foreach ($v in 'hl2_misc', 'hl2_textures', 'hl2_sound_misc', 'hl2_sound_vo_english', 'hl2_pak') {
    Get-ChildItem "$SteamHL2\hl2" -Filter "${v}_*.vpk" | ForEach-Object { Put-File $_.FullName "$content\retail_hl2\$($_.Name)" }
}
Get-ChildItem "$SteamTF2\hl2" -Filter 'hl2_misc_*.vpk' | ForEach-Object { Put-File $_.FullName "$content\retail_tf2\$($_.Name)" }
foreach ($d in 'resource', 'scripts', 'media', 'maps', 'cfg', 'gamepadui') {
    if (Test-Path "$SteamHL2\hl2\$d") { Put-Tree "$SteamHL2\hl2\$d" "$content\retail_hl2_loose\$d" }
}
# Portal's content, for the portal gun (models, particles, sounds)
Get-ChildItem "$SteamPortal\portal" -Filter 'portal_pak_*.vpk' | ForEach-Object { Put-File $_.FullName "$content\retail_portal\$($_.Name)" }

@"
"GameInfo"
{
	game	"GammaSource"
	type	singleplayer_only
	FileSystem
	{
		SteamAppId	220
		SearchPaths
		{
			game+mod+mod_write+default_write_path	|gameinfo_path|.
			gamebin	|gameinfo_path|bin
			game	|gameinfo_path|..\content\gammashaders
			game	|gameinfo_path|..\content\gmodshaders
			game	|gameinfo_path|..\content\converted
			game	|gameinfo_path|..\content\gammasource_content_2018
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
rem GammaSource (64-bit, 2018 engine). Needs no Steam.
cd /d "%~dp0"
start "" hl2_launcher.exe -game gammasource -multirun -dxlevel 95 -novid %*
"@ | Set-Content -Encoding ascii "$g18\play.cmd"

if ($Include2003) {
    Put-Tree $Build2003 "$out\GammaSource_2003" -Exclude @('t_*\*', '*.log', 'demoheader.tmp')
}

Write-Host "done: $out ($script:linked hard links, $script:copied copies)"
$out
