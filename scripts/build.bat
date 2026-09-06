@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer was not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VBS_VS=%%i"
if not defined VBS_VS (
  echo Visual Studio with C++ tools was not found.
  exit /b 1
)

set "VBS_ARCH=x64"
set "VBS_VCVARS=vcvars64.bat"
if /i "%~1"=="x86" (
  set "VBS_ARCH=x86"
  set "VBS_VCVARS=vcvars32.bat"
)
call "%VBS_VS%\VC\Auxiliary\Build\%VBS_VCVARS%"
if errorlevel 1 exit /b %errorlevel%

set "VBS_CMAKE=%VBS_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "VBS_NINJA=%VBS_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

"%VBS_CMAKE%" -S "%~dp0.." -B "%~dp0..\build-ninja-%VBS_ARCH%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%VBS_NINJA%"
if errorlevel 1 exit /b %errorlevel%
"%VBS_CMAKE%" --build "%~dp0..\build-ninja-%VBS_ARCH%"
if errorlevel 1 exit /b %errorlevel%
"%VBS_CMAKE%" --build "%~dp0..\build-ninja-%VBS_ARCH%" --target test
exit /b %errorlevel%
