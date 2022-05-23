#!/bin/sh
# Must use LF line endings

#-------------------------------------
# Setup Variables

PROJECT_PATH=$1
TARGET_NAME=$2
CONFIGURATION=$3
UE_OUTPUT_APP=$4

IDENTITY=$5
if [[ -z "$IDENTITY" ]]; then
    IDENTITY=$MODUMATE_ENV_IDENTITY
fi

IS_JENKINS=$6

oldDir=$(pwd)
highlight=$(tput setaf 2)
normal=$(tput sgr0)

MODUMATE_GIT_PATH="${PROJECT_PATH}/.."
UE4_PATH="${MODUMATE_GIT_PATH}/UE4"
ENGINE_BINARIES_PATH="${UE4_PATH}/Engine/Binaries/Mac"

BUILD_VERSION=$(python3 ${MODUMATE_GIT_PATH}/Modumate/Scripts/utils.py get_project_version)
ENGINE_VERSION=$(python3 ${MODUMATE_GIT_PATH}/Modumate/Scripts/utils.py get_engine_version)

if [[ -z "$UE_OUTPUT_APP" ]]; then
    if [[ "$TARGET_NAME" == *"Editor"* ]]; then
    UE_OUTPUT_APP="${ENGINE_BINARIES_PATH}/UE4Editor.app"
    else
        if [ $CONFIGURATION == "Development" ]; then
            UE_OUTPUT_APP="${PROJECT_PATH}/Binaries/Mac/Modumate.app"
        else
            UE_OUTPUT_APP="${PROJECT_PATH}/Binaries/Mac/Modumate-Mac-Shipping.app"
        fi
    fi
fi
echo "${highlight}Running Post Build on ${UE_OUTPUT_APP}${normal}"

CEF_FRAMEWORK_PATH="${UE4_PATH}/Engine/Source/ThirdParty/CEF3/cef_binary_99.2.15.g71e9523_macosx64/Release"
CEF_FRAMEWORK="${CEF_FRAMEWORK_PATH}/Chromium Embedded Framework.framework"

CEF_HELPER_UTIL="${ENGINE_BINARIES_PATH}/UnrealCEFSubProcess.app"
CEF_HELPER_GPU="${ENGINE_BINARIES_PATH}/UnrealCEFSubProcess (GPU).app"
CEF_HELPER_RENDER="${ENGINE_BINARIES_PATH}/UnrealCEFSubProcess (Renderer).app"

#-------------------------------------
# Copy UnrealCEFSubProcess(es) to App Bundle

chmod a+rx "${CEF_HELPER_UTIL}/Contents/MacOS/UnrealCEFSubProcess"
chmod a+rx "${CEF_HELPER_GPU}/Contents/MacOS/UnrealCEFSubProcess (GPU)"
chmod a+rx "${CEF_HELPER_RENDER}/Contents/MacOS/UnrealCEFSubProcess (Renderer)"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks"

echo "${highlight}Placing UnrealCEFSubprocess in ${TARGET_NAME} App Bundle${normal}"
cp -R "${CEF_HELPER_UTIL}" "${UE_OUTPUT_APP}/Contents/Frameworks"

echo "${highlight}Placing UnrealCEFSubprocess (GPU) in ${TARGET_NAME} App Bundle${normal}"
cp -R "${CEF_HELPER_GPU}" "${UE_OUTPUT_APP}/Contents/Frameworks"

echo "${highlight}Placing UnrealCEFSubprocess (Renderer) in ${TARGET_NAME} App Bundle${normal}"
cp -R "${CEF_HELPER_RENDER}" "${UE_OUTPUT_APP}/Contents/Frameworks"


#-------------------------------------
# Copy and Link frameworks to App Bundle
echo "${highlight}Placing CEF Framework in ${TARGET_NAME} App Bundle and creating symlinks${normal}"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks"
cp -R "${CEF_FRAMEWORK}" "${UE_OUTPUT_APP}/Contents/Frameworks"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess.app/Contents/Frameworks"
cp -R "${CEF_FRAMEWORK}" "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess.app/Contents/Frameworks"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app/Contents/Frameworks"
cp -R "${CEF_FRAMEWORK}" "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app/Contents/Frameworks"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app/Contents/Frameworks"
cp -R "${CEF_FRAMEWORK}" "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app/Contents/Frameworks"

INFO_PLIST="${UE_OUTPUT_APP}/Contents/Info.plist"

#-------------------------------------
# For shipping, we need to strip the 
# Mac-Shipping part of the bundle
# name before any of the following
# steps
if [[ ! -z $IS_JENKINS ]]; then

    #-------------------------------------
    # Do some cleanup of unused components
    if [[ -d "${UE_OUTPUT_APP}/Contents/UE4/Engine/Build" ]]; then
        echo "${highlight}Removing Contents/UE4/Engine/Build${normal}"
        rm -rf "${UE_OUTPUT_APP}/Contents/UE4/Engine/Build"
    fi
    if [[ -d "${UE_OUTPUT_APP}/Contents/Resources/RadioEffectUnit.component" ]]; then
        echo "${highlight}Removing CContents/Resources/RadioEffectUnit.component${normal}"
        rm -rf "${UE_OUTPUT_APP}/Contents/Resources/RadioEffectUnit.component"
    fi

    echo "Changing Modumate-Mac-Shipping.app to Modumate.app"
    folder=$(dirname "${UE_OUTPUT_APP}")
    
    #move the app bundle from Modumate-Mac-Shipping.app to Modumate.app
    mv "${UE_OUTPUT_APP}" "${folder}/Modumate.app"
    UE_OUTPUT_APP="${folder}/Modumate.app"

    #move the binary in the same way
    mv "${UE_OUTPUT_APP}/Contents/MacOS/Modumate-Mac-Shipping" "${UE_OUTPUT_APP}/Contents/MacOS/Modumate"

    #Update the reference to the plist so it points to the
    # renamed app bundle
    INFO_PLIST="${UE_OUTPUT_APP}/Contents/Info.plist"
    plutil -replace CFBundleName -string Modumate $INFO_PLIST
    plutil -replace CFBundleExecutable -string Modumate $INFO_PLIST

    # For every dylib in the app bundle (excluding CEFs), change the rpaths
    # dirname gives us a path up to current pwd, so make sure we've cd'd into the package before adding rpaths    
    cd "${UE_OUTPUT_APP}/Contents/"
    find "./UE4/Engine/Binaries/ThirdParty/PhysX3" -name "*.dylib" -type f -exec sh -c 'install_name_tool -delete_rpath "/Volumes/Work/Perforce/UE4/Engine/Binaries/ThirdParty/PhysX3/Mac" "$1"' sh {} \;
    find "./UE4" -name "*.dylib" -type f -exec sh -c 'install_name_tool -add_rpath $(dirname "$1") "$1"' sh {} \;
    cd $oldDir

fi

#-------------------------------------
# Add URL Scheme to plist
plutil -replace CFBundleURLTypes -json '[]' $INFO_PLIST
plutil -replace CFBundleURLTypes.0 -json '{}' $INFO_PLIST

plutil -insert CFBundleURLTypes.0.CFBundleTypeRole -string "Viewer" $INFO_PLIST
plutil -insert CFBundleURLTypes.0.CFBundleURLName -string "com.Modumate.Modumate" $INFO_PLIST
plutil -insert CFBundleURLTypes.0.CFBundleURLSchemes -json '["mdmt"]' $INFO_PLIST
plutil -replace NSMicrophoneUsageDescription -string "Uses the microphone for chat" $INFO_PLIST

#-------------------------------------
# Modify PLIST with correct version and add provisionprofile
if [[ ! -z "$BUILD_VERSION" ]]; then
    if [[ ! -z "$ENGINE_VERSION" ]]; then
        if [[ -f "${INFO_PLIST}" ]]; then
            echo "${highlight}Updating Info.plist version${normal}"
            cd ${MODUMATE_GIT_PATH}
            sedcom="sed -I b 's/"${ENGINE_VERSION}"/"${BUILD_VERSION}"/' ${INFO_PLIST}"
            echo $sedcom
            eval "$sedcom"
            #sed creates a backup on mac no matter the args, don't know why -- but it needs to be deleted
            # prior to signing
            rm "${INFO_PLIST}b"
            cp "${MODUMATE_GIT_PATH}/Modumate/Scripts/Modumate_Mac_Client.provisionprofile" "${UE_OUTPUT_APP}/Contents/embedded.provisionprofile"
        fi
    fi
fi

#-------------------------------------
# Sign everything
if [[ ! -z "$IDENTITY" ]]; then

    BUNDLE_GPU_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app"
    BUNDLE_RENDERER_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app"
    BUNDLE_HELPER_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess.app"

    EXTENSION_FRAMEWORK="/Contents/Frameworks/Chromium Embedded Framework.framework"
    EXTENSION_BINARY_FRAMEWORK="${EXTENSION_FRAMEWORK}/Chromium Embedded Framework"

    ENTITLEMENTS_UE="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_ue4editor.plist"
    ENTITLEMENTS_HELPER="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"
    ENTITLEMENTS_FRAMEWORK="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"

    echo "${highlight}Signing dylibs${normal}"
    find "${UE_OUTPUT_APP}/Contents" -name '*.dylib' -print0 | xargs -0 -J % codesign -f -v -s "$IDENTITY" --options runtime --timestamp "%"

    echo "${highlight}Signing CEF Framework Binaries${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}${EXTENSION_BINARY_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_GPU_APP}${EXTENSION_BINARY_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_RENDERER_APP}${EXTENSION_BINARY_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_HELPER_APP}${EXTENSION_BINARY_FRAMEWORK}"

    echo "${highlight}Signing CEF Frameworks${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}${EXTENSION_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_GPU_APP}${EXTENSION_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_RENDERER_APP}${EXTENSION_FRAMEWORK}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_HELPER_APP}${EXTENSION_FRAMEWORK}"

    echo "${highlight}Signing CrashReportClient.app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}/Contents/UE4/Engine/Binaries/Mac/CrashReportClient.app"

    echo "${highlight}Signing UnrealCEFSubProcess.app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_HELPER_APP}"

    echo "${highlight}Signing UnrealCEFSubProcess (GPU).app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_GPU_APP}"

    echo "${highlight}Signing UnrealCEFSubProcess (Renderer).app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_RENDERER_APP}"

    echo "${highlight}Signing CrashReportClient Binary${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}/Contents/UE4/Engine/Binaries/Mac/CrashReportClient.app/Contents/MacOS/CrashReportClient"

    for file in ${UE_OUTPUT_APP}/Contents/MacOS/*
    do
        echo "${highlight}Signing Modumate Binary${normal}"
        codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "$file"
    done

    echo "${highlight}Signing ${UE_OUTPUT_APP}${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}"

else
    echo "${highlight}Skipping signing (no identity found)${normal}"
fi
cd $oldDir
