IFNDEF ADVAPI32_INC
ADVAPI32_INC EQU 1

extern RegCreateKeyExA : proc
extern RegOpenKeyExA : proc
extern RegSetValueExA : proc
extern RegQueryValueExA : proc
extern RegCloseKey : proc

HKEY_CURRENT_USER EQU 80000001h
KEY_ALL_ACCESS    EQU 0F003Fh
REG_DWORD         EQU 4
REG_SZ            EQU 1

ENDIF
