@echo off
setlocal
if "%~2"=="" (
  echo Usage: verify-exports.bat path-to-x86-dll path-to-x64-dll
  exit /b 2
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VB_VS=%%i"
call "%VB_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
for %%f in ("%~1" "%~2") do (
  echo Verifying %%~ff
  dumpbin /headers "%%~ff" | findstr /c:"machine (x86)" /c:"machine (x64)" >nul || exit /b 1
  dumpbin /exports "%%~ff" | findstr /r /c:"[ ]CreateBonDriver$" >nul || exit /b 1
)
echo CreateBonDriver exports and PE machine types verified.
