#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#define IOCTL_MON_CHILD			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SUB_CHILD_INJ		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MON_START			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MON_STOP			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESUME_PROC		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef LONG(NTAPI* NtResumeProcess)(IN HANDLE ProcessHandle);

void ResumeProcessByPid(DWORD processId) {
    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    THREADENTRY32 threadEntry;
    threadEntry.dwSize = sizeof(THREADENTRY32);

    Thread32First(hThreadSnapshot, &threadEntry);

    do
    {
        if (threadEntry.th32OwnerProcessID == processId)
        {
            HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE,
                threadEntry.th32ThreadID);
            std::cout << "Threadid: " << hThread << " Res: " << ResumeThread(hThread) << std::endl;
            CloseHandle(hThread);
        }
    } while (Thread32Next(hThreadSnapshot, &threadEntry));

    CloseHandle(hThreadSnapshot);
}

static DWORD Job(LPVOID Context) {
    std::cout << "Hello from thread!" << std::endl;
    ULONG_PTR   Value = 0;
    DWORD       Size = 0;
    if (!DeviceIoControl((HANDLE)Context, IOCTL_SUB_CHILD_INJ, NULL, 0, &Value, sizeof(ULONG_PTR), &Size, NULL)) {
        std::cerr << "IOCTL failed. Error: " << GetLastError() << std::endl;
        CloseHandle(Context);
        return 1;
    }
    std::cout << "Recieved value: " << Value << std::endl;
   
    ResumeProcessByPid(Value);

    //if (!DeviceIoControl((HANDLE)Context, IOCTL_RESUME_PROC, &Value, sizeof(ULONG_PTR), NULL, 0, &Size, NULL)) {
    //    std::cerr << "IOCTL failed. Error: " << GetLastError() << std::endl;
    //    CloseHandle(Context);
    //    return 1;
    //}
    return 0;
}

int main() {
    HANDLE DeviceHandle;
    DWORD BytesReturned;

    // Open a handle to the device
    DeviceHandle = CreateFile(L"\\\\.\\KeChildTracer", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (DeviceHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open device. Error: " << GetLastError() << std::endl;
        return 1;
    }

    if (!DeviceIoControl(DeviceHandle, IOCTL_MON_START, NULL, 0, NULL, 0, &BytesReturned, NULL)) {
        std::cerr << "IOCTL failed. Error: " << GetLastError() << std::endl;
        CloseHandle(DeviceHandle);
        return 1;
    }

    int IntegerValue = GetProcessId(GetCurrentProcess());

    // Send the IOCTL request
    if (!DeviceIoControl(DeviceHandle, IOCTL_MON_CHILD, &IntegerValue, sizeof(IntegerValue), NULL, 0, &BytesReturned, NULL)) {
        std::cerr << "IOCTL failed. Error: " << GetLastError() << std::endl;
        CloseHandle(DeviceHandle);
        return 1;
    }

    std::cout << "IOCTL succeeded. Sent Integer: " << IntegerValue << std::endl;
    CreateThread(NULL, 0, Job, DeviceHandle, NULL, 0);
    // Launch cmd.exe
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcess(L"C:\\Windows\\System32\\cmd.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start cmd.exe. Error: " << GetLastError() << std::endl;
        CloseHandle(DeviceHandle);
        return 1;
    }

    std::cout << "Launched cmd.exe. Waiting for it to exit..." << std::endl;

    // Wait for the command prompt to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    std::cout << "cmd.exe has exited." << std::endl;

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(DeviceHandle);

    return 0;
}
