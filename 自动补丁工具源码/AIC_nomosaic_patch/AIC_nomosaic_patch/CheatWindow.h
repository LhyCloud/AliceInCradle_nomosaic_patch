
#pragma once
#ifndef CHEAT_SETTINGS_WINDOW_H
#define CHEAT_SETTINGS_WINDOW_H

#include <windows.h>
#define AIC_DEBUG_CONFIG_IMPLEMENTATION
#include "debug_config.h"
#include "define.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef CHEAT_SETTINGS_MAIN_WINDOW
#define CHEAT_SETTINGS_MAIN_WINDOW hWnd
#endif

#ifdef CHEAT_SETTINGS_DECLARE_MAIN_WINDOW
extern HWND CHEAT_SETTINGS_MAIN_WINDOW;
#endif

#define CHEAT_SETTINGS_CLASSNAME   L"CheatSettings_WindowClass"

#ifndef CHEAT_SETTINGS_TEXT_TITLE
#define CHEAT_SETTINGS_TEXT_TITLE  L"游戏调试设置"   // 游戏作弊设置
#endif
#ifndef CHEAT_SETTINGS_TEXT_OK
#define CHEAT_SETTINGS_TEXT_OK     L"\u786E\u5B9A"                            // 确定
#endif
#ifndef CHEAT_SETTINGS_TEXT_CANCEL
#define CHEAT_SETTINGS_TEXT_CANCEL L"\u53D6\u6D88"                            // 取消
#endif


#define CHEAT_SETTINGS_WIDTH       400
#define CHEAT_SETTINGS_HEIGHT      320


#define CHEAT_SETTINGS_ID_OK       0x7F01
#define CHEAT_SETTINGS_ID_CANCEL   0x7F02

// 窗口销毁时发给 hWnd 的通知消息（wParam/lParam 都为 0）
#define CHEAT_SETTINGS_WM_CLOSED   (WM_APP + 0x5E1)

#include "debug_config.h"

// 复选框起始 ID；第 i 个键对应 9001 + i
#ifndef DEBUG_CHECKBOX_FIRST_ID
#define DEBUG_CHECKBOX_FIRST_ID  9001
#endif

// 读取 _debug.txt，把 9001-9006 六个复选框设成文件里的值
inline bool LoadDebugConfigToCheckboxes(HWND ahWnd, const std::wstring& path)
{
    // 文件里缺失的键，valid[i] 会是 0
    int values[AIC_DEBUG_KEY_COUNT] = { 0 };
    int valid[AIC_DEBUG_KEY_COUNT] = { 0 };

    if (!LoadDebugConfigFromFile(path, values, valid)) {
        MessageBoxW(ahWnd, L"无法读取 _debug.txt（文件不存在或被占用）。",
            L"_debug.txt 出错", MB_OK | MB_ICONERROR);
        return false;
    }

    for (int i = 0; i < (int)AIC_DEBUG_KEY_COUNT; ++i) {
        const HWND checkbox = ::GetDlgItem(ahWnd, DEBUG_CHECKBOX_FIRST_ID + i);
        if (checkbox == NULL) {
            MessageBoxW(ahWnd, L"找不到某个复选框控件（资源 ID 与键名表不一致）。",
                L"_debug.txt 出错", MB_OK | MB_ICONERROR);
            return false;
        }

        if (!valid[i]) {
            // 文件里没有这一项：禁用该复选框，表示不可编辑（也不回写）
            ::SendMessageW(checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
            ::EnableWindow(checkbox, FALSE);
            continue;
        }

        // 读到值：先恢复可用，再按值勾选（非 0 视为勾选）
        ::EnableWindow(checkbox, TRUE);
        ::SendMessageW(checkbox, BM_SETCHECK,
            values[i] != 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return true;
}

// 读取 9001-9006 六个复选框，写回 _debug.txt
// 返回 0 表示没有需要写入的改动；返回 -1 表示失败（已弹窗）；返回 1 表示已保存
inline int SaveDebugConfigFromCheckboxes(HWND ahWnd, const std::wstring& path)
{
    int values[AIC_DEBUG_KEY_COUNT] = { 0 };
    int valid[AIC_DEBUG_KEY_COUNT] = { 0 };

    for (int i = 0; i < (int)AIC_DEBUG_KEY_COUNT; ++i) {
        const HWND checkbox = ::GetDlgItem(ahWnd, DEBUG_CHECKBOX_FIRST_ID + i);
        if (checkbox == NULL) {
            MessageBoxW(ahWnd, L"找不到某个复选框控件（资源 ID 与键名表不一致）。",
                L"_debug.txt 出错", MB_OK | MB_ICONERROR);
            return -1;
        }

        // 禁用的控件：文件中本来就没有这一项，不参与写入
        if (!::IsWindowEnabled(checkbox)) continue;

        const LRESULT state = ::SendMessageW(checkbox, BM_GETCHECK, 0, 0);
        values[i] = (state == BST_CHECKED) ? 1 : 0;
        valid[i] = 1;
    }

    if (!SaveDebugConfigToFile(path, values, valid)) {
        MessageBoxW(ahWnd, L"写入 _debug.txt 失败（文件可能被占用或只读）。",
            L"_debug.txt 出错", MB_OK | MB_ICONERROR);
        return -1;
    }
    return 1;
}
// ================================ 内部实现 ===================================
namespace cheat_settings_detail
{

    // 取「本头文件所在模块」的实例句柄。
    // HMODULE 本质就是模块基址，模块内任意静态对象的地址都可以当 HMODULE 用，
    // 所以这种写法在 EXE 和注入的 DLL 里都是对的；GetModuleHandleW(nullptr)
    // 在 DLL 里会错误地拿到宿主 EXE 的实例。
    inline HINSTANCE ModuleInstance()
    {
        static int s_moduleAnchor = 0;
        static HINSTANCE s_inst = reinterpret_cast<HINSTANCE>(&s_moduleAnchor);
        return s_inst;
    }

    // 当前正在显示的设置窗句柄（inline 函数里的静态变量在所有 TU 之间共享）
    inline HWND& CurrentWindow()
    {
        static HWND s_current = nullptr;
        return s_current;
    }

    // 取 owner（也就是全局 hWnd）。顶层窗口不要用 GetParent()。
    inline HWND OwnerOf(HWND hwnd)
    {
        HWND h = reinterpret_cast<HWND>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!h)
            h = GetWindow(hwnd, GW_OWNER);
        return h;
    }

    // 让窗口居中于主窗口
    inline void CenterOnOwner(HWND hwnd, HWND hOwner)
    {
        if (!hwnd || !IsWindow(hwnd) || !IsWindow(hOwner))
            return;

        RECT rcOwner = { 0 }, rcSelf = { 0 };
        GetWindowRect(hOwner, &rcOwner);
        GetWindowRect(hwnd, &rcSelf);

        const int w = rcSelf.right - rcSelf.left;
        const int h = rcSelf.bottom - rcSelf.top;
        const int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - w) / 2;
        const int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - h) / 2;

        SetWindowPos(hwnd, nullptr, x, y, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // 窗口过程
    inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_NCCREATE:
        {
            // 顶层窗口的 CREATESTRUCT.hwndParent 就是 owner（全局 hWnd），先记下来
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(cs->hwndParent));
            break;  // 交给 DefWindowProc，让它返回 TRUE
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);                  // 文字底透明，不画方块
            return reinterpret_cast<LRESULT>(static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));   // 用窗口同色画刷
        }
        case WM_COMMAND:
        {
            // 把按钮点击原样转给 hWnd：
            //     hWnd 里用 LOWORD(wParam) 取控件 ID，
            //              (HWND)lParam   取按钮句柄，
            //              HIWORD(wParam) 取通知码（按钮点击是 BN_CLICKED）。
            HWND hOwner = OwnerOf(hwnd);
            if (hOwner && hOwner != hwnd)
                SendMessageW(hOwner, WM_COMMAND, wParam, lParam);

#ifndef CHEAT_SETTINGS_NO_AUTO_CLOSE
            // 默认按钮顺手关窗，让窗口开箱可用。想完全交给 hWnd 就定义
            // CHEAT_SETTINGS_NO_AUTO_CLOSE，然后在 hWnd 里自己关。
            if (LOWORD(wParam) == CHEAT_SETTINGS_ID_CANCEL)
            {
                DestroyWindow(hwnd);
            }
            if (HIWORD(wParam) == BN_CLICKED)
            {
                //按钮点击事件
            }
            if (LOWORD(wParam) == CHEAT_SETTINGS_ID_OK) {
                if(SaveDebugConfigFromCheckboxes(hwnd, GamePath + L"\\AliceInCradle_Data\\StreamingAssets\\_debug.txt")!=-1)MessageBoxW(hwnd, L"设置保存成功!", L"提示", MB_OK | MB_ICONINFORMATION);
                DestroyWindow(hwnd);
            }
#endif
            return 0;
        }

        case WM_CLOSE:
            if(MessageBoxW(hwnd, L"你还未保存设置，确定放弃更改？", L"提示", MB_OKCANCEL | MB_ICONINFORMATION)==1)DestroyWindow(hwnd);   // 不要 PostQuitMessage
            return 0;

        case WM_DESTROY:
        {
            HWND hOwner = OwnerOf(hwnd);
            if (hOwner && hOwner != hwnd)
                PostMessageW(hOwner, CHEAT_SETTINGS_WM_CLOSED, 0, 0);

            if (CurrentWindow() == hwnd)
                CurrentWindow() = nullptr;
            return 0;
        }
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // 注册窗口类
    inline bool EnsureClassRegistered()
    {
        static bool s_done = false;
        if (s_done)
            return true;

        HINSTANCE hInst = ModuleInstance();

        // 已经注册过就直接用
        WNDCLASSEXW existing = { 0 };
        if (GetClassInfoExW(hInst, CHEAT_SETTINGS_CLASSNAME, &existing))
        {
            s_done = true;
            return true;
        }

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &WindowProc;
        wc.hInstance = hInst;
        // IDI_APPLICATION 和 IDC_ARROW 都是资源号 32512，这里显式用 W 版宏，
        // 免得在没定义 UNICODE 的工程里 IDI_APPLICATION 展开成 ANSI 版而类型不匹配。
        wc.hIcon = nullptr;
        wc.hIconSm = nullptr;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        wc.lpszClassName = CHEAT_SETTINGS_CLASSNAME;

        s_done = (RegisterClassExW(&wc) != 0);
        return s_done;
    }

} // namespace cheat_settings_detail

// 定义了 CHEAT_SETTINGS_NO_DEFAULT_CONTENT 又没提供 BUILD_CONTENT 时，
// 让它退化成空语句，而不是留一个没定义的宏名导致编译报错。
#ifndef CHEAT_SETTINGS_BUILD_CONTENT
#define CHEAT_SETTINGS_BUILD_CONTENT(hwnd) ((void)0)
#endif


inline void ShowCheatSettings()
{
    using namespace cheat_settings_detail;

    if (CurrentWindow() && IsWindow(CurrentWindow()))
    {
        SetForegroundWindow(CurrentWindow());
        return;
    }

    if (!EnsureClassRegistered())
        return;

    HWND hOwner = CHEAT_SETTINGS_MAIN_WINDOW;  
    if (!IsWindow(hOwner))
        hOwner = nullptr;       

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CHEAT_SETTINGS_CLASSNAME,
        CHEAT_SETTINGS_TEXT_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, CHEAT_SETTINGS_WIDTH, CHEAT_SETTINGS_HEIGHT,
        hOwner, 
        nullptr,
        ModuleInstance(),
        nullptr);

    if (!hwnd)
        return;

    CurrentWindow() = hwnd;

    // 建控件。按钮的 WM_COMMAND 会由 WindowProc 转发给 hWnd。
 
    HDC hdc = GetDC(NULL); // 获取整个屏幕的设备上下文
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    // 定义字体属性1
    LOGFONT lf;
    ZeroMemory(&lf, sizeof(LOGFONT));
    lstrcpy(lf.lfFaceName, L"Microsoft YaHei"); // 字体名称
    lf.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72); // 字体大小  点
    lf.lfWeight = FW_NORMAL; // 字体粗细
    HFONT hFont = CreateFontIndirect(&lf);

    const HINSTANCE csInst = cheat_settings_detail::ModuleInstance();

    HWND tStatic = CreateWindow(L"STATIC",
        L"_debug.txt",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 17, 300, 50, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NULL)),
        csInst, NULL);
    SendMessage(tStatic, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    HWND Static = CreateWindow(L"STATIC",
        L"<DEBUG>        开启调试\nmighty             诺艾尔攻击力极高\nnodamage       诺艾尔不受到伤害\nweak                受到伤害立即晕厥\nallskill               解锁全部魔法技能\nannounce         启动游戏时显示报错",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        50, 45, 300, 200, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NULL)),
        csInst, NULL);
    SendMessage(Static, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    HWND checkbox1 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 50, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9001)),
        csInst, NULL);
    HWND checkbox2 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 71, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9002)),
        csInst, NULL);
    HWND checkbox3 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 92, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9003)),
        csInst, NULL);
    HWND checkbox4 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 113, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9004)),
        csInst, NULL);
    HWND checkbox5 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 134, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9005)),
        csInst, NULL);
    HWND checkbox6 = CreateWindow(L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | BS_AUTOCHECKBOX,
        30, 155, 13, 13, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9006)),
        csInst, NULL);
    HWND button1 = CreateWindow(L"BUTTON", L"保存设置",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE | BS_DEFPUSHBUTTON,
        30, 220, 130, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CHEAT_SETTINGS_ID_OK)),
        csInst, NULL);
    SendMessage(button1, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    HWND button2 = CreateWindow(L"BUTTON", L"放弃更改",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE,
        220, 220, 130, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CHEAT_SETTINGS_ID_CANCEL)),
        csInst, NULL);
    SendMessage(button2, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

    LoadDebugConfigToCheckboxes(hwnd, GamePath + L"\\AliceInCradle_Data\\StreamingAssets\\_debug.txt");

#ifndef CHEAT_SETTINGS_NO_CENTER
    if (hOwner)
        CenterOnOwner(hwnd, hOwner);
#endif

    bool modal = false;
#ifndef CHEAT_SETTINGS_NO_MODAL
    if (hOwner)
    {
        EnableWindow(hOwner, FALSE);   // 主窗口禁用
        modal = true;
    }
#endif

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    if (!modal)
        return;       // 非模态：窗口已经出来了，直接返回

    // 嵌套消息循环，直到设置窗被销毁
    MSG msg;
    while (IsWindow(hwnd))
    {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0)
        {
            // 嵌套循环里收到 WM_QUIT 必须交还给外层循环，
            // 否则消息被这里吃掉，整个程序退不出去。
            if (got == 0)
                PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }

#ifndef CHEAT_SETTINGS_NO_ESCAPE
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            continue;
        }
#endif
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (IsWindow(hOwner))
    {
        EnableWindow(hOwner, TRUE);    // 恢复，否则主窗口永久卡死
        SetActiveWindow(hOwner);
    }
}

#endif // CHEAT_SETTINGS_WINDOW_H
