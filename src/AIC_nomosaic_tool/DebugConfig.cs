using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace AicNoMosaic
{
    /// <summary>
    /// 读写游戏自带的 _debug.txt 调试开关。
    /// </summary>
    internal static class DebugConfig
    {
        // 工具暴露的键分两组（同一样本 _debug.txt；_player 起为作者警告区，不进 UI，原样保留）
        public const int KeyCount = 10;

        // 组边界：前 CheatGroupCount 个是"作弊开关"，其余是"进阶排障/功能开关"
        public const int CheatGroupCount = 6;

        // 顺序与 UI 复选框一一对应（0..5 作弊组；6..9 进阶组）
        public static readonly string[] KeyNames =
        {
            "<DEBUG>", "mighty", "nodamage", "weak", "allskill", "announce",
            "nosnd", "reloadmtr", "timestamp", "albumunlock",
        };

        // 各键中文说明（给调试窗口显示）
        public static readonly string[] KeyTexts =
        {
            "开启调试（调试功能总开关）",
            "诺艾尔攻击力极高",
            "诺艾尔不受到伤害",
            "受到伤害立即晕厥",
            "解锁全部魔法技能",
            "启动游戏时显示报错",
            "跳过音效资源加载（缩短加载，无事声音）",
            "允许 F9 热重载本地化文本，启用本地化写入",
            "导出日志带时间戳；与 announce 同开可用 F7 调试菜单",
            "战斗中可切换诺艾尔新旧立绘（0.24+）",
        };

        public struct Entry
        {
            public int Value;
            public bool Valid;   // false = 文件里没有这个键（不参与写入）
        }

        private static readonly UTF8Encoding Utf8NoBom = new UTF8Encoding(false);

        /// <summary>读取六个键的值。文件缺失/占用返回 false；缺个别键也算成功（对应 Valid=false）。</summary>
        public static bool TryRead(string path, Entry[] entries)
        {
            if (entries == null || entries.Length != KeyCount) return false;
            for (int i = 0; i < KeyCount; i++)
            {
                entries[i].Value = 0;
                entries[i].Valid = false;
            }

            string[] lines;
            try
            {
                if (!File.Exists(path)) return false;
                lines = File.ReadAllLines(path, Utf8NoBom);   // 同时兼容 LF / CRLF
            }
            catch { return false; }

            foreach (string line in lines)
            {
                string key, value;
                if (!TryParseEntry(line, out key, out value)) continue;

                int index = IndexOfKey(key);
                if (index < 0) continue;                       // 未知键 → 忽略

                int parsed;
                if (int.TryParse(value, out parsed))
                {
                    entries[index].Value = parsed;
                    entries[index].Valid = true;
                }
            }
            return true;
        }

        public static bool TryWrite(string path, Entry[] entries)
        {
            if (entries == null || entries.Length != KeyCount) return false;

            // 是否有任何要写的键：没有则不产生 IO（保 mtime）
            bool any = false;
            for (int i = 0; i < KeyCount; i++)
                if (entries[i].Valid) { any = true; break; }
            if (!any) return true;

            string[] lines;
            try
            {
                if (!File.Exists(path)) return false;
                lines = File.ReadAllLines(path, Utf8NoBom);
            }
            catch { return false; }

            var result = new List<string>(lines.Length + KeyCount);
            var handled = new bool[KeyCount];
            bool changed = false;

            foreach (string original in lines)
            {
                // 解析这一行：目标键 → 重建；其它（未知键/注释/空行）原样
                string key, valueToken;
                int index;
                if (TryParseEntry(original, out key, out valueToken)
                    && (index = IndexOfKey(key)) >= 0
                    && entries[index].Valid)
                {
                    string rewritten = KeyNames[index] + " " + (entries[index].Value != 0 ? "1" : "0");
                    if (rewritten != original.TrimEnd('\r')) changed = true;
                    result.Add(rewritten);
                    handled[index] = true;
                }
                else
                {
                    result.Add(original.TrimEnd('\r'));        // 去 CR：统一 LF 输出
                }
            }

            for (int i = 0; i < KeyCount; i++)
            {
                if (entries[i].Valid && !handled[i])
                {
                    result.Add(KeyNames[i] + " " + (entries[i].Value != 0 ? "1" : "0"));
                    changed = true;
                }
            }

            if (!changed) return true;                          // 无差异 → 免写，保 mtime

            // 原子写回：同目录临时文件 + File.Replace；\n 显式拼接保证 LF
            string tmp = path + ".tmp_aic_dbg";
            try
            {
                File.WriteAllText(tmp, string.Join("\n", result) + "\n", Utf8NoBom);
                if (File.Exists(path)) File.Replace(tmp, path, null, true);
                else File.Move(tmp, path);
                return true;
            }
            catch
            {
                try { File.Delete(tmp); } catch { }
                return false;
            }
        }

        private static bool TryParseEntry(string line, out string key, out string value)
        {
            key = null;
            value = null;

            if (line == null) return false;
            string trimmed = line.Trim();
            if (trimmed.Length == 0 || trimmed.StartsWith("//", StringComparison.Ordinal)) return false;

            int comment = trimmed.IndexOf("//", StringComparison.Ordinal);
            if (comment >= 0) trimmed = trimmed.Substring(0, comment).Trim();
            if (trimmed.Length == 0) return false;

            int firstSpace = IndexOfSpace(trimmed);
            if (firstSpace < 0) return false;                  // 只有键名没有值

            key = trimmed.Substring(0, firstSpace);
            value = trimmed.Substring(firstSpace + 1).Trim();
            return value.Length > 0;
        }

        private static int IndexOfSpace(string s)
        {
            for (int i = 0; i < s.Length; i++)
            {
                char c = s[i];
                if (c == ' ' || c == '\t') return i;
            }
            return -1;
        }

        private static int IndexOfKey(string key)
        {
            for (int i = 0; i < KeyNames.Length; i++)
                if (string.Equals(KeyNames[i], key, StringComparison.OrdinalIgnoreCase))
                    return i;
            return -1;
        }
    }
}
