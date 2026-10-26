@echo off
setlocal EnableExtensions
if "%~1"=="" (
  echo Usage: IMPORT_MEGABUILD_AND_INSTALL.cmd ^<physical-engine-root^> [Mega-Build-path]
  echo Example: IMPORT_MEGABUILD_AND_INSTALL.cmd "C:\Games\GammaSource" "%USERPROFILE%\Documents\HL2BetaMegaCuck"
  exit /b 2
)
set "MEGAROOT=%~2"
if "%MEGAROOT%"=="" set "MEGAROOT=%USERPROFILE%\Documents\HL2BetaMegaCuck"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_GAMMASOURCE_CONTENT.ps1" -GameRoot "%~1" -MegaBuildRoot "%MEGAROOT%"
exit /b %errorlevel%
