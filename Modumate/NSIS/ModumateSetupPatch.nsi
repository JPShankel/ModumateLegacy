; Script generated by the HM NIS Edit Script Wizard.

Unicode True
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "Modumate"

#Note: product version expected as input parameter
#!define PRODUCT_VERSION "UnSetVersion"

!define PRODUCT_PUBLISHER "Modumate, Inc"
!define PRODUCT_WEB_SITE "https://www.modumate.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\Modumate.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\Build\Windows\Application.ico"
!define MUI_UNICON "..\Build\Windows\Application.ico"

!define MUI_TEXT_WELCOME_INFO_TEXT "Setup will update the installed Modumate from v${PREVIOUS_VERSION} to v${PRODUCT_VERSION}.$\r$\n$\r$\n$_CLICK"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "..\Build\EULA.rtf"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\Modumate.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

!define PATH_OUT "${PATH_OUT_ROOT}/${PRODUCT_VERSION}"
!system 'md "${PATH_OUT}"'

; !finalize '.\SignInstaller.bat "%1"'

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PATH_OUT}\ModumatePatchSetup.exe"
InstallDir "$PROGRAMFILES64\Modumate"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

!define PATCH_INSTALL_ROOT "$INSTDIR"
!define PATCH_SOURCE_ROOT "${BUILD_DIR}\${PRODUCT_VERSION}\WindowsNoEditor"
!define PATCH_FILES_ROOT "${BUILD_DIR}\patchFiles"
!include "${BUILD_DIR}\patchFiles.nsi"

Section "Patch Install Core"
  SectionIn RO
  SetOutPath $INSTDIR

  Call patchFilesModified 
SectionEnd

Section -AdditionalIcons
  SetOutPath $INSTDIR
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\Modumate\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\Modumate\Uninstall.lnk" "$INSTDIR\UninstallModumate.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\UninstallModumate.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Modumate.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\UninstallModumate.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Modumate.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\UninstallModumate.exe"
  Delete "$INSTDIR\Modumate\Plugins\Revulytics\Binaries\ThirdParty\RevulyticsLibrary\Win64\ruiSDK_5.5.1.x64.dll"
  Delete "$INSTDIR\Modumate\Content\Splash\Splash.bmp"
  Delete "$INSTDIR\Modumate\Content\Paks\pakchunk0-WindowsNoEditor.sig"
  Delete "$INSTDIR\Modumate\Content\Paks\pakchunk0-WindowsNoEditor.pak"
  Delete "$INSTDIR\Modumate\Binaries\Win64\PDFNetDotNetCore.dll"
  Delete "$INSTDIR\Modumate\Binaries\Win64\PDFNetC.dll"
  Delete "$INSTDIR\Modumate\Binaries\Win64\Modumate-Win64-Shipping.exe"
  Delete "$INSTDIR\Engine\Programs\CrashReportClient\Content\Paks\CrashReportClient.pak"
  Delete "$INSTDIR\Engine\Extras\Redist\en-us\UE4PrereqSetup_x64.exe"
  Delete "$INSTDIR\Engine\Content\SlateDebug\Fonts\LastResort.ttf"
  Delete "$INSTDIR\Engine\Content\SlateDebug\Fonts\LastResort.tps"
  Delete "$INSTDIR\Engine\Binaries\Win64\CrashReportClient.pdb"
  Delete "$INSTDIR\Engine\Binaries\Win64\CrashReportClient.exe"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\Windows\XAudio2_9\x64\xaudio2_9redist.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\Vorbis\Win64\VS2015\libvorbis_64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\Vorbis\Win64\VS2015\libvorbisfile_64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\PxPvdSDK_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\PxFoundation_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\PhysX3_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\PhysX3Cooking_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\PhysX3Common_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\NvCloth_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\APEX_Legacy_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\APEX_Clothing_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\PhysX3\Win64\VS2015\ApexFramework_x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\Ogg\Win64\VS2015\libogg_64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\NVIDIA\NVaftermath\Win64\GFSDK_Aftermath_Lib.x64.dll"
  Delete "$INSTDIR\Engine\Binaries\ThirdParty\DbgHelp\dbghelp.dll"
  Delete "$INSTDIR\Modumate.exe"

  Delete "$SMPROGRAMS\Modumate\Uninstall.lnk"
  Delete "$SMPROGRAMS\Modumate\Website.lnk"
  Delete "$DESKTOP\Modumate.lnk"
  Delete "$SMPROGRAMS\Modumate\Modumate.lnk"

  RMDir "$SMPROGRAMS\Modumate"
  RMDir /r "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd