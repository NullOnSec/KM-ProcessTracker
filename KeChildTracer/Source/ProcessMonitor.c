#include <ProcessMonitor.h>

extern PDEVICE_OBJECT   gDeviceObject;
extern KEVENT           PrepChildInjEvt;
extern KAPIS            ProcUtilsApis;
ULONG_PTR               ChildPidToNotify;
t_vector                PPidWatchlist       = { 0 }; // Free me probably @ drive unload?

static BOOLEAN IsPidPresent(void* Val, void* UserData) {
    int *value = (int*)Val;
    t_ppid_on_watch* OnWatch = (t_ppid_on_watch*)UserData;
    if (OnWatch->ppid == *value) {
        OnWatch->IsWatched = TRUE;
        return TRUE;
    }
    return FALSE;
}

void CreateProcessNotifyRoutineExCB(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
    UNREFERENCED_PARAMETER(Process); UNREFERENCED_PARAMETER(ProcessId);
    if (CreateInfo) {
        HANDLE ParentProcessId = CreateInfo->ParentProcessId;
        t_ppid_on_watch OnWatch = { (ULONG_PTR)ParentProcessId, FALSE };
        //DebugPrint("Process Created: PID = %d, PPID = %d\n", ProcessId, ParentProcessId);
        if (PPidWatchlist.Data && PPidWatchlist.Size > 0) {
            VectorSafeIter(&PPidWatchlist, IsPidPresent, (void*)&OnWatch);
            if (OnWatch.IsWatched) {
                if (!NT_SUCCESS(ProcUtilsApis.KSuspendProccess(Process))) {
                    DebugPrint("Unable to suspend target: %d\n", ProcessId);
                    return;
                } else DebugPrint("Target: %d suspended succesfully\n", ProcessId);
                Break();
                ChildPidToNotify = HandleToULong(ProcessId);
                KeSetEvent(&PrepChildInjEvt, 0, FALSE);
            }
        }
    }
}

NTSTATUS RegisterCallbacks(BOOLEAN RemoveCB) {
    NTSTATUS Status = STATUS_SUCCESS;

    Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineExCB, RemoveCB);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsSetCreateProcessNotifyRoutineEx(): Error installing Callback: [%d]\n", Status);
        return Status;
    }

    return Status;
}

NTSTATUS ResumeProcessByPid(ULONG_PTR Pid) {
    PEPROCESS   Process;
    NTSTATUS    Status;

    Break();
    Status = ProcUtilsApis.KLookupProcessById((HANDLE)Pid, &Process);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsLookupProcessByProcessId(): Unable to obtain EPROCESS from PID %ld\n", Pid);
        return Status;
    }

    Status = ProcUtilsApis.KResumeProccess(Process);
    if (!NT_SUCCESS(Status)) {
        DebugPrint("PsResumeProcess(): Unable to obtain resume PID %ld\n", Pid);
        return Status;
    }

    DebugPrint("PsResumeProcess(): Process PID: %ld resumed succesfully\n", Pid);
    return Status;
}
