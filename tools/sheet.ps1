param([string]$Out, [int]$X, [int]$Y, [int]$W, [int]$H, [string[]]$Items)
# Contact sheet of game screenshots (.tga): crop the same region of each, label it, 2 per row.
Add-Type -AssemblyName System.Drawing
$sp = Split-Path $MyInvocation.MyCommand.Path
$cols = 2
$rows = [Math]::Ceiling($Items.Count / $cols)
$sheet = New-Object Drawing.Bitmap ($W * $cols), ($H * $rows)
$g = [Drawing.Graphics]::FromImage($sheet)
$font = New-Object Drawing.Font 'Arial', 11
for ($i = 0; $i -lt $Items.Count; $i++) {
    $label, $path = $Items[$i].Split('=', 2)
    $png = "$env:TEMP\sheet_$i.png"
    if ($path -like '*.tga') { py -3 -I "$sp\tga2png.py" $path $png } else { Copy-Item $path $png }
    $im = [Drawing.Bitmap]::FromFile($png)
    $dx = ($i % $cols) * $W; $dy = [Math]::Floor($i / $cols) * $H
    $g.DrawImage($im, (New-Object Drawing.Rectangle $dx, $dy, $W, $H), (New-Object Drawing.Rectangle $X, $Y, $W, $H), [Drawing.GraphicsUnit]::Pixel)
    $g.FillRectangle([Drawing.Brushes]::Black, $dx, $dy, 10 * $label.Length + 8, 18)
    $g.DrawString($label, $font, [Drawing.Brushes]::Yellow, $dx + 3, $dy + 1)
    $im.Dispose()
}
$sheet.Save($Out)
