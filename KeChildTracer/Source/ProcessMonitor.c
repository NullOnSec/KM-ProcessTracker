#include <ProcessMonitor.h>

extern PDEVICE_OBJECT   gDeviceObject;
extern KEVENT           PrepChildInjEvt;

t_vector                PPidWatchlist       = { 0 }; // Free me probably @ drive unload?

extern pPsLookupProcessByProcessId  LookupProcessById;
extern pPsSuspendProcess            SuspendProccess;

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
    UNREFERENCED_PARAMETER(Process);
    if (CreateInfo) {
        HANDLE ParentProcessId = CreateInfo->ParentProcessId;
        t_ppid_on_watch OnWatch = { (ULONG_PTR)ParentProcessId, FALSE };
        DebugPrint("Process Created: PID = %d, PPID = %d\n", ProcessId, ParentProcessId);
        if (PPidWatchlist.Data && PPidWatchlist.Size > 0) {
            VectorSafeIter(&PPidWatchlist, IsPidPresent, (void*)&OnWatch);
            if (OnWatch.IsWatched) {
                Break();
                if (!NT_SUCCESS(SuspendProccess(Process))) {
                    DebugPrint("Unable to suspend target: %d", ProcessId);
                    return;
                } else DebugPrint("Target: %d suspended succesfully", ProcessId); 
                /* 
                Target is suspended succesfully, seemingly ntdll is loaded at this point so it should be possible to inject our target dll.

                */
            }
        }
    }
}
