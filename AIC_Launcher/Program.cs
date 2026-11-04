// aic - minimal Unity player host.
//
// Loads UnityPlayer.dll from its own folder and calls UnityMain (alias
// UnityMain2) - which is all the original player executable ever did. The
// game's own wrapper is never executed, so whatever protection it carried
// never runs either.
//
// Deployment: copy as "<GameName>.exe" into the game folder. Unity derives the
// data folder name from the host executable (<exe name>_Data), so the file
// name is not cosmetic.
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace aic
{
    internal static class Program
    {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr LoadLibraryW(string path);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
        private static extern IntPtr GetProcAddress(IntPtr module, string name);

        [DllImport("kernel32.dll")]
        private static extern IntPtr GetModuleHandleW(IntPtr name);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern bool SetCurrentDirectoryW(string path);

        // Replaces System.Windows.Forms.MessageBox.Show - same user32 call it
        // makes internally, without dragging in WinForms.
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);

        private const uint MB_ICONERROR = 0x00000010;

        // ===== 游戏窗口图标：用 exe 资源里的 AIC.ico（csc /win32icon 写进去，图标组 ID 32512）=====
        private const int IDI_APP_ICON = 32512;
        private const uint IMAGE_ICON = 1;
        private const uint WM_SETICON = 0x0080;
        private const int ICON_SMALL = 0;
        private const int ICON_BIG = 1;
        private const int SM_CXICON = 11;
        private const int SM_CYICON = 12;
        private const int SM_CXSMICON = 49;
        private const int SM_CYSMICON = 50;

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr FindWindowW(string className, string windowName);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr LoadImageW(IntPtr hInst, IntPtr name, uint type,
                                                int cx, int cy, uint fuLoad);
        [DllImport("user32.dll")]
        private static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")]
        private static extern int GetSystemMetrics(int index);
        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc proc, IntPtr param);
        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowTextLengthW(IntPtr hWnd);
        private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr param);
        [DllImport("kernel32.dll")]
        private static extern uint GetCurrentProcessId();

        private static IntPtr g_bigIcon = IntPtr.Zero;
        private static IntPtr g_smallIcon = IntPtr.Zero;

        [UnmanagedFunctionPointer(CallingConvention.Winapi, CharSet = CharSet.Unicode)]
        private delegate int UnityMainFn(IntPtr hInstance, IntPtr hPrevInstance,
                                         string lpCmdLine, int nShowCmd);

        private const int SW_SHOWNORMAL = 1;

        // ===== 启动前的提示框=====
        private const uint MB_YESNO = 0x00000004;           // 是/否 两个按钮
        private const uint MB_ICONINFORMATION = 0x00000040;
        private const int IDYES = 6;
        private const string kPromptTitle = "AliceInCradle补丁说明";
        private const string kPromptText = "此补丁仅供开发者使用，严禁二次分发。\n游戏本体和补丁均为免费，若付费获得，请举报商家。\n你是否同意以上规则且已满18岁？";

        [STAThread]
        private static int Main(string[] args)
        {
            // 弹窗
            if (MessageBoxW(IntPtr.Zero, kPromptText, kPromptTitle,
                    MB_YESNO | MB_ICONINFORMATION) != IDYES)
                return 0;

            string dir = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
            if (string.IsNullOrEmpty(dir)) dir = Environment.CurrentDirectory;
            SetCurrentDirectoryW(dir);

            string dll = Path.Combine(dir, "UnityPlayer.dll");
            if (!File.Exists(dll))
            {
                Fail("UnityPlayer.dll is missing next to " + dir);
                return 2;
            }

            IntPtr module = LoadLibraryW(dll);
            if (module == IntPtr.Zero)
            {
                Fail("LoadLibrary(UnityPlayer.dll) failed, error " + Marshal.GetLastWin32Error());
                return 3;
            }

            IntPtr entry = GetProcAddress(module, "UnityMain");
            if (entry == IntPtr.Zero) entry = GetProcAddress(module, "UnityMain2");
            if (entry == IntPtr.Zero)
            {
                Fail("UnityMain export not found in UnityPlayer.dll");
                return 4;
            }

            try
            {
                // 游戏窗口是 Unity 起来之后才创建的，先挂个后台线程等着它出现
                WatchGameWindow();

                UnityMainFn unityMain = (UnityMainFn)Marshal.GetDelegateForFunctionPointer(
                    entry, typeof(UnityMainFn));
                return unityMain(GetModuleHandleW(IntPtr.Zero), IntPtr.Zero, RawArguments(), SW_SHOWNORMAL);
            }
            catch (Exception ex)
            {
                Fail("UnityMain failed: " + ex.GetType().Name + " " + ex.Message);
                return 5;
            }
        }

        // 把游戏主窗口的图标换成资源里的 AIC.ico（大小两个尺寸都设）
        private static void SetGameIcon(IntPtr hwnd)
        {
            if (hwnd == IntPtr.Zero) return;

            IntPtr mod = GetModuleHandleW(IntPtr.Zero);
            if (g_bigIcon == IntPtr.Zero)
                g_bigIcon = LoadImageW(mod, (IntPtr)IDI_APP_ICON, IMAGE_ICON,
                                       GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
            if (g_smallIcon == IntPtr.Zero)
                g_smallIcon = LoadImageW(mod, (IntPtr)IDI_APP_ICON, IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

            if (g_bigIcon != IntPtr.Zero)
                SendMessageW(hwnd, WM_SETICON, (IntPtr)ICON_BIG, g_bigIcon);
            if (g_smallIcon != IntPtr.Zero)
                SendMessageW(hwnd, WM_SETICON, (IntPtr)ICON_SMALL, g_smallIcon);
        }

        // 等 Unity 建好自己的窗口（类名 UnityWndClass），设置一次图标就结束。
        // Unity 本身不设图标（所以原来显示的是默认图标），设一次就不会被改回去。
        private static void WatchGameWindow()
        {
            Thread t = new Thread(delegate ()
            {
                for (int i = 0; i < 600; ++i)                  // 最多等 60 秒
                {
                    IntPtr hwnd = FindWindowW("UnityWndClass", null);
                    if (hwnd == IntPtr.Zero)
                        hwnd = FindOwnTopWindow();             // 类名不是 UnityWndClass 时兜底
                    if (hwnd != IntPtr.Zero && IsOwnWindow(hwnd))
                    {
                        SetGameIcon(hwnd);
                        return;
                    }
                    Thread.Sleep(100);
                }
            });
            t.IsBackground = true;
            t.Start();
        }

        private static bool IsOwnWindow(IntPtr hwnd)
        {
            uint pid;
            GetWindowThreadProcessId(hwnd, out pid);
            return pid == GetCurrentProcessId();               // 只认自己进程的窗口
        }

        // 兜底：找本进程里第一个可见、有标题的顶层窗口
        private static IntPtr FindOwnTopWindow()
        {
            uint me = GetCurrentProcessId();
            IntPtr found = IntPtr.Zero;
            EnumWindows(delegate (IntPtr h, IntPtr p)
            {
                uint pid;
                GetWindowThreadProcessId(h, out pid);
                if (pid == me && IsWindowVisible(h) && GetWindowTextLengthW(h) > 0)
                {
                    found = h;
                    return false;                              // 找到就停
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }

        // Pass everything after the executable name through untouched, so that
        // quoted values such as -screen-quality "Very High" survive.
        private static string RawArguments()
        {
            string cmd = Environment.CommandLine;
            int i;
            if (cmd.Length > 0 && cmd[0] == '"')
            {
                i = cmd.IndexOf('"', 1);
                i = i < 0 ? cmd.Length : i + 1;
            }
            else
            {
                i = cmd.IndexOf(' ');
                i = i < 0 ? cmd.Length : i;
            }
            return cmd.Substring(i).TrimStart();
        }
        private static void Fail(string text)
        {
            MessageBoxW(IntPtr.Zero, text, "aic", MB_ICONERROR);
        }
    }
}
