#pragma once
#ifndef AIC_VM_PATCH_WINDOW_H
#define AIC_VM_PATCH_WINDOW_H

// ===========================================================================
//  虚拟机检测绕过（虚拟补丁）窗口
// ---------------------------------------------------------------------------
//  实现 AIC-0.30i-VM-bypass.md 里的整套方案，全部由这个窗口一键完成：
//
//   1) 换宿主 exe：把资源里的启动器（IDR_LAUNCHER）写成 <游戏目录>\AliceInCradle.exe
//      —— 文件名必须还叫 AliceInCradle.exe，Unity 靠 <exe名>_Data 找数据目录。
//      原版 exe 先备份成 <游戏目录>\_AliceInCradle_shield.exe，卸载时原样还原。
//      壳（.aic0/.aic1/.aic2）根本不执行，VM 检测 / 反调试 / NtRaiseHardError 全不参与。
//
//   2) 托管层两个 IL 补丁：调用 AIC_nomosaic_patch_cmd.exe 的新指令
//          -vm install   -dll <...>\AliceInCradle_Data\Managed\unsafeAssem.dll
//          -vm uninstall -dll <同上>
//      补丁 A  IL_02B9 ldc.i4.0 -> ldc.i4.1（虚拟机判据不再清零 ok 标志）
//      补丁 B  IL_0219 版本串比较整段短路成 br.s IL_022D（因为 exe 已经换掉了）
//      输出约定沿用原来的 TRUE / repeat / notfound。
//
//   3) 重算哈希清单：打个补丁后 unsafeAssem.dll 的 SHA-256 变了，
//      调用 ShaRecalcWindow.h 里的 RecalcManifest() 按原格式写回。
//
//  用法（窗口写法/尺寸/模态行为与 CheatWindow.h 完全一致，不需要把按键回传给主窗口）：
//
//      #include "VmPatchWindow.h"   // 内部已包含 define.h + ShaRecalcWindow.h
//      ...
//      ShowAicVmPatchWindow();       // 400x320、居中、模态、微软雅黑
//
//  除了 define.h 里的全局变量（GamePath / cmdPath / hWnd），不依赖工程里任何其它文件：
//  命令行调用、文件占用判断、SHA-256、清单重写都在本文件和 ShaRecalcWindow.h 里。
//
//  路径全部用 define.h 里的全局 GamePath 拼出来，存在性由主程序保证，这里不再判断。
// ===========================================================================

#include <windows.h>
#include <string>
#include <vector>

#include "define.h"
#include "ShaRecalcWindow.h"    // SHA-256 + 清单重算（内部已包含 wincrypt.h）

#ifndef IDR_LAUNCHER
#include "resource.h"           // IDR_LAUNCHER（启动器 exe 的 RT_RCDATA 资源）
#endif

#define AIC_VM_WINDOW_CLASSNAME     L"AicVmPatch_WindowClass"
#define AIC_VM_WINDOW_TITLE         L"虚拟机检测绕过"
#define AIC_VM_WINDOW_WIDTH         400
#define AIC_VM_WINDOW_HEIGHT        320

#define AIC_VM_ID_EXE               9101
#define AIC_VM_ID_DLL               9102
#define AIC_VM_ID_HASH              9103

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

    inline std::wstring ShieldPath()
    {
        return GamePath + L"\\_AliceInCradle_shield.exe";
    }

    inline std::wstring DllPath()
    {
        return GamePath + L"\\AliceInCradle_Data\\Managed\\unsafeAssem.dll";
    }

    // ------------------------------ 资源 / 写文件 ----------------------------

    inline bool ReadResourceBytes(int id, const wchar_t* type, std::vector<BYTE>& out)
    {
        out.clear();

        HRSRC hRes = ::FindResourceW(nullptr, MAKEINTRESOURCEW(id), type);
        if (!hRes)
            return false;

        const DWORD size = ::SizeofResource(nullptr, hRes);
        HGLOBAL hData = ::LoadResource(nullptr, hRes);
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

    // ------------------------------ 第 1 步：换 exe --------------------------

    // 返回 false 时 err 里是原因
    inline bool InstallExe(std::wstring& err)
    {
        std::vector<BYTE> launcher;
        if (!ReadResourceBytes(IDR_LAUNCHER, L"RT_RCDATA", launcher))
        {
            err = L"资源里找不到启动器（IDR_LAUNCHER），请重新编译本工具";
            return false;
        }

        // 已经换过了就别再备份一次（否则会把启动器当成“原版”备份下来）
        std::wstring launcherHex, currentHex;
        const bool isLauncher =
            aic_sha_detail::Sha256OfMemory(&launcher[0], launcher.size(), launcherHex)
            && aic_sha_detail::Sha256OfFile(ExePath(), currentHex)
            && currentHex == launcherHex;

        if (isLauncher)
            return true;

        if (!FileExists(ShieldPath()))
        {
            ::SetFileAttributesW(ExePath().c_str(), FILE_ATTRIBUTE_NORMAL);
            if (!::CopyFileW(ExePath().c_str(), ShieldPath().c_str(), FALSE))
            {
                err = L"备份原版主程序失败（游戏可能正在运行）";
                return false;
            }
        }

        if (!WriteAllBytes(ExePath(), launcher))
        {
            err = L"写入启动器失败（游戏可能正在运行，或文件被设为只读）";
            return false;
        }
        return true;
    }

    inline bool RestoreExe(std::wstring& err)
    {
        if (!FileExists(ShieldPath()))
        {
            err = L"没有找到原版主程序备份 _AliceInCradle_shield.exe";
            return false;
        }

        ::SetFileAttributesW(ExePath().c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!::CopyFileW(ShieldPath().c_str(), ExePath().c_str(), FALSE))
        {
            err = L"还原主程序失败（游戏可能正在运行）";
            return false;
        }
        return true;
    }

    // ------------------------------ 第 2 步：IL 补丁 -------------------------

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

        if (raw.find("TRUE") != std::string::npos)
            return PatchOk;
        if (raw.find("repeat") != std::string::npos)
            return PatchRepeat;
        if (raw.find("notfound") != std::string::npos)
            return PatchNotFound;
        return PatchError;
    }

    // ------------------------------ 第 3 步：重算哈希 ------------------------

    // 返回：更新条目数；-1 = 失败
    inline int RecalcHash()
    {
        return aic_sha_detail::RecalcManifest(aic_sha_detail::ManifestPath(),
            aic_sha_detail::DataDir());
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

    inline bool IsChecked(HWND hwnd, int id)
    {
        HWND h = ::GetDlgItem(hwnd, id);
        if (!h || !::IsWindowEnabled(h))
            return false;
        return ::SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    // 安装流程（顺序很重要：先换 exe，再打托管层补丁，最后重算哈希）
    inline void DoInstall(HWND hwnd)
    {
        const bool doExe = IsChecked(hwnd, AIC_VM_ID_EXE);
        const bool doDll = IsChecked(hwnd, AIC_VM_ID_DLL);
        const bool doHash = IsChecked(hwnd, AIC_VM_ID_HASH);

        if (!doExe && !doDll && !doHash)
        {
            ::MessageBoxW(hwnd, L"请先勾选要执行的项目。", L"提示", MB_OK | MB_ICONINFORMATION);
            return;
        }

        ::EnableWindow(hwnd, FALSE);
        std::wstring log;
        bool ok = true;

        if (doExe)
        {
            std::wstring err;
            if (aic_vm_detail::InstallExe(err))
                log += L"主程序：已替换为启动器\r\n";
            else
            {
                ok = false;
                log += L"主程序：" + err + L"\r\n";
            }
        }

        if (doDll)
        {
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
                    log += L"unsafeAssem.dll：补丁 A+B 已写入\r\n";
                else if (state == aic_vm_detail::PatchRepeat)
                    log += L"unsafeAssem.dll：补丁已存在，未重复写入\r\n";
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
        }

        if (doHash)
        {
            const int n = aic_vm_detail::RecalcHash();
            if (n < 0)
            {
                ok = false;
                log += L"哈希清单：读写失败\r\n";
            }
            else
            {
                log += L"哈希清单：已更新 " + std::to_wstring(n) + L" 项\r\n";
            }
        }

        ::EnableWindow(hwnd, TRUE);

        if (ok)
            log += L"\r\n安装完成，直接用 AliceInCradle.exe 启动游戏。";
        else
            log += L"\r\n有项目没有完成，详见上面的说明。";

        ::MessageBoxW(hwnd, log.c_str(), ok ? L"安装完成" : L"未全部成功",
            MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));

        if (ok)
            ::DestroyWindow(hwnd);
    }

    // 卸载流程：先还原托管层，再还原 exe，最后重算哈希
    inline void DoUninstall(HWND hwnd)
    {
        ::EnableWindow(hwnd, FALSE);
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
                log += L"unsafeAssem.dll：补丁已还原\r\n";
            else if (state == aic_vm_detail::PatchRepeat)
                log += L"unsafeAssem.dll：本来就是原版，无需还原\r\n";
            else if (state == aic_vm_detail::PatchNotFound)
            {
                ok = false;
                log += L"unsafeAssem.dll：字节不是已知状态，未做修改\r\n";
            }
            else
            {
                ok = false;
                log += L"unsafeAssem.dll：调用失败 " + aic_vm_detail::WidenAnsi(raw) + L"\r\n";
            }
        }

        {
            std::wstring err;
            if (aic_vm_detail::RestoreExe(err))
                log += L"主程序：已还原原版 exe\r\n";
            else
            {
                ok = false;
                log += L"主程序：" + err + L"\r\n";
            }
        }

        {
            const int n = aic_vm_detail::RecalcHash();
            if (n < 0)
            {
                ok = false;
                log += L"哈希清单：读写失败\r\n";
            }
            else
            {
                log += L"哈希清单：已更新 " + std::to_wstring(n) + L" 项\r\n";
            }
        }

        ::EnableWindow(hwnd, TRUE);

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
        if (::MessageBoxW(hwnd, L"确定放弃本次操作？", L"提示",
            MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
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
            switch (LOWORD(wParam))
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
            DoClose(hwnd);
            return 0;
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

    const HFONT hFont = MakeFont(12);
    const HFONT hSmall = MakeFont(10);
    const HINSTANCE csInst = ModuleInstance();

    HWND tTitle = ::CreateWindowW(L"STATIC",
        L"虚拟机检测绕过（虚拟补丁）",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 14, 330, 20, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND cExe = ::CreateWindowW(L"BUTTON",
        L"替换游戏主程序（备份为 _AliceInCradle_shield.exe）",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        25, 45, 350, 22, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_EXE)), csInst, nullptr);
    ::SendMessageW(cExe, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));
    ::SendMessageW(cExe, BM_SETCHECK, BST_CHECKED, 0);

    HWND cDll = ::CreateWindowW(L"BUTTON",
        L"写入 unsafeAssem.dll 补丁 A+B（环境/版本判定）",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        25, 70, 350, 22, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_DLL)), csInst, nullptr);
    ::SendMessageW(cDll, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));
    ::SendMessageW(cDll, BM_SETCHECK, BST_CHECKED, 0);

    HWND cHash = ::CreateWindowW(L"BUTTON",
        L"完成后重算哈希清单（_sha.txt / hash.txt）",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        25, 95, 350, 22, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_HASH)), csInst, nullptr);
    ::SendMessageW(cHash, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));
    ::SendMessageW(cHash, BM_SETCHECK, BST_CHECKED, 0);

    HWND tHint = ::CreateWindowW(L"STATIC",
        L"安装后请用游戏目录里的 AliceInCradle.exe 启动。\r\n"
        L"安装/卸载期间窗口卡顿属于正常现象，请勿关闭工具。\r\n"
        L"补丁只作用于当前选中的游戏目录。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        25, 125, 350, 62, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tHint, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));

    HWND tPath = ::CreateWindowW(L"STATIC",
        GamePath.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
        25, 190, 350, 20, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tPath, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));

    HWND bInstall = ::CreateWindowW(L"BUTTON", L"安装补丁",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE | BS_DEFPUSHBUTTON,
        30, 220, 110, 30, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_OK)), csInst, nullptr);
    ::SendMessageW(bInstall, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND bRevert = ::CreateWindowW(L"BUTTON", L"卸载补丁",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE,
        145, 220, 110, 30, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_REVERT)), csInst, nullptr);
    ::SendMessageW(bRevert, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND bCancel = ::CreateWindowW(L"BUTTON", L"放弃更改",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE,
        260, 220, 110, 30, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_VM_ID_CANCEL)), csInst, nullptr);
    ::SendMessageW(bCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

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
