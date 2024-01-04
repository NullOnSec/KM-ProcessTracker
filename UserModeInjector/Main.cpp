#include <Windows.h>
#include <iostream>

#define IOCTL_MON_CHILD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SUB_CHILD_INJ		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

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
