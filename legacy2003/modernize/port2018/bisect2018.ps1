param([string[]]$Layers, [string]$Name, [string]$Extra = '', [string]$Style = 'Minimized')
# Boot the 2018 engine with a chosen set of content layers (before the retail VPKs) and report how it ends.
$i = 'C:\Users\doge6\Downloads\source2018_install'
$retail = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life 2'
$dir = "$i\$Name"
New-Item -ItemType Directory -Force $dir | Out-Null
$lines = @("game+mod+mod_write+default_write_path`t|gameinfo_path|.", "gamebin`t|gameinfo_path|..\hl2\bin", "game`t`"C:\Users\doge6\Downloads\source2018_assets\gammashaders`"", "game`t`"C:\Users\doge6\Downloads\source2018_assets\gmodshaders`"", "game`t|gameinfo_path|..\hl2\converted")
$lines += "game`t`"C:\Users\doge6\Videos\GammaSource-main\gammasource_content_2018`""
foreach ($l in $Layers) { $lines += "game`t`"$l`"" }
$lines += "game`t`"C:\Program Files (x86)\Steam\steamapps\common\Team Fortress 2\hl2\hl2_misc.vpk`""
foreach ($v in 'hl2_misc','hl2_textures','hl2_sound_misc','hl2_sound_vo_english','hl2_pak') { $lines += "game`t`"$retail\hl2\$v.vpk`"" }
$lines += "game`t`"$retail\hl2`""
$lines += "platform`t|all_source_engine_paths|platform"
$lines += "platform`t`"$retail\platform\platform_misc.vpk`""
$gi = "`"GameInfo`"`r`n{`r`n`tgame`t`"GammaSource 2018 test`"`r`n`ttype`tsingleplayer_only`r`n`tFileSystem`r`n`t{`r`n`t`tSteamAppId`t220`r`n`t`tSearchPaths`r`n`t`t{`r`n" + (($lines | ForEach-Object { "`t`t`t$_" }) -join "`r`n") + "`r`n`t`t}`r`n`t}`r`n}`r`n"
Set-Content -Path "$dir\gameinfo.txt" -Value $gi -Encoding ascii
Remove-Item "$i\console.log" -ErrorAction SilentlyContinue
$p = Start-Process "$i\hl2_launcher.exe" -ArgumentList "-game $Name -window -w 1024 -h 640 -novid -dev -condebug -dxlevel 95 $Extra" -WorkingDirectory $i -PassThru -WindowStyle $Style
$p.WaitForExit(45000) | Out-Null
if ($p.HasExited) { "[$Name] exited code $($p.ExitCode)" } else { "[$Name] still running after 45s"; Stop-Process -Id $p.Id -Force }
if (Test-Path "$i\console.log") { Get-Content "$i\console.log" -Tail 4 }



