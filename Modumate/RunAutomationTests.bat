@echo off
pushd %~dp0
echo:
echo Testing Modumate...
call "%~dp0..\UE4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%~dp0Modumate.uproject" -noP4 -buildmachine -unattended -nullrhi -run "-RunAutomationTest=Modumate"
