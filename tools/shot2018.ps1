param([string]$Game, [string]$Extra, [string]$Out, [int]$Wait = 40, [int]$RestoreAt = 0, [string]$Install = 'C:\Users\doge6\Downloads\source2018_install')
# Launch the 2018 test engine minimized, show it without taking focus, and capture its window.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class SH{[DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr h,int c);
[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);
[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out R r);public struct R{public int L,T,Ri,B;}}
"@
$i = $Install
Remove-Item "$i\console.log", "$i\$Game\console.log" -ErrorAction SilentlyContinue
$p = Start-Process "$i\hl2_launcher.exe" -ArgumentList "-game $Game -window -w 1024 -h 640 -novid -dev $Extra" -WorkingDirectory $i -PassThru -WindowStyle Minimized
if ($RestoreAt) { Start-Sleep $RestoreAt; $p.Refresh(); [SH]::ShowWindow($p.MainWindowHandle, 4) | Out-Null; Start-Sleep ($Wait - $RestoreAt) } else { Start-Sleep $Wait }
if ($p.HasExited) { "exited $($p.ExitCode)"; return }
$p.Refresh(); $h = $p.MainWindowHandle
[SH]::ShowWindow($h, 4) | Out-Null   # SW_SHOWNOACTIVATE
Start-Sleep 6
$r = New-Object SH+R; [SH]::GetWindowRect($h, [ref]$r) | Out-Null
$bmp = New-Object Drawing.Bitmap ($r.Ri - $r.L), ($r.B - $r.T)
$g = [Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[SH]::PrintWindow($h, $dc, 2) | Out-Null; $g.ReleaseHdc($dc); $bmp.Save($Out)
Stop-Process -Id $p.Id -Force
'saved'




