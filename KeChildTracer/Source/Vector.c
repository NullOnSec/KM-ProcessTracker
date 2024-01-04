#include <Vector.h>


NTSTATUS VectorNew(t_vector* Vector, size_t Size) {
    Vector->Data = (void**)ExAllocatePoolWithTag(NonPagedPool, Size * sizeof(void*), 'IVec');
    if (!Vector->Data) return STATUS_INSUFFICIENT_RESOURCES;
    Vector->Size = 0;
    Vector->MaxSize = Size;
    RtlZeroMemory(Vector->Data, Size * sizeof(void*));
    return STATUS_SUCCESS;
}

NTSTATUS VectorDelete(t_vector* Vector) {
    if (Vector->Data) {
        ExFreePool(Vector->Data);
        Vector->Data = NULL;
        Vector->Size = 0;
        Vector->MaxSize = 0;
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_ADDRESS;
}

NTSTATUS VectorResize(t_vector* Vector, size_t NewSize) {
    void** NewData = (void**)ExAllocatePoolWithTag(NonPagedPool, NewSize * sizeof(void*), 'IVec');
    if (!NewData) return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(NewData, Vector->Data, Vector->Size * sizeof(void*));
    ExFreePool(Vector->Data);
    Vector->Data = NewData;
    Vector->MaxSize = NewSize;
    return STATUS_SUCCESS;
}

NTSTATUS VectorAdd(t_vector* Vector, void* Value) {
    if (!Vector) return STATUS_INVALID_ADDRESS;
    if (Vector->Size >= Vector->MaxSize) {
        NTSTATUS Status = VectorResize(Vector, Vector->MaxSize * 2);
        if (!NT_SUCCESS(Status)) return Status;
    }
    Vector->Data[Vector->Size++] = Value;
    return STATUS_SUCCESS;
}

NTSTATUS VectorGetValueSafe(t_vector* Vector, int Index, void** Value) {
    if (!Vector || !Value) return STATUS_INVALID_ADDRESS;
    if (Index < 0 || Index >= Vector->Size) return STATUS_ARRAY_BOUNDS_EXCEEDED;
    *Value = Vector->Data[Index];
    return STATUS_SUCCESS;
}

NTSTATUS VectorSafeIter(t_vector* Vector, VectorIterCb Callback, void* UserData) {
    if (!Vector->Data) return STATUS_INVALID_ADDRESS;
    for (size_t i = 0; i < Vector->Size; i++) {
        if (Callback(Vector->Data[i], UserData)) return STATUS_SUCCESS;
    }
    return STATUS_SUCCESS;
}