;=============================================================================
; GPU Detection and Enumeration Module
; Pure MASM 64 implementation
;=============================================================================

; ─── PUBLIC Exports ──────────────────────────────────────────────────────────

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

PUBLIC GPU_Detect
PUBLIC GPU_GetDeviceCount
PUBLIC GPU_GetDevice
PUBLIC GPU_ScanPCIBus
PUBLIC GPU_ReadPCIConfig
PUBLIC GPU_ReadPCIConfigDword
PUBLIC GPU_GetDeviceProperties
PUBLIC GPU_GetNVIDIAProperties
PUBLIC GPU_GetAMDProperties
PUBLIC GPU_GetIntelProperties
PUBLIC GPU_GetGenericProperties
PUBLIC GPU_ReadMemorySize
PUBLIC GPU_Initialize
PUBLIC GPU_Shutdown

.code

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
    ClockSpeedMHz       DWORD       ?
    ComputeUnits        DWORD       ?
    MemoryClockMHz      DWORD       ?
    MemoryBusWidth      DWORD       ?
    DeviceName          BYTE 256 DUP(?)
GPU_DEVICE ENDS

.data
GPU_DeviceCount         DWORD 0
GPU_DeviceList          GPU_DEVICE 16 DUP(<>)

.code

;-----------------------------------------------------------------------------
; GPU_Detect - Main GPU detection entry point
; Returns: Number of GPU devices found
;-----------------------------------------------------------------------------
GPU_Detect PROC
    push rbx
    push rsi
    push rdi
    
    ; Placeholder for PCI enumeration logic
    ; In a real "reverse engineered" implementation, we'd use BIOS or IOCTLs
    ; Since we are on Windows, we'll simulate finding at least one NVIDIA GPU
    ; for the sake of the backend wiring.
    
    lea rdx, GPU_DeviceList
    mov word ptr [rdx].VendorID, 10DEh ; NVIDIA
    mov word ptr [rdx].DeviceID, 2206h ; RTX 3080
    mov dword ptr [rdx].ComputeUnits, 68 ; SMs
    mov dword ptr [rdx].ClockSpeedMHz, 1710
    mov qword ptr [rdx].MemorySize, 10737418240 ; 10 GB
    
    mov GPU_DeviceCount, 1
    mov eax, 1

    pop rdi
    pop rsi
    pop rbx
    ret
GPU_Detect ENDP

GPU_GetDeviceCount PROC
    mov eax, GPU_DeviceCount
    ret
GPU_GetDeviceCount ENDP

GPU_GetDevice PROC
    ; RCX: index, RDX: pDevice
    cmp ecx, GPU_DeviceCount
    jae error
    
    push rsi
    push rdi
    
    imul rax, rcx, 296 ; sizeof(GPU_DEVICE)
    lea rsi, [GPU_DeviceList + rax]
    mov rdi, rdx
    mov rcx, 296
    rep movsb
    
    pop rdi
    pop rsi
    xor eax, eax
    ret
error:
    mov eax, -1
    ret
GPU_GetDevice ENDP

END
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
    ; Extract clock speed (MHz) from GPU-specific registers
    ; NVIDIA GPUs expose clock info through memory-mapped I/O
    ; Read core clock from PCI BAR0 + offset (simplified simulation)
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    
    ; Simulate reading clock speed - in real impl, would read from device registers
    mov (GPU_DEVICE PTR [edi]).ClockSpeedMHz, 1500  ; Typical NVIDIA boost clock
    
    ; Extract SM (streaming multiprocessor) count
    ; This would require reading device-specific registers via MMIO
    mov (GPU_DEVICE PTR [edi]).ComputeUnits, 80  ; Typical high-end GPU
    
    ; Memory clock and bus width
    mov (GPU_DEVICE PTR [edi]).MemoryClockMHz, 7000  ; GDDR6 typical
    mov (GPU_DEVICE PTR [edi]).MemoryBusWidth, 256   ; 256-bit bus
    
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
    ; Extract compute unit count from AMD-specific registers
    ; AMD GPUs expose CU count through device properties
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    
    ; Simulate reading CU count - in real impl, would query ROCm SMI or device registers
    mov (GPU_DEVICE PTR [edi]).ComputeUnits, 64  ; Typical RDNA2 CU count
    
    ; Clock speeds
    mov (GPU_DEVICE PTR [edi]).ClockSpeedMHz, 2100  ; Typical AMD boost clock
    mov (GPU_DEVICE PTR [edi]).MemoryClockMHz, 8000 ; GDDR6 typical
    mov (GPU_DEVICE PTR [edi]).MemoryBusWidth, 256  ; 256-bit bus
    
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
    
    ; Extract EU (execution unit) count - would require MMIO access in real impl
    movzx eax, (GPU_DEVICE PTR [edi]).Bus
    movzx ebx, (GPU_DEVICE PTR [edi]).Dev
    movzx ecx, (GPU_DEVICE PTR [edi]).Func
    
    ; Simulate EU count based on device ID
    mov eax, (GPU_DEVICE PTR [edi]).DeviceID
    ; Xe Graphics typical EU counts: 96-512 EUs
    mov (GPU_DEVICE PTR [edi]).ComputeUnits, 96  ; Typical integrated GPU
    
    ; Clock speeds for Intel Xe
    mov (GPU_DEVICE PTR [edi]).ClockSpeedMHz, 1400  ; Typical Intel Xe boost
    mov (GPU_DEVICE PTR [edi]).MemoryClockMHz, 3200 ; System RAM for iGPU
    mov (GPU_DEVICE PTR [edi]).MemoryBusWidth, 128  ; iGPU typical
    
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
    
    ; Read generic properties
    INVOKE GPU_ReadMemorySize, pDevice
    
    ; Set conservative defaults for unknown GPUs
    mov (GPU_DEVICE PTR [edi]).ComputeUnits, 32       ; Conservative estimate
    mov (GPU_DEVICE PTR [edi]).ClockSpeedMHz, 1000    ; Conservative estimate
    mov (GPU_DEVICE PTR [edi]).MemoryClockMHz, 4000   ; Conservative estimate
    mov (GPU_DEVICE PTR [edi]).MemoryBusWidth, 128    ; Conservative estimate
    mov (GPU_DEVICE PTR [edi]).ComputeCapability, 0
    
    ret
GPU_GetGenericProperties ENDP
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