ECHO Fetch from Perforce
p4 set P4CLIENT=JasonShankel_DESKTOP-DEVCHECK
cd c:\modumate\devcheck

p4 clean

p4 sync
p4 set P4CLIENT=JasonShankel_DESKTOP-DEV

ECHO Build project
rmdir /S /Q C:\Modumate\TempBuild\WindowsNoEditor

call RunUAT BuildCookRun -project="C:\Modumate\devcheck\Modumate.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -serverconfig=Shipping -cook -allmaps -build -stage -pak -archive -archivedirectory="C:\Modumate\TempBuild"

cd C:\Modumate\TempBuild
del ModumateLatest.zip

ECHO Send a zipped and an unzipped copy to the distribution drive
7z a -r ModumateLatest.zip *.*
del "G:\Shared Drives\Modumate\ENGINEERING\Distribution\ModumateLatest.zip"
move ModumateLatest.zip "G:\Shared Drives\Modumate\ENGINEERING\Distribution\ModumateLatest.zip"

cd c:\modumate

exit