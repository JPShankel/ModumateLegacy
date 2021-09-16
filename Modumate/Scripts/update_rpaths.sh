chmod -R a+rw "/Users/buildy/Documents/"$1
infoplist="/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/Info.plist"
sedcom="sed -I b 's/"$2"/"$1"/' $infoplist"
eval "$sedcom"
splitcom="split -l 37 $infoplist urlsplit"
eval "$splitcom"
splitcom="cat ./urlsplitaa ./Modumate/Scripts/url_scheme.info ./urlsplitab > $infoplist"
eval "$splitcom"
rm ./urlsplitaa
rm ./urlsplitab
infoplist="${infoplist}b"
rm $infoplist
cp ./Modumate/Scripts/Modumate_Mac_Client.provisionprofile ~/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/embedded.provisionprofile
rm -r /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Build
rm -r /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/Resources/RadioEffectUnit.component
find /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents -type f '(' -name '*.dylib' ')' -exec sh -c 'install_name_tool -add_rpath $(dirname "$1") "$1"' sh {} \;
find /Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents | grep .dylib | xargs codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --options runtime --timestamp
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Binaries/Mac/UnrealCEFSubProcess.app"
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Binaries/ThirdParty/CEF3/Mac/Chromium Embedded Framework.framework"
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app/Contents/UE4/Engine/Binaries/Mac/CrashReportClient.app"
codesign -f -v -s "Developer ID Application: Modumate, Inc. (44CC46TSLW)" --entitlements "./Modumate/Scripts/entitlements.plist" --options runtime --timestamp "/Users/buildy/Documents/$1/ModumatePackaged/MacNoEditor/Modumate-Mac-Shipping.app"
