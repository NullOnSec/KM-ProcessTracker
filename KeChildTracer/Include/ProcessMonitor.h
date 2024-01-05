#pragma once
#include <ntddk.h>
#include <Vector.h>
#include <DebugHelp.h>

typedef struct s_ppid_on_watch {
    ULONG_PTR      ppid;
    BOOLEAN     IsWatched;
}t_ppid_on_watch;

typedef NTSTATUS(*pPsSuspendProcess)(PEPROCESS Process);
typedef NTSTATUS(*pPsResumeProcess)(PEPROCESS Process);
typedef NTSTATUS(*pPsLookupProcessByProcessId)(HANDLE ProcessId, PEPROCESS* Process);

typedef struct s_kernel_apis {
    pPsSuspendProcess           KSuspendProccess;
    pPsResumeProcess            KResumeProccess;
    pPsLookupProcessByProcessId KLookupProcessById;
}KAPIS;

void CreateProcessNotifyRoutineExCB(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
