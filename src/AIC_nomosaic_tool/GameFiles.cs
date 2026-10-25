using System;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace AicNoMosaic
{
    /// <summary>游戏文件路径与状态工具。</summary>
    internal static class GameFiles
    {
        public const string DataFolder = "AliceInCradle_Data";

        public static string ManagedDir(string root)
        {
            return Path.Combine(root, DataFolder, "Managed");
        }

        public static string AssemblyPath(string root)
        {
            return Path.Combine(ManagedDir(root), "Assembly-CSharp.dll");
        }

        /// <summary>首次安装时的原始 DLL 备份路径。</summary>
        public static string BackupPath(string root)
        {
            return AssemblyPath(root) + ".bak";
        }

        public static string DebugConfigPath(string root)
        {
            return Path.Combine(root, DataFolder, "StreamingAssets", "_debug.txt");
        }

        /// <summary>游戏防篡改校验清单（与 _debug.txt 同目录）。记录 Managed/Assembly-CSharp.dll 等全部文件 SHA256。</summary>
        public static string ShaManifestPath(string root)
        {
            return Path.Combine(root, DataFolder, "StreamingAssets", "_sha.txt");
        }

        public static bool IsGameRoot(string dir)
        {
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return false;
            return File.Exists(AssemblyPath(dir));
        }

        /// <summary>尝试以独占方式打开，判断文件是否被游戏进程占用。</summary>
        public static bool IsFileInUse(string path)
        {
            try
            {
                using (FileStream fs = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                {
                    return false;
                }
            }
            catch (FileNotFoundException) { return false; }
            catch (DirectoryNotFoundException) { return false; }
            catch (IOException) { return true; }                  // 共享冲突/锁冲突 = 被占用
            catch (UnauthorizedAccessException) { return false; } // 权限不足不视为被占用
        }
    }

    /// <summary>记住上一次选择的游戏目录（%AppData%\AIC_nomosaic_tool\settings.txt）。</summary>
    internal static class AppSettings
    {
        public static string GamePath { get; internal set; }

        private static string FilePath()
        {
            string dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "AIC_nomosaic_tool");
            try { Directory.CreateDirectory(dir); } catch { }
            return Path.Combine(dir, "settings.txt");
        }

        public static void Load()
        {
            GamePath = "";
            try
            {
                string fp = FilePath();
                if (!File.Exists(fp)) return;
                foreach (string lineRaw in File.ReadAllLines(fp))
                {
                    string line = lineRaw.Trim();
                    int eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    if (line.Substring(0, eq).Trim() == "GamePath")
                    {
                        GamePath = line.Substring(eq + 1).Trim();
                        return;
                    }
                }
            }
            catch { GamePath = ""; }
        }

        public static void Save()
        {
            try
            {
                var sb = new StringBuilder();
                sb.AppendLine("GamePath=" + (GamePath ?? ""));
                File.WriteAllText(FilePath(), sb.ToString());
            }
            catch { }
        }
    }

    /// <summary>Program Files 等受保护目录写入失败时，提供 UAC 提权重启。</summary>
    internal static class Elevation
    {
        /// <summary>请求以管理员身份重启本工具。用户在 UAC 确认后返回 true。</summary>
        public static bool RestartAsAdmin()
        {
            try
            {
                var psi = new System.Diagnostics.ProcessStartInfo();
                psi.FileName = Application.ExecutablePath;
                psi.UseShellExecute = true;
                psi.Verb = "runas";
                System.Diagnostics.Process.Start(psi);
                return true;
            }
            catch { return false; }
        }
    }
}
