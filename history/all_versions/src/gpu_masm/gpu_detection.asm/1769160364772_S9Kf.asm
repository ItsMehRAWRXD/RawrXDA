;=============================================================================
; GPU Detection and Enumeration Module
; Pure MASM 64 implementation - no external dependencies
;=============================================================================

.686
.XMM
.MODEL FLAT, C

;=============================================================================
; Include Files
;=============================================================================

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

;=============================================================================
; Constants and Definitions
;=============================================================================

; PCI Configuration Space Offsets
PCI_VENDOR_ID           EQU 00h
PCI_DEVICE_ID           EQU 02h
PCI_CLASS_CODE          EQU 09h
PCI_HEADER_TYPE         EQU 0Eh
PCI_BASE_ADDRESS_0      EQU 10h
PCI_INTERRUPT_LINE      EQU 3Ch

; GPU Vendor IDs
NVIDIA_VENDOR_ID        EQU 10DEh
AMD_VENDOR_ID           EQU 1002h
INTEL_VENDOR_ID         EQU 8086h

; GPU Class Codes
GPU_CLASS_CODE          EQU 030000h  ; VGA compatible controller
GPU_CLASS_CODE_3D       EQU 030200h  ; 3D controller

; Maximum PCI buses to scan
MAX_PCI_BUS             EQU 256
MAX_PCI_DEV             EQU 32
MAX_PCI_FUNC            EQU 8

; GPU Device Structure
GPU_DEVICE STRUCT
    VendorID            WORD        ?
    DeviceID            WORD        ?
    ClassCode           DWORD       ?
    Bus                 BYTE        ?
    Dev                 BYTE        ?
    Func                BYTE        ?
    MemorySize          QWORD       ?
    ComputeCapability   DWORD       ?
    DeviceName          BYTE 256 DUP(?)
GPU_DEVICE ENDS

;=============================================================================
; Data Section
;=============================================================================

.DATA

; Global GPU device list
GPU_DeviceCount         DWORD 0
GPU_DeviceList          GPU_DEVICE 16 DUP(<>)  ; Support up to 16 GPUs

; Error messages
szErrorPCIAccess        DB 'Error: Cannot access PCI configuration space', 0
szErrorNoGPU            DB 'Error: No GPU devices found', 0
szErrorUnknownVendor    DB 'Error: Unknown GPU vendor', 0

; Vendor strings
szNVIDIA                DB 'NVIDIA Corporation', 0
szAMD                   DB 'Advanced Micro Devices, Inc.', 0
szIntel                 DB 'Intel Corporation', 0
szUnknown               DB 'Unknown Vendor', 0

;=============================================================================
; Code Section
;=============================================================================

.CODE

;-----------------------------------------------------------------------------
; GPU_Detect - Main GPU detection entry point
; Returns: Number of GPU devices found
;-----------------------------------------------------------------------------
GPU_Detect PROC
    LOCAL dwCount:DWORD
    
    ; Initialize device count
    mov dwCount, 0
    
    ; Scan PCI bus for GPU devices
    INVOKE GPU_ScanPCIBus, ADDR GPU_DeviceList, ADDR dwCount
    
    ; Store global device count
    mov eax, dwCount
    mov GPU_DeviceCount, eax
    
    ; Return device count
    mov eax, dwCount
    ret
GPU_Detect ENDP

;-----------------------------------------------------------------------------
; GPU_ScanPCIBus - Scan PCI bus for GPU devices
; Parameters:
;   pDeviceList - Pointer to GPU_DEVICE array
;   pCount      - Pointer to count variable
;-----------------------------------------------------------------------------
GPU_ScanPCIBus PROC USES ebx ecx edx esi edi,
    pDeviceList:PTR GPU_DEVICE,
    pCount:PTR DWORD
    
    LOCAL dwBus:DWORD
    LOCAL dwDev:DWORD
    LOCAL dwFunc:DWORD
    LOCAL dwCount:DWORD
    LOCAL bHeaderType:BYTE
    
    ; Initialize count
    mov eax, pCount
    mov dword ptr [eax], 0
    mov dwCount, 0
    
    ; Scan all PCI buses
    mov dwBus, 0
    .WHILE dwBus < MAX_PCI_BUS
        mov dwDev, 0
        .WHILE dwDev < MAX_PCI_DEV
            mov dwFunc, 0
            
            ; Read vendor ID
            INVOKE GPU_ReadPCIConfig, dwBus, dwDev, dwFunc, PCI_VENDOR_ID
            movzx ebx, ax
            
            ; Check if device exists
            .IF bx == 0FFFFh
                ; No device, skip to next
                jmp @next_dev
            .ENDIF
            
            ; Read class code
            INVOKE GPU_ReadPCIConfigDword, dwBus, dwDev, dwFunc, PCI_CLASS_CODE
            mov ecx, eax
            and ecx, 0FFFFFFh  ; Mask class code
            
            ; Check if this is a GPU device
            .IF ecx == GPU_CLASS_CODE || ecx == GPU_CLASS_CODE_3D
                ; Found GPU device
                .IF dwCount < 16  ; Don't exceed array size
                    ; Fill device structure
                    mov edi, pDeviceList
                    mov eax, dwCount
                    mov ecx, SIZEOF GPU_DEVICE
                    mul ecx
                    add edi, eax
                    
                    ; Store vendor ID
                    mov (GPU_DEVICE PTR [edi]).VendorID, bx
                    
                    ; Store device ID
                    INVOKE GPU_ReadPCIConfig, dwBus, dwDev, dwFunc, PCI_DEVICE_ID
                    movzx ebx, ax
                    mov (GPU_DEVICE PTR [edi]).DeviceID, bx
                    
                    ; Store class code
                    mov (GPU_DEVICE PTR [edi]).ClassCode, ecx
                    
                    ; Store bus, dev, func
                    mov eax, dwBus
                    mov (GPU_DEVICE PTR [edi]).Bus, al
                    mov eax, dwDev
                    mov (GPU_DEVICE PTR [edi]).Dev, al
                    mov eax, dwFunc
                    mov (GPU_DEVICE PTR [edi]).Func, al
                    
                    ; Get device properties
                    INVOKE GPU_GetDeviceProperties, edi
                    
                    ; Increment count
                    inc dwCount
                .ENDIF
            .ENDIF
            
            ; Check for multi-function devices
            INVOKE GPU_ReadPCIConfig, dwBus, dwDev, dwFunc, PCI_HEADER_TYPE
            mov bHeaderType, al
            test al, 80h  ; Check multi-function bit
            .IF ZERO?
                ; Single function device, skip to next
                jmp @next_dev
            .ENDIF
            
            ; Try next function
            inc dwFunc
            .IF dwFunc < MAX_PCI_FUNC
                jmp @check_func
            .ENDIF
            
@next_dev:
            inc dwDev
        .ENDW
        
        inc dwBus
    .ENDW
    
    ; Store final count
    mov eax, pCount
    mov ebx, dwCount
    mov dword ptr [eax], ebx
    
    mov eax, dwCount
    ret
GPU_ScanPCIBus ENDP

;-----------------------------------------------------------------------------
; GPU_ReadPCIConfig - Read PCI configuration space word
; Parameters:
;   dwBus  - PCI bus number
;   dwDev  - PCI device number
;   dwFunc - PCI function number
;   dwReg  - Register offset
; Returns: Value in AX
;-----------------------------------------------------------------------------
GPU_ReadPCIConfig PROC USES ebx ecx edx,
    dwBus:DWORD,
    dwDev:DWORD,
    dwFunc:DWORD,
    dwReg:DWORD
    
    ; Build PCI configuration address
    mov eax, dwBus
    shl eax, 16
    mov ebx, dwDev
    shl ebx, 11
    or eax, ebx
    mov ebx, dwFunc
    shl ebx, 8
    or eax, ebx
    mov ebx, dwReg
    and ebx, 0FCh  ; Align to dword boundary
    or eax, ebx
    or eax, 80000000h  ; Enable configuration space
    
    ; Write address to configuration space
    mov dx, 0CF8h
    out dx, eax
    
    ; Read data from configuration space
    mov dx, 0CFCh
    in eax, dx
    
    ; Return requested word
    mov ecx, dwReg
    and ecx, 2
    .IF ecx == 0
        ; Return lower word
        and eax, 0FFFFh
    .ELSE
        ; Return upper word
        shr eax, 16
    .ENDIF
    
    ret
GPU_ReadPCIConfig ENDP

;-----------------------------------------------------------------------------
; GPU_ReadPCIConfigDword - Read PCI configuration space dword
; Parameters:
;   dwBus  - PCI bus number
;   dwDev  - PCI device number
;   dwFunc - PCI function number
;   dwReg  - Register offset
; Returns: Value in EAX
;-----------------------------------------------------------------------------
GPU_ReadPCIConfigDword PROC USES ebx ecx edx,
    dwBus:DWORD,
    dwDev:DWORD,
    dwFunc:DWORD,
    dwReg:DWORD
    
    ; Build PCI configuration address
    mov eax, dwBus
    shl eax, 16
    mov ebx, dwDev
    shl ebx, 11
    or eax, ebx
    mov ebx, dwFunc
    shl ebx, 8
    or eax, ebx
    mov ebx, dwReg
    and ebx, 0FCh  ; Align to dword boundary
    or eax, ebx
    or eax, 80000000h  ; Enable configuration space
    
    ; Write address to configuration space
    mov dx, 0CF8h
    out dx, eax
    
    ; Read data from configuration space
    mov dx, 0CFCh
    in eax, dx
    
    ret
GPU_ReadPCIConfigDword ENDP

;-----------------------------------------------------------------------------
; GPU_GetDeviceProperties - Get detailed GPU properties
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_GetDeviceProperties PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    LOCAL dwVendorID:WORD
    LOCAL dwDeviceID:WORD
    
    mov edi, pDevice
    
    ; Get vendor ID
    movzx eax, (GPU_DEVICE PTR [edi]).VendorID
    mov dwVendorID, ax
    
    ; Get device ID
    movzx eax, (GPU_DEVICE PTR [edi]).DeviceID
    mov dwDeviceID, ax
    
    ; Determine vendor and set properties
    mov ax, dwVendorID
    .IF ax == NVIDIA_VENDOR_ID
        ; NVIDIA GPU
        INVOKE GPU_GetNVIDIAProperties, edi
        
    .ELSEIF ax == AMD_VENDOR_ID
        ; AMD GPU
        INVOKE GPU_GetAMDProperties, edi
        
    .ELSEIF ax == INTEL_VENDOR_ID
        ; Intel GPU
        INVOKE GPU_GetIntelProperties, edi
        
    .ELSE
        ; Unknown vendor
        INVOKE GPU_GetGenericProperties, edi
        
    .ENDIF
    
    ret
GPU_GetDeviceProperties ENDP

;-----------------------------------------------------------------------------
; GPU_GetNVIDIAProperties - Get NVIDIA-specific properties
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_GetNVIDIAProperties PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    mov edi, pDevice
    
    ; Set vendor name
    lea esi, szNVIDIA
    lea edi, (GPU_DEVICE PTR [edi]).DeviceName
    INVOKE lstrcpy, edi, esi
    
    ; Read NVIDIA-specific registers
    INVOKE GPU_ReadMemorySize, pDevice
    ; Map device ID to compute capability (simplified, real mapping would use a lookup table)
    mov eax, (GPU_DEVICE PTR [edi]).DeviceID
    cmp eax, 0x1B80
    je nvidia_pascal
    cmp eax, 0x2204
    je nvidia_turing
    cmp eax, 0x2504
    je nvidia_ampere
    ; Default: unknown, set to 0
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0
    jmp nvidia_cc_done
nvidia_pascal:
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x60
    jmp nvidia_cc_done
nvidia_turing:
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x70
    jmp nvidia_cc_done
nvidia_ampere:
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x80
nvidia_cc_done:
    ; Extract clock speed (MHz) from PCI config if available (example: offset 0x10C)
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    mov edx, 0x10C
    INVOKE GPU_ReadPCIConfig, eax, ebx, ecx, edx
    mov (GPU_DEVICE PTR [edi]).ClockSpeed, ax
    
    ret
GPU_GetNVIDIAProperties ENDP

;-----------------------------------------------------------------------------
; GPU_GetAMDProperties - Get AMD-specific properties
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_GetAMDProperties PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    mov edi, pDevice
    
    ; Set vendor name
    lea esi, szAMD
    lea edi, (GPU_DEVICE PTR [edi]).DeviceName
    INVOKE lstrcpy, edi, esi
    
    ; Read AMD-specific registers
    INVOKE GPU_ReadMemorySize, pDevice
    ; Set GCN version based on device ID (simplified)
    mov eax, (GPU_DEVICE PTR [edi]).DeviceID
    cmp eax, 0x67DF
    je amd_gcn3
    cmp eax, 0x731F
    je amd_rdna
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0
    jmp amd_cc_done
amd_gcn3:
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x300
    jmp amd_cc_done
amd_rdna:
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x400
amd_cc_done:
    ; Extract CU count from PCI config (example: offset 0x104)
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    mov edx, 0x104
    INVOKE GPU_ReadPCIConfig, eax, ebx, ecx, edx
    mov (GPU_DEVICE PTR [edi]).CUCount, ax
    
    ret
GPU_GetAMDProperties ENDP

;-----------------------------------------------------------------------------
; GPU_GetIntelProperties - Get Intel-specific properties
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_GetIntelProperties PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    mov edi, pDevice
    
    ; Set vendor name
    lea esi, szIntel
    lea edi, (GPU_DEVICE PTR [edi]).DeviceName
    INVOKE lstrcpy, edi, esi
    
    ; Read Intel-specific registers
    INVOKE GPU_ReadMemorySize, pDevice
    ; Extract EU count (example: offset 0x94)
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    mov edx, 0x94
    INVOKE GPU_ReadPCIConfig, eax, ebx, ecx, edx
    mov (GPU_DEVICE PTR [edi]).EUCount, ax
    ; Extract max frequency (example: offset 0x198)
    mov edx, 0x198
    INVOKE GPU_ReadPCIConfig, eax, ebx, ecx, edx
    mov (GPU_DEVICE PTR [edi]).MaxFrequency, ax
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0x10
    
    ret
GPU_GetIntelProperties ENDP

;-----------------------------------------------------------------------------
; GPU_GetGenericProperties - Get generic GPU properties
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_GetGenericProperties PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    mov edi, pDevice
    
    ; Set generic vendor name
    lea esi, szUnknown
    lea edi, (GPU_DEVICE PTR [edi]).DeviceName
    INVOKE lstrcpy, edi, esi
    
    ; Read memory size from BAR registers
    INVOKE GPU_ReadMemorySize, pDevice
    
    ret
GPU_GetGenericProperties ENDP

;-----------------------------------------------------------------------------
; GPU_ReadMemorySize - Read GPU memory size from BAR registers
; Parameters:
;   pDevice - Pointer to GPU_DEVICE structure
;-----------------------------------------------------------------------------
GPU_ReadMemorySize PROC USES ebx ecx edx esi edi,
    pDevice:PTR GPU_DEVICE
    
    mov edi, pDevice
    
    ; Read BAR0 (Base Address Register 0)
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    mov edx, PCI_BASE_ADDRESS_0
    INVOKE GPU_ReadPCIConfigDword, eax, ebx, ecx, edx
    mov esi, eax
    ; Mask lower 4 bits (flags)
    and esi, 0FFFFFFF0h
    ; Write all 1s to BAR0 to get size mask
    mov eax, 0FFFFFFFFh
    INVOKE GPU_WritePCIConfigDword, eax, ebx, ecx, edx, eax
    INVOKE GPU_ReadPCIConfigDword, eax, ebx, ecx, edx
    not eax
    and eax, 0FFFFFFF0h
    mov (GPU_DEVICE PTR [edi]).MemorySize, eax
    ; Restore original BAR0 value
    mov eax, esi
    INVOKE GPU_WritePCIConfigDword, eax, ebx, ecx, edx, eax
    
    ret
GPU_ReadMemorySize ENDP

;-----------------------------------------------------------------------------
; GPU_GetDeviceCount - Get number of detected GPU devices
; Returns: Device count in EAX
;-----------------------------------------------------------------------------
GPU_GetDeviceCount PROC
    mov eax, GPU_DeviceCount
    ret
GPU_GetDeviceCount ENDP

;-----------------------------------------------------------------------------
; GPU_GetDevice - Get GPU device by index
; Parameters:
;   dwIndex - Device index
; Returns: Pointer to GPU_DEVICE structure in EAX
;-----------------------------------------------------------------------------
GPU_GetDevice PROC,
    dwIndex:DWORD
    
    mov eax, dwIndex
    .IF eax >= GPU_DeviceCount
        xor eax, eax  ; Return NULL if index out of range
        ret
    .ENDIF
    
    ; Calculate device structure pointer
    mov ecx, SIZEOF GPU_DEVICE
    mul ecx
    lea eax, GPU_DeviceList
    add eax, edx  ; Add high part of multiplication
    add eax, eax  ; Add low part (already in eax)
    
    ret
GPU_GetDevice ENDP

;-----------------------------------------------------------------------------
; GPU_Initialize - Initialize GPU subsystem
; Returns: TRUE if successful, FALSE otherwise
;-----------------------------------------------------------------------------
GPU_Initialize PROC
    LOCAL dwCount:DWORD
    
    ; Detect GPUs
    INVOKE GPU_Detect
    mov dwCount, eax
    
    ; Check if any GPUs found
    .IF dwCount == 0
        mov eax, FALSE
    .ELSE
        mov eax, TRUE
    .ENDIF
    
    ret
GPU_Initialize ENDP

;-----------------------------------------------------------------------------
; GPU_Shutdown - Shutdown GPU subsystem
;-----------------------------------------------------------------------------
GPU_Shutdown PROC
    ; Cleanup GPU resources
    mov GPU_DeviceCount, 0
    ; Optionally zero out GPU_DeviceList
    lea edi, GPU_DeviceList
    mov ecx, SIZEOF GPU_DEVICE * 16 / 4
    xor eax, eax
    rep stosd
    
    ret
GPU_Shutdown ENDP

;-----------------------------------------------------------------------------
; DllMain - DLL entry point
;-----------------------------------------------------------------------------
DllMain PROC hInstance:HINSTANCE, dwReason:DWORD, lpReserved:DWORD
    .IF dwReason == DLL_PROCESS_ATTACH
        ; Initialize GPU subsystem
        INVOKE GPU_Initialize
    .ELSEIF dwReason == DLL_PROCESS_DETACH
        ; Shutdown GPU subsystem
        INVOKE GPU_Shutdown
    .ENDIF
    
    mov eax, TRUE
    ret
DllMain ENDP

END DllMain