#!/bin/sh

#-------------------------------------
# Setup Variables

PROJECT_PATH=$1
TARGET_NAME=$2
CONFIGURATION=$3
oldDir=$(pwd)
highlight=$(tput setaf 2)
normal=$(tput sgr0)

MODUMATE_GIT_PATH="${PROJECT_PATH}/.."
UE4_PATH="${MODUMATE_GIT_PATH}/UE4"
ENGINE_BINARIES_PATH="${UE4_PATH}/Engine/Binaries/Mac"

if [[ "$TARGET_NAME" == *"Editor"* ]]; then
  UE_OUTPUT_APP="${ENGINE_BINARIES_PATH}/UE4Editor.app"
else
    if [ $CONFIGURATION == "Development" ]; then
        UE_OUTPUT_APP="${PROJECT_PATH}/Binaries/Mac/Modumate.app"
    else
        UE_OUTPUT_APP="${PROJECT_PATH}/Binaries/Mac/Modumate-Mac-Shipping.app"
    fi
fi

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
ln -sf "${UE_OUTPUT_APP}/Contents/Frameworks/Chromium Embedded Framework.framework" ... "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess.app/Contents/Frameworks"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app/Contents/Frameworks"
ln -sf "${UE_OUTPUT_APP}/Contents/Frameworks/Chromium Embedded Framework.framework" ... "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app/Contents/Frameworks"

mkdir -p "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app/Contents/Frameworks"
ln -sf "${UE_OUTPUT_APP}/Contents/Frameworks/Chromium Embedded Framework.framework" ... "${UE_OUTPUT_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app/Contents/Frameworks"

cd $oldDir
