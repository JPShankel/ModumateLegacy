@echo off
pushd %~dp0
echo:
echo Building ModumateEditor...
call "%~dp0\..\UE4\Engine\Build\BatchFiles\Build.bat" ModumateEditor Win64 Development "%~dp0\Modumate.uproject" -WaitMutex -FromMsBuild
echo:
echo Cooking content...
call "%~dp0\..\UE4\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%~dp0\Modumate.uproject" -run=cook -targetplatform=WindowsNoEditor -iterate
echo:
echo Building Modumate...
call "%~dp0\..\UE4\Engine\Build\BatchFiles\Build.bat" Modumate Win64 Development "%~dp0\Modumate.uproject" -WaitMutex -FromMsBuild
popd
