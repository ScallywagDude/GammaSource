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
call build_hl2_vs2026.cmd "%~1"
exit /b %errorlevel%
