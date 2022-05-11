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

#-------------------------------------
# Do some cleanup of unused component
if [[ -f "${UE_OUTPUT_APP}/Contents/UE4/Engine/Build" ]]; then
    rm -r "${UE_OUTPUT_APP}/Contents/UE4/Engine/Build"
fi
if [[ -f "${UE_OUTPUT_APP}/Contents/Resources/RadioEffectUnit.component" ]]; then
    rm -r "${UE_OUTPUT_APP}/Contents/Resources/RadioEffectUnit.component"
fi

#-------------------------------------
# Update the rpaths
INFO_PLIST="${UE_OUTPUT_APP}/Contents/Info.plist"

if [[ ! -z "$BUILD_VERSION" ]]; then
    if [[ ! -z "$ENGINE_VERSION" ]]; then
        if [[ -f "${INFO_PLIST}" ]]; then
            echo "${highlight}Updating Info.plist${normal}"
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
    BUNDLE_FRAMEWORK="${UE_OUTPUT_APP}/Contents/Frameworks/Chromium Embedded Framework.framework"

    ENTITLEMENTS_UE="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_ue4editor.plist"
    ENTITLEMENTS_HELPER="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"
    ENTITLEMENTS_FRAMEWORK="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"

    echo "${highlight}Signing dylibs${normal}"
    find "${UE_OUTPUT_APP}/Contents" -name '*.dylib' -print0 | xargs -0 -J % codesign -f -v -s "$IDENTITY" --options runtime --timestamp "%"

    echo "${highlight}Signing CEF Framework${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_FRAMEWORK}"

    echo "${highlight}Signing CrashReportClient.app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}/Contents/UE4/Engine/Binaries/Mac/CrashReportClient.app"

    echo "${highlight}Signing UnrealCEFSubProcess.app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_HELPER_APP}"

    echo "${highlight}Signing UnrealCEFSubProcess (GPU).app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_GPU_APP}"

    echo "${highlight}Signing UnrealCEFSubProcess (Renderer).app${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_HELPER" --sign "$IDENTITY" --timestamp --verbose "${BUNDLE_RENDERER_APP}"

    echo "${highlight}Signing ${UE_OUTPUT_APP}${normal}"
    codesign --force --options runtime --entitlements "$ENTITLEMENTS_UE" --sign "$IDENTITY" --timestamp --verbose "${UE_OUTPUT_APP}"

else
    echo "${highlight}Skipping signing (no identity found)${normal}"
fi
cd $oldDir
