#pragma once

#include <windows.h>
#include <iostream>
#include "define.h"

bool IsDotNetInstalled() {
    HKEY hKey;
    DWORD dwType = 0;
    DWORD dwData = 0;
    DWORD dwSize = sizeof(DWORD);

    // 查询 .NET Framework 4.5 及以上
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, L"Release", NULL, &dwType, (LPBYTE)&dwData, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            // dwData >= 378389 表示安装了 .NET Framework 4.5+
            return dwData >= 378389;
        }
        RegCloseKey(hKey);
    }
    return false;
}

#include <fstream>

bool ExtractResourceToFile(int resourceID, const wchar_t* type, const std::wstring& outputPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCE(resourceID), type);
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(hData);

    std::ofstream file(outputPath, std::ios::binary);
    file.write((char*)data, size);
    file.close();

    return true;
}
#include <string>

bool RunCommandHidden(const std::wstring& cmd)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // 隐藏窗口

    // CreateProcess 需要命令行可写
    wchar_t* cmdLine = new wchar_t[cmd.size() + 1];
    wcscpy_s(cmdLine, cmd.size() + 1, cmd.c_str());

    bool result = CreateProcessW(
        NULL,      // 可执行文件名为空，命令行里指定
        cmdLine,   // 命令行
        NULL, NULL,// 默认安全属性
        FALSE,     // 不继承句柄
        0,         // 默认创建标志
        NULL, NULL,// 默认环境和目录
        &si, &pi
    );

    if (!result) {
        delete[] cmdLine;
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    delete[] cmdLine;
    return true;
}
#include <vector>
std::string RunCmd(const std::wstring& cmd)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    si.wShowWindow = SW_HIDE;

    wchar_t* cmdLine = new wchar_t[cmd.size() + 1];
    wcscpy_s(cmdLine, cmd.size() + 1, cmd.c_str());

    bool result = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    delete[] cmdLine;
    CloseHandle(hWrite);

    if (!result)
    {
        DWORD err = GetLastError();
        wchar_t* msg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err,
            0,
            (LPWSTR)&msg,
            0,
            NULL
        );
        std::wstring wmsg = msg ? msg : L"Unknown error";
        LocalFree(msg);
        // wchar_t 转 string
        std::string errorMsg(wmsg.begin(), wmsg.end());
        return "Failed.创建dnlib命令行进程失败，错误信息： ("
            + std::to_string(err)
            + "): "
            + errorMsg;
    }

    std::string output;
    std::vector<char> buffer(4096);
    DWORD bytesRead;

    while (ReadFile(hRead, buffer.data(), (DWORD)buffer.size() - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        output += buffer.data();
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return output; // 这里就包含 TRUE / notfound / repeat
}
#include <vector>
#include <string>
#include <regex>

bool IsFileInUse(const std::wstring& filePath)
{
    // 尝试以独占方式打开文件
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,  // 尝试读写
        0,                             // 不共享
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION)
        {
            return true; // 文件被占用
        }
        return false; // 文件不存在或其他错误
    }

    // 文件可以打开，说明没被占用
    CloseHandle(hFile);
    return false;
}
bool patch(const std::wstring& cmdPath,
    const std::wstring& dllPath,bool aaa)
{
    if (IsFileInUse(AssemblyDllPath)) {
        MessageBox(hWnd, L"游戏文件被占用，请先解除占用。", L"提示", MB_OK);
        return false;
    }
        
    std::wstring outputDLL = dllPath;

    
    HWND hButton;
    if (aaa == false) hButton = GetDlgItem(hWnd, IDM_test); // 获取按钮句柄
    else hButton = GetDlgItem(hWnd, IDM_test2); // 获取按钮句柄
    //SetWindowTextW(hButton, L"正在反编译...");

    if (aaa == false) {
        SetWindowTextW(hButton, L"正在安装...");
        std::wstring cmd =
            L"\"" + cmdPath + L"\"" + L" -m false" +
            L" -dll \"" + dllPath + L"\"" + L" -out \"" + GamePath + L"\\AliceInCradle_Data\\Managed\\assemblycsharptemp.dll\"";
        //MessageBox(NULL, cmd.c_str(), L"info", MB_OK | MB_ICONWARNING);
        std::string result = RunCmd(cmd);
        
        if (result.find("TRUE") != std::string::npos) {

        }
        else if(result.find("notfound") != std::string::npos) {
            MessageBox(hWnd, L"游戏文件异常！找不到补丁安装位置。", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        else if (result.find("repeat") != std::string::npos) {
            MessageBox(hWnd, L"补丁已安装，请勿重复安装！", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        else {
            MessageBoxA(NULL, result.c_str(), "错误", MB_OK | MB_ICONHAND);
            return false;
        }
        
        std::wstring aaaaa = GamePath + L"\\AliceInCradle_Data\\Managed\\assemblycsharptemp.dll";
        HANDLE hFile = CreateFileW(aaaaa.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            MessageBox(hWnd, L"补丁生成失败\n错误信息：补丁文件未生成", L"错误", MB_OK | MB_ICONHAND);
            return false; // 打不开文件
        }
        LARGE_INTEGER size;
        bool ok = GetFileSizeEx(hFile, &size);
        CloseHandle(hFile);
        if (!ok) {
            MessageBox(hWnd, L"补丁生成失败\n错误信息：无法获取补丁大小", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        if (size.QuadPart > 0.75 * 1024 * 1024) {
            if (CopyFileW(aaaaa.c_str(), dllPath.c_str(), FALSE)) {
                DeleteFile(aaaaa.c_str());
                return true;
            }
            else {
                //std::wcout << L"复制失败，错误码：" << GetLastError() << L"\n";
                DeleteFile(aaaaa.c_str());
                MessageBox(hWnd, L"写入失败。\n错误信息：补丁写入游戏失败", L"错误", MB_OK | MB_ICONHAND);
                return false;
            }
            
        }
        DeleteFile(aaaaa.c_str());
        MessageBox(hWnd, L"补丁生成失败\n错误信息：补丁文件大小错误，请联系补丁作者", L"错误", MB_OK | MB_ICONHAND);
        return false;
    }
    else {
        SetWindowTextW(hButton, L"正在卸载...");
        std::wstring cmd =
            L"\"" + cmdPath + L"\"" + L" -m true" +
            L" -dll \"" + dllPath + L"\"" + L" -out \"" + GamePath + L"\\AliceInCradle_Data\\Managed\\assemblycsharptemp.dll\"";
        //MessageBox(NULL, cmd.c_str(), L"info", MB_OK | MB_ICONWARNING);
        std::string result = RunCmd(cmd);

        if (result.find("TRUE") != std::string::npos) {

        }
        else if (result.find("notfound") != std::string::npos) {
            MessageBox(hWnd, L"游戏文件异常！找不到补丁安装位置。", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        else if (result.find("repeat") != std::string::npos) {
            MessageBox(hWnd, L"未找到已安装的补丁！", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        else {
            result = "发生未知错误，请联系补丁作者\n错误信息：" + result;
            MessageBoxA(NULL, result.c_str(), "错误", MB_OK | MB_ICONHAND);
            return false;
        }

        std::wstring aaaaa = GamePath + L"\\AliceInCradle_Data\\Managed\\assemblycsharptemp.dll";
        HANDLE hFile = CreateFileW(aaaaa.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            MessageBox(hWnd, L"补丁生成失败\n错误信息：补丁文件未生成", L"错误", MB_OK | MB_ICONHAND);
            return false; // 打不开文件
        }
        LARGE_INTEGER size;
        bool ok = GetFileSizeEx(hFile, &size);
        CloseHandle(hFile);
        if (!ok) {
            MessageBox(hWnd, L"补丁生成失败\n错误信息：无法获取补丁大小", L"错误", MB_OK | MB_ICONHAND);
            return false;
        }
        if (size.QuadPart > 0.75 * 1024 * 1024) {
            if (CopyFileW(aaaaa.c_str(), dllPath.c_str(), FALSE)) {
                DeleteFile(aaaaa.c_str());
                return true;
            }
            else {
                //std::wcout << L"复制失败，错误码：" << GetLastError() << L"\n";
                DeleteFile(aaaaa.c_str());
                MessageBox(hWnd, L"写入失败。\n错误信息：补丁写入游戏失败", L"错误", MB_OK | MB_ICONHAND);
                return false;
            }

        }
        DeleteFile(aaaaa.c_str());
        MessageBox(hWnd, L"补丁生成失败\n错误信息：补丁文件大小错误，请联系补丁作者", L"错误", MB_OK | MB_ICONHAND);
        return false;
    }

}

#include <commdlg.h>
std::wstring OpenExeFileDialog()
{
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = L"AliceInCradle.exe";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"请选择游戏主程序";
    if (GetOpenFileNameW(&ofn))
    {
        // 用户选择了文件，返回路径
        return std::wstring(szFile);
    }
    else
    {
        // 用户取消或出错，返回空字符串
        return L"";
    }
}
std::wstring GetExeFolder(const std::wstring& exePath)
{
    size_t pos = exePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        return exePath.substr(0, pos); // 返回目录
    else
        return L""; // 没有父目录
}
bool CheckAssemblyDll(const std::wstring& baseDir)
{
    // 构造 DLL 路径
    std::wstring dllPath = baseDir + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll";

    DWORD attrs = GetFileAttributesW(dllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false; // 文件不存在或路径错误

    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return false; // 是目录，不是文件

    return true; // 文件存在
}



std::wstring GetCurrentExeFolder()
{
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
    {
        return L""; // 获取失败
    }

    std::wstring path(buffer);
    // 找最后一个斜杠
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        return path.substr(0, pos); // 返回目录
    else
        return L""; // 不存在斜杠，返回空
}

bool IsAlreadyRunning()
{
    // 创建一个全局互斥体
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"MyUniqueAppMutexName_12345");
    if (hMutex == NULL)
    {
        // 创建失败，认为程序不可运行
        return true;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // 互斥体已存在 → 已经有实例运行
        CloseHandle(hMutex);
        return true;
    }

    // 互斥体创建成功，当前是唯一实例
    return false;
}