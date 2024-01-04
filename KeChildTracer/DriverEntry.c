#include <ntddk.h>
#include <IOCTLHandler.h>
#include <Vector.h>
#include <DebugHelp.h>
#include <ProcessMonitor.h>

PDEVICE_OBJECT                  gDeviceObject;
KEVENT                          PrepChildInjEvt;
PIO_WORKITEM                    ChildNotifierWI;
UNICODE_STRING                  DeviceName  = RTL_CONSTANT_STRING(L"\\Device\\KeChildTracer");
UNICODE_STRING                  SymLinkName = RTL_CONSTANT_STRING(L"\\??\\KeChildTracer");
pPsSuspendProcess               SuspendProccess = NULL;
pPsLookupProcessByProcessId     LookupProcessById = NULL;

/*
    IOCTL Codes:
        - Add parent process to watchlist       - Implemented
        - Remove parent process from watchlist  - Not Implemented 
            - Could also be handled by the CreateProcessNotifyRoutineExCB callback by catching when a process dies 
                and if that pid is on watch then just remove from the list. Anyways, implementing this IOCTL Code 
                could be interesting just in case.
        - IOCTL code that essentially lets a UserMode process to get notified when a process is ready for injection; - Partial - WIP
            This, will send to the UM routine the PID of the target. -> UM: After injecting and before resuming the target, 
                add the target to the watchlist.
        - IOCTL code that resumes a process by a given PID. (The intention is that this resumes the previously suspended target) - Not Implemented
        - IOCTL to start intercepting process creations ? - Not Implemented
        - IOCTL to stop intercepting process creations

    Ideas:
        Somehow implement a mechanism that is able to sort of create a process graph? based on the process that have been intercepted.

    ToDo:
        - Remove dead code and data structures
        - Check for memory leaks
        - Check edge cases that could cause the system to crash
*/

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DriverObject);
    Break();

    Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineExCB, TRUE);
    if (!NT_SUCCESS(Status)) DebugPrint("PsSetCreateProcessNotifyRoutineEx(): Error removing Callback: [%d]\n", Status);

    Status = IoDeleteSymbolicLink(&SymLinkName);
    if (!NT_SUCCESS(Status)) DebugPrint("IoDeleteSymbolicLink(): Error removing DeviceSymLink: [%d]\n", Status);

    IoDeleteDevice(gDeviceObject);
    if (Status == STATUS_SUCCESS) DebugPrint("Driver unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(RegistryPath);
    Break();

    DriverObject->DriverUnload = UnloadDriver;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = IoControlDispatch;
    DriverObject->MajorFunction[IRP_MJ_CREATE]          = DriverCreateClose; // Function to handle create requests
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = DriverCreateClose; // Function to handle close requests

    Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &gDeviceObject);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("Failed to create device object\n");
        return Status;
    }

    Status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("Failed to create symlink object\n");
        return Status;
    }

    Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineExCB, FALSE);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsSetCreateProcessNotifyRoutineEx(): Error installing Callback: [%d]\n", Status);
        return Status;
    }

    ChildNotifierWI = IoAllocateWorkItem(gDeviceObject);
    if (!ChildNotifierWI) {
        DebugPrint("IoAllocateWorkItem(): Failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeEvent(&PrepChildInjEvt, NotificationEvent, FALSE);

    UNICODE_STRING RoutineName;
    RtlInitUnicodeString(&RoutineName, L"PsSuspendProcess");
    SuspendProccess =   (pPsSuspendProcess)MmGetSystemRoutineAddress(&RoutineName);
    
    RtlInitUnicodeString(&RoutineName, L"PsLookupProcessByProcessId");
    LookupProcessById = (pPsLookupProcessByProcessId)MmGetSystemRoutineAddress(&RoutineName);

    if (!SuspendProccess || !LookupProcessById) return STATUS_FATAL_APP_EXIT;
    
    if (NT_SUCCESS(Status)) DebugPrint("Driver loaded: [%d]\n", Status);

    return Status;
}
