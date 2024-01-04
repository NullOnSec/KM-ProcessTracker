#pragma once

#ifdef _DEBUG
#define DebugPrint(...) DbgPrint(__VA_ARGS__)
#define Break() DbgBreakPoint();
#else
#define Break() (void)0
#define DebugPrint(...) (void)0
#endif