#pragma once
#include <windows.h>
HWND hWnd = NULL;

std::wstring dllPath;
std::wstring cmdPath;

std::wstring toolPath;
std::wstring GamePath;
std::wstring AssemblyDllPath;
std::wstring GameVer;

#define IDM_test 1001
#define IDM_choose 1002
#define IDM_test2 1003
#define ID_path 1004
#define ID_ver 1005