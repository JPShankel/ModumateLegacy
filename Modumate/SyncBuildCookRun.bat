@echo off
cd %~dp0
call "%~dp0\SyncBuildCook.bat"
echo Building Modumate...
call "%~dp0\..\UE4\Engine\Build\BatchFiles\Build.bat" Modumate Win64 Development "%~dp0\Modumate.uproject" -WaitMutex -FromMsBuild
echo:
echo Launching Modumate...
"%~dp0\Binaries\Win64\Modumate.exe"
