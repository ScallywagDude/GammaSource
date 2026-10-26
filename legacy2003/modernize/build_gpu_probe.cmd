@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where cl >nul 2>&1
if errorlevel 1 (
  echo MSVC cl.exe not found. Use the Visual Studio 2026 Native Tools prompt.
  exit /b 2
)
cl /nologo /W4 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /Fe:gpu_api_probe.exe gpu_api_probe.cpp
if errorlevel 1 exit /b 1
gpu_api_probe.exe
exit /b %errorlevel%
