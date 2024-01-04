#pragma once
#include <ntddk.h>
#include <Vector.h>
#include <DebugHelp.h>

typedef struct s_ppid_on_watch {
    ULONG_PTR      ppid;
    BOOLEAN     IsWatched;
}t_ppid_on_watch;

typedef NTSTATUS(*pPsSuspendProcess)(PEPROCESS Process);
typedef NTSTATUS(*pPsLookupProcessByProcessId)( HANDLE    ProcessId, PEPROCESS* Process);

void CreateProcessNotifyRoutineExCB(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void LoadImageNotifyRoutineCB(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
