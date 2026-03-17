; RawrXD Win32 IDE Installer Script
; NSIS (Nullsoft Scriptable Install System)
; Version 1.0.0

!define PRODUCT_NAME "RawrXD Win32 IDE"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "RawrXD Project"
!define PRODUCT_WEB_SITE "https://github.com/your-org/RawrXD"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI Settings
!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\AgenticIDEWin.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Installer attributes
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "RawrXD-Win32-Setup-v1.0.0.exe"
InstallDir "$PROGRAMFILES64\RawrXD"
InstallDirRegKey HKLM "${PRODUCT_UNINST_KEY}" "UninstallString"
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  SetOutPath "$INSTDIR\bin"
  SetOverwrite on
  File "D:\RawrXD-Win32-Deploy\bin\AgenticIDEWin.exe"
  
  SetOutPath "$INSTDIR\config"
  File "D:\RawrXD-Win32-Deploy\config\config.json"
  
  SetOutPath "$INSTDIR\docs"
  File /r "D:\RawrXD-Win32-Deploy\docs\*.*"
  
  SetOutPath "$INSTDIR"
  File "D:\RawrXD-Win32-Deploy\README.md"
  File "D:\RawrXD-Win32-Deploy\RawrXD.bat"
  File "D:\RawrXD-Win32-Deploy\RawrXD.ps1"
  
  ; Create shortcuts
  CreateDirectory "$SMPROGRAMS\RawrXD"
  CreateShortCut "$SMPROGRAMS\RawrXD\RawrXD IDE.lnk" "$INSTDIR\bin\AgenticIDEWin.exe"
  CreateShortCut "$SMPROGRAMS\RawrXD\Uninstall.lnk" "$INSTDIR\uninst.exe"
  CreateShortCut "$DESKTOP\RawrXD IDE.lnk" "$INSTDIR\bin\AgenticIDEWin.exe"
  
  ; Create AppData directories on first run
  CreateDirectory "$APPDATA\RawrXD"
  CreateDirectory "$LOCALAPPDATA\RawrXD\logs"
  
  ; Copy default config if not exists
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
  
  ; Add to PATH (optional)
  MessageBox MB_YESNO "Add RawrXD to system PATH?" IDNO skip_path
    ; Add to PATH
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
  
  Delete "$INSTDIR\bin\AgenticIDEWin.exe"
  RMDir "$INSTDIR\bin"
  
  Delete "$INSTDIR\config\config.json"
  RMDir "$INSTDIR\config"
  
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\orchestration"
  
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
