using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace AicNoMosaic
{
    /// <summary>
    /// 从 &lt;游戏根目录&gt;\AliceInCradle_Data\globalgamemanagers
    /// （Unity SerializedFile）解析 PlayerSettings.bundleVersion —— 即运行时
    /// Application.version。
    /// </summary>
    internal static class GameVersion
    {
        private const string DataFolder = "AliceInCradle_Data";
        private const string FileName = "globalgamemanagers";
        private const long MaxFileBytes = 64 * 1024 * 1024;

        // 引擎版本前缀 -> bundleVersion 的长度前缀在 PlayerSettings 对象内的相对偏移。
        // 换 Unity 版本时校验失败会自动降级为对象内扫描，缺条目也不会返回错值。
        // 实测（0.30i，Unity 2022.3.62f2）: 长度前缀在对象内 612，字符串本体在 616。
        private static int? FindLayoutOffset(string engineVersion)
        {
            return engineVersion != null && engineVersion.StartsWith("2022.3", StringComparison.Ordinal)
                ? (int?)612
                : null;
        }

        public static string FromFolder(string gameFolder)
        {
            try
            {
                if (string.IsNullOrEmpty(gameFolder)) return "";
                string path = Path.Combine(gameFolder, DataFolder, FileName);
                if (!File.Exists(path)) return "";
                var fi = new FileInfo(path);
                if (fi.Length < 64 || fi.Length > MaxFileBytes) return "";
                return FromBytes(File.ReadAllBytes(path));
            }
            catch { return ""; }
        }

        /// <summary>字节级入口（测试/诊断用）：行为与 FromFolder 的文件解析部分完全一致。</summary>
        public static string FromBytes(byte[] b)
        {
            try
            {
                if (b == null || b.Length < 64 || b.Length > MaxFileBytes) return "";
                return ExtractVersion(b) ?? "";
            }
            catch { return ""; }
        }

        /// <summary>
        /// 全流程自诊断：返回多行报告（文件存在性/大小/头部各字段/"0.30i" 位置），
        /// 显示"未知"时用于定位在哪个环节失败。不抛异常。
        /// </summary>
        public static string Diagnose(string gameFolder)
        {
            var sb = new StringBuilder();
            try
            {
                string path = string.IsNullOrEmpty(gameFolder)
                    ? ""
                    : Path.Combine(gameFolder, DataFolder, FileName);
                sb.AppendLine("文件路径: " + path);
                if (string.IsNullOrEmpty(path) || !File.Exists(path))
                {
                    sb.Append("结论: globalgamemanagers 不存在");
                    return sb.ToString();
                }

                long len = new FileInfo(path).Length;
                sb.AppendLine("文件大小: " + len.ToString("N0") + " 字节");
                if (len < 64 || len > MaxFileBytes)
                {
                    sb.Append("结论: 大小超出 64B..64MB 范围，拒绝解析");
                    return sb.ToString();
                }

                byte[] b = File.ReadAllBytes(path);
                sb.AppendLine("读取长度: " + b.Length.ToString("N0"));

                HeadInfo h = ParseHead(b);
                sb.AppendLine("头部: Ok=" + h.Ok
                    + " 格式版本=" + h.FormatVersion
                    + " 元数据=" + h.MetadataSize
                    + " 声明大小=" + h.FileSize
                    + " 数据区=" + h.DataOffset
                    + " 引擎=" + (h.EngineVersion ?? "(无)"));
                if (!h.Ok)
                {
                    sb.Append("结论: 头部结构自洽性校验失败");
                    return sb.ToString();
                }
                sb.AppendLine(h.FileSize == (ulong)b.Length
                    ? "声明大小与实际一致"
                    : "!! 声明大小(" + h.FileSize + ") ≠ 实际(" + b.Length + ")");

                // 直接走完整管线（含类型表/对象表/PS 定位），输出最终版本
                string final = ExtractVersion(b);
                sb.AppendLine(string.IsNullOrEmpty(final)
                    ? "结论: 下游（类型表/对象表/PlayerSettings/版本串）某环节失败"
                    : "结论: 解析成功，版本 = " + final);
                return sb.ToString();
            }
            catch (Exception ex)
            {
                sb.Append("!! 诊断过程异常: " + ex.GetType().Name + " " + ex.Message);
                return sb.ToString();
            }
        }

        // ============================ 容器头 ================================

        /// <summary>容器头解析结果（L1 大端整数 + 元数据头引擎串）。</summary>
        internal readonly struct HeadInfo
        {
            public readonly bool Ok;
            public readonly uint FormatVersion;
            public readonly ulong MetadataSize;
            public readonly ulong FileSize;       // 头部声明的文件大小
            public readonly ulong DataOffset;
            public readonly string EngineVersion;

            internal HeadInfo(bool ok, uint formatVersion, ulong metadataSize,
                ulong fileSize, ulong dataOffset, string engineVersion)
            {
                Ok = ok;
                FormatVersion = formatVersion;
                MetadataSize = metadataSize;
                FileSize = fileSize;
                DataOffset = dataOffset;
                EngineVersion = engineVersion;
            }
        }

        /// <summary>
        /// 只解析容器头（48 字节 L1 + 元数据头里的引擎串），不触碰数据区。
        /// </summary>
        internal static HeadInfo ParseHead(byte[] b)
        {
            if (b.Length < 64) return default;

            uint formatVersion = ReadU32BE(b, 8);       // 8..11
            ulong metadataSize = ReadU64BE(b, 16);      // 16..23
            ulong fileSize = ReadU64BE(b, 24);          // 24..31
            ulong dataOffset = ReadU64BE(b, 32);        // 32..39

            bool selfConsistent =
                (uint)(formatVersion - 17U) <= (24U - 17U)
                && metadataSize <= fileSize
                && (int)dataOffset == 48 + AlignUp((int)metadataSize, 16);
            if (!selfConsistent) return default;

            int nul = Array.IndexOf(b, (byte)0, 48);
            if (nul < 0 || nul - 48 > 64) return default;
            string engineVersion = Encoding.ASCII.GetString(b, 48, nul - 48);

            return new HeadInfo(true, formatVersion, metadataSize, fileSize, dataOffset, engineVersion);
        }

        // ============================== 核心提取 =============================

        private static string ExtractVersion(byte[] b)
        {
            if (b.Length < 64) return null;

            // ---- L1+L2：容器头（结构性校验），声明大小必须等于实际长度 ----
            HeadInfo head = ParseHead(b);
            if (!head.Ok || head.FileSize != (ulong)b.Length) return null;   // 截断/伪装文件

            // ---- L2 后半：类型表入口 ----
            int pos = AlignUp(48 + head.EngineVersion.Length + 1, 4);
            pos += 4;                                   // int32 targetPlatform（用不到）
            bool enableTypeTree = b[pos] != 0;
            int typeCount = ReadI32LE(b, pos + 1);
            if (typeCount < 0 || typeCount > 200000) return null;
            int typeTableStart = pos + 5;

            // ---- L3a: 类型表（只取 classID 列表；失败也不影响后续）----
            // 条目 = int32 classID + uint8 isStripped + int16 scriptTypeIndex + 16B 哈希；
            // classID==114 的条目（脚本引用）额外多 16 字节。
            List<int> classIds = null;
            if (!enableTypeTree && TryReadClassIds(b, typeTableStart, typeCount, out classIds, out int afterTypes))
                pos = afterTypes;

            // ---- L3b: 定位对象表（先按位置推算，再用结构判据回查）----
            var objects = new List<ObjEntry>();
            ulong[] slacks = { 0UL, 8192UL };
            int scanEnd = Math.Min((int)Math.Min(head.DataOffset, int.MaxValue), typeTableStart + (2 << 20));

            bool located = false;
            for (int k = 0; k < 2 && !located; k++)
            {
                if (classIds != null && TryReadObjectTable(b, pos, head.DataOffset, head.FileSize, classIds, objects, slacks[k]))
                { located = true; break; }

                objects.Clear();
                for (int cand = typeTableStart; cand + 4 <= scanEnd; cand++)
                {
                    if (TryReadObjectTable(b, cand, head.DataOffset, head.FileSize, classIds, objects, slacks[k]))
                    { located = true; break; }
                    objects.Clear();
                }
            }
            if (!located) return null;

            // ---- L4: 定位 PlayerSettings（优先 classID==129；否则按内容特征兜底）----
            ObjEntry ps = FindPlayerSettings(b, (int)head.DataOffset, objects);
            if (ps.Size == 0) return null;

            // ---- L5: bundleVersion 字符串 ----
            int psBase = (int)head.DataOffset + ps.Start;

            // 快路：已知引擎布局（2022.3 实测长度前缀在对象内 612）；失败自动降级为扫描。
            int? rel = FindLayoutOffset(head.EngineVersion);
            if (rel.HasValue && TryReadVersionFieldAt(b, psBase, ps.Size, rel.Value, out string quick))
                return quick;

            // 慢路：观测 bundleVersion 是 PlayerSettings 对象内最后一个版本样式串
            var cands = CollectVersionCandidates(b, psBase, ps.Size);
            return cands.Count == 0 ? null : cands[cands.Count - 1];
        }

        // ============================ 类型表 ================================

        private static bool TryReadClassIds(byte[] b, int start, int typeCount, out List<int> classIds, out int next)
        {
            classIds = new List<int>(Math.Min(typeCount, 100000));
            next = start;
            int p = start;
            for (int i = 0; i < typeCount; i++)
            {
                if (p + 23 > b.Length) return false;
                classIds.Add(ReadI32LE(b, p));
                p += 4 + 1 + 2 + 16;                 // classID + isStripped + scriptTypeIndex + 哈希
                if (classIds[classIds.Count - 1] == 114) p += 16;   // 脚本引用条目额外 16B
            }
            next = p;
            return true;
        }

        // ============================ 对象表 ================================

        private readonly struct ObjEntry
        {
            public readonly int Start;               // 数据区内偏移
            public readonly int Size;
            public readonly int ClassId;

            internal ObjEntry(int start, int size, int classId)
            { Start = start; Size = size; ClassId = classId; }
        }

        /// <summary>
        /// 只用文件自身的结构约束判定"这里是不是真的对象表"（与内容无关）：
        /// 数量合理、表不越过数据区、byteStart 单调不减且间隙 &lt;64、对象都落在数据区内。
        /// 条目 = pathID(i64) + byteStart(u64) + size(u32) + typeId(i32) = 24B（0.30i 实测仍是此布局）。
        /// </summary>
        private static bool TryReadObjectTable(byte[] b, int countPos,
            ulong dataOffset, ulong fileSize, List<int> classIds, List<ObjEntry> objects, ulong tailSlack)
        {
            objects.Clear();
            if (countPos < 0 || countPos + 4 > b.Length || countPos + 4 > (long)dataOffset) return false;

            int count = ReadI32LE(b, countPos);
            if (count < 1 || count > 200000) return false;

            int first = AlignUp(countPos + 4, 8);
            if (first > b.Length || (long)count * 24 > b.Length - first) return false;
            if ((ulong)(first - countPos) + (ulong)count * 24UL > dataOffset - (ulong)countPos) return false;

            ulong dataSize = fileSize - dataOffset;
            int p = first;
            long prev = 0;
            for (int i = 0; i < count; i++)
            {
                p += 8;                                                        // int64 pathID
                long byteStart = unchecked((long)ReadU64LE(b, p));
                uint byteSize = ReadU32LE(b, p + 8);
                int typeId = ReadI32LE(b, p + 12);
                p += 16;                                                       // 本条共 24B

                if (byteStart < 0 || byteStart < prev || byteStart - prev > 64) return false;
                if ((ulong)byteStart + byteSize > dataSize) return false;
                prev = byteStart + byteSize;

                int classId = classIds != null && typeId >= 0 && typeId < classIds.Count
                    ? classIds[typeId]
                    : -1;
                objects.Add(new ObjEntry((int)byteStart, unchecked((int)byteSize), classId));
            }

            return prev <= (long)dataSize && (ulong)((long)dataSize - prev) <= tailSlack;
        }

        // ================== PlayerSettings 定位与版本串 ======================

        private static ObjEntry FindPlayerSettings(byte[] b, int dataOffset, List<ObjEntry> objects)
        {
            foreach (var o in objects)
                if (o.ClassId == 129) return o;

            // 类型表不可用时按内容特征兜底：对象内同时含版本样式串与包名样式串，
            // 取版本候选最少者（PlayerSettings 只有几个）。
            ObjEntry best = default;
            int bestCount = int.MaxValue;
            foreach (var o in objects)
            {
                if (o.Size == 0 || o.Start < 0 || (long)dataOffset + o.Start + o.Size > b.Length) continue;

                var versions = CollectVersionCandidates(b, dataOffset + o.Start, o.Size);
                if (versions.Count == 0) continue;

                bool hasBundleId = false;
                for (int off = 0; off + 4 <= o.Size && !hasBundleId; off += 4)
                {
                    uint lenU = ReadU32LE(b, dataOffset + o.Start + off);
                    if (lenU < 1 || lenU > 32 || off + 4 + lenU > o.Size) continue;
                    int len = (int)lenU;
                    if (!PrintableAscii(b, dataOffset + o.Start + off + 4, len)) continue;
                    if (LooksLikeBundleId(b, dataOffset + o.Start + off + 4, len)) hasBundleId = true;
                }
                if (!hasBundleId) continue;

                if (versions.Count < bestCount) { bestCount = versions.Count; best = o; }
            }
            return best;
        }

        /// <summary>按已知相对偏移读取 bundleVersion 串（做完整校验）。</summary>
        private static bool TryReadVersionFieldAt(byte[] b, int baseOff, int size, int rel, out string value)
        {
            value = null;
            if (!InRange(b.Length, baseOff, size)) return false;
            if (rel + 4 > size) return false;

            uint lenU = ReadU32LE(b, baseOff + rel);
            if (lenU < 1 || lenU > 32 || rel + 4 + lenU > size) return false;
            int len = (int)lenU;

            if (!PrintableAscii(b, baseOff + rel + 4, len) || !LooksLikeVersion(b, baseOff + rel + 4, len))
                return false;

            int pad = AlignUp(len, 4);
            if (rel + 4 + pad > size || pad > 32) return false;
            for (int k = rel + 4 + len; k < rel + 4 + pad; k++)
                if (b[baseOff + k] != 0) return false;

            value = Encoding.ASCII.GetString(b, baseOff + rel + 4, len);
            return true;
        }

        private static List<string> CollectVersionCandidates(byte[] b, int baseOff, int size)
        {
            var versions = new List<string>();
            for (int off = 0; off + 4 <= size; off += 4)
            {
                uint lenU = ReadU32LE(b, baseOff + off);
                if (lenU < 1 || lenU > 32 || off + 4 + lenU > size) continue;
                int len = (int)lenU;

                if (!PrintableAscii(b, baseOff + off + 4, len)) continue;

                int padded = AlignUp(len, 4);
                if (off + 4 + padded > size || padded > 32) continue;
                bool zeroPad = true;
                for (int k = off + 4 + len; k < off + 4 + padded; k++)
                    if (b[baseOff + k] != 0) { zeroPad = false; break; }
                if (!zeroPad) continue;

                if (LooksLikeVersion(b, baseOff + off + 4, len))
                    versions.Add(Encoding.ASCII.GetString(b, baseOff + off + 4, len));
            }
            return versions;
        }

        // ============================= 字符串判定 ============================

        private static bool PrintableAscii(byte[] b, int off, int len)
        {
            for (int i = 0; i < len; i++)
                if (b[off + i] < 0x20 || b[off + i] > 0x7E) return false;
            return true;
        }

        // 版本样式: 1~4 段数字以 '.' 分隔，可选尾部单字母 + 数字
        // 命中: "1.0" "0.29b" "0.29b2" "1.2.3"   不命中: "Very Low" "x" "0"
        private static bool LooksLikeVersion(byte[] b, int off, int len)
        {
            if (len < 3 || len > 24) return false;
            int i = 0;
            int groups = 0;
            for (;;)
            {
                int st = i;
                while (i < len && IsAsciiDigit((char)b[off + i])) i++;
                if (i - st < 1 || i - st > 4) return false;
                if (++groups > 4) return false;
                if (i < len && b[off + i] == '.') { i++; continue; }
                break;
            }
            if (groups < 2) return false;
            if (i < len && IsAsciiLetter((char)b[off + i]))
            {
                i++;
                while (i < len && IsAsciiDigit((char)b[off + i])) i++;
            }
            return i == len;
        }

        // 包名样式 com.Company.Product：至少两段 '.'，本身不是版本串
        private static bool LooksLikeBundleId(byte[] b, int off, int len)
        {
            if (len < 5 || len > 160 || LooksLikeVersion(b, off, len)) return false;
            int dots = 0;
            for (int i = 0; i < len; i++)
            {
                byte c = b[off + i];
                if (c == '.') { dots++; continue; }
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
                    return false;
            }
            return dots >= 2;
        }

        private static bool IsAsciiDigit(char c) => c >= '0' && c <= '9';
        private static bool IsAsciiLetter(char c) => (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        private static bool IsAsciiLetterOrDigit(char c) => IsAsciiLetter(c) || IsAsciiDigit(c);

        private static bool InRange(int size, int off, int len)
        {
            return off >= 0 && len >= 0 && off <= size && len <= size - off;
        }

        private static int AlignUp(int v, int align) => (v + (align - 1)) & ~(align - 1);

        // ======================== 手写大小端读取 ==============================

        private static uint ReadU32BE(byte[] b, int p)
        {
            return ((uint)b[p] << 24) | ((uint)b[p + 1] << 16) | ((uint)b[p + 2] << 8) | b[p + 3];
        }

        private static uint ReadU32LE(byte[] b, int p)
        {
            return b[p] | ((uint)b[p + 1] << 8) | ((uint)b[p + 2] << 16) | ((uint)b[p + 3] << 24);
        }

        private static int ReadI32LE(byte[] b, int p) { return unchecked((int)ReadU32LE(b, p)); }

        private static ulong ReadU64BE(byte[] b, int p)
        {
            return ((ulong)ReadU32BE(b, p) << 32) | ReadU32BE(b, p + 4);
        }

        private static ulong ReadU64LE(byte[] b, int p)
        {
            ulong v = 0;
            for (int i = 7; i >= 0; --i) v = (v << 8) | b[p + i];
            return v;
        }
    }
}
