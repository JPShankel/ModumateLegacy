@echo off
pushd %~dp0
echo:
echo Building Modumate...
call "%~dp0..\UE4\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="%~dp0Modumate.uproject" BuildCookRun -nop4 -project="%~dp0Modumate.uproject" -ue4exe="%~dp0..\UE4\Engine\Binaries\Win64\UE4Editor-Cmd.exe" -ddc=DerivedDataBackendGraph -targetplatform=Win64 -build -target=Modumate -serverconfig=Development -utf8output -compile
pushd "%~dp0..\UE4"
git restore Engine/Build/Build.version
popd
popd
