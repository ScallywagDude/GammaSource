@echo off
setlocal
pushd "%~dp0" || exit /b 1
where py >nul 2>nul
if errorlevel 1 (
    echo [GammaSource] Python 3 launcher "py" was not found.
    popd
    exit /b 1
)
if /I "%~1"=="configure" (
    echo [GammaSource] Configuring 64-bit Release build...
    py -3 waf configure -T release
) else (
    echo [GammaSource] Building active x64 engine and game...
    py -3 waf build -j16 -k %*
)
set "BUILD_RC=%ERRORLEVEL%"
if not "%BUILD_RC%"=="0" echo [GammaSource] Build failed with exit code %BUILD_RC%.
popd
exit /b %BUILD_RC%
