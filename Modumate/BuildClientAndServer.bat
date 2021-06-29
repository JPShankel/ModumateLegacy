@echo off
pushd %~dp0
echo Building Modumate...
call "%~dp0\..\UE4\Engine\Build\BatchFiles\Build.bat" Modumate Win64 Development "%~dp0\Modumate.uproject" -WaitMutex -FromMsBuild
echo:
echo Building ModumateServer...
call "%~dp0\..\UE4\Engine\Build\BatchFiles\Build.bat" ModumateServer Win64 Development "%~dp0\Modumate.uproject" -WaitMutex -FromMsBuild
popd
