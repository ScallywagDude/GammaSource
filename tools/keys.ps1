param([string]$Keys)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;using System.Runtime.InteropServices;
public class F{[DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);}
"@
$p = Get-Process hl2 | Select-Object -First 1
[F]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 400
[System.Windows.Forms.SendKeys]::SendWait($Keys)
