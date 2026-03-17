; RawrXD Win32 IDE Installer Script
; NSIS (Nullsoft Scriptable Install System)
; Version 1.0.0

!define PRODUCT_NAME "RawrXD Win32 IDE"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "RawrXD Project"
!define PRODUCT_WEB_SITE "https://github.com/your-org/RawrXD"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

!ifndef SOURCE_ROOT
!define SOURCE_ROOT ".."
!endif
!define BIN_PATH "${SOURCE_ROOT}\build-win32-only\bin\Release\AgenticIDEWin.exe"
!define CONFIG_PATH "${SOURCE_ROOT}\config.example.json"
!define DOCS_PATH "${SOURCE_ROOT}\docs\*.md"
!define README_PATH "${SOURCE_ROOT}\WIN32_README.md"
!define SUMMARY_PATH "${SOURCE_ROOT}\COMPLETION_SUMMARY.md"
!define LICENSE_PATH "${SOURCE_ROOT}\LICENSE"

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_PATH}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\AgenticIDEWin.exe"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "RawrXD-Win32-Setup-${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES64\RawrXD"
InstallDirRegKey HKLM "${PRODUCT_UNINST_KEY}" "UninstallString"
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  SetOutPath "$INSTDIR\bin"
  SetOverwrite on
  File "${BIN_PATH}"

  SetOutPath "$INSTDIR\config"
  File "${CONFIG_PATH}"

  SetOutPath "$INSTDIR\docs"
  File /r "${DOCS_PATH}"

  SetOutPath "$INSTDIR"
  File "${README_PATH}"
  File "${SUMMARY_PATH}"
  File "${LICENSE_PATH}"
  File "${SOURCE_ROOT}\RawrXD.bat"
  File "${SOURCE_ROOT}\RawrXD.ps1"

  CreateDirectory "$SMPROGRAMS\RawrXD"
  CreateShortCut "$SMPROGRAMS\RawrXD\RawrXD IDE.lnk" "$INSTDIR\bin\AgenticIDEWin.exe"
  CreateShortCut "$SMPROGRAMS\RawrXD\Uninstall.lnk" "$INSTDIR\uninst.exe"
  CreateShortCut "$DESKTOP\RawrXD IDE.lnk" "$INSTDIR\bin\AgenticIDEWin.exe"

  CreateDirectory "$APPDATA\RawrXD"
  CreateDirectory "$LOCALAPPDATA\RawrXD\logs"

  IfFileExists "$APPDATA\RawrXD\config.json" skip_config
    CopyFiles "$INSTDIR\config\config.json" "$APPDATA\RawrXD\config.json"
  skip_config:
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\RawrXD\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\AgenticIDEWin.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  MessageBox MB_YESNO "Add RawrXD to system PATH?" IDNO skip_path
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    StrCpy $0 "$0;$INSTDIR"
    WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" $0
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
  skip_path:
SectionEnd

Section Uninstall
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\RawrXD.ps1"
  Delete "$INSTDIR\RawrXD.bat"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\COMPLETION_SUMMARY.md"
  Delete "$INSTDIR\LICENSE"

  Delete "$INSTDIR\bin\AgenticIDEWin.exe"
  RMDir "$INSTDIR\bin"

  Delete "$INSTDIR\config\config.json"
  RMDir "$INSTDIR\config"

  RMDir /r "$INSTDIR\docs"

  Delete "$SMPROGRAMS\RawrXD\Uninstall.lnk"
  Delete "$SMPROGRAMS\RawrXD\Website.lnk"
  Delete "$SMPROGRAMS\RawrXD\RawrXD IDE.lnk"
  RMDir "$SMPROGRAMS\RawrXD"

  Delete "$DESKTOP\RawrXD IDE.lnk"

  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  SetAutoClose true

  MessageBox MB_YESNO "Remove user data and configuration?" IDNO skip_userdata
    RMDir /r "$APPDATA\RawrXD"
    RMDir /r "$LOCALAPPDATA\RawrXD"
  skip_userdata:
SectionEnd
