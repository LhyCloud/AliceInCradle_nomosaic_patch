using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace AicNoMosaic
{
    /// <summary>
    /// BepInEx 框架检测与插件部署。
    ///
    /// 检测到 BepInEx（BepInEx/core/BepInEx.dll）时，工具的"安装/卸载"自动切换
    /// 为插件方式：把随工具内嵌的 AIC_nomosaic_bepinex.dll 释放到 BepInEx/plugins/，
    /// 未检测到 BepInEx 时走原 dnlib 静态补丁流程。
    /// </summary>
    internal static class BepInExDeployer
    {
        /// <summary>随工具嵌入的插件资源名（csproj 内 Target 动态注入的 LogicalName）。</summary>
        private const string PluginResourceName = "AIC_nomosaic_bepinex.dll";

        /// <summary>BepInEx 判定：游戏目录存在 BepInEx/core/BepInEx.dll 即视为框架可用。</summary>
        public static bool IsFrameworkPresent(string gameRoot)
        {
            if (string.IsNullOrEmpty(gameRoot)) return false;
            return File.Exists(Path.Combine(gameRoot, "BepInEx", "core", "BepInEx.dll"));
        }

        /// <summary>插件落位点：BepInEx/plugins/ 下与插件程序集同名，便于排查版本。</summary>
        public static string PluginPath(string gameRoot)
        {
            return Path.Combine(gameRoot, "BepInEx", "plugins", "AIC_nomosaic_bepinex.dll");
        }

        public static bool IsPluginInstalled(string gameRoot)
        {
            if (string.IsNullOrEmpty(gameRoot)) return false;
            return File.Exists(PluginPath(gameRoot));
        }

        /// <summary>
        /// 释放（或更新）内嵌插件。已装内容与嵌入产物逐字节一致时跳过写入
        /// （幂等，不触碰文件）。返回 true=实际写入；false=已是当前版本。
        /// </summary>
        public static bool ExtractPlugin(string gameRoot)
        {
            return ExtractPlugin(gameRoot, ReadEmbeddedPlugin());
        }

        /// <summary>写入核心（payload 参数化以便测试器验证幂等/替换逻辑）。</summary>
        internal static bool ExtractPlugin(string gameRoot, byte[] payload)
        {
            string path = PluginPath(gameRoot);
            string dir = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

            try
            {
                if (File.Exists(path))
                {
                    byte[] existing = File.ReadAllBytes(path);
                    if (existing.Length == payload.Length && existing.SequenceEqual(payload))
                        return false;
                }

                // 与静态补丁同风格：临时文件 + 同卷原子替换，中途断电不留半截插件
                string temp = path + ".tmp_aic";
                File.WriteAllBytes(temp, payload);
                if (File.Exists(path))
                    File.Replace(temp, path, null);
                else
                    File.Move(temp, path);
                return true;
            }
            catch (IOException ex)
            {
                // 插件文件被占用（游戏/BepInEx 未关）→ 与静态补丁同一拒绝理由语义
                throw new IOException("插件文件被占用，请先关闭游戏再操作。\n" + ex.Message);
            }
        }

        /// <summary>移除插件。返回 true=确实删除了文件；false=本来就没装。</summary>
        public static bool RemovePlugin(string gameRoot)
        {
            string path = PluginPath(gameRoot);
            if (!File.Exists(path)) return false;
            File.Delete(path);
            return true;
        }

        /// <summary>读取随工具嵌入的插件 dll（csproj 构建期从 bepinex 项目产物注入）。</summary>
        private static byte[] ReadEmbeddedPlugin()
        {
            Assembly self = typeof(BepInExDeployer).Assembly;
            using (Stream s = self.GetManifestResourceStream(PluginResourceName))
            {
                if (s == null)
                    throw new InvalidOperationException(
                        "工具缺少内嵌插件资源（" + PluginResourceName + "），安装包可能不完整，请重新下载。");
                byte[] data = new byte[s.Length];
                int total = 0;
                while (total < data.Length)
                {
                    int n = s.Read(data, total, data.Length - total);
                    if (n <= 0) break;
                    total += n;
                }
                if (total != data.Length)
                    throw new InvalidOperationException("内嵌插件资源读取不完整，安装包可能损坏。");
                return data;
            }
        }
    }
}
