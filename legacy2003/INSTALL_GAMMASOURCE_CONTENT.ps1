# Installs the engine's built-in beta HUD, validated viewmodels and imported
# Mega Build content into the *physical* GammaSource installation directory.
# The original hl2 game folder and its junction targets are never modified.
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$MegaBuildRoot = ''
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $sourceRoot 'gammasource_content'
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Bundled content directory not found: $source" }
$physicalGameRoot = [IO.Path]::GetFullPath($GameRoot)
if (-not (Test-Path -LiteralPath $physicalGameRoot -PathType Container)) { throw "Engine root directory not found: $GameRoot" }
$target = Join-Path $physicalGameRoot 'gammasource_content'
if (Test-Path -LiteralPath $target) {
    $item = Get-Item -LiteralPath $target -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to modify a junction/symlink content destination: $target"
    }
}
if ($MegaBuildRoot) {
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        & py -3 (Join-Path $sourceRoot 'modernize\integrate_beta_megabuild.py') --mega-root $MegaBuildRoot --destination $source
    } else {
        & python (Join-Path $sourceRoot 'modernize\integrate_beta_megabuild.py') --mega-root $MegaBuildRoot --destination $source
    }
    if ($LASTEXITCODE -ne 0) { throw 'Mega Build asset integration failed; original engine/game content was not changed.' }
}
$files = @(Get-ChildItem -LiteralPath $source -File -Recurse)
foreach ($item in $files) {
    $relative = $item.FullName.Substring($source.Length).TrimStart('\','/')
    $destFile = Join-Path $target $relative
    $dir = Split-Path -Parent $destFile
    # Protect against traversing existing symlinks/junctions in the destination.
    $part = $dir
    while ($part -and $part.StartsWith($physicalGameRoot,[StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $part) {
            $obj = Get-Item -LiteralPath $part -Force
            if (($obj.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to copy through a junction: $part"
            }
        }
        if ($part -eq $physicalGameRoot) { break }
        $part = Split-Path -Parent $part
    }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    if (Test-Path -LiteralPath $destFile -PathType Leaf) {
        $oldHash = (Get-FileHash -LiteralPath $destFile -Algorithm SHA256).Hash
        $newHash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        if ($oldHash -eq $newHash) { continue }
        $backup = $destFile + '.before-gammasource.bak'
        if (-not (Test-Path -LiteralPath $backup)) { Copy-Item -LiteralPath $destFile -Destination $backup }
    }
    Copy-Item -LiteralPath $item.FullName -Destination $destFile -Force
}
Write-Host "Installed $($files.Count) bundled and merged files into $target"
Write-Host 'GammaSource automatically mounts this content directory at startup; no -gammasourcecontent launch flag is needed.'
