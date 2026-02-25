; BigDaddyG IDE - Custom NSIS Installer Script
; Adds custom pages and post-install configuration

!macro customInit
  ; Check for existing installations
  ReadRegStr $0 HKCU "Software\BigDaddyG IDE" "InstallLocation"
  StrCmp $0 "" new_install
  
  MessageBox MB_YESNO "BigDaddyG IDE is already installed at $0. Do you want to upgrade?" IDYES new_install
  Abort
  
  new_install:
!macroend

!macro customInstall
  ; Create BigDaddyG data directory
  CreateDirectory "$APPDATA\BigDaddyG"
  CreateDirectory "$APPDATA\BigDaddyG\extensions"
  CreateDirectory "$APPDATA\BigDaddyG\diagnostics"
  
  ; Write registry keys
  WriteRegStr HKCU "Software\BigDaddyG IDE" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\BigDaddyG IDE" "Version" "2.0.0"
  
  ; Create .bigdaddyg directory in user home
  CreateDirectory "$PROFILE\.bigdaddyg"
  
  ; Copy default configurations if they don't exist
  IfFileExists "$APPDATA\BigDaddyG\settings.json" skip_default_settings
  CopyFiles "$INSTDIR\electron\default-settings.json" "$APPDATA\BigDaddyG\settings.json"
  skip_default_settings:
  
  ; Register file associations
  WriteRegStr HKCR ".bigdaddy" "" "BigDaddyG.Project"
  WriteRegStr HKCR "BigDaddyG.Project" "" "BigDaddyG Project File"
  WriteRegStr HKCR "BigDaddyG.Project\DefaultIcon" "" "$INSTDIR\${APP_EXECUTABLE_FILENAME},0"
  WriteRegStr HKCR "BigDaddyG.Project\shell\open\command" "" '"$INSTDIR\${APP_EXECUTABLE_FILENAME}" "%1"'
  
  ; Add to PATH (optional)
  ${EnvVarUpdate} $0 "PATH" "A" "HKCU" "$INSTDIR"
  
  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  ; Add to Add/Remove Programs
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "DisplayName" "BigDaddyG IDE"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "DisplayIcon" "$INSTDIR\${APP_EXECUTABLE_FILENAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "DisplayVersion" "2.0.0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "Publisher" "BigDaddyG IDE"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE" "NoRepair" 1
!macroend

!macro customUnInstall
  ; Remove BigDaddyG data directory (ask user first)
  MessageBox MB_YESNO "Do you want to remove all BigDaddyG settings and data? (This will delete your memories, rules, and extensions)" IDYES remove_data IDNO skip_data
  
  remove_data:
    RMDir /r "$APPDATA\BigDaddyG"
    RMDir /r "$PROFILE\.bigdaddyg"
  
  skip_data:
  
  ; Remove from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR"
  
  ; Remove file associations
  DeleteRegKey HKCR ".bigdaddy"
  DeleteRegKey HKCR "BigDaddyG.Project"
  
  ; Remove from Add/Remove Programs
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BigDaddyG IDE"
  
  ; Remove registry keys
  DeleteRegKey HKCU "Software\BigDaddyG IDE"
!macroend

; Custom installer page - Import VS Code/Cursor settings
!macro customPage
  !insertmacro MUI_PAGE_CUSTOM ImportSettingsPage ImportSettingsPageLeave
  
  Function ImportSettingsPage
    !insertmacro MUI_HEADER_TEXT "Import Settings" "Would you like to import your existing VS Code or Cursor settings?"
    
    nsDialogs::Create 1018
    Pop $0
    
    ${NSD_CreateLabel} 0 0 100% 30u "BigDaddyG can automatically import all your settings, extensions, keybindings, and snippets from VS Code or Cursor."
    Pop $1
    
    ${NSD_CreateCheckbox} 10u 40u 100% 12u "Import from VS Code"
    Pop $IMPORT_VSCODE
    
    ${NSD_CreateCheckbox} 10u 60u 100% 12u "Import from Cursor"
    Pop $IMPORT_CURSOR
    
    ${NSD_CreateLabel} 10u 80u 100% 40u "This will copy your settings, install your extensions, and make BigDaddyG feel identical to your current setup."
    Pop $2
    
    nsDialogs::Show
  FunctionEnd
  
  Function ImportSettingsPageLeave
    ; Store user choices for post-install
    ${NSD_GetState} $IMPORT_VSCODE $SHOULD_IMPORT_VSCODE
    ${NSD_GetState} $IMPORT_CURSOR $SHOULD_IMPORT_CURSOR
  FunctionEnd
!macroend

