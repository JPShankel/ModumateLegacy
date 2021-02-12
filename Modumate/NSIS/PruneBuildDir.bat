REM Remove non-released components from build archive
REM Usage: PruneBuildDir MODUMATE_INSTALL_ROOT

setlocal
if "%1"=="" exit /B 1

set INSTALL_ROOT=%1

del %INSTALL_ROOT%\Manifest_DebugFiles_Win64.txt
del %INSTALL_ROOT%\Manifest_NonUFSFiles_Win64.txt
del %INSTALL_ROOT%\Modumate\Binaries\Win64\Modumate-Win64-Shipping.pdb
del %INSTALL_ROOT%\Engine\Binaries\Win64\CrashReportClient.pdb
del %INSTALL_ROOT%\Engine\Binaries\Win64\turbojpeg.dll
rmdir /S /Q %INSTALL_ROOT%\Engine\Content
