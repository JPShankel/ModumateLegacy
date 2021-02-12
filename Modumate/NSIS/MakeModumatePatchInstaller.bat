REM Create a delta (patch) installer from PREVIOUS_VERSION and NEW_VERSION,
REM installed under BUILDS_ROOT.
REM Uses nsisPatchGen (http://sourceforge.net/projects/nsispatchgen).
REM Usage: MakeModumatePatchInstaller BUILDS_ROOT PREVIOUS_VERSION NEW_VERSION

setlocal
if "%3"=="" exit /B 1
set BUILDS_ROOT=%1%
set PREVIOUS_VERSION=%2%
set NEW_VERSION=%3%
set NSIS_DIR=C:\Program Files (x86)\NSIS
set INSTALLER_DIR=%BUILDS_ROOT%\Patch\Installers

set PREVIOUS_BUILD=%BUILDS_ROOT%\Release\%PREVIOUS_VERSION%\WindowsNoEditor
set NEW_BUILD=%BUILDS_ROOT%\Release\%NEW_VERSION%\WindowsNoEditor
set PATCH_DIR=%BUILDS_ROOT%\Patch\patchFiles
rmdir /S /Q %PATCH_DIR%

REM For GenPat.exe:
set PATH=%PATH%;%NSIS_DIR%\Bin
Modumate\NSIS\nsisPatchGen\nsisPatGen.exe %PREVIOUS_BUILD% %NEW_BUILD% %PATCH_DIR% %BUILDS_ROOT%\Patch\patchFiles.nsi

"%NSIS_DIR%\Bin\makensis.exe" /DPRODUCT_VERSION="%NEW_VERSION%" /DPREVIOUS_VERSION="%PREVIOUS_VERSION%" /DPATH_OUT_ROOT="%INSTALLER_DIR%\%PREVIOUS_VERSION%" /DBUILD_DIR="%BUILDS_ROOT%\Patch" Modumate\NSIS\ModumateSetupPatch.nsi
