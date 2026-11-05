#pragma once
#ifndef AIC_VM_PATCH_WINDOW_H
#define AIC_VM_PATCH_WINDOW_H

// ===========================================================================
//  虚拟机检测绕过（虚拟补丁）窗口
// ---------------------------------------------------------------------------
//
//  用法（窗口写法/尺寸/模态行为与 CheatWindow.h 一致，不需要回传按键）：
//
//      #include "VmPatchWindow.h"   // 内部已包含 define.h
//      ...
//      ShowAicVmPatchWindow();       // 400x320、居中、模态、微软雅黑
//
//  除了 define.h 里的全局变量（GamePath / cmdPath / hWnd），不依赖工程里
// ===========================================================================

#include <windows.h>
#include <string>
#include <vector>

#include "define.h"

#ifndef IDR_LAUNCHER
#include "resource.h"           // IDR_LAUNCHER（启动器 exe 的 RT_RCDATA 资源）
#endif

#define AIC_VM_WINDOW_CLASSNAME     L"AicVmPatch_WindowClass"
#define AIC_VM_WINDOW_TITLE         L"虚拟机检测"
#define AIC_VM_WINDOW_WIDTH         400
#define AIC_VM_WINDOW_HEIGHT        320

#define AIC_VM_ID_EXE               9101
#define AIC_VM_ID_DLL               9102

#define AIC_VM_ID_OK                0x7F21   // 安装
#define AIC_VM_ID_REVERT            0x7F22   // 卸载
#define AIC_VM_ID_CANCEL            0x7F23   // 放弃

// ===========================================================================
//  实现
// ===========================================================================
namespace aic_vm_detail
{
    // ------------------------------ 路径 -------------------------------------

    inline std::wstring ExePath()
    {
        return GamePath + L"\\AliceInCradle.exe";
    }

    // 原版 exe 的备份：AliceInCradle.exe.bak
    inline std::wstring ExeBackupPath()
    {
        return ExePath() + L".bak";
    }

    inline std::wstring DllPath()
    {
        return GamePath + L"\\AliceInCradle_Data\\Managed\\unsafeAssem.dll";
    }

    // 补丁工具写出的中间文件（正常情况下它自己会删，这里再兜一次底）
    inline std::wstring DllTempPath()
    {
        return DllPath() + L".tmp";
    }

    // ------------------------------ 资源 / 写文件 ----------------------------

    inline HRSRC FindOwnResource(int id)
    {
        HMODULE mod = ::GetModuleHandleW(nullptr);
        HRSRC hRes = ::FindResourceW(mod, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!hRes)
            hRes = ::FindResourceW(mod, MAKEINTRESOURCEW(id), L"RT_RCDATA");
        return hRes;
    }

    inline bool ReadResourceBytes(int id, std::vector<BYTE>& out)
    {
        out.clear();

        HMODULE mod = ::GetModuleHandleW(nullptr);
        HRSRC hRes = FindOwnResource(id);
        if (!hRes)
            return false;

        const DWORD size = ::SizeofResource(mod, hRes);
        HGLOBAL hData = ::LoadResource(mod, hRes);
        if (!hData || size == 0)
            return false;

        const void* data = ::LockResource(hData);
        if (!data)
            return false;

        const BYTE* p = static_cast<const BYTE*>(data);
        out.assign(p, p + size);
        return true;
    }

    inline bool WriteAllBytes(const std::wstring& path, const std::vector<BYTE>& data)
    {
        // 目标可能是只读，先清属性再试
        ::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);

        HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        DWORD written = 0;
        const bool ok =
            ::WriteFile(hFile, data.empty() ? "" : reinterpret_cast<const char*>(&data[0]),
                (DWORD)data.size(), &written, nullptr) != FALSE
            && written == (DWORD)data.size();

        ::CloseHandle(hFile);
        return ok;
    }

    inline bool FileExists(const std::wstring& path)
    {
        return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    // 文件大小（字节）；失败返回 (ULONGLONG)-1
    inline ULONGLONG FileSize(const std::wstring& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
            return (ULONGLONG)-1;
        return (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    }

    inline void DeleteQuietly(const std::wstring& path)
    {
        if (!FileExists(path))
            return;
        ::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        ::DeleteFileW(path.c_str());
    }

    // 收尾：删掉补丁工具可能留下的中间文件（dll 本身不备份，所以没有 .bak 要清）
    inline void CleanupTemp()
    {
        DeleteQuietly(DllTempPath());
    }

    // 文件是否被别的进程独占（游戏开着的时候补丁写不进去）
    inline bool IsFileInUse(const std::wstring& path)
    {
        HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            const DWORD err = ::GetLastError();
            return err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION;
        }
        ::CloseHandle(hFile);
        return false;
    }

    // ------------------------------ 调命令行工具 -----------------------------

    // 跑一条命令，把它的 stdout/stderr 一起收回来（隐藏窗口）。
    // 不用 ShellExecute / system()，也不会闪黑框。
    inline std::string RunTool(const std::wstring& cmd)
    {
        std::string output;

        SECURITY_ATTRIBUTES sa;
        ::ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hRead = nullptr;
        HANDLE hWrite = nullptr;
        if (!::CreatePipe(&hRead, &hWrite, &sa, 0))
            return output;
        ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si;
        ::ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.hStdInput = nullptr;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi;
        ::ZeroMemory(&pi, sizeof(pi));

        // CreateProcessW 要求命令行可写
        std::vector<wchar_t> line(cmd.begin(), cmd.end());
        line.push_back(L'\0');

        const BOOL started = ::CreateProcessW(nullptr, &line[0], nullptr, nullptr, TRUE, 0,
            nullptr, nullptr, &si, &pi);

        ::CloseHandle(hWrite);   // 必须由我们这边关掉写端，否则 ReadFile 永远等不到 EOF
        if (!started)
        {
            ::CloseHandle(hRead);
            return output;
        }

        char buf[4096];
        for (;;)
        {
            DWORD got = 0;
            if (!::ReadFile(hRead, buf, (DWORD)sizeof(buf), &got, nullptr) || got == 0)
                break;
            output.append(buf, got);
        }

        ::CloseHandle(hRead);
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
        return output;
    }

    // 工具输出是控制台的 ANSI（GBK）字节，按系统代码页转成宽字符再显示
    inline std::wstring WidenAnsi(const std::string& s)
    {
        if (s.empty())
            return std::wstring();

        const int n = ::MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n <= 0)
            return std::wstring();

        std::wstring out((size_t)n, L'\0');
        ::MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], n);

        // 去掉结尾多余的空行，MessageBox 里更整齐
        while (!out.empty() && (out[out.size() - 1] == L'\n' || out[out.size() - 1] == L'\r'))
            out.erase(out.size() - 1);

        return out;
    }

    // ------------------------------ 第 2 步：换 exe --------------------------

    // 只用文件大小判断：启动器只有几百 KB，原版主程序是十几 MB。
    // 小于 1MB 一律当启动器，其余（含正好 1MB）一律当原版。
    static const ULONGLONG kLauncherSizeLimit = 1024ULL * 1024ULL;

    inline bool LooksLikeLauncher(const std::wstring& path)
    {
        const ULONGLONG size = FileSize(path);
        return size != 0 && size < kLauncherSizeLimit;   // 文件不存在时返回 false
    }

    // 返回 false 时 err 里是原因；changed 表示这次有没有真的写 exe
    inline bool InstallExe(std::wstring& err, bool& changed)
    {
        changed = false;

        // 先判“是不是已经装过了”：exe 小于 1MB 就是启动器，
        // 直接算安装完成 —— 不读资源、不碰备份，重复安装不会报找不到资源
        if (LooksLikeLauncher(ExePath()))
        {
            // 备份本身也是启动器的话，等于没有可用的原版，提醒一下
            if (FileExists(ExeBackupPath()) && LooksLikeLauncher(ExeBackupPath()))
            {
                err = L"备份 AliceInCradle.exe.bak 是启动器，没有可用的原版备份";
                return false;
            }
            return true;
        }

        std::vector<BYTE> launcher;
        if (!ReadResourceBytes(IDR_LAUNCHER, launcher))
        {
            err = L"资源里找不到启动器（IDR_LAUNCHER），请重新编译本工具";
            return false;
        }

        // 当前是原版：以它为准刷新备份。
        // 卸载后 exe 已还原、或游戏更新换了 exe 时，旧备份可能已经过期，
        // 这种情况必须覆盖，否则下次卸载会还原成旧版本的 exe。
        ::SetFileAttributesW(ExePath().c_str(), FILE_ATTRIBUTE_NORMAL);
        ::SetFileAttributesW(ExeBackupPath().c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!::CopyFileW(ExePath().c_str(), ExeBackupPath().c_str(), FALSE))
        {
            err = L"备份原版主程序失败（游戏可能正在运行）";
            return false;
        }

        if (!WriteAllBytes(ExePath(), launcher))
        {
            err = L"写入启动器失败（游戏可能正在运行，或文件被设为只读）";
            return false;
        }

        changed = true;
        return true;
    }

    // 还原原版 exe；成功后把备份删掉（backupDeleted 给界面提示用）
    inline bool RestoreExe(std::wstring& err, bool& backupDeleted)
    {
        backupDeleted = false;

        if (!LooksLikeLauncher(ExePath()))
        {
            if (FileExists(ExeBackupPath()))
            {
                DeleteQuietly(ExeBackupPath());
                backupDeleted = true;
            }
            return true;
        }

        if (!FileExists(ExeBackupPath()))
        {
            err = L"没有找到原版主程序备份 AliceInCradle.exe.bak";
            return false;
        }

        // 备份本身是启动器（小于 1MB）的话，拿来还原等于什么都没做
        if (LooksLikeLauncher(ExeBackupPath()))
        {
            err = L"备份 AliceInCradle.exe.bak 其实是启动器，无法还原";
            return false;
        }

        ::SetFileAttributesW(ExePath().c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!::CopyFileW(ExeBackupPath().c_str(), ExePath().c_str(), FALSE))
        {
            err = L"还原主程序失败（游戏可能正在运行）";
            return false;
        }

        // 还原成功，备份用完即删
        DeleteQuietly(ExeBackupPath());
        backupDeleted = true;
        return true;
    }

    // ------------------------------ 第 1 步：IL 补丁 -------------------------

    enum PatchState
    {
        PatchOk = 0,        // 已写入 / 已还原
        PatchRepeat,        // 本来就是这个状态
        PatchNotFound,      // 版本不符，找不到补丁位置
        PatchError          // 调用或输出异常
    };

    inline PatchState RunDllPatch(bool uninstall, std::string& raw)
    {
        if (cmdPath.empty())
        {
            raw = "cmdPath 为空：命令行工具还没释放到临时目录";
            return PatchError;
        }

        const std::wstring cmd =
            L"\"" + cmdPath + L"\"" +
            (uninstall ? L" -vm uninstall" : L" -vm install") +
            L" -dll \"" + DllPath() + L"\"";

        raw = RunTool(cmd);

        // 不管成败，工具留下的中间文件都清掉
        CleanupTemp();

        if (raw.find("TRUE") != std::string::npos)
            return PatchOk;
        if (raw.find("repeat") != std::string::npos)
            return PatchRepeat;
        if (raw.find("notfound") != std::string::npos)
            return PatchNotFound;
        return PatchError;
    }
}

namespace aic_vm_ui
{
    inline HINSTANCE ModuleInstance()
    {
        static int s_moduleAnchor = 0;
        static HINSTANCE s_inst = reinterpret_cast<HINSTANCE>(&s_moduleAnchor);
        return s_inst;
    }

    inline HWND& CurrentWindow()
    {
        static HWND s_current = nullptr;
        return s_current;
    }

    // 正在执行安装/卸载：期间禁止关闭窗口
    inline bool& Busy()
    {
        static bool s_busy = false;
        return s_busy;
    }

    inline HWND OwnerOf(HWND hwnd)
    {
        HWND h = reinterpret_cast<HWND>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!h)
            h = ::GetWindow(hwnd, GW_OWNER);
        return h;
    }

    inline void CenterOnOwner(HWND hwnd, HWND hOwner)
    {
        if (!hwnd || !::IsWindow(hwnd) || !::IsWindow(hOwner))
            return;

        RECT rcOwner = { 0 }, rcSelf = { 0 };
        ::GetWindowRect(hOwner, &rcOwner);
        ::GetWindowRect(hwnd, &rcSelf);

        const int w = rcSelf.right - rcSelf.left;
        const int h = rcSelf.bottom - rcSelf.top;
        const int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - w) / 2;
        const int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - h) / 2;

        ::SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    inline HFONT MakeFont(int pointSize)
    {
        LOGFONTW lf;
        ::ZeroMemory(&lf, sizeof(lf));
        ::lstrcpyW(lf.lfFaceName, L"Microsoft YaHei");
        lf.lfHeight = -::MulDiv(pointSize, ::GetDeviceCaps(::GetDC(nullptr), LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        return ::CreateFontIndirectW(&lf);
    }

    // 前两个勾选框是强制的：点不掉，始终是勾选状态
    inline void KeepChecked(HWND hwnd, int id)
    {
        HWND h = ::GetDlgItem(hwnd, id);
        if (h && ::IsWindow(h))
            ::SendMessageW(h, BM_SETCHECK, BST_CHECKED, 0);
    }

    inline void KeepBothChecked(HWND hwnd)
    {
        KeepChecked(hwnd, AIC_VM_ID_EXE);
        KeepChecked(hwnd, AIC_VM_ID_DLL);
    }

    inline bool IsChecked(HWND hwnd, int id)
    {
        HWND h = ::GetDlgItem(hwnd, id);
        if (!h || !::IsWindowEnabled(h))
            return false;
        return ::SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    // 进入“执行中”：禁用按钮、把被点的按钮文字改成“正在安装/卸载…”，并禁止关闭
    inline void BeginBusy(HWND hwnd, int id, const wchar_t* busyText)
    {
        Busy() = true;

        HWND hOk = ::GetDlgItem(hwnd, AIC_VM_ID_OK);
        HWND hRevert = ::GetDlgItem(hwnd, AIC_VM_ID_REVERT);
        HWND hCancel = ::GetDlgItem(hwnd, AIC_VM_ID_CANCEL);

        if (hOk && ::IsWindow(hOk))
            ::EnableWindow(hOk, FALSE);
        if (hRevert && ::IsWindow(hRevert))
            ::EnableWindow(hRevert, FALSE);
        if (hCancel && ::IsWindow(hCancel))
            ::EnableWindow(hCancel, FALSE);

        HWND hBtn = ::GetDlgItem(hwnd, id);
        if (hBtn && ::IsWindow(hBtn))
            ::SetWindowTextW(hBtn, busyText);

        HMENU hSys = ::GetSystemMenu(hwnd, FALSE);
        if (hSys)
            ::EnableMenuItem(hSys, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        // 后面是同步阻塞的工作，这里强制重画一次，“正在安装…”才能马上看见
        ::RedrawWindow(hwnd, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }

    // 退出“执行中”：按钮文字改回来、按钮恢复可用
    inline void EndBusy(HWND hwnd, int id, const wchar_t* normalText)
    {
        Busy() = false;

        if (!hwnd || !::IsWindow(hwnd))
            return;

        HWND hBtn = ::GetDlgItem(hwnd, id);
        if (hBtn && ::IsWindow(hBtn))
            ::SetWindowTextW(hBtn, normalText);

        HWND hOk = ::GetDlgItem(hwnd, AIC_VM_ID_OK);
        HWND hRevert = ::GetDlgItem(hwnd, AIC_VM_ID_REVERT);
        HWND hCancel = ::GetDlgItem(hwnd, AIC_VM_ID_CANCEL);

        if (hOk && ::IsWindow(hOk))
            ::EnableWindow(hOk, TRUE);
        if (hRevert && ::IsWindow(hRevert))
            ::EnableWindow(hRevert, TRUE);
        if (hCancel && ::IsWindow(hCancel))
            ::EnableWindow(hCancel, TRUE);

        HMENU hSys = ::GetSystemMenu(hwnd, FALSE);
        if (hSys)
            ::EnableMenuItem(hSys, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);

        ::RedrawWindow(hwnd, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        ::SetForegroundWindow(hwnd);
    }

    // 安装流程：先改 dll，成功了才换 exe
    inline void DoInstall(HWND hwnd)
    {
        if (Busy())
            return;

        KeepBothChecked(hwnd);
        BeginBusy(hwnd, AIC_VM_ID_OK, L"正在安装…");

        std::wstring log;
        bool ok = true;

        // ------------------ 第 1 步：改 dll ------------------
        if (aic_vm_detail::IsFileInUse(aic_vm_detail::DllPath()))
        {
            ok = false;
            log += L"unsafeAssem.dll：文件被占用，请先关闭游戏\r\n";
        }
        else
        {
            std::string raw;
            const aic_vm_detail::PatchState state = aic_vm_detail::RunDllPatch(false, raw);

            if (state == aic_vm_detail::PatchOk)
                //log += L"unsafeAssem.dll：补丁已写入\r\n";
                int a = 1;
            else if (state == aic_vm_detail::PatchRepeat)
                //log += L"unsafeAssem.dll：补丁已存在，未重复写入\r\n";
                int a = 1;
            else if (state == aic_vm_detail::PatchNotFound)
            {
                ok = false;
                log += L"unsafeAssem.dll：版本不符，找不到补丁位置（未修改）\r\n";
            }
            else
            {
                ok = false;
                log += L"unsafeAssem.dll：调用失败 " + aic_vm_detail::WidenAnsi(raw) + L"\r\n";
            }
        }

        // ------------------ 第 2 步：dll 成功后才换 exe ------------------
        if (!ok)
        {
            log += L"主程序：dll 没有改成功，本次不替换 exe\r\n";
        }
        else
        {
            std::wstring err;
            bool changed = false;
            if (aic_vm_detail::InstallExe(err, changed))
            {
                log += changed
                    ? L"补丁安装成功！\r\n"
                    : L"已经安装过本补丁！无需重复安装。\r\n";
                // 之前就已经是启动器、又没有备份时，这次不会新增备份，提醒一下
                if (!aic_vm_detail::FileExists(aic_vm_detail::ExeBackupPath()))
                    log += L"主程序：注意，没有原版备份（原来就是启动器），卸载时无法还原 exe\r\n";
            }
            else
            {
                ok = false;
                //log += L"主程序：" + err + L"\r\n";
            }
        }

        aic_vm_detail::CleanupTemp();
        EndBusy(hwnd, AIC_VM_ID_OK, L"安装补丁");

        if (ok)
            log += L"\r\n安装完成，游戏已支持在虚拟机启动。";
        else
            log += L"\r\n有项目没有完成，详见上面的说明。";

        ::MessageBoxW(hwnd, log.c_str(), ok ? L"安装完成" : L"未全部成功",
            MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));

        if (ok)
            ::DestroyWindow(hwnd);
    }

    // 卸载流程：先还原 dll，再还原 exe
    inline void DoUninstall(HWND hwnd)
    {
        if (Busy())
            return;

        KeepBothChecked(hwnd);
        BeginBusy(hwnd, AIC_VM_ID_REVERT, L"正在卸载…");

        std::wstring log;
        bool ok = true;

        if (aic_vm_detail::IsFileInUse(aic_vm_detail::DllPath()))
        {
            ok = false;
            log += L"unsafeAssem.dll：文件被占用，请先关闭游戏\r\n";
        }
        else
        {
            std::string raw;
            const aic_vm_detail::PatchState state = aic_vm_detail::RunDllPatch(true, raw);

            if (state == aic_vm_detail::PatchOk)
                //log += L"unsafeAssem.dll：补丁已还原\r\n";
                int a = 1;
            else if (state == aic_vm_detail::PatchRepeat)
                //log += L"unsafeAssem.dll：本来就是原版，无需还原\r\n";
                int a = 1;
            else if (state == aic_vm_detail::PatchNotFound)
            {
                ok = false;
                log += L"unsafeAssem.dll：不是已知状态，未做修改\r\n";
            }
            else
            {
                ok = false;
                log += L"unsafeAssem.dll：调用失败 " + aic_vm_detail::WidenAnsi(raw) + L"\r\n";
            }
        }

        // dll 还原成功后才动 exe，和安装保持同一套顺序
        if (!ok)
        {
            log += L"主程序：dll 没有还原成功，本次不还原 exe\r\n";
        }
        else
        {
            std::wstring err;
            bool backupDeleted = false;
            if (aic_vm_detail::RestoreExe(err, backupDeleted))
                log += backupDeleted
                ? L"补丁卸载成功，备份 AliceInCradle.exe.bak 已删除\r\n"
                : L"补丁卸载成功！\r\n";
            else
            {
                ok = false;
                log += L"主程序：" + err + L"\r\n";
            }
        }

        aic_vm_detail::CleanupTemp();
        EndBusy(hwnd, AIC_VM_ID_REVERT, L"卸载补丁");

        if (ok)
            log += L"\r\n卸载完成，游戏已恢复成原版启动方式。";
        else
            log += L"\r\n有项目没有完成，详见上面的说明。";

        ::MessageBoxW(hwnd, log.c_str(), ok ? L"卸载完成" : L"未全部成功",
            MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));

        if (ok)
            ::DestroyWindow(hwnd);
    }

    inline void DoClose(HWND hwnd)
    {
        if (Busy())
        {
            ::MessageBeep(MB_ICONWARNING);
            return;
        }

        //if (::MessageBoxW(hwnd, L"确定放弃本次操作？", L"提示",  MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
        ::DestroyWindow(hwnd);
    }

    inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_NCCREATE:
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->hwndParent));
            break;   // 交给 DefWindowProc，让它返回 TRUE
        }
        case WM_CTLCOLORSTATIC:
        {
            ::SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
            return reinterpret_cast<LRESULT>(static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
        }
        case WM_COMMAND:
        {
            // 这里不回传按键给主窗口（用不到），全部在本窗口里处理
            const int id = LOWORD(wParam);

            // 前两个勾选框强制勾选，点不掉
            if (id == AIC_VM_ID_EXE || id == AIC_VM_ID_DLL)
            {
                KeepChecked(hwnd, id);
                return 0;
            }

            switch (id)
            {
            case AIC_VM_ID_OK:
                DoInstall(hwnd);
                return 0;
            case AIC_VM_ID_REVERT:
                DoUninstall(hwnd);
                return 0;
            case AIC_VM_ID_CANCEL:
                DoClose(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_CLOSE:
            // 执行过程中不接受关闭（含 Alt+F4、标题栏 ×）
            if (Busy())
            {
                ::MessageBeep(MB_ICONWARNING);
                return 0;
            }
            DoClose(hwnd);
            return 0;
        case WM_SYSCOMMAND:
            // Alt+F4 / 系统菜单里的“关闭”
            if ((wParam & 0xFFF0) == SC_CLOSE && Busy())
            {
                ::MessageBeep(MB_ICONWARNING);
                return 0;
            }
            break;
        case WM_DESTROY:
            if (CurrentWindow() == hwnd)
                CurrentWindow() = nullptr;
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    inline bool EnsureClassRegistered()
    {
        static bool s_done = false;
        if (s_done)
            return true;

        HINSTANCE hInst = ModuleInstance();

        WNDCLASSEXW existing = { 0 };
        if (::GetClassInfoExW(hInst, AIC_VM_WINDOW_CLASSNAME, &existing))
        {
            s_done = true;
            return true;
        }

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &WindowProc;
        wc.hInstance = hInst;
        wc.hIcon = nullptr;
        wc.hIconSm = nullptr;
        wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));   // IDC_ARROW
        wc.hbrBackground = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
        wc.lpszClassName = AIC_VM_WINDOW_CLASSNAME;

        s_done = (::RegisterClassExW(&wc) != 0);
        return s_done;
    }

} // namespace aic_vm_ui

// 显示虚拟机检测绕过窗口（模态，返回时窗口已关闭）
inline bool ShowAicVmPatchWindow()
{
    using namespace aic_vm_ui;

    if (GamePath.empty())
    {
        ::MessageBoxW(nullptr, L"请先选择游戏目录。", L"错误", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (CurrentWindow() && ::IsWindow(CurrentWindow()))
    {
        ::SetForegroundWindow(CurrentWindow());
        return true;
    }

    if (!EnsureClassRegistered())
        return false;

    HWND hOwner = hWnd;
    if (!::IsWindow(hOwner))
        hOwner = nullptr;

    HWND hwnd = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        AIC_VM_WINDOW_CLASSNAME,
        AIC_VM_WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, AIC_VM_WINDOW_WIDTH, AIC_VM_WINDOW_HEIGHT,
        hOwner, nullptr, ModuleInstance(), nullptr);

    if (!hwnd)
        return false;

    CurrentWindow() = hwnd;
    Busy() = false;

    const HFONT hFont = MakeFont(12);
    const HFONT hSmall = MakeFont(10);
    const HINSTANCE csInst = ModuleInstance();

    HWND tTitle = ::CreateWindowW(L"STATIC",
        L"解除虚拟机检测（虚拟机补丁）",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 14, 330, 20, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND tHint = ::CreateWindowW(L"STATIC",
        L"说明：\r\n"
        L"解决游戏在虚拟机环境下无法启动问题。\r\n"
        L"本补丁仅供开发者学习使用。\r\n"
        L"严禁将本补丁用于违规用途。严禁二次分发。\r\n"
        L"安装/卸载期间窗口不可关闭，约5s。\r\n"
        L"补丁只作用于当前选中的游戏目录。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 50, 350, 130, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tHint, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    /*HWND tPath = ::CreateWindowW(L"STATIC",
        GamePath.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
        25, 172, 350, 20, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tPath, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));*/

    HWND bInstall = ::CreateWindowW(L"BUTTON", L"安装补丁",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE | BS_DEFPUSHBUTTON,
        30, 205, 120, 35, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_OK)), csInst, nullptr);
    ::SendMessageW(bInstall, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND bRevert = ::CreateWindowW(L"BUTTON", L"卸载补丁",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE,
        215, 205, 120, 35, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_REVERT)), csInst, nullptr);
    ::SendMessageW(bRevert, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));


    if (hOwner)
        CenterOnOwner(hwnd, hOwner);

    bool modal = false;
    if (hOwner)
    {
        ::EnableWindow(hOwner, FALSE);
        modal = true;
    }

    ::ShowWindow(hwnd, SW_SHOW);
    ::SetForegroundWindow(hwnd);
    ::UpdateWindow(hwnd);

    if (!modal)
        return true;

    // 嵌套消息循环，直到窗口被销毁
    MSG message;
    while (::IsWindow(hwnd))
    {
        const BOOL got = ::GetMessageW(&message, nullptr, 0, 0);
        if (got <= 0)
        {
            if (got == 0)
                ::PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }

        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE)
        {
            DoClose(hwnd);
            continue;
        }

        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    if (::IsWindow(hOwner))
    {
        ::EnableWindow(hOwner, TRUE);
        ::SetActiveWindow(hOwner);
    }

    return true;
}

#endif // AIC_VM_PATCH_WINDOW_H
