#include <IOCTLHandler.h>
#include <Vector.h>

extern PIO_WORKITEM ChildNotifierWI;
extern ULONG_PTR    ChildPidToNotify; // To be removed
extern KEVENT       PrepChildInjEvt;
extern t_vector     PPidWatchlist;

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0; // No extra information needed

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

/*
Implement some kind of queue:
    - the queue will be able to hold multiple pids
    - the data will be sent in a different way so we can tell the caller that the queue is not empty for ex:
        10bytes buffer just in case:
            bytes 0-7: pid - bytes 7-9: bit 0: queue status flag
    - Also a more efficient possibility would be to sent everything at once (if there are more than 1 pid in the queue)
        Prob for this we'd have to implement 2 ioctl calls? One to retrieve the buffer size and the other one to actually get the data?
*/
VOID ChildInjDelayedWorker(PDEVICE_OBJECT DeviceObject, PVOID Context) {
    ULONG_PTR   *UserBuf;
    PIRP        Irp = (PIRP)Context;

    UNREFERENCED_PARAMETER(DeviceObject);

    KeWaitForSingleObject(&PrepChildInjEvt, Executive, KernelMode, FALSE, NULL);

    if (!Irp->AssociatedIrp.SystemBuffer) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CompleteRequest;
    }

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG_PTR)) {
        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        goto CompleteRequest;
    }

    UserBuf = (ULONG_PTR*)Irp->AssociatedIrp.SystemBuffer;
    *UserBuf = ChildPidToNotify;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof(ULONG_PTR);

CompleteRequest:
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
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
        }
        else Status = STATUS_BUFFER_TOO_SMALL;
        break;

    case IOCTL_SUB_CHILD_INJ:
        IoMarkIrpPending(Irp);
        IoQueueWorkItem(ChildNotifierWI, ChildInjDelayedWorker, DelayedWorkQueue, Irp);
        return STATUS_PENDING;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}
