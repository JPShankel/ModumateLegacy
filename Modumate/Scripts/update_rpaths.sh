rm -r /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Build
rm -r /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/Resources/RadioEffectUnit.component
find /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents -type f '(' -name '*.dylib' ')' -exec sh -c 'install_name_tool -add_rpath $(dirname "$1") "$1"' sh {} \;
find /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents | grep .dylib | xargs codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Binaries/Mac/CrashReportClient.app"
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app"