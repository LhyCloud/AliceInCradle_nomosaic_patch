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

        [UnmanagedFunctionPointer(CallingConvention.Winapi, CharSet = CharSet.Unicode)]
        private delegate int UnityMainFn(IntPtr hInstance, IntPtr hPrevInstance,
                                         string lpCmdLine, int nShowCmd);

        private const int SW_SHOWNORMAL = 1;

        [STAThread]
        private static int Main(string[] args)
        {
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