@echo off
rem Builds the plugin. Usage: tools\build.cmd [preset]   (default: "dev" if CMakeUserPresets.json exists, else "release")
setlocal
set "PF86=%ProgramFiles(x86)%"
if not defined PF86 set "PF86=C:\Program Files (x86)"
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo MSVC x64 toolset not found & exit /b 1)
if not defined VCPKG_ROOT (echo VCPKG_ROOT is not set & exit /b 1)
rem vcvars64.bat overwrites VCPKG_ROOT with the (older) vcpkg bundled into Build Tools; keep ours.
set "K7_VCPKG_ROOT=%VCPKG_ROOT%"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "VCPKG_ROOT=%K7_VCPKG_ROOT%"
cd /d "%~dp0.."
set "PRESET=%~1"
if not defined PRESET (
  if exist CMakeUserPresets.json (set "PRESET=dev") else (set "PRESET=release")
)
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
