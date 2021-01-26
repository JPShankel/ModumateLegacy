REM Generate UE4 pak delta between two releases
REM Usage: MakeUE4PakPatch PREVIOUS_VERSION NEW_VERSION

setlocal
if "%2"=="" exit /B 1

set PREVIOUS_VERSION=%1%
set NEW_VERSION=%2%

set BUILD_AREA=C:\Modumate\Builds

REM Previous version
set GIT_BRANCH=release/%PREVIOUS_VERSION%
git checkout --recurse-submodules %GIT_BRANCH%

call UE4\Setup.bat --force
call UE4\GenerateProjectFiles.bat
call UE4\Engine\Binaries\DotNET\UnrealBuildTool.exe -projectfiles -project="%CD%\Modumate\Modumate.uproject" -game -engine
call UE4\Engine\Build\BatchFiles\Build.bat ModumateEditor Win64 Development "%CD%\Modumate\Modumate.uproject" -WaitMutex -FromMsBuild
call UE4\Engine\Build\BatchFiles\Build.bat ShaderCompileWorker Win64 Development -WaitMutex -FromMsBuild

call "%CD%\UE4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\Modumate\Modumate.uproject" -BUILDMACHINE -CrashReporter -compressed -prereqs -utf8output -formal -distribution -NoP4 -Branch="%GIT_BRANCH%" -platform="Win64" -clientconfig="Shipping" -serverconfig="Shipping" -cook -allmaps -build -stage -pak -archive -archivedirectory="%BUILD_AREA%\Patch\Base\%PREVIOUS_VERSION%" -CodeSign -createreleaseversionroot="%BUILD_AREA%\Patch\ReleaseBases" -basedonreleaseversionroot="%BUILD_AREA%\ReleaseBases" -createreleaseversion=%PREVIOUS_VERSION%

REM New version
set GIT_BRANCH=release/%NEW_VERSION%
git checkout --recurse-submodules %GIT_BRANCH%

call UE4\GenerateProjectFiles.bat
call UE4\Engine\Binaries\DotNET\UnrealBuildTool.exe -projectfiles -project="%CD%\Modumate\Modumate.uproject" -game -engine
call UE4\Engine\Build\BatchFiles\Build.bat ModumateEditor Win64 Development "%CD%\Modumate\Modumate.uproject" -WaitMutex -FromMsBuild
call UE4\Engine\Build\BatchFiles\Build.bat ShaderCompileWorker Win64 Development -WaitMutex -FromMsBuild

call "%CD%\UE4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\Modumate\Modumate.uproject" -BUILDMACHINE -CrashReporter -compressed -prereqs -utf8output -formal -distribution -NoP4 -Branch="%GIT_BRANCH%" -platform="Win64" -clientconfig="Shipping" -serverconfig="Shipping" -cook -allmaps -build -stage -pak -archive -archivedirectory="%BUILD_AREA%\Patch\%NEW_VERSION%" -CodeSign -createreleaseversionroot="%BUILD_AREA%\Patch\ReleaseBases" -basedonreleaseversionroot="%BUILD_AREA%\ReleaseBases" -generatepatch -basedonreleaseversion=%PREVIOUS_VERSION% -createreleaseversion=%NEW_VERSION%

xcopy "%BUILD_AREA%\Patch\Base\%PREVIOUS_VERSION%\WindowsNoEditor\Modumate\Content\Paks\*WindowsNoEditor.*" "%BUILD_AREA%\Patch\%NEW_VERSION%\WindowsNoEditor\Modumate\Content\Paks" /Y

Modumate\NSIS\MakeModumatePatchInstaller.bat %PREVIOUS_VERSION% %NEW_VERSION%