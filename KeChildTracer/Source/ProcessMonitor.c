#include <ProcessMonitor.h>

extern PDEVICE_OBJECT   gDeviceObject;
extern KAPIS            ProcUtilsApis;
ULONG_PTR               ChildPidToNotify;
extern KMUTEX           PPIdWatchListMTX, SuspendedProcsMTX;
t_vector                PPidWatchlist       = { 0 }; // Free me probably @ drive unload?
t_vector                SuspendedProcs = { 0 };

static BOOLEAN FindEntry(void** Val, void* UserData) {
    t_suspended_proc* value = (t_suspended_proc*)*Val;
    t_suspended_on_watch* OnWatch = (t_suspended_on_watch*)UserData;
    if (OnWatch->Pid == value->Pid) {
        OnWatch->Entry = value;
        return TRUE;
    }
    return FALSE;
}


static BOOLEAN IsPidPresentTh(void** Val, void* UserData) {
    t_suspended_proc* value = (t_suspended_proc*)*Val;
    t_pid_on_watch* OnWatch = (t_pid_on_watch*)UserData;
    if (OnWatch->pid == (ULONG_PTR)value->Pid) {
        OnWatch->IsWatched = TRUE;
        return TRUE;
    }
    return FALSE;
}

static BOOLEAN IsPidPresent(void** Val, void* UserData) {
    int *value = (int*)*Val;
    t_pid_on_watch* OnWatch = (t_pid_on_watch*)UserData;
    if (OnWatch->pid == *value) {
        OnWatch->IsWatched = TRUE;
        return TRUE;
    }
    return FALSE;
}

static BOOLEAN RemoveTargetFromWatchlistCb(void** Val, void* UserData) {
    int* value = (int*)*Val;
    int Current = HandleToULong(UserData);

    if (Current == *value) {
        *value = 0;
        return TRUE;
    }
    return FALSE;
}

void CreateProcessNotifyRoutineExCB(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
    UNREFERENCED_PARAMETER(Process); UNREFERENCED_PARAMETER(ProcessId);
    if (CreateInfo) {
        HANDLE ParentProcessId = CreateInfo->ParentProcessId;
        t_pid_on_watch OnWatch = { (ULONG_PTR)ParentProcessId, FALSE };
        KeWaitForSingleObject(&PPIdWatchListMTX, Executive, KernelMode, FALSE, NULL);
        if (PPidWatchlist.Data && PPidWatchlist.Size > 0) {
            VectorSafeIter(&PPidWatchlist, IsPidPresent, (void*)&OnWatch);
            if (OnWatch.IsWatched) {
                Break();
                ChildPidToNotify = HandleToULong(ProcessId);
                int* Value = ExAllocatePoolWithTag(NonPagedPool, sizeof(int), 'Val');
                if (!Value) goto finish;
                *Value = HandleToULong(ProcessId);
                VectorAdd(&PPidWatchlist, Value);
                KeWaitForSingleObject(&SuspendedProcsMTX, Executive, KernelMode, FALSE, NULL);
                if (!SuspendedProcs.Data) VectorNew(&SuspendedProcs, 0x10);
                if (SuspendedProcs.Data) {
                    t_suspended_proc *Entry = ExAllocatePoolWithTag(NonPagedPool, sizeof(t_suspended_proc), 'SPRC');
                    Entry->Pid          = ProcessId; Entry->Process = Process;
                    Entry->ThreadId     = 0;         Entry->Thread  = NULL;
                    Entry->Suspended    = FALSE;
                    VectorAdd(&SuspendedProcs, Entry);
                }
                KeReleaseMutex(&SuspendedProcsMTX, FALSE);
            }
        }
    } else {
        KeWaitForSingleObject(&PPIdWatchListMTX, Executive, KernelMode, FALSE, NULL);
        VectorSafeIter(&PPidWatchlist, RemoveTargetFromWatchlistCb, ProcessId);
    }

finish:
    KeReleaseMutex(&PPIdWatchListMTX, FALSE);
}

void CreateThreadNotifyRoutineCB(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
    KeWaitForSingleObject(&SuspendedProcsMTX, Executive, KernelMode, FALSE, NULL);
    if (Create && SuspendedProcs.Data && SuspendedProcs.Size) {

        t_pid_on_watch Entry = { (ULONG_PTR)ProcessId, FALSE };
        VectorSafeIter(&SuspendedProcs, IsPidPresentTh, &Entry);
        if (Entry.IsWatched) {
            Break();
            t_suspended_on_watch VectorEntry = { ProcessId, NULL };
            VectorSafeIter(&SuspendedProcs, FindEntry, &VectorEntry);
            if (VectorEntry.Entry && !VectorEntry.Entry->Suspended) {
                Break();
                VectorEntry.Entry->ThreadId = ThreadId;
                PEPROCESS Process;
                ProcUtilsApis.KLookupProcessById(ProcessId, &Process);
                ProcUtilsApis.KSuspendProccess(Process);
                ProcUtilsApis.KGetThreadByThreadId(ThreadId, &VectorEntry.Entry->Thread);
                VectorEntry.Entry->Suspended = TRUE;
            }
        }
    }
    KeReleaseMutex(&SuspendedProcsMTX, FALSE);
}

NTSTATUS RegisterCallbacks(BOOLEAN RemoveCB) {
    NTSTATUS Status = STATUS_SUCCESS;

    Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineExCB, RemoveCB);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsSetCreateProcessNotifyRoutineEx(): Error installing Callback: [%d]\n", Status);
        return Status;
    }

    if (RemoveCB == FALSE) {
        Status = PsSetCreateThreadNotifyRoutine(CreateThreadNotifyRoutineCB);
        if (!NT_SUCCESS(Status)) {
            DebugPrint("PsSetCreateThreadNotifyRoutine(): Error installing Callback: [%d]\n", Status);
            return Status;
        }
    } else {
        Status = PsRemoveCreateThreadNotifyRoutine(CreateThreadNotifyRoutineCB);
        if (!NT_SUCCESS(Status)) {
            DebugPrint("PsSetCreateThreadNotifyRoutine(): Error installing Callback: [%d]\n", Status);
            return Status;
        }
    }

    return Status;
}

NTSTATUS ResumeProcessByPid(ULONG_PTR Pid) {
    PEPROCESS   Process;
    NTSTATUS    Status;

    Break();
    KeWaitForSingleObject(&SuspendedProcsMTX, Executive, KernelMode, FALSE, NULL);

    Status = ProcUtilsApis.KLookupProcessById((HANDLE)Pid, &Process);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsLookupProcessByProcessId(): Unable to obtain EPROCESS from PID %ld\n", Pid);
        goto finish;
    }

    t_suspended_on_watch TargetEntry = { (HANDLE)Pid, NULL };
    VectorSafeIter(&SuspendedProcs, FindEntry, &TargetEntry);
    if (TargetEntry.Entry) ProcUtilsApis.KResumeProccess(TargetEntry.Entry->Process);

finish:
    KeReleaseMutex(&SuspendedProcsMTX, FALSE);
    return Status;
}
