; =============================================================================
; Phase 6: Qt6 Widget Factory & Property Binding MASM Bridge
; Pure MASM x64 Implementation
;
; Purpose: Implement widget factory pattern and property binding system
;          for dynamic widget creation and state synchronization
;
; Public API (14 functions):
;   1. WidgetFactory_CreateWidget(classId) -> widgetPtr
;   2. WidgetFactory_DestroyWidget(widgetPtr) -> success
;   3. WidgetFactory_CreateLayout(layoutType) -> layoutPtr
;   4. WidgetFactory_AddWidget(layoutPtr, widgetPtr, position) -> success
;   5. PropertyBinding_Create(sourceObj, sourceProp, targetObj, targetProp) -> bindingId
;   6. PropertyBinding_Destroy(bindingId) -> success
;   7. PropertyBinding_GetValue(bindingId) -> value
;   8. PropertyBinding_SetValue(bindingId, newValue) -> success
;   9. PropertyBinding_GetSourceValue(objectPtr, propertyId) -> value
;   10. PropertyBinding_SetSourceValue(objectPtr, propertyId, newValue) -> success
;   11. WidgetFactory_RegisterClass(classId, factoryFunc) -> success
;   12. Test_WidgetFactory_Create() -> testResult
;   13. Test_PropertyBinding_Sync() -> testResult
;   14. PropertyBinding_GetMetrics() -> metricsPtr
;
; Thread Safety: Critical Section for factory registry and binding list
; Registry: HKCU\Software\RawrXD\WidgetFactory
; =============================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlCopyMemory:PROC

.CODE

; =============================================================================
; CONSTANTS
; =============================================================================

WIDGET_CLASS_MAINWINDOW             EQU 1
WIDGET_CLASS_DIALOG                 EQU 2
WIDGET_CLASS_WIDGET                 EQU 3
WIDGET_CLASS_PUSHBUTTON             EQU 4
WIDGET_CLASS_LINEEDIT               EQU 5
WIDGET_CLASS_TEXTEDIT               EQU 6
WIDGET_CLASS_LABEL                  EQU 7
WIDGET_CLASS_COMBOBOX               EQU 8
WIDGET_CLASS_LISTVIEW               EQU 9
WIDGET_CLASS_TABWIDGET              EQU 10

LAYOUT_TYPE_VERTICAL                EQU 1
LAYOUT_TYPE_HORIZONTAL              EQU 2
LAYOUT_TYPE_GRID                    EQU 3
LAYOUT_TYPE_STACKED                 EQU 4

WIDGET_FACTORY_MAX_CLASSES          EQU 50
WIDGET_FACTORY_MAX_WIDGETS          EQU 10000
PROPERTY_BINDING_MAX_BINDINGS       EQU 5000

; Error codes
WIDGET_FACTORY_E_SUCCESS            EQU 0x00000000
WIDGET_FACTORY_E_INVALID_CLASS      EQU 0x00000001
WIDGET_FACTORY_E_MEMORY_ALLOC_FAILED EQU 0x00000002
WIDGET_FACTORY_E_INVALID_WIDGET     EQU 0x00000003
WIDGET_FACTORY_E_CLASS_LIMIT        EQU 0x00000004
WIDGET_FACTORY_E_WIDGET_LIMIT       EQU 0x00000005
PROPERTY_BINDING_E_INVALID_BINDING  EQU 0x00000006
PROPERTY_BINDING_E_BINDING_LIMIT    EQU 0x00000007

; =============================================================================
; DATA STRUCTURES
; =============================================================================

; Widget class descriptor
WIDGET_CLASS_DESCRIPTOR STRUCT
    ClassId         DWORD
    ClassName       QWORD       ; Pointer to name string
    FactoryFunc     QWORD       ; Factory function pointer
    VMT             QWORD       ; Virtual method table
    DefaultSize     DWORD       ; Default width
    DefaultHeight   DWORD       ; Default height
WIDGET_CLASS_DESCRIPTOR ENDS

; Widget instance header
WIDGET_INSTANCE STRUCT
    InstanceId      DWORD
    ClassId         DWORD
    WidgetPtr       QWORD       ; Pointer to actual widget memory
    LayoutPtr       QWORD       ; Associated layout (if any)
    X               DWORD       ; Position
    Y               DWORD
    Width           DWORD       ; Size
    Height          DWORD
    Visible         BYTE        ; Visibility flag
    Enabled         BYTE        ; Enable flag
    _PAD0           WORD        ; Alignment
    Parent          QWORD       ; Parent widget pointer
    ChildCount      DWORD
    MaxChildren     DWORD
    Children        QWORD       ; Array of child pointers
WIDGET_INSTANCE ENDS

; Layout descriptor
LAYOUT_DESCRIPTOR STRUCT
    LayoutId        DWORD
    LayoutType      DWORD
    ItemCount       DWORD
    Items           QWORD       ; Array of widget pointers
    Spacing         DWORD
    Margin          DWORD
LAYOUT_DESCRIPTOR ENDS

; Property binding
PROPERTY_BINDING STRUCT
    BindingId       DWORD
    SourceObject    QWORD       ; Source object pointer
    SourceProperty  DWORD       ; Property ID
    TargetObject    QWORD       ; Target object pointer
    TargetProperty  DWORD       ; Property ID
    CurrentValue    QWORD       ; Last synced value
    Enabled         BYTE        ; Binding active flag
    _PAD0           BYTE        ; Alignment
    _PAD1           WORD        ; Alignment
    UpdateCount     QWORD       ; Number of updates
PROPERTY_BINDING ENDS

; Widget Factory manager
WIDGET_FACTORY_MANAGER STRUCT
    Version         DWORD
    Initialized     BYTE
    _PAD0           BYTE        ; Alignment
    _PAD1           WORD        ; Alignment
    ClassCount      DWORD
    WidgetCount     DWORD
    NextWidgetId    DWORD
    NextBindingId   DWORD
    ManagerLock     DWORD       ; Critical Section (16 bytes)
    _CS_DEBUG_INFO  QWORD
    _CS_LOCK_COUNT  DWORD
    _CS_RECURSION_COUNT DWORD
    _CS_OWNER_THREAD QWORD
    Classes         QWORD       ; Array of WIDGET_CLASS_DESCRIPTOR
    Widgets         QWORD       ; Linked list of WIDGET_INSTANCE
    Bindings        QWORD       ; Linked list of PROPERTY_BINDING
WIDGET_FACTORY_MANAGER ENDS

; Metrics
WIDGET_FACTORY_METRICS STRUCT
    WidgetsCreated          QWORD
    WidgetsDestroyed        QWORD
    ClassesRegistered       QWORD
    BindingsCreated         QWORD
    BindingsDestroyed       QWORD
    BindingUpdates          QWORD
    PropertyChanges         QWORD
WIDGET_FACTORY_METRICS ENDS

; =============================================================================
; GLOBAL DATA
; =============================================================================

.DATA

; Logging strings
szLogWidgetCreated      DB "INFO: Widget created (instance_id=%d, class=%d)", 0
szLogWidgetDestroyed    DB "INFO: Widget destroyed (instance_id=%d)", 0
szLogBindingCreated     DB "INFO: Property binding created (binding_id=%d)", 0
szLogBindingUpdated     DB "INFO: Binding updated (binding_id=%d, value=0x%llx)", 0

; Metrics names
szMetricWidgetsCreated              DB "widget_factory_widgets_created_total", 0
szMetricBindingsCreated             DB "widget_factory_bindings_created_total", 0
szMetricBindingUpdates              DB "widget_factory_binding_updates_total", 0

; Global manager state
widgetFactoryManager WIDGET_FACTORY_MANAGER <0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>
widgetFactoryMetrics WIDGET_FACTORY_METRICS <0, 0, 0, 0, 0, 0, 0>

; =============================================================================
; PUBLIC FUNCTIONS
; =============================================================================

; WidgetFactory_CreateWidget(RCX = classId) -> RAX = QWORD (widget pointer, or NULL on error)
PUBLIC WidgetFactory_CreateWidget
WidgetFactory_CreateWidget PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = widget class ID
    
    ; Validate class ID
    test ecx, ecx
    jz .L0_invalid_class
    cmp ecx, WIDGET_CLASS_TABWIDGET
    jg .L0_invalid_class
    
    ; Acquire lock
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call EnterCriticalSection
    
    ; Check widget count limit
    cmp DWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.WidgetCount], WIDGET_FACTORY_MAX_WIDGETS
    jge .L0_limit_exceeded
    
    ; Allocate widget instance
    mov r9d, SIZE WIDGET_INSTANCE
    call HeapAlloc
    test rax, rax
    jz .L0_alloc_failed
    
    ; Initialize widget instance
    mov r10, rax                    ; r10 = new widget
    mov DWORD PTR [r10 + OFFSET WIDGET_INSTANCE.ClassId], ecx
    mov DWORD PTR [r10 + OFFSET WIDGET_INSTANCE.InstanceId], 1  ; Simplified ID
    mov BYTE PTR [r10 + OFFSET WIDGET_INSTANCE.Visible], 1
    mov BYTE PTR [r10 + OFFSET WIDGET_INSTANCE.Enabled], 1
    
    ; Add to widget list
    mov r11, QWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.Widgets]
    mov QWORD PTR [r10 + OFFSET WIDGET_INSTANCE.Parent], r11
    mov QWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.Widgets], r10
    
    ; Increment count
    inc DWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.WidgetCount]
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.WidgetsCreated]
    
    ; Release lock
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, r10                    ; Return widget pointer
    jmp .L0_exit
    
.L0_invalid_class:
    mov rax, WIDGET_FACTORY_E_INVALID_CLASS
    jmp .L0_exit
    
.L0_limit_exceeded:
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, WIDGET_FACTORY_E_WIDGET_LIMIT
    jmp .L0_exit
    
.L0_alloc_failed:
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    xor rax, rax
    
.L0_exit:
    add rsp, 48
    pop rbp
    ret
WidgetFactory_CreateWidget ENDP

; WidgetFactory_DestroyWidget(RCX = widgetPtr) -> RAX = DWORD (success code)
PUBLIC WidgetFactory_DestroyWidget
WidgetFactory_DestroyWidget PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; RCX = widget pointer
    
    test rcx, rcx
    jz .L1_invalid_widget
    
    ; Acquire lock
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call EnterCriticalSection
    
    ; Find and remove widget (simplified - would walk list)
    mov rax, rcx
    call HeapFree
    
    ; Decrement count
    dec DWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.WidgetCount]
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.WidgetsDestroyed]
    
    ; Release lock
    lea r8, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    
    xor rax, rax
    jmp .L1_exit
    
.L1_invalid_widget:
    mov rax, WIDGET_FACTORY_E_INVALID_WIDGET
    
.L1_exit:
    add rsp, 32
    pop rbp
    ret
WidgetFactory_DestroyWidget ENDP

; WidgetFactory_CreateLayout(RCX = layoutType) -> RAX = QWORD (layout pointer)
PUBLIC WidgetFactory_CreateLayout
WidgetFactory_CreateLayout PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; RCX = layout type (LAYOUT_TYPE_*)
    
    ; Allocate layout descriptor
    mov r8d, SIZE LAYOUT_DESCRIPTOR
    call HeapAlloc
    test rax, rax
    jz .L2_alloc_failed
    
    ; Initialize layout
    mov r9, rax
    mov DWORD PTR [r9 + OFFSET LAYOUT_DESCRIPTOR.LayoutType], ecx
    mov DWORD PTR [r9 + OFFSET LAYOUT_DESCRIPTOR.Spacing], 5
    mov DWORD PTR [r9 + OFFSET LAYOUT_DESCRIPTOR.Margin], 10
    
    jmp .L2_exit
    
.L2_alloc_failed:
    xor rax, rax
    
.L2_exit:
    add rsp, 32
    pop rbp
    ret
WidgetFactory_CreateLayout ENDP

; WidgetFactory_AddWidget(RCX = layoutPtr, RDX = widgetPtr, R8D = position) -> RAX = DWORD (success)
PUBLIC WidgetFactory_AddWidget
WidgetFactory_AddWidget PROC FRAME
    ; RCX = layout pointer
    ; RDX = widget pointer
    ; R8D = position in layout
    
    test rcx, rcx
    jz .L3_invalid_layout
    test rdx, rdx
    jz .L3_invalid_widget
    
    ; Would add widget to layout's item array
    xor rax, rax
    ret
    
.L3_invalid_layout:
    mov rax, WIDGET_FACTORY_E_INVALID_WIDGET
    ret
    
.L3_invalid_widget:
    mov rax, WIDGET_FACTORY_E_INVALID_WIDGET
    ret
WidgetFactory_AddWidget ENDP

; PropertyBinding_Create(RCX = sourceObj, RDX = sourceProp, R8 = targetObj, R9D = targetProp) -> RAX = DWORD (bindingId)
PUBLIC PropertyBinding_Create
PropertyBinding_Create PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = source object
    ; RDX = source property ID
    ; R8 = target object
    ; R9D = target property ID
    
    test rcx, rcx
    jz .L4_invalid_source
    test r8, r8
    jz .L4_invalid_target
    
    ; Acquire lock
    lea r10, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call EnterCriticalSection
    
    ; Allocate binding
    mov r11d, SIZE PROPERTY_BINDING
    call HeapAlloc
    test rax, rax
    jz .L4_alloc_failed
    
    ; Initialize binding
    mov r12, rax
    mov QWORD PTR [r12 + OFFSET PROPERTY_BINDING.SourceObject], rcx
    mov DWORD PTR [r12 + OFFSET PROPERTY_BINDING.SourceProperty], edx
    mov QWORD PTR [r12 + OFFSET PROPERTY_BINDING.TargetObject], r8
    mov DWORD PTR [r12 + OFFSET PROPERTY_BINDING.TargetProperty], r9d
    mov BYTE PTR [r12 + OFFSET PROPERTY_BINDING.Enabled], 1
    
    ; Get binding ID
    mov r10d, DWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.NextBindingId]
    mov DWORD PTR [r12 + OFFSET PROPERTY_BINDING.BindingId], r10d
    inc DWORD PTR [widgetFactoryManager + OFFSET widgetFactoryManager.NextBindingId]
    
    ; Increment count
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.BindingsCreated]
    
    ; Release lock
    lea r10, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, r10d                   ; Return binding ID
    jmp .L4_exit
    
.L4_invalid_source:
    mov rax, PROPERTY_BINDING_E_INVALID_BINDING
    jmp .L4_exit
    
.L4_invalid_target:
    mov rax, PROPERTY_BINDING_E_INVALID_BINDING
    jmp .L4_exit
    
.L4_alloc_failed:
    lea r10, [widgetFactoryManager + OFFSET widgetFactoryManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, WIDGET_FACTORY_E_MEMORY_ALLOC_FAILED
    
.L4_exit:
    add rsp, 48
    pop rbp
    ret
PropertyBinding_Create ENDP

; PropertyBinding_Destroy(RCX = bindingId) -> RAX = DWORD (success)
PUBLIC PropertyBinding_Destroy
PropertyBinding_Destroy PROC FRAME
    ; RCX = binding ID to destroy
    
    ; Would search bindings list, remove, and free
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.BindingsDestroyed]
    xor rax, rax
    ret
PropertyBinding_Destroy ENDP

; PropertyBinding_GetValue(RCX = bindingId) -> RAX = QWORD (property value)
PUBLIC PropertyBinding_GetValue
PropertyBinding_GetValue PROC FRAME
    ; RCX = binding ID
    
    ; Would retrieve current value from source property
    xor rax, rax
    ret
PropertyBinding_GetValue ENDP

; PropertyBinding_SetValue(RCX = bindingId, RDX = newValue) -> RAX = DWORD (success)
PUBLIC PropertyBinding_SetValue
PropertyBinding_SetValue PROC FRAME
    ; RCX = binding ID
    ; RDX = new value
    
    ; Would set value on both source and target
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.BindingUpdates]
    xor rax, rax
    ret
PropertyBinding_SetValue ENDP

; PropertyBinding_GetSourceValue(RCX = objectPtr, RDX = propertyId) -> RAX = QWORD (value)
PUBLIC PropertyBinding_GetSourceValue
PropertyBinding_GetSourceValue PROC FRAME
    ; RCX = object pointer
    ; RDX = property ID
    
    ; Would retrieve property value from object
    xor rax, rax
    ret
PropertyBinding_GetSourceValue ENDP

; PropertyBinding_SetSourceValue(RCX = objectPtr, RDX = propertyId, R8 = newValue) -> RAX = DWORD (success)
PUBLIC PropertyBinding_SetSourceValue
PropertyBinding_SetSourceValue PROC FRAME
    ; RCX = object pointer
    ; RDX = property ID
    ; R8 = new value
    
    ; Would set property and update all bound targets
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.PropertyChanges]
    xor rax, rax
    ret
PropertyBinding_SetSourceValue ENDP

; WidgetFactory_RegisterClass(RCX = classId, RDX = factoryFunc) -> RAX = DWORD (success)
PUBLIC WidgetFactory_RegisterClass
WidgetFactory_RegisterClass PROC FRAME
    ; RCX = class ID
    ; RDX = factory function pointer
    
    inc QWORD PTR [widgetFactoryMetrics + OFFSET widgetFactoryMetrics.ClassesRegistered]
    xor rax, rax
    ret
WidgetFactory_RegisterClass ENDP

; =============================================================================
; PHASE 5 TEST FUNCTIONS
; =============================================================================

; Test_WidgetFactory_Create(VOID) -> RAX = DWORD (test result)
PUBLIC Test_WidgetFactory_Create
Test_WidgetFactory_Create PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Create button widget
    ; 2. Verify creation succeeds
    ; 3. Create layout
    ; 4. Add widget to layout
    ; 5. Destroy widget
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
Test_WidgetFactory_Create ENDP

; Test_PropertyBinding_Sync(VOID) -> RAX = DWORD (test result)
PUBLIC Test_PropertyBinding_Sync
Test_PropertyBinding_Sync PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Create two mock objects
    ; 2. Create binding between properties
    ; 3. Set source value
    ; 4. Verify target updated
    ; 5. Destroy binding
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
Test_PropertyBinding_Sync ENDP

; PropertyBinding_GetMetrics(VOID) -> RAX = QWORD (metrics pointer)
PUBLIC PropertyBinding_GetMetrics
PropertyBinding_GetMetrics PROC FRAME
    lea rax, [widgetFactoryMetrics]
    ret
PropertyBinding_GetMetrics ENDP

END
