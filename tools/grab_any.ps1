param([int]$ProcId, [string]$Out)
# Capture a process's "Error!" dialog by window rect, without focusing it.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Text;using System.Runtime.InteropServices;
public class GD{public delegate bool CB(IntPtr h,IntPtr l);[DllImport("user32.dll")]public static extern bool EnumWindows(CB cb,IntPtr l);
[DllImport("user32.dll")]public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
[DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out R r);
[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);
public struct R{public int L,T,Ri,B;}}
"@
$script:hit = [IntPtr]::Zero
[GD]::EnumWindows({ param($h, $l)
    $p = 0; [GD]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $ProcId) { $sb = New-Object Text.StringBuilder 256; [GD]::GetWindowText($h, $sb, 256) | Out-Null
        if ($sb.ToString() -match 'Error|rror|Warn|Fatal|Source|Half') { $script:hit = $h } }
    $true }, [IntPtr]::Zero) | Out-Null
if ($script:hit -eq [IntPtr]::Zero) { 'no dialog'; return }
$r = New-Object GD+R; [GD]::GetWindowRect($script:hit, [ref]$r) | Out-Null
$bmp = New-Object Drawing.Bitmap ($r.Ri - $r.L), ($r.B - $r.T)
$g = [Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[GD]::PrintWindow($script:hit, $dc, 2) | Out-Null; $g.ReleaseHdc($dc)
$bmp.Save($Out); 'saved'


