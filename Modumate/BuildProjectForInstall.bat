@ECHO off
ECHO Fetching from Perforce...
pushd "%~dp0"
call update_p4_client.py

REM p4 clean
p4 sync

ECHO Building project...
rmdir /S /Q "%~dp0..\Builds\WindowsNoEditor"

call "%~dp0..\UE4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%~dp0\Modumate.uproject" -CrashReporter -compressed -prereqs -utf8output -distribution -P4 -platform=Win64 -clientconfig=Shipping -serverconfig=Shipping -cook -allmaps -build -stage -pak -archive -archivedirectory="%~dp0..\Builds" -codesign
popd
