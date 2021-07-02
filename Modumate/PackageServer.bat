@echo off
pushd %~dp0
echo:
echo Packaging ModumateServer...
call "%~dp0..\UE4\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="%~dp0Modumate.uproject" BuildCookRun -nop4 -project="%~dp0Modumate.uproject" -cook -iterate -stage -archive -archivedirectory="%~dp0..\Builds" -package -ue4exe="%~dp0..\UE4\Engine\Binaries\Win64\UE4Editor-Cmd.exe" -compressed -ddc=DerivedDataBackendGraph -pak -prereqs -distribution -manifests -targetplatform=Win64 -build -target=ModumateServer -serverconfig=Development -utf8output -compile
pushd "%~dp0..\UE4"
git restore Engine/Build/Build.version
popd
popd
