# Copy freshly built 2018 binaries over the installed ones (waf install sometimes skips them).
$b = 'C:\Users\doge6\Downloads\source-engine-master\source-engine-master\build'
$i = 'C:\Users\doge6\Downloads\source2018_install'
Get-Process hl2_launcher -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$n = 0
foreach ($dst in Get-ChildItem $i -Recurse -Include *.dll, *.exe -File) {
    $src = Get-ChildItem $b -Recurse -Filter $dst.Name -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($src -and $src.LastWriteTime -gt $dst.LastWriteTime) { Copy-Item $src.FullName $dst.FullName -Force; $n++ }
}
"updated $n binaries"
