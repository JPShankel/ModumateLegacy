ECHO Build installer

cd "%MDMROOT%"\main\Modumate\InstallShield\ModumateSetup

iscmdbld -p ModumateSetup.ism -c COMP -e y

del "G:\Shared Drives\Modumate\ENGINEERING\Distribution\ModumateSetup.exe"

del "G:\Shared Drives\Modumate\ENGINEERING\Distribution\ModumateSetup.msi"

move "%MDMROOT%\main\Modumate\InstallShield\ModumateSetup\ModumateSetup\Default Configuration\Release\DiskImages\DISK1\ModumateSetup.exe" "G:\Shared Drives\Modumate\ENGINEERING\Distribution\ModumateSetup.exe"

move "%MDMROOT%\main\Modumate\InstallShield\ModumateSetup\ModumateSetup\Default Configuration\Release\DiskImages\DISK1\Modumate.msi" "G:\Shared Drives\Modumate\ENGINEERING\Distribution\Modumate.msi"


