@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VB_VS=%%i"
if not defined VB_VS (
  echo Visual Studio 2022 with Desktop development with C++ was not found.
  exit /b 1
)
set "VB_CMAKE=%VB_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%VB_CMAKE%" --preset x64-release
if errorlevel 1 exit /b %errorlevel%
"%VB_CMAKE%" --build --preset x64-release
if errorlevel 1 exit /b %errorlevel%
"%VB_CMAKE%" --preset win32-release
if errorlevel 1 exit /b %errorlevel%
"%VB_CMAKE%" --build --preset win32-release
if errorlevel 1 exit /b %errorlevel%
"%VB_CMAKE%" --build build/x64-release --config Release --target RUN_TESTS
if errorlevel 1 exit /b %errorlevel%
"%VB_CMAKE%" --build build/win32-release --config Release --target RUN_TESTS
exit /b %errorlevel%
