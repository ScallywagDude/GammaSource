param([string]$Out)
# Captures the game window without bringing it to the front (no input, no focus change).
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class PW{[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);
[DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr h,out R r);public struct R{public int L,T,Ri,B;}}
"@
$p = Get-Process hl2 | Select-Object -First 1
$r = New-Object PW+R
[PW]::GetClientRect($p.MainWindowHandle, [ref]$r) | Out-Null
$bmp = New-Object Drawing.Bitmap ($r.Ri - $r.L + 40), ($r.B - $r.T + 60)
$g = [Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[PW]::PrintWindow($p.MainWindowHandle, $dc, 2) | Out-Null
$g.ReleaseHdc($dc)
$bmp.Save($Out)
