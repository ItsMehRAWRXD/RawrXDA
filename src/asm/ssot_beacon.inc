; ssot_beacon.inc - ASM bridge constants/API for SSOT ownership checks
OPTION CASEMAP:NONE

RAWR_SSOT_OWNER_NONE      EQU 0
RAWR_SSOT_OWNER_CORE      EQU 1
RAWR_SSOT_OWNER_EXT       EQU 2
RAWR_SSOT_OWNER_AUTO      EQU 3
RAWR_SSOT_OWNER_STUBS     EQU 4
RAWR_SSOT_OWNER_FEATURES  EQU 5

EXTERN rawr_ssot_owner_for_hash : PROC
EXTERN rawr_ssot_active_owner : PROC
