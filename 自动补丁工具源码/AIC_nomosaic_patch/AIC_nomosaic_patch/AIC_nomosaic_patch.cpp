// AIC_nomosaic_patch.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "AIC_nomosaic_patch.h"
#include "NET_SDK.h"
#include "define.h"
#include "version.h"
#include "CheatWindow.h"
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名



// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    if (IsAlreadyRunning())
    {
        MessageBox(NULL, L"程序已经在运行", L"提示", MB_OK | MB_ICONWARNING);
        return 0; // 退出程序
    }

    if (!IsDotNetInstalled()) {
        MessageBox(NULL,
            L"未检测到 .NET Framework 4.x，点击确定跳转下载页面。\n请安装后重启本程序。",
            L"运行依赖缺失", MB_OK | MB_ICONWARNING);
        ShellExecute(NULL, L"open", L"https://dotnet.microsoft.com/en-us/download/dotnet-framework", NULL, NULL, SW_SHOWNORMAL);
        return 1;
    }
    //释放资源
    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);
    dllPath = std::wstring(tempPath) + L"dnlib.dll";
    ExtractResourceToFile(IDR_DLL, L"RT_RCDATA", dllPath);
    cmdPath = std::wstring(tempPath) + L"AIC_nomosaic_patch_cmd.exe";
    ExtractResourceToFile(IDR_CMD, L"RT_RCDATA", cmdPath);
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AICNOMOSAICPATCH, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AICNOMOSAICPATCH));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AICNOMOSAICPATCH));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_AICNOMOSAICPATCH);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = wcex.hIcon;

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   hWnd = CreateWindowW(szWindowClass, L"Alice In Cradle 补丁工具 v1.2.0"/*szTitle*/, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, 0, 500, 420, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HDC hdc = GetDC(NULL); // 获取整个屏幕的设备上下文
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        // 定义字体属性1
        LOGFONT lf;
        ZeroMemory(&lf, sizeof(LOGFONT));
        lstrcpy(lf.lfFaceName, L"Microsoft YaHei"); // 字体名称
        lf.lfHeight = -MulDiv(15, GetDeviceCaps(hdc, LOGPIXELSY), 72); // 字体大小  点
        lf.lfWeight = FW_NORMAL; // 字体粗细
        HFONT hFont = CreateFontIndirect(&lf);
        // 定义字体属性2
        LOGFONT lf2;
        ZeroMemory(&lf2, sizeof(LOGFONT));
        lstrcpy(lf2.lfFaceName, L"Microsoft YaHei"); // 字体名称
        lf2.lfHeight = -MulDiv(28, GetDeviceCaps(hdc, LOGPIXELSY), 72); // 字体大小18点
        lf2.lfWeight = FW_NORMAL; // 字体粗细
        HFONT bFont = CreateFontIndirect(&lf2);
        // 定义字体属性3
        LOGFONT lf3;
        ZeroMemory(&lf3, sizeof(LOGFONT));
        lstrcpy(lf3.lfFaceName, L"Microsoft YaHei"); // 字体名称
        lf3.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72); // 字体大小18点
        lf3.lfWeight = FW_NORMAL; // 字体粗细
        HFONT cFont = CreateFontIndirect(&lf3);

        HWND Static = CreateWindow(L"STATIC",
            L"Alice In Cradle",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            115, 28, 300, 50, hWnd, (HMENU)NULL,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Static, WM_SETFONT, (WPARAM)bFont, MAKELPARAM(TRUE, 0));
        HWND Static2 = CreateWindow(L"STATIC",
            L"全版本通用补丁",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            165, 70, 300, 50, hWnd, (HMENU)NULL,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Static2, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
        HWND Static3 = CreateWindow(L"STATIC",
            L"游戏版本：\n游戏路径：\n补丁选项：",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            60, 120, 300, 80, hWnd, (HMENU)NULL,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Static3, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
        HWND Statictip = CreateWindow(L"STATIC",
            L"安装/卸载均需约10秒，请勿重复安装/卸载\n安装/卸载过程窗口卡顿属于正常现象\n安装/卸载完成前请勿关闭工具,否则游戏可能损坏",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            60, 260, 360, 100, hWnd, (HMENU)NULL,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Statictip, WM_SETFONT, (WPARAM)cFont, MAKELPARAM(TRUE, 0));
        HWND Static4 = CreateWindow(L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            160, 150, 400, 20, hWnd, (HMENU)ID_path,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Static4, WM_SETFONT, (WPARAM)cFont, MAKELPARAM(TRUE, 0));
        HWND Static5 = CreateWindow(L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            160, 120, 300, 80, hWnd, (HMENU)ID_ver,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(Static5, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
        HWND button = CreateWindow(L"BUTTON", L"安装去马补丁",
            /*WS_TABSTOP |*/ WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE/*换行样式*/ | WS_DISABLED,
            57, 210, 160, 40, hWnd, (HMENU)IDM_test,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(button, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
        HWND button3 = CreateWindow(L"BUTTON", L"卸载去马补丁",
            /*WS_TABSTOP |*/ WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE/*换行样式*/| WS_DISABLED,
            257, 210, 160, 40, hWnd, (HMENU)IDM_test2,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(button3, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
        HWND button2 = CreateWindow(L"BUTTON", L"选择游戏目录",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_MULTILINE/*换行样式*/ ,
            165, 130, 175, 40, hWnd, (HMENU)IDM_choose,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(button2, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

        //检查目录
        toolPath = GetCurrentExeFolder();
        if (CheckAssemblyDll(toolPath) == true) {
            //MessageBox(hWnd, L"yes", L"选择的", MB_OK);
            GamePath = toolPath;
            GameVer = GetVer(GamePath);
            HWND hStatic = GetDlgItem(hWnd, ID_path);
            SetWindowTextW(hStatic, GamePath.c_str());
            HWND hStatic2 = GetDlgItem(hWnd, ID_ver);
            SetWindowTextW(hStatic2, GameVer.c_str());
            HWND hButton = GetDlgItem(hWnd, IDM_choose);
            ShowWindow(hButton, SW_HIDE); // 隐藏控件
            HWND hButton2 = GetDlgItem(hWnd, IDM_test);
            EnableWindow(hButton2, TRUE);
            HWND hButton3 = GetDlgItem(hWnd, IDM_test2);
            EnableWindow(hButton3, TRUE);
            AssemblyDllPath = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll";
            if (IsFileInUse(AssemblyDllPath))
                MessageBox(hWnd, L"游戏文件被占用，请先解除占用。", L"提示", MB_OK);
        }
    }
    break;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(0, 0, 0)); // 设置文本颜色为黑色
        SetBkMode(hdcStatic, TRANSPARENT); // 设置背景模式为透明
        return (LONG)GetStockObject(HOLLOW_BRUSH);// 返回空画刷，以确保不绘制背景
    }
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                //DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                ShellExecute(NULL, L"open", L"https://github.com/YXC-Lhy/AliceInCradle_nomosaic_patch/releases",
                    NULL, NULL, SW_SHOWNORMAL);
                break;
            case ID_32772:
                MessageBox(hWnd, L"补丁工具版本：v1.2.0\n发布日期：2026年10月6日\n兼容游戏版本：已发布的所有版本及未来版本\n编译器：VS2019（ C++ 17 、.NET Framework 4.7.2）\n使用dnlib 4.5.0", L"版本信息", MB_OK | MB_ICONINFORMATION);
                break;
            case ID_32771:
                ShellExecute(NULL, L"open", L"https://github.com/YXC-Lhy/AliceInCradle_nomosaic_patch/blob/main/README.md",
                    NULL, NULL, SW_SHOWNORMAL);
                break;
            case IDM_test:{
                EnableWindow(GetDlgItem(hWnd, IDM_test), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDM_test2), FALSE);
                if (patch(cmdPath, AssemblyDllPath,false)) {
                    MessageBox(hWnd, L"安装补丁成功！", L"提示", MB_OK | MB_ICONINFORMATION);
                }
                else MessageBox(hWnd, L"安装补丁失败！", L"提示", MB_OK | MB_ICONHAND);
                HWND hButton111 = GetDlgItem(hWnd, IDM_test); // 获取按钮句柄
                SetWindowTextW(hButton111, L"安装去码补丁");

                std::wstring dupmfile = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll.res";
                DeleteFile(dupmfile.c_str());
                dupmfile = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.pdb";
                DeleteFile(dupmfile.c_str());
                dupmfile = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll.mod.il";
                DeleteFile(dupmfile.c_str());
                dupmfile = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll.il";
                DeleteFile(dupmfile.c_str());

                EnableWindow(GetDlgItem(hWnd, IDM_test), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDM_test2), TRUE);
                }break;
            case IDM_test2:{
                EnableWindow(GetDlgItem(hWnd, IDM_test), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDM_test2), FALSE);
                if (patch(cmdPath, AssemblyDllPath, true)) {
                    MessageBox(hWnd, L"卸载补丁成功！", L"提示", MB_OK | MB_ICONINFORMATION);
                }
                else MessageBox(hWnd, L"卸载补丁失败！", L"提示", MB_OK | MB_ICONHAND);
                HWND hButton222 = GetDlgItem(hWnd, IDM_test2); // 获取按钮句柄
                SetWindowTextW(hButton222, L"卸载去码补丁");

                
                EnableWindow(GetDlgItem(hWnd, IDM_test2), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDM_test), TRUE);
                }break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDM_choose:
            {
                std::wstring exePath = OpenExeFileDialog();
                
                if (!exePath.empty())
                {
                    //MessageBox(hWnd, exePath.c_str(), L"选择的 EXE", MB_OK);
                    //MessageBox(hWnd, GetExeFolder(exePath).c_str(), L"选择的", MB_OK);
                    if (CheckAssemblyDll(GetExeFolder(exePath)) == true) {
                        //MessageBox(hWnd, L"yes", L"选择的", MB_OK);
                        GamePath = GetExeFolder(exePath);
                        GameVer= GetVer(GetExeFolder(exePath));
                        HWND hStatic = GetDlgItem(hWnd, ID_path);
                        SetWindowTextW(hStatic, GamePath.c_str());
                        HWND hStatic2 = GetDlgItem(hWnd, ID_ver);
                        SetWindowTextW(hStatic2, GameVer.c_str());
                        HWND hButton = GetDlgItem(hWnd, IDM_choose);
                        ShowWindow(hButton, SW_HIDE); // 隐藏控件
                        HWND hButton2 = GetDlgItem(hWnd, IDM_test);
                        EnableWindow(hButton2, TRUE);
                        HWND hButton3 = GetDlgItem(hWnd, IDM_test2);
                        EnableWindow(hButton3, TRUE);
                        AssemblyDllPath = GamePath + L"\\AliceInCradle_Data\\Managed\\Assembly-CSharp.dll";
                        if (IsFileInUse(AssemblyDllPath))
                            MessageBox(hWnd, L"游戏文件被占用，请先解除占用。", L"提示", MB_OK);
                    }
                    else {
                        MessageBox(hWnd, L"未找到游戏文件，请选择正确路径。", L"错误", MB_OK);
                    }
                }
                else
                {
                    //MessageBox(hWnd, L"选择不能为空", L"提示", MB_OK | MB_ICONWARNING);
                }
            }break;
            case ID_32773:
            {
                if (GamePath == L"") {
                    MessageBox(hWnd, L"请先选择游戏目录。", L"错误", MB_OK | MB_ICONWARNING);
                    break;
                }
                else {
                
                    ShowCheatSettings();
                }
            }
            break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        //清理临时文件
        DeleteFile(dllPath.c_str());
        DeleteFile(cmdPath.c_str());
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        
        break;
    }
    return (INT_PTR)FALSE;
    }
}
