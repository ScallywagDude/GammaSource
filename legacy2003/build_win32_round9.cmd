@echo off
setlocal EnableExtensions
rem Include the installed Beta Mega Build content in GammaSource's default GAME layer.
rem This one-time step never imports foreign engine DLLs or edits the base game.
if not exist "%~dp0gammasource_content\MEGA_BUILD_ASSET_MANIFEST.json" (
  if exist "%USERPROFILE%\Documents\HL2BetaMegaCuck\hl2" (
    where py >nul 2>&1
    if not errorlevel 1 (
      py -3 "%~dp0modernize\integrate_beta_megabuild.py" --mega-root "%USERPROFILE%\Documents\HL2BetaMegaCuck" --destination "%~dp0gammasource_content"
      if errorlevel 1 exit /b 2
    ) else (
      python "%~dp0modernize\integrate_beta_megabuild.py" --mega-root "%USERPROFILE%\Documents\HL2BetaMegaCuck" --destination "%~dp0gammasource_content"
      if errorlevel 1 exit /b 2
    )
  )
)
where py >nul 2>&1
if not errorlevel 1 goto run_py
where python >nul 2>&1
if not errorlevel 1 goto run_python
echo Python 3 is required to run the Win32 build helper.
exit /b 2
:run_py
py -3 "%~dp0modernize\build_win32_round9.py" %*
exit /b %errorlevel%
:run_python
python "%~dp0modernize\build_win32_round9.py" %*
exit /b %errorlevel%
