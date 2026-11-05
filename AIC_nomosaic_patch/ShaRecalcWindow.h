#pragma once
#ifndef AIC_SHA_RECALC_WINDOW_H
#define AIC_SHA_RECALC_WINDOW_H

// ===========================================================================
//  AIC 哈希清单重算（窗口 + 重算实现）
// ---------------------------------------------------------------------------
//  游戏目录里放着一份“文件 -> SHA-256”清单，打完任何补丁后对应文件的
//  SHA-256 就变了，必须按原格式重写清单，否则游戏的完整性校验会报错。
//
//  清单格式（AIC 0.30i 实测，纯 ASCII、LF 换行、末尾没有空行）：
//
//      Managed/unsafeAssem.dll                            <- 路径行
//      4a76d01ab280c34e...b114e                           <- 该文件的 SHA-256
//      Managed/ttr.dll                                    <- 64 位小写十六进制
//      738d0e5be6f3f3c6...929f2
//      Plugins/x86_64/nanosockets.dll
//      037985bd36c9b698...aaf00
//
//      * 路径相对于 <游戏目录>\AliceInCradle_Data，用 '/' 分隔
//      * 一行路径、一行哈希，交替排列，顺序保持清单原样
//      * 只有 LF（0x0A），没有 CR，没有 BOM
//
//  用法（和 CheatWindow.h 一样是纯头文件，窗口尺寸/样式/模态行为保持一致）：
//
//      #include "define.h"            // 需要全局 GamePath
//      #include "ShaRecalcWindow.h"
//      ...
//      ShowAicShaRecalcWindow();      // 400x320、居中、模态、微软雅黑
//
//      重算核心也可以直接调用（VM 补丁窗口就是这么用的）：
//      int n = aic_sha_detail::RecalcManifest(aic_sha_detail::ManifestPath(),
//                                             aic_sha_detail::DataDir());
//      // n >= 0 为更新的条目数，-1 表示清单读/写失败
//
//  路径全部由全局变量 GamePath 拼出来，存在性由主程序保证，这里不再判断。
// ===========================================================================

#include <windows.h>
#include <wincrypt.h>   // CryptoAPI：本工程 framework.h 定义了 WIN32_LEAN_AND_MEAN，
                        // windows.h 不会自动带上 wincrypt.h，必须显式包含，否则
                        // HCRYPTPROV / CryptAcquireContextW 等全部未声明
#include <string>
#include <vector>

#include "define.h"

#pragma comment(lib, "advapi32.lib")

// 清单文件名：默认 _sha.txt；想换成别的名字（例如 hash.txt）就在包含本头文件前定义
#ifndef AIC_SHA_MANIFEST_NAME
#define AIC_SHA_MANIFEST_NAME   L"_sha.txt"
#endif

#define AIC_SHA_WINDOW_CLASSNAME    L"AicShaRecalc_WindowClass"
#define AIC_SHA_WINDOW_TITLE        L"重算哈希清单"
#define AIC_SHA_WINDOW_WIDTH        400
#define AIC_SHA_WINDOW_HEIGHT       320

#define AIC_SHA_ID_LIST             0x7F31
#define AIC_SHA_ID_OK               0x7F32
#define AIC_SHA_ID_CANCEL           0x7F33

// ===========================================================================
//  重算实现
// ===========================================================================
namespace aic_sha_detail
{
    // ------------------------------ 编码转换 ---------------------------------

    inline std::string Narrow(const std::wstring& s)
    {
        if (s.empty())
            return std::string();

        const int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                            nullptr, 0, nullptr, nullptr);
        if (n <= 0)
            return std::string();

        std::string out((size_t)n, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
                              &out[0], n, nullptr, nullptr);
        return out;
    }

    inline std::wstring Widen(const std::string& s)
    {
        if (s.empty())
            return std::wstring();

        const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n <= 0)
            return std::wstring();

        std::wstring out((size_t)n, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
        return out;
    }

    inline std::string TrimAscii(const std::string& s)
    {
        size_t b = 0, e = s.size();
        while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
        while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
        return s.substr(b, e - b);
    }

    // 清单里用 '/' 分隔，文件系统要用 '\'
    inline std::wstring ToBackslash(const std::wstring& s)
    {
        std::wstring out = s;
        for (size_t i = 0; i < out.size(); ++i)
            if (out[i] == L'/') out[i] = L'\\';
        return out;
    }

    // ------------------------------ 路径 -------------------------------------

    // <游戏目录>\AliceInCradle_Data —— 清单里的相对路径以它为基准
    inline std::wstring DataDir()
    {
        return GamePath + L"\\AliceInCradle_Data";
    }

    // <游戏目录>\AliceInCradle_Data\StreamingAssets\_sha.txt
    // 默认文件名找不到时退回到另一个常见写法（_sha.txt / hash.txt 二选一）
    inline std::wstring ManifestPath()
    {
        const std::wstring dir = DataDir() + L"\\StreamingAssets\\";

        const std::wstring primary = dir + AIC_SHA_MANIFEST_NAME;
        if (::GetFileAttributesW(primary.c_str()) != INVALID_FILE_ATTRIBUTES)
            return primary;

        const wchar_t* alt = (::lstrcmpiW(AIC_SHA_MANIFEST_NAME, L"_sha.txt") == 0)
            ? L"hash.txt" : L"_sha.txt";
        const std::wstring fallback = dir + alt;
        if (::GetFileAttributesW(fallback.c_str()) != INVALID_FILE_ATTRIBUTES)
            return fallback;

        return primary;
    }

    // ------------------------------ SHA-256 ----------------------------------

    inline std::wstring DigestToHex(const BYTE* p, DWORD len)
    {
        static const wchar_t* kHex = L"0123456789abcdef";
        std::wstring s;
        s.resize((size_t)len * 2);
        for (DWORD i = 0; i < len; ++i)
        {
            s[(size_t)i * 2] = kHex[(p[i] >> 4) & 0x0F];
            s[(size_t)i * 2 + 1] = kHex[p[i] & 0x0F];
        }
        return s;
    }

    inline bool Sha256Begin(HCRYPTPROV& prov, HCRYPTHASH& hash)
    {
        prov = 0;
        hash = 0;
        if (!::CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            return false;
        if (!::CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash))
        {
            ::CryptReleaseContext(prov, 0);
            prov = 0;
            return false;
        }
        return true;
    }

    inline bool Sha256End(HCRYPTPROV prov, HCRYPTHASH hash, bool ok, std::wstring& hexOut)
    {
        BYTE digest[32] = { 0 };
        DWORD len = (DWORD)sizeof(digest);

        if (ok)
            ok = (::CryptGetHashParam(hash, HP_HASHVAL, digest, &len, 0) != FALSE) && len == 32;

        ::CryptDestroyHash(hash);
        ::CryptReleaseContext(prov, 0);

        if (!ok)
            return false;

        hexOut = DigestToHex(digest, len);
        return true;
    }

    // 内存块的 SHA-256（64 位小写十六进制）
    inline bool Sha256OfMemory(const void* data, size_t size, std::wstring& hexOut)
    {
        HCRYPTPROV prov = 0;
        HCRYPTHASH hash = 0;
        if (!Sha256Begin(prov, hash))
            return false;

        bool ok = (size == 0) ||
            (::CryptHashData(hash, static_cast<const BYTE*>(data), (DWORD)size, 0) != FALSE);

        if (!ok)
        {
            ::CryptDestroyHash(hash);
            ::CryptReleaseContext(prov, 0);
            return false;
        }
        return Sha256End(prov, hash, true, hexOut);
    }

    // 文件的 SHA-256（按块读，几十 MB 也不会占内存）
    inline bool Sha256OfFile(const std::wstring& path, std::wstring& hexOut)
    {
        HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        HCRYPTPROV prov = 0;
        HCRYPTHASH hash = 0;
        bool ok = Sha256Begin(prov, hash);

        if (ok)
        {
            std::vector<BYTE> buf(1 << 20);   // 1 MB
            for (;;)
            {
                DWORD got = 0;
                if (!::ReadFile(hFile, &buf[0], (DWORD)buf.size(), &got, nullptr))
                {
                    ok = false;
                    break;
                }
                if (got == 0)
                    break;
                if (!::CryptHashData(hash, &buf[0], got, 0))
                {
                    ok = false;
                    break;
                }
            }
        }

        ::CloseHandle(hFile);

        if (!ok)
        {
            ::CryptDestroyHash(hash);
            ::CryptReleaseContext(prov, 0);
            return false;
        }
        return Sha256End(prov, hash, true, hexOut);
    }

    // ------------------------------ 清单读写 ---------------------------------

    struct Entry
    {
        std::wstring rel;      // Managed/unsafeAssem.dll
        std::wstring hash;     // 64 位小写十六进制
    };

    inline bool ReadManifest(const std::wstring& path, std::vector<Entry>& out)
    {
        out.clear();

        HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        std::string raw;
        char buf[8192];
        for (;;)
        {
            DWORD got = 0;
            if (!::ReadFile(hFile, buf, (DWORD)sizeof(buf), &got, nullptr))
            {
                ::CloseHandle(hFile);
                return false;
            }
            if (got == 0)
                break;
            raw.append(buf, got);
        }
        ::CloseHandle(hFile);

        // 按 LF 切行，跳过空行（CR 一并去掉，兼容被记事本改写过的情况）
        std::vector<std::string> lines;
        std::string cur;
        for (size_t i = 0; i < raw.size(); ++i)
        {
            const char c = raw[i];
            if (c == '\n')
            {
                const std::string t = TrimAscii(cur);
                if (!t.empty()) lines.push_back(t);
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }
        {
            const std::string t = TrimAscii(cur);
            if (!t.empty()) lines.push_back(t);
        }

        // 一行路径、一行哈希
        for (size_t i = 0; i + 1 < lines.size(); i += 2)
        {
            Entry e;
            e.rel = Widen(lines[i]);
            e.hash = Widen(lines[i + 1]);
            out.push_back(e);
        }
        return !out.empty();
    }

    // 写回，格式与游戏原文件一致：路径行 + 哈希行，LF 结尾，无 BOM
    inline bool WriteManifest(const std::wstring& path, const std::vector<Entry>& entries)
    {
        std::string raw;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            raw += Narrow(entries[i].rel);
            raw += '\n';
            raw += Narrow(entries[i].hash);
            raw += '\n';
        }

        // 先写临时文件再替换，避免写一半把清单弄坏
        const std::wstring tmp = path + L".tmp";

        HANDLE hFile = ::CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        bool ok = true;
        size_t written = 0;
        while (written < raw.size())
        {
            DWORD chunk = (DWORD)((raw.size() - written) > 0x10000 ? 0x10000 : (raw.size() - written));
            DWORD done = 0;
            if (!::WriteFile(hFile, raw.data() + written, chunk, &done, nullptr) || done == 0)
            {
                ok = false;
                break;
            }
            written += done;
        }
        ::FlushFileBuffers(hFile);
        ::CloseHandle(hFile);

        if (!ok)
        {
            ::DeleteFileW(tmp.c_str());
            return false;
        }

        if (!::MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            ::DeleteFileW(tmp.c_str());
            return false;
        }
        return true;
    }

    // ------------------------------ 重算 -------------------------------------

    // 把清单里每个文件重新算一遍 SHA-256 并写回（只改不一致的条目）。
    // 返回：更新的条目数；-1 = 清单读取或写入失败。
    // changedFiles 不为空时，把被更新的相对路径追加进去。
    inline int RecalcManifest(const std::wstring& manifest, const std::wstring& baseDir,
                              std::vector<std::wstring>* changedFiles = nullptr)
    {
        std::vector<Entry> entries;
        if (!ReadManifest(manifest, entries))
            return -1;

        int changed = 0;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const std::wstring full = baseDir + L"\\" + ToBackslash(entries[i].rel);

            std::wstring hex;
            if (!Sha256OfFile(full, hex))
                continue;                       // 读不到就保留原值，不破坏清单

            if (hex != entries[i].hash)
            {
                entries[i].hash = hex;
                ++changed;
                if (changedFiles)
                    changedFiles->push_back(entries[i].rel);
            }
        }

        if (changed > 0 && !WriteManifest(manifest, entries))
            return -1;

        return changed;
    }
}

// ===========================================================================
//  窗口
// ===========================================================================
namespace aic_sha_ui
{
    // 取「本头文件所在模块」的实例句柄（EXE / DLL 都适用）
    inline HINSTANCE ModuleInstance()
    {
        static int s_moduleAnchor = 0;
        static HINSTANCE s_inst = reinterpret_cast<HINSTANCE>(&s_moduleAnchor);
        return s_inst;
    }

    // 当前正在显示的窗口句柄
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

    // 生成清单当前状态的文字报告（显示在窗口里的只读文本框）
    inline std::wstring BuildReport(const std::wstring& manifest, const std::wstring& baseDir)
    {
        std::vector<aic_sha_detail::Entry> entries;
        if (!aic_sha_detail::ReadManifest(manifest, entries))
            return L"读取清单失败：\r\n" + manifest;

        int diff = 0, missing = 0;
        std::wstring detail;

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const std::wstring full = baseDir + L"\\" + aic_sha_detail::ToBackslash(entries[i].rel);

            std::wstring hex;
            if (!aic_sha_detail::Sha256OfFile(full, hex))
            {
                ++missing;
                detail += L"[读不到]  " + entries[i].rel + L"\r\n";
                continue;
            }
            if (hex != entries[i].hash)
            {
                ++diff;
                detail += L"[不一致]  " + entries[i].rel + L"\r\n";
            }
        }

        std::wstring text;
        //text += L"清单：" + manifest + L"\r\n";
        text += L"条目：" + std::to_wstring(entries.size()) +
            L"    不一致：" + std::to_wstring(diff) +
            L"    读不到：" + std::to_wstring(missing) + L"\r\n";
        text += L"-------------------------------------------------\r\n";

        if (diff == 0 && missing == 0)
            text += L"清单与文件完全一致，不需要重算。\r\n";
        else
            text += detail + L"\r\n点「重算并写入」按原格式写回清单。\r\n";

        return text;
    }

    inline void RefreshList(HWND hwnd)
    {
        HWND hList = ::GetDlgItem(hwnd, AIC_SHA_ID_LIST);
        if (!hList)
            return;

        const std::wstring text =
            BuildReport(aic_sha_detail::ManifestPath(), aic_sha_detail::DataDir());
        ::SetWindowTextW(hList, text.c_str());
    }

    // 点「重算并写入」
    inline void DoRecalc(HWND hwnd)
    {
        const std::wstring manifest = aic_sha_detail::ManifestPath();
        const std::wstring baseDir = aic_sha_detail::DataDir();

        ::SetWindowTextW(::GetDlgItem(hwnd, AIC_SHA_ID_OK), L"正在重算...");
        ::EnableWindow(hwnd, FALSE);

        const int n = aic_sha_detail::RecalcManifest(manifest, baseDir);

        ::EnableWindow(hwnd, TRUE);
        ::SetWindowTextW(::GetDlgItem(hwnd, AIC_SHA_ID_OK), L"重算并写入");
        RefreshList(hwnd);

        if (n < 0)
        {
            ::MessageBoxW(hwnd, L"清单读写失败。\n请确认游戏目录正确，且文件没有被占用或设为只读。",
                L"重算失败", MB_OK | MB_ICONHAND);
            return;
        }

        if (n == 0)
        {
            ::MessageBoxW(hwnd, L"所有条目本来就一致，没有写入改动。",
                L"提示", MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::wstring msg = L"已重算并写回 " + std::to_wstring(n) + L" 项。\n\n" + manifest;
        ::MessageBoxW(hwnd, msg.c_str(), L"完成", MB_OK | MB_ICONINFORMATION);
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
            switch (LOWORD(wParam))
            {
            case AIC_SHA_ID_OK:
                DoRecalc(hwnd);
                return 0;
            case AIC_SHA_ID_CANCEL:
                ::DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_CLOSE:
            ::DestroyWindow(hwnd);      // 只读窗口，不用确认
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
        if (::GetClassInfoExW(hInst, AIC_SHA_WINDOW_CLASSNAME, &existing))
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
        wc.lpszClassName = AIC_SHA_WINDOW_CLASSNAME;

        s_done = (::RegisterClassExW(&wc) != 0);
        return s_done;
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

} // namespace aic_sha_ui

// 显示重算窗口（模态，返回时窗口已关闭）
inline bool ShowAicShaRecalcWindow()
{
    using namespace aic_sha_ui;

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
        AIC_SHA_WINDOW_CLASSNAME,
        AIC_SHA_WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, AIC_SHA_WINDOW_WIDTH, AIC_SHA_WINDOW_HEIGHT,
        hOwner, nullptr, ModuleInstance(), nullptr);

    if (!hwnd)
        return false;

    CurrentWindow() = hwnd;

    const HFONT hFont = MakeFont(12);
    const HFONT hSmall = MakeFont(10);
    const HINSTANCE csInst = ModuleInstance();

    HWND tTitle = ::CreateWindowW(L"STATIC",
        L"哈希清单重算",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 14, 330, 20, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    HWND tDesc = ::CreateWindowW(L"STATIC",
        L"解决游戏启动时报错游戏文件完整性校验失败。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 38, 345, 18, hwnd, nullptr, csInst, nullptr);
    ::SendMessageW(tDesc, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));

    HWND hList = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        20, 60, 345, 148, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_SHA_ID_LIST)), csInst, nullptr);
    ::SendMessageW(hList, WM_SETFONT, reinterpret_cast<WPARAM>(hSmall), MAKELPARAM(TRUE, 0));

    HWND bOk = ::CreateWindowW(L"BUTTON", L"重算并写入",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE ,
        120, 230, 130, 30, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_SHA_ID_OK)), csInst, nullptr);
    ::SendMessageW(bOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));

    /*HWND bCancel = ::CreateWindowW(L"BUTTON", L"关闭",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE,
        220, 220, 130, 30, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(AIC_SHA_ID_CANCEL)), csInst, nullptr);
    ::SendMessageW(bCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));*/

    RefreshList(hwnd);

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
            ::DestroyWindow(hwnd);
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

#endif // AIC_SHA_RECALC_WINDOW_H
