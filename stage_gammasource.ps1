# Stage the unified GammaSource root without any sibling checkout.
# This wrapper leaves legacy2003 and all user game processes untouched.
[CmdletBinding()]
param(
    [string]$Dest = (Join-Path $env:USERPROFILE 'Documents\GammaSourceBuilds'),
    [Parameter(Mandatory=$true)][string]$Install,
    [Parameter(Mandatory=$true)][string]$Assets,
    [Parameter(Mandatory=$true)][string]$Beta2003,
    [Parameter(Mandatory=$true)][string]$SteamHL2,
    [Parameter(Mandatory=$true)][string]$SteamTF2,
    [Parameter(Mandatory=$true)][string]$SteamPortal,
    [string]$Build2003 = '',
    [switch]$Include2003
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$scriptFile = Join-Path $root 'legacy2003\modernize\port2018\make_build.ps1'
if (-not (Test-Path -LiteralPath (Join-Path $root 'wscript') -PathType Leaf)) {
    throw 'The script must be run from the unified GammaSource source archive.'
}
foreach ($required in @($Install, $Assets, $Beta2003, $SteamHL2, $SteamTF2, $SteamPortal)) {
    if (-not (Test-Path -LiteralPath $required -PathType Container)) {
        throw "Missing required content/install directory: $required"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $Install 'hl2_launcher.exe') -PathType Leaf)) {
    throw "Build installation lacks hl2_launcher.exe: $Install"
}
$arguments = @{
    Dest = $Dest; Install = $Install; Repo = $root; Assets = $Assets;
    Beta2003 = $Beta2003; SteamHL2 = $SteamHL2;
    SteamTF2 = $SteamTF2; SteamPortal = $SteamPortal
}
if ($Include2003) {
    if (-not $Build2003 -or -not (Test-Path -LiteralPath $Build2003 -PathType Container)) {
        throw 'Provide -Build2003 with an existing directory when using -Include2003.'
    }
    $arguments.Include2003 = $true
    $arguments.Build2003 = $Build2003
}
& $scriptFile @arguments
if (-not $?) { throw 'GammaSource staging failed.' }
