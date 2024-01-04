#pragma once

#include <ntddk.h>

typedef struct s_vector {
    void** Data;
    size_t Size;
    size_t MaxSize;
} t_vector;

typedef BOOLEAN(*VectorIterCb)(void*, void*);

NTSTATUS VectorNew(t_vector* Vector, size_t Size);
NTSTATUS VectorDelete(t_vector* Vector);
NTSTATUS VectorResize(t_vector* Vector, size_t NewSize);
NTSTATUS VectorAdd(t_vector* Vector, void* Value);
NTSTATUS VectorGetValueSafe(t_vector* Vector, int Index, void** Value);
NTSTATUS VectorSafeIter(t_vector* Vector, VectorIterCb Callback, void* UserData);
