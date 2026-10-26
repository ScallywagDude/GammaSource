@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
where py >nul 2>&1
if errorlevel 1 (
  echo Python 3 launcher not found. Install Python 3 and retry.
  exit /b 2
)
py -3 modernize\apply_modernization.py
if errorlevel 1 exit /b 1
if "%~1"=="" (
  echo Structural validation finished. To attempt the Windows build:
  echo   modernize\build_windows.cmd Debug
  echo   modernize\build_windows.cmd Release
  exit /b 0
)
rem Fail once for a missing Perl prerequisite, not hundreds of times per shader.
py -3 modernize\run_shader_tool.py --probe
if errorlevel 1 (
  echo Legacy shader generation requires Perl 5. Set HL2_MODERN_PERL or add perl.exe to PATH.
  exit /b 2
)
call build_hl2_vs2026.cmd "%~1"
exit /b %errorlevel%
