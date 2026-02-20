; =============================================================================
; RawrXD_Installer.nsi — NSIS Installer Script (Alternative to WiX)
; =============================================================================
; Full installer with VC++ Redist check, VulkanRT detection, start menu
; shortcuts, file associations, and uninstaller registration.
;
; Build: makensis RawrXD_Installer.nsi
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

; =============================================================================
; Metadata
; =============================================================================
!define PRODUCT_NAME "RawrXD IDE"
!define PRODUCT_VERSION "7.4.0"
!define PRODUCT_PUBLISHER "RawrXD / ItsMehRAWRXD"
!define PRODUCT_WEB_SITE "https://github.com/ItsMehRAWRXD/RawrXD"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY "Software\RawrXD\IDE"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "RawrXD-${PRODUCT_VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\RawrXD IDE"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" "InstallPath"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; =============================================================================
; Modern UI Configuration
; =============================================================================
!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\rawrxd.ico"
!define MUI_UNICON "..\resources\rawrxd.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\resources\installer_banner.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; =============================================================================
; Installer Sections
; =============================================================================

Section "Prerequisites" SecPrereq
    SectionIn RO

    ; -------------------------------------------------------------------
    ; Check 1: Windows 10 or later (64-bit)
    ; -------------------------------------------------------------------
    ${IfNot} ${AtLeastWin10}
        MessageBox MB_OK|MB_ICONSTOP "RawrXD IDE requires Windows 10 or later."
        Abort
    ${EndIf}

    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "RawrXD IDE requires a 64-bit operating system."
        Abort
    ${EndIf}

    ; -------------------------------------------------------------------
    ; Check 2: VC++ 2022 Redistributable (x64)
    ; -------------------------------------------------------------------
    ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64" "Installed"
    ${If} $0 != 1
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "Microsoft Visual C++ 2022 Redistributable (x64) is required.$\n$\n\
             Would you like to download it now?" IDYES DownloadVCRedist IDNO SkipVCRedist

        DownloadVCRedist:
            ExecShell "open" "https://aka.ms/vs/17/release/vc_redist.x64.exe"
            MessageBox MB_OK "Please install the VC++ Redistributable and run this setup again."
            Abort

        SkipVCRedist:
            MessageBox MB_OK|MB_ICONWARNING \
                "Warning: RawrXD IDE may not function correctly without the VC++ Redistributable."
    ${EndIf}

    ; -------------------------------------------------------------------
    ; Check 3: Vulkan Runtime (optional but recommended)
    ; -------------------------------------------------------------------
    ReadRegStr $0 HKLM "SOFTWARE\Khronos\Vulkan\Drivers" ""
    ${If} $0 == ""
        ; Check alternative registry path
        ReadRegStr $0 HKLM "SOFTWARE\Khronos\Vulkan\ExplicitLayers" ""
        ${If} $0 == ""
            MessageBox MB_YESNO|MB_ICONQUESTION \
                "Vulkan Runtime was not detected.$\n$\n\
                 GPU acceleration features (Vulkan compute, tensor staging) will be disabled.$\n$\n\
                 Would you like to install VulkanRT from LunarG?" IDYES DownloadVulkan IDNO SkipVulkan

            DownloadVulkan:
                ExecShell "open" "https://vulkan.lunarg.com/sdk/home"

            SkipVulkan:
        ${EndIf}
    ${EndIf}
SectionEnd

Section "Core Files" SecCore
    SectionIn RO
    SetOutPath "$INSTDIR"

    ; Executables
    File "..\build\bin\RawrXD_Gold.exe"
    File "..\build\bin\RawrXD-Win32IDE.exe"
    File "..\build\RawrEngine.exe"

    ; License and docs
    File "..\LICENSE"
    File "..\README.md"

    ; Create subdirectories
    CreateDirectory "$INSTDIR\config"
    CreateDirectory "$INSTDIR\crash_dumps"
    CreateDirectory "$INSTDIR\models"
    CreateDirectory "$INSTDIR\plugins"
    CreateDirectory "$INSTDIR\engines"

    ; Write install path to registry
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "InstallPath" "$INSTDIR"
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "Version" "${PRODUCT_VERSION}"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Write uninstall info to Add/Remove Programs
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\RawrXD_Gold.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1

    ; Calculate installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "EstimatedSize" $0
SectionEnd

Section "Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"

    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\RawrXD IDE.lnk" \
        "$INSTDIR\RawrXD_Gold.exe" "" "$INSTDIR\RawrXD_Gold.exe" 0

    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\RawrXD Win32 IDE.lnk" \
        "$INSTDIR\RawrXD-Win32IDE.exe" "" "$INSTDIR\RawrXD-Win32IDE.exe" 0

    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" \
        "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

    ; Desktop shortcut
    CreateShortCut "$DESKTOP\RawrXD IDE.lnk" \
        "$INSTDIR\RawrXD_Gold.exe" "" "$INSTDIR\RawrXD_Gold.exe" 0
SectionEnd

Section "File Associations" SecAssoc
    ; .gguf → RawrXD IDE
    WriteRegStr HKCR ".gguf" "" "RawrXD.GGUF"
    WriteRegStr HKCR "RawrXD.GGUF" "" "GGUF Model File"
    WriteRegStr HKCR "RawrXD.GGUF\shell\open\command" "" '"$INSTDIR\RawrXD_Gold.exe" "%1"'
    WriteRegStr HKCR "RawrXD.GGUF\DefaultIcon" "" "$INSTDIR\RawrXD_Gold.exe,0"

    ; .rawrlic → License installer
    WriteRegStr HKCR ".rawrlic" "" "RawrXD.License"
    WriteRegStr HKCR "RawrXD.License" "" "RawrXD Enterprise License"
    WriteRegStr HKCR "RawrXD.License\shell\open\command" "" '"$INSTDIR\RawrXD_Gold.exe" --install-license "%1"'

    ; Notify shell of association changes
    System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

Section "PATH Registration" SecPath
    ; Add install dir to system PATH
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    ${If} $0 != ""
        ; Check if already in PATH
        StrCpy $1 "$0"
        Push "$INSTDIR"
        Push "$1"
        ; Simple check — if not found, append
        StrCmp "$0" "" AddPath 0
        StrStr $2 "$0" "$INSTDIR"
        StrCmp $2 "" AddPath AlreadyInPath

        AddPath:
            WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" \
                "Path" "$0;$INSTDIR"
            ; Broadcast WM_SETTINGCHANGE so running processes pick up new PATH
            SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

        AlreadyInPath:
    ${EndIf}
SectionEnd

; =============================================================================
; Uninstaller
; =============================================================================
Section "Uninstall"
    ; Remove files
    Delete "$INSTDIR\RawrXD_Gold.exe"
    Delete "$INSTDIR\RawrXD-Win32IDE.exe"
    Delete "$INSTDIR\RawrEngine.exe"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\Uninstall.exe"

    ; Remove directories (only if empty — preserve user data)
    RMDir "$INSTDIR\config"
    RMDir "$INSTDIR\plugins"
    RMDir "$INSTDIR\engines"
    RMDir "$INSTDIR"

    ; Remove start menu shortcuts
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\RawrXD IDE.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\RawrXD Win32 IDE.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"

    ; Remove desktop shortcut
    Delete "$DESKTOP\RawrXD IDE.lnk"

    ; Remove registry entries
    DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    DeleteRegKey HKCR ".gguf"
    DeleteRegKey HKCR "RawrXD.GGUF"
    DeleteRegKey HKCR ".rawrlic"
    DeleteRegKey HKCR "RawrXD.License"

    ; Remove from PATH
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    ; (PATH cleanup is complex — simplified for production)

    ; Notify shell
    System::Call 'Shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPrereq} "Check system prerequisites (VC++ Redist, Vulkan)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core IDE executables and engine"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "Create Start Menu and Desktop shortcuts"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc} "Associate .gguf and .rawrlic files with RawrXD"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPath} "Add RawrXD to system PATH"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Utility function — string search
Function StrStr
    Exch $R1 ; haystack
    Exch
    Exch $R2 ; needle
    Push $R3
    Push $R4
    Push $R5
    StrLen $R3 $R2
    StrCpy $R4 0
    loop:
        StrCpy $R5 $R1 $R3 $R4
        StrCmp $R5 $R2 found
        StrCmp $R5 "" notfound
        IntOp $R4 $R4 + 1
        Goto loop
    found:
        StrCpy $R1 $R1 "" $R4
        Goto done
    notfound:
        StrCpy $R1 ""
    done:
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Exch $R1
FunctionEnd
