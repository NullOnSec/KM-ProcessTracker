#include <IOCTLHandler.h>
#include <Vector.h>
#include <ProcessMonitor.h>

extern PIO_WORKITEM ChildNotifierWI;
extern ULONG_PTR    ChildPidToNotify; // To be removed
extern t_vector     PPidWatchlist;
extern KAPIS        ProcUtilsApis;
extern KMUTEX                  PPIdWatchListMTX;

BOOLEAN ProcCreationCB = FALSE;

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0; // No extra information needed

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS IoControlDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    NTSTATUS            Status  = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IrpSp   = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_MON_CHILD:
        // Extract the integer from the IOCTL request
        if (IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(int)) {
            Break();
            KeWaitForSingleObject(&PPIdWatchListMTX, Executive, KernelMode, FALSE, NULL);
            if (!PPidWatchlist.Data) {
                Status = VectorNew(&PPidWatchlist, 0x10);// Arbitrary size;
                if (!NT_SUCCESS(Status)) break;
            }
            int     *IntegerValue = (int*)Irp->AssociatedIrp.SystemBuffer;
            int     *Value = ExAllocatePoolWithTag(NonPagedPool, sizeof(int), 'Val'); // Find a way to free this safely
            if (!Value) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            *Value = *IntegerValue;
            DebugPrint("Received Integer: %d\n", *IntegerValue); // add ppid recieved to the watchlist
            VectorAdd(&PPidWatchlist, Value);
        } else Status = STATUS_BUFFER_TOO_SMALL;
        KeReleaseMutex(&PPIdWatchListMTX, FALSE);
        break;

    case IOCTL_MON_START:
        Break();
        Status = RegisterCallbacks(FALSE);
        if (NT_SUCCESS(Status)) ProcCreationCB = TRUE;
        break;

    case IOCTL_MON_STOP:// In this case its probably ideal to do some cleanup once the actual pipeline is more advanced
        Status = RegisterCallbacks(TRUE);
        if (NT_SUCCESS(Status)) ProcCreationCB = FALSE;
        break;

    case IOCTL_RESUME_PROC:
        Break();
        if (IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(int)) {
            int* Pid = ExAllocatePoolWithTag(NonPagedPool, sizeof(int), 'IPID');
            if (!Pid) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            int* Aux = (int*)Irp->AssociatedIrp.SystemBuffer;
            *Pid = *Aux;
            Status = ResumeProcessByPid(*Pid);
            ExFreePool(Pid);
        } else Status = STATUS_BUFFER_TOO_SMALL;
        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}
