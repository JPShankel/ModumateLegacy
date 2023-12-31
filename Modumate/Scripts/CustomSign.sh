#!/bin/sh

#-------------------------------------
# Setup Variables

IDENTITY=$MODUMATE_ENV_IDENTITY
PROJECT_PATH=$1
APP_BUNDLE=$2

oldDir=$(pwd)
highlight=$(tput setaf 2)
normal=$(tput sgr0)

MODUMATE_GIT_PATH="${PROJECT_PATH}/.."
UE4_PATH="${MODUMATE_GIT_PATH}/UE4"
ENGINE_BINARIES_PATH="${UE4_PATH}/Engine/Binaries/Mac"
UE_OUTPUT_APP=${APP_BUNDLE}

CEF_GPU_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app"
CEF_RENDERER_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app"
CEF_HELPER_APP="${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess.app"
CEF_FRAMEWORK="${UE_OUTPUT_APP}/Contents/Frameworks/Chromium Embedded Framework.framework"


ENTITLEMENTS_UE="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_ue4editor.plist"
ENTITLEMENTS_HELPER="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"
ENTITLEMENTS_FRAMEWORK="${MODUMATE_GIT_PATH}/Modumate/Scripts/ent_cef_helper.plist"


echo "${highlight}Signing CEF Framework${normal}"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}/Libraries/libEGL.dylib"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}/Libraries/libGLESv2.dylib"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}/Libraries/libswiftshader_libEGL.dylib"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}/Libraries/libswiftshader_libGLESv2.dylib"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}/Libraries/libvk_swiftshader.dylib"
codesign --force --options runtime --entitlements "$ENTITLEMENTS_FRAMEWORK" --sign "$IDENTITY" --timestamp --verbose "${CEF_FRAMEWORK}"

echo "${highlight}Signing UnrealCEFSubProcess.app${normal}"
codesign -f -v -s "$IDENTITY" --entitlements "${ENTITLEMENTS_HELPER}" --options runtime --timestamp "${CEF_HELPER_APP}"

echo "${highlight}Signing UnrealCEFSubProcess (GPU).app${normal}"
codesign -f -v -s "$IDENTITY" --entitlements "${ENTITLEMENTS_HELPER}" --options runtime --timestamp "${CEF_GPU_APP}"

echo "${highlight}Signing UnrealCEFSubProcess (Renderer).app${normal}"
codesign -f -v -s "$IDENTITY" --entitlements "${ENTITLEMENTS_HELPER}" --options runtime --timestamp "${CEF_RENDERER_APP}"

echo "${highlight}Signing ${UE_OUTPUT_APP}${normal}"
codesign -f -v -s "$IDENTITY" --entitlements "${ENTITLEMENTS_UE}" --options runtime --timestamp "${UE_OUTPUT_APP}"