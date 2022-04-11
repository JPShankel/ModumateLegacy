#!/bin/sh
UE_APP="/Users/nourij/Code/ModumateGit/UE4/Engine/Binaries/Mac/UE4Editor.app"
CEF_GPU_APP="${UE_APP}/Contents/Frameworks/UnrealCEFSubProcess (GPU).app"
CEF_RENDERER_APP="${UE_APP}/Contents/Frameworks/UnrealCEFSubProcess (Renderer).app"
CEF_HELPER_APP="${UE_APP}/Contents/Frameworks/UnrealCEFSubProcess.app"
CEF_FRAMEWORK="${UE_APP}/Contents/Frameworks/Chromium Embedded Framework.framework"

#CERTIFICATE_IDENTITY="Developer ID Application: Modumate, Inc. (44CC46TSLW)"
ENTITLEMENTS_UE="/Users/nourij/Code/ModumateGit/Modumate/Scripts/ent_ue4editor.plist"
ENTITLEMENTS_CEF="/Users/nourij/Code/ModumateGit/Modumate/Scripts/ent_cef_helper.plist"
ENTITLEMENTS_FRAMEWORK="/Users/nourij/Code/ModumateGit/Modumate/Scripts/ent_cef_helper.plist"

highlight=$(tput setaf 2)
normal=$(tput sgr0)

echo "${highlight}Removing Signature on CEF Framework${normal}"
codesign --remove-signature "${CEF_FRAMEWORK}/Libraries/libEGL.dylib"
codesign --remove-signature "${CEF_FRAMEWORK}/Libraries/libGLESv2.dylib"
codesign --remove-signature "${CEF_FRAMEWORK}/Libraries/libswiftshader_libEGL.dylib"
codesign --remove-signature "${CEF_FRAMEWORK}/Libraries/libswiftshader_libGLESv2.dylib"
codesign --remove-signature "${CEF_FRAMEWORK}/Libraries/libvk_swiftshader.dylib"
codesign --remove-signature "${CEF_FRAMEWORK}"

echo "${highlight}Removing Signature on UnrealCEFSubProcess.app${normal}"
codesign --remove-signature "${CEF_HELPER_APP}"

echo "${highlight}Removing Signature on UnrealCEFSubProcess (GPU).app${normal}"
codesign --remove-signature "${CEF_GPU_APP}"

echo "${highlight}Removing Signature on UnrealCEFSubProcess (Renderer).app${normal}"
codesign --remove-signature "${CEF_RENDERER_APP}"

echo "${highlight}Removing Signature on UE4Editor.app${normal}"
codesign --remove-signature "${UE_APP}"

