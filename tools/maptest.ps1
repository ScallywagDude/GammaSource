param([string[]]$Maps, [int]$Seconds = 40)
$r = 'C:\Users\doge6\Videos\GammaRuntime'
$log = "$r\gammasource_content\console.log"
foreach ($m in $Maps) {
    Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep 1
    Remove-Item $log -ErrorAction SilentlyContinue
    $t0 = Get-Date
    $p = Start-Process "$r\hl2.exe" -ArgumentList "-game hl2_2003 -window -width 800 -height 600 -dev -condebug +map $m" -WorkingDirectory $r -PassThru
    Start-Sleep $Seconds
    $alive = -not $p.HasExited
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    $spawned = $text -match "Spawn Server $m"
    $signon = $text -match 'Signon network traffic'
    $err = ([regex]::Matches($text, '(Host_Error[^\r\n]*|map load failed[^\r\n]*|bsp version[^\r\n]*)') | ForEach-Object Value) -join ' | '
    $crash = Get-WinEvent -FilterHashtable @{LogName='Application';Id=1000;StartTime=$t0} -ErrorAction SilentlyContinue |
        Where-Object Message -match 'hl2\.exe' | Select-Object -First 1
    $where = ''
    if ($crash) { $where = (($crash.Message -split "`n") | Where-Object { $_ -match 'Faulting module name|Fault offset' }) -join ' ' }
    [pscustomobject]@{ map=$m; alive=$alive; spawned=$spawned; inGame=$signon; error=$err; crash=$where.Trim() }
}
Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
