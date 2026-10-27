# Selector icons for the weapons added in the 2018 build, drawn in the style of the HL1-like
# selector's atlases (white outline art on black, 128x64 cells in a 256x256 texture).
# The silhouettes come from the retail HL2 icon font and Portal-Base's portal gun font; each
# glyph is rendered, then reduced to its outline.
#
#   make_icons.ps1 -Out <content dir> [-Preview <png>]
param([string]$Out, [string]$Preview,
      [string]$HL2Font = (Join-Path $(if ($env:GAMMA_RETAIL_ROOT) { $env:GAMMA_RETAIL_ROOT } else { 'C:\GammaSource\retail' }) 'Half-Life 2\hl2\resource\halflife2.ttf'),
      [string]$PortalFont = 'C:\Users\doge6\Downloads\source2018_assets\portalbase_res\RealBeta''s Weapon Icons.ttf')
Add-Type -AssemblyName System.Drawing
$here = Split-Path $MyInvocation.MyCommand.Path

# cell (x, y) -> font, character, label for the preview
$cells = @(
    @{ X = 0;   Y = 0;  Font = $HL2Font;    Char = 'e'; Name = 'weapon_357' },
    @{ X = 128; Y = 0;  Font = $HL2Font;    Char = 'l'; Name = 'weapon_ar2future' },
    @{ X = 0;   Y = 64; Font = $HL2Font;    Char = 'e'; Name = 'weapon_proto1'; Mirror = $true },
    @{ X = 128; Y = 64; Font = $PortalFont; Char = 'l'; Name = 'weapon_portalgun'; Lines = $true }	# already line art
)

$atlas = New-Object Drawing.Bitmap 256, 256
$ga = [Drawing.Graphics]::FromImage($atlas)
$ga.Clear([Drawing.Color]::Black)
foreach ($c in $cells) {
    $pfc = New-Object Drawing.Text.PrivateFontCollection
    $pfc.AddFontFile($c.Font)
    $font = New-Object Drawing.Font $pfc.Families[0], 64, ([Drawing.GraphicsUnit]::Pixel)
    # render the filled glyph large, then fit it into the cell
    $big = New-Object Drawing.Bitmap 512, 256
    $g = [Drawing.Graphics]::FromImage($big)
    $g.Clear([Drawing.Color]::Black)
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $font = New-Object Drawing.Font $pfc.Families[0], 200, ([Drawing.GraphicsUnit]::Pixel)
    $g.DrawString($c.Char, $font, [Drawing.Brushes]::White, 10, 0)
    # bounding box of the glyph
    $minX = 512; $minY = 256; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt 256; $y++) { for ($x = 0; $x -lt 512; $x++) {
        if ($big.GetPixel($x, $y).R -gt 100) { if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }; if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y } } } }
    $bw = $maxX - $minX + 1; $bh = $maxY - $minY + 1
    $scale = [Math]::Min(116.0 / $bw, 52.0 / $bh)
    $cw = [int]($bw * $scale); $ch = [int]($bh * $scale)
    $cell = New-Object Drawing.Bitmap 128, 64
    $gc = [Drawing.Graphics]::FromImage($cell)
    $gc.Clear([Drawing.Color]::Black)
    $gc.InterpolationMode = 'HighQualityBicubic'
    $dst = New-Object Drawing.Rectangle ([int]((128 - $cw) / 2)), ([int]((64 - $ch) / 2)), $cw, $ch
    $gc.DrawImage($big, $dst, (New-Object Drawing.Rectangle $minX, $minY, $bw, $bh), [Drawing.GraphicsUnit]::Pixel)
    if ($c.Mirror) { $cell.RotateFlip([Drawing.RotateFlipType]::RotateNoneFlipX) }
    # outline: a filled pixel with an empty 4-neighbour
    for ($y = 0; $y -lt 64; $y++) { for ($x = 0; $x -lt 128; $x++) {
        $on = $cell.GetPixel($x, $y).R -gt 110
        if ($c.Lines) { $v = [Math]::Min(255, [int]($cell.GetPixel($x, $y).R * 1.6)); $atlas.SetPixel($c.X + $x, $c.Y + $y, [Drawing.Color]::FromArgb($v, $v, $v)); continue }
        $edge = $false
        if ($on) {
            foreach ($d in @(@(1, 0), @(-1, 0), @(0, 1), @(0, -1))) {
                $nx = $x + $d[0]; $ny = $y + $d[1]
                if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge 128 -or $ny -ge 64 -or $cell.GetPixel($nx, $ny).R -le 110) { $edge = $true }
            }
        }
        $v = if ($edge) { 255 } else { 0 }
        $atlas.SetPixel($c.X + $x, $c.Y + $y, [Drawing.Color]::FromArgb($v, $v, $v))
    } }
}
if ($Preview) { $atlas.Save($Preview) }
if ($Out) {
    # raw RGB for the VTF writer
    $raw = New-Object byte[] (256 * 256 * 3)
    for ($y = 0; $y -lt 256; $y++) { for ($x = 0; $x -lt 256; $x++) {
        $p = $atlas.GetPixel($x, $y); $i = ($y * 256 + $x) * 3; $raw[$i] = $p.R; $raw[$i + 1] = $p.G; $raw[$i + 2] = $p.B } }
    $rawPath = Join-Path $env:TEMP 'w_icons5b.rgb'
    [IO.File]::WriteAllBytes($rawPath, $raw)
    $mat = Join-Path $Out 'materials\sprites'
    New-Item -ItemType Directory -Force $mat | Out-Null
    py -3 -I (Join-Path $here 'rgb2vtf.py') $rawPath 256 256 (Join-Path $mat 'w_icons5b.vtf')
    Copy-Item (Join-Path $Out '..\gammasource_content\materials\sprites\w_icons4b.vmt') (Join-Path $mat 'w_icons5b.vmt') -ErrorAction SilentlyContinue
    (Get-Content (Join-Path $mat 'w_icons5b.vmt') -Raw) -replace 'w_icons4b', 'w_icons5b' | Set-Content -Encoding ascii (Join-Path $mat 'w_icons5b.vmt')
}
