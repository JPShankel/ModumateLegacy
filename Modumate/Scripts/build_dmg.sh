rm /Users/buildy/Documents/$1/Modumate-Installer-Unconverted.dmg
hdiutil create -volname Modumate-Installer -srcfolder /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/ -ov -format UDRO /Users/buildy/Documents/$1/Modumate-Installer-Unconverted.dmg
rm /Users/buildy/Documents/$1/Modumate-Installer.dmg
hdiutil convert /Users/buildy/Documents/$1/Modumate-Installer-Unconverted.dmg -format UDRO -o /Users/buildy/Documents/$1/Modumate-Installer.dmg
rm /Users/buildy/Documents/$1/Modumate-Installer-Unconverted.dmg
codesign  -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)"  --timestamp -i "com.Modumate.Modumate" "/Users/buildy/Documents/$1/Modumate-Installer.dmg"
