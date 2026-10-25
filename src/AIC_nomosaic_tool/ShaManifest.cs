using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace AicNoMosaic
{
    /// <summary>
    /// 维护 _sha.txt（游戏防篡改校验清单）与补丁状态的一致性。
    ///
    /// 文件格式（实测 0.30i，与 _debug.txt 同目录 StreamingAssets/）：
    ///   * 路径行 + 哈希行 交替（路径相对游戏根目录，如 "Managed/Assembly-CSharp.dll"）；
    ///   * 哈希 = 64 位小写 hex SHA256；
    ///   * 无 BOM、LF 换行。
    ///
    /// 本类职责：
    ///   * 打补丁/卸载后把 Assembly-CSharp.dll 的新 SHA256 回写到清单；
    ///   * 首次改动前把原始清单备份为 _sha.txt.orig，卸载还原时恢复原始哈希记录
    ///     （防止误操作导致原始校验值丢失）。
    /// 其余条目行原样保留，只重写目标键那一行。
    /// </summary>
    internal static class ShaManifest
    {
        public const string ManagedEntry = "Managed/Assembly-CSharp.dll";

        private static readonly UTF8Encoding Utf8NoBom = new UTF8Encoding(false);

        /// <summary>计算文件 SHA256（64 位小写 hex）。文件缺失/不可读返回 null。</summary>
        public static string ComputeFileSha256(string path)
        {
            try
            {
                if (!File.Exists(path)) return null;
                using (var sha = SHA256.Create())
                using (FileStream fs = File.OpenRead(path))
                {
                    var sb = new StringBuilder(64);
                    foreach (byte b in sha.ComputeHash(fs))
                        sb.Append(b.ToString("x2", CultureInfo.InvariantCulture));
                    return sb.ToString();
                }
            }
            catch { return null; }
        }

        /// <summary>
        /// 把 entryPath 对应条目的哈希更新为 currentDllSha（null → 删除该条目）。
        /// 其余行逐行原样保留；写前确保存在 .orig 原始备份（只备份一次）。
        /// 返回 false 表示 I/O 失败（调用方提示，但不阻断补丁本身）。
        /// </summary>
        public static bool TryUpdateEntry(string manifestPath, string entryPath, string newSha)
        {
            try
            {
                if (!File.Exists(manifestPath)) return false;

                // ---- 读原始行序列 ----
                string[] lines;
                try
                {
                    lines = File.ReadAllLines(manifestPath, Utf8NoBom);
                }
                catch { return false; }

                // ---- 首次改动前备份原始清单 ----
                string orig = manifestPath + ".orig";
                if (!File.Exists(orig))
                {
                    try { File.Copy(manifestPath, orig, false); }
                    catch { /* 备份失败不阻断主流程，但更新照常进行 */ }
                }

                // ---- 找到目标条目（路径行 + 相邻哈希行）----
                int targetPathLine = -1;
                for (int i = 0; i + 1 < lines.Length; i += 2)
                {
                    if (string.Equals(lines[i].Trim(), entryPath, StringComparison.OrdinalIgnoreCase))
                    {
                        targetPathLine = i;
                        break;
                    }
                }

                var result = new List<string>(lines.Length);
                if (targetPathLine >= 0)
                {
                    // 更新已有条目：目标哈希行替换，其余行原样
                    for (int i = 0; i < lines.Length; i++)
                    {
                        if (i == targetPathLine)
                        {
                            result.Add(lines[i]);               // 路径行原样
                        }
                        else if (i == targetPathLine + 1)
                        {
                            if (newSha != null) result.Add(newSha);   // 更新哈希
                            // newSha==null → 整条删除（路径行也被跳过）
                        }
                        else
                        {
                            result.Add(lines[i]);
                        }
                    }
                }
                else if (newSha != null)
                {
                    // 清单里没有该条目 → 追加到末尾
                    foreach (string l in lines) result.Add(l);
                    result.Add(entryPath);
                    result.Add(newSha);
                }
                else
                {
                    return true;   // 没有条目且要求删除 → 无事可做
                }

                // ---- 原子写回：UTF-8 无 BOM + LF ----
                string tmp = manifestPath + ".tmp_aic_sha";
                File.WriteAllText(tmp, string.Join("\n", result) + "\n", Utf8NoBom);
                File.Replace(tmp, manifestPath, null, true);
                return true;
            }
            catch
            {
                try { File.Delete(manifestPath + ".tmp_aic_sha"); } catch { }
                return false;
            }
        }

        /// <summary>从 .orig 恢复原始清单（卸载还原配套）。.orig 不存在返回 false。</summary>
        public static bool TryRestoreOriginal(string manifestPath)
        {
            string orig = manifestPath + ".orig";
            try
            {
                if (!File.Exists(orig)) return false;
                File.Copy(orig, manifestPath, true);
                File.Delete(orig);
                return true;
            }
            catch { return false; }
        }
    }
}
