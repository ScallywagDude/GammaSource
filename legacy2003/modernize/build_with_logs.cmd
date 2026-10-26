@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
if /I not "%~1"=="Debug" if /I not "%~1"=="Release" (
  echo Usage: modernize\build_with_logs.cmd Debug^|Release [--continue-on-error]
  exit /b 2
)
where py >nul 2>&1
if errorlevel 1 (
  echo Python 3 launcher not found. Install Python 3 and retry.
  exit /b 2
)
py -3 modernize\build_driver.py "%~1" %2
exit /b %errorlevel%
