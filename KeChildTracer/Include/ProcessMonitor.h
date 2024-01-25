#pragma once
#include <ntddk.h>
#include <Vector.h>
#include <DebugHelp.h>

typedef NTSTATUS(*pPsSuspendProcess)(PEPROCESS Process);
typedef NTSTATUS(*pPsResumeProcess)(PEPROCESS Process);
typedef NTSTATUS(*pPsLookupProcessByProcessId)(HANDLE ProcessId, PEPROCESS* Process);
typedef NTSTATUS(*pPsLookupThreadByThreadId)(HANDLE   ThreadId, PETHREAD* Thread);

typedef struct s_pid_on_watch {
    ULONG_PTR   pid;
    BOOLEAN     IsWatched;
}t_pid_on_watch;

typedef struct s_suspended_proc {
    HANDLE      Pid;
    PEPROCESS   Process;
    HANDLE      ThreadId;
    PETHREAD    Thread;
    BOOLEAN     Suspended;
}t_suspended_proc;

typedef struct s_suspended_on_watch {
    HANDLE              Pid;
    t_suspended_proc    *Entry;
    
}t_suspended_on_watch;

typedef struct s_kernel_apis {
    pPsSuspendProcess           KSuspendProccess;//
    pPsResumeProcess            KResumeProccess;//;
    pPsLookupProcessByProcessId KLookupProcessById;//
    pPsLookupThreadByThreadId   KGetThreadByThreadId;//
}KAPIS;


void        CreateProcessNotifyRoutineExCB(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
NTSTATUS    RegisterCallbacks(BOOLEAN RemoveCB);
NTSTATUS    ResumeProcessByPid(ULONG_PTR Pid);
