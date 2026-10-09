#pragma once
// ===========================================================================
//  读取游戏版本号
// ===========================================================================
//      #include "game_version.h"
//      std::wstring ver = GetGameVersionFromFolder(L"D:\\Games\\AliceInCradle");
//      if (!ver.empty()) { /* ver == L"0.29b" */ }
//
//      * 传入游戏根目录，内部固定拼 <根目录>\AliceInCradle_Data\globalgamemanagers
//      * 取到返回版本号；任何情况取不到都返回空 wstring
//
//  原理: 版本号来自 globalgamemanagers (Unity SerializedFile) 里 PlayerSettings 对象的
//        bundleVersion 字段 —— 即运行时 Application.version。本头文件解析容器结构来
//        定位字段，而不是硬编码文件偏移。
// ===========================================================================

#ifndef GAME_VERSION_H_INCLUDED
#define GAME_VERSION_H_INCLUDED

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace game_version_detail {

    using u8 = std::uint8_t;

    // ------------------------------ 常量 ---------------------------------------

    const wchar_t kDataFolder[] = L"AliceInCradle_Data";
    const wchar_t kFileName[] = L"globalgamemanagers";
    const long    kMaxFileBytes = 64L * 1024L * 1024L;   // 实际只有几百 KB，仅作防误传

    // ------------------------------ 字节读取 -----------------------------------

    inline std::uint32_t ReadU32BE(const u8* p) {
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
            (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
    }
    inline std::uint32_t ReadU32LE(const u8* p) {
        return  std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
            (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    }
    inline std::int32_t  ReadI32LE(const u8* p) { return static_cast<std::int32_t>(ReadU32LE(p)); }
    inline std::uint64_t ReadU64BE(const u8* p) {
        return (std::uint64_t(ReadU32BE(p)) << 32) | std::uint64_t(ReadU32BE(p + 4));
    }
    inline std::uint64_t ReadU64LE(const u8* p) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
        return v;
    }

    // off + len <= size，且不产生溢出
    inline bool InRange(std::size_t size, std::size_t off, std::size_t len) {
        return off <= size && len <= size - off;
    }
    inline std::size_t AlignUp(std::size_t v, std::size_t a) { return (v + a - 1) & ~(a - 1); }

    // ------------------------------ 字符串语义判定 -----------------------------

    inline bool PrintableAscii(const char* s, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x20 || c > 0x7E) return false;
        }
        return true;
    }

    // 版本样式: 1~4 段数字以 '.' 分隔，可选尾部单字母 + 可选数字
    // 命中: "1.0" "0.29b" "0.29b2" "1.2.3"   不命中: "Very Low" "x" "0"
    inline bool LooksLikeVersion(const char* s, std::size_t n) {
        if (n < 3 || n > 24) return false;
        std::size_t i = 0;
        int groups = 0;
        for (;;) {
            const std::size_t st = i;
            while (i < n && s[i] >= '0' && s[i] <= '9') ++i;
            const std::size_t digits = i - st;
            if (digits < 1 || digits > 4) return false;
            if (++groups > 4) return false;
            if (i < n && s[i] == '.') { ++i; continue; }
            break;
        }
        if (groups < 2) return false;
        if (i < n && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))) {
            ++i;
            while (i < n && s[i] >= '0' && s[i] <= '9') ++i;
        }
        return i == n;
    }

    // 包名样式 com.Company.Product（至少两段 '.'，且本身不是版本串）
    inline bool LooksLikeBundleId(const char* s, std::size_t n) {
        if (n < 5 || n > 160) return false;
        if (LooksLikeVersion(s, n)) return false;
        int dots = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const char c = s[i];
            if (c == '.') { ++dots; continue; }
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
        }
        return dots >= 2;
    }

    inline std::wstring WidenAscii(const std::string& s) {
        std::wstring w;
        w.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i)
            w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(s[i])));
        return w;
    }

    // ------------------------------ 整文件读入 ---------------------------------

    inline bool ReadWholeFile(const std::wstring& path, std::vector<u8>& out) {
#if defined(_WIN32)
        FILE* f = nullptr;
        if (::_wfopen_s(&f, path.c_str(), L"rb") != 0) return false;   // 失败只返回空指针
#else
        std::string narrow;                       // 非 Windows: 宽字符按 UTF-8 编码后打开
        narrow.reserve(path.size());
        for (std::size_t i = 0; i < path.size(); ++i) {
            std::uint32_t cp = static_cast<std::uint32_t>(path[i]);
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < path.size()) {
                const std::uint32_t lo = static_cast<std::uint32_t>(path[i + 1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); ++i; }
            }
            if (cp < 0x80) narrow.push_back(static_cast<char>(cp));
            else if (cp < 0x800) {
                narrow.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                narrow.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000) {
                narrow.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                narrow.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else {
                narrow.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                narrow.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                narrow.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        FILE* f = std::fopen(narrow.c_str(), "rb");
#endif
        if (!f) return false;

        bool ok = false;
        if (std::fseek(f, 0, SEEK_END) == 0) {
            const long sz = std::ftell(f);
            if (sz >= 64 && sz <= kMaxFileBytes && std::fseek(f, 0, SEEK_SET) == 0) {
                try { out.assign(static_cast<std::size_t>(sz), 0); ok = true; }
                catch (...) { ok = false; }
                if (ok) {
                    std::size_t got = 0;
                    while (got < out.size()) {
                        const std::size_t r = std::fread(out.data() + got, 1, out.size() - got, f);
                        if (r == 0) break;
                        got += r;
                    }
                    if (got != out.size()) { out.clear(); ok = false; }   // 读不全就放弃
                }
            }
        }
        std::fclose(f);
        return ok;
    }

    // ------------------------------ 序列化串扫描 -------------------------------

    struct Candidate {
        std::size_t rel;          // 指向 4 字节长度前缀（对象内相对偏移）
        std::string value;
    };

    // 扫描 [base, base+size) 内所有"格式良好"的 Unity 序列化字符串。
    // 校验: 4 字节对齐起点 + 长度 1..32 + 全可打印 ASCII + 对齐填充字节必须为 0
    inline void ScanSerializedStrings(const std::vector<u8>& b, std::size_t base, std::size_t size,
        std::vector<Candidate>* versions, std::vector<std::string>* ids) {
        if (!InRange(b.size(), base, size)) return;
        for (std::size_t off = 0; off + 4 <= size; off += 4) {
            const std::uint32_t len = ReadU32LE(b.data() + base + off);
            if (len < 1 || len > 32) continue;
            if (off + 4 + len > size) continue;
            const char* s = reinterpret_cast<const char*>(b.data() + base + off + 4);
            if (!PrintableAscii(s, len)) continue;

            const std::size_t pad = AlignUp(len, 4);
            if (off + 4 + pad > size || pad > 32) continue;
            bool zeroPad = true;
            for (std::size_t k = off + 4 + len; k < off + 4 + pad; ++k)
                if (b[base + k] != 0) { zeroPad = false; break; }
            if (!zeroPad) continue;

            if (ids && LooksLikeBundleId(s, len)) ids->push_back(std::string(s, len));
            if (versions && LooksLikeVersion(s, len)) { Candidate c; c.rel = off; c.value.assign(s, len); versions->push_back(c); }
        }
    }

    // ------------------------------ 对象表结构校验 -----------------------------

    struct ObjEntry {
        std::int64_t  byteStart = 0;
        std::uint32_t byteSize = 0;
        std::int32_t  classId = -1;
    };

    // 只用文件自身的结构约束判定"这里是不是真的对象表"（与内容无关）:
    //    * 对象数合理，且整张表能放进元数据区（不越过 dataOffset）
    //    * byteStart 非递减，相邻间隙 < 64（Unity 会把对象数据对齐到 4/8/16 字节）
    //    * 每个对象都落在数据区内
    //    * 最后一个对象顶到数据区末尾（tailSlack 允许留白；带 .resS 的文件放宽）
    inline bool ValidateObjectTable(const std::vector<u8>& b, std::size_t countPos,
        std::uint64_t dataOffset, std::uint64_t fileSize,
        const std::vector<std::int32_t>* classIds,
        std::vector<ObjEntry>& out, std::uint64_t tailSlack) {
        out.clear();
        if (!InRange(b.size(), countPos, 4)) return false;
        if (static_cast<std::uint64_t>(countPos) + 4 > dataOffset) return false;

        const std::int32_t count = ReadI32LE(b.data() + countPos);
        if (count < 1 || count > 200000) return false;

        const std::size_t first = AlignUp(countPos + 4, 8);
        if (first > dataOffset) return false;
        const std::uint64_t tableBytes = static_cast<std::uint64_t>(first - countPos) +
            static_cast<std::uint64_t>(count) * 24ull;
        if (tableBytes > dataOffset - countPos) return false;
        if (!InRange(b.size(), first, static_cast<std::size_t>(count) * 24u)) return false;

        const std::uint64_t dataSize = fileSize - dataOffset;
        std::int64_t prev = 0;
        out.reserve(static_cast<std::size_t>(count));
        std::size_t p = first;
        for (std::int32_t i = 0; i < count; ++i) {
            ObjEntry o;
            p += 8;                                                    // int64 pathID（用不到）
            o.byteStart = static_cast<std::int64_t>(ReadU64LE(b.data() + p)); p += 8;
            o.byteSize = ReadU32LE(b.data() + p);                            p += 4;
            const std::int32_t typeId = ReadI32LE(b.data() + p);              p += 4;

            if (o.byteStart < 0 || o.byteStart < prev) return false;
            if (o.byteStart - prev > 64) return false;                 // 间隙不能过大
            if (static_cast<std::uint64_t>(o.byteStart) + o.byteSize > dataSize) return false;
            prev = o.byteStart + static_cast<std::int64_t>(o.byteSize);

            if (classIds && typeId >= 0 && static_cast<std::size_t>(typeId) < classIds->size())
                o.classId = (*classIds)[static_cast<std::size_t>(typeId)];
            out.push_back(o);
        }
        if (prev > static_cast<std::int64_t>(dataSize)) return false;
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(dataSize) - prev) <= tailSlack;
    }

    // --------------------------- 字段布局表（按引擎版本） ----------------------

    // 引擎版本前缀 -> bundleVersion 的 4 字节长度前缀在 PlayerSettings 对象内的相对偏移。
    // 换 Unity 版本时若校验失败会自动降级为对象内扫描，因此这里缺条目也不会返回错值。
    inline const std::size_t* FindLayoutOffset(const std::string& engineVersion) {
        static const struct { const char* prefix; std::size_t rel; } kLayouts[] = {
            { "2022.3", 612 },   // 实测: 长度前缀在对象内 612，字符串本体在 616
        };
        for (std::size_t i = 0; i < sizeof(kLayouts) / sizeof(kLayouts[0]); ++i) {
            const std::size_t n = std::char_traits<char>::length(kLayouts[i].prefix);
            if (engineVersion.size() >= n && engineVersion.compare(0, n, kLayouts[i].prefix) == 0)
                return &kLayouts[i].rel;
        }
        return nullptr;
    }

    // 按已知相对偏移取串并做全量校验
    inline bool ReadFieldAt(const std::vector<u8>& b, std::size_t base, std::size_t size,
        std::size_t rel, std::string& out) {
        if (!InRange(b.size(), base, size)) return false;
        if (rel + 4 > size) return false;
        const std::uint32_t len = ReadU32LE(b.data() + base + rel);
        if (len < 1 || len > 32 || rel + 4 + len > size) return false;

        const char* s = reinterpret_cast<const char*>(b.data() + base + rel + 4);
        if (!PrintableAscii(s, len) || !LooksLikeVersion(s, len)) return false;

        const std::size_t pad = AlignUp(len, 4);
        if (rel + 4 + pad > size || pad > 32) return false;
        for (std::size_t k = rel + 4 + len; k < rel + 4 + pad; ++k)
            if (b[base + k] != 0) return false;

        out.assign(s, len);
        return true;
    }

    // ------------------------------ 核心提取 -----------------------------------

    inline bool ExtractVersion(const std::vector<u8>& b, std::wstring& out) {
        const std::size_t n = b.size();
        if (n < 64) return false;
        const u8* d = b.data();

        // ---- L1: 文件头 48 字节（这些整数是大端）----
        const std::uint32_t formatVersion = ReadU32BE(d + 8);
        const std::uint64_t metadataSize = ReadU64BE(d + 16);
        const std::uint64_t fileSize = ReadU64BE(d + 24);
        const std::uint64_t dataOffset = ReadU64BE(d + 32);

        if (formatVersion < 17 || formatVersion > 24) return false;
        if (fileSize != n) return false;
        if (metadataSize > n) return false;
        if (dataOffset != 48ull + AlignUp(static_cast<std::size_t>(metadataSize), 16)) return false;

        // ---- L2: 元数据头（小端）----
        std::size_t e = 48;
        while (e < n && d[e] != 0) ++e;
        if (e >= n || e - 48 > 64) return false;
        const std::string engineVersion(reinterpret_cast<const char*>(d + 48), e - 48);

        std::size_t pos = AlignUp(e + 1, 4);
        if (!InRange(n, pos, 9)) return false;
        pos += 4;                                                   // int32 targetPlatform
        const bool enableTypeTree = (d[pos] != 0); pos += 1;
        const std::int32_t typeCount = ReadI32LE(d + pos); pos += 4;
        if (typeCount < 0 || typeCount > 200000) return false;

        const std::size_t typeTableStart = pos;

        // ---- L3a: 类型表（只取 classID 列表；失败也不影响后续）----
        // 条目 = int32 classID + uint8 isStripped + int16 scriptTypeIndex + 16 字节哈希；
        // 实测 classID==114 的条目额外再带 16 字节。
        std::vector<std::int32_t> classIds;
        bool haveClassIds = false;
        if (!enableTypeTree) {
            std::size_t p = pos;
            bool ok = true;
            try { classIds.reserve(static_cast<std::size_t>(typeCount)); }
            catch (...) { ok = false; }
            for (std::int32_t i = 0; ok && i < typeCount; ++i) {
                if (!InRange(n, p, 23)) { ok = false; break; }
                const std::int32_t cid = ReadI32LE(d + p);
                p += 4 + 1 + 2 + 16;
                if (cid == 114) {
                    if (!InRange(n, p, 16)) { ok = false; break; }
                    p += 16;
                }
                try { classIds.push_back(cid); }
                catch (...) { ok = false; }
            }
            if (ok) { haveClassIds = true; pos = p; }
            else { classIds.clear(); }
        }

        // ---- L3b: 定位对象表（先按结构推算位置，再用结构判据回查）----
        std::vector<ObjEntry> objects;
        const std::vector<std::int32_t>* pids = haveClassIds ? &classIds : nullptr;
        const std::uint64_t slacks[2] = { 0ull, 8192ull };
        const std::size_t limit = static_cast<std::size_t>(dataOffset);
        std::size_t scanEnd = limit;
        if (scanEnd > typeTableStart + (2u << 20)) scanEnd = typeTableStart + (2u << 20);   // 回查范围限 2MB

        bool located = false;
        for (int k = 0; k < 2 && !located; ++k) {
            if (haveClassIds && ValidateObjectTable(b, pos, dataOffset, fileSize, pids, objects, slacks[k])) {
                located = true;
                break;
            }
            objects.clear();
            for (std::size_t cand = typeTableStart; cand + 4 <= scanEnd; ++cand) {
                if (ValidateObjectTable(b, cand, dataOffset, fileSize, pids, objects, slacks[k])) {
                    located = true;
                    break;
                }
                objects.clear();
            }
        }
        if (!located) return false;

        // ---- L4: 定位 PlayerSettings ----
        // 优先 classID==129；类型表不可用时按内容特征兜底（该对象内同时含包名样式串与
        // 版本样式串），取版本候选最少者 —— PlayerSettings 只有几个。
        const ObjEntry* ps = nullptr;
        for (std::size_t i = 0; i < objects.size(); ++i)
            if (objects[i].classId == 129) { ps = &objects[i]; break; }

        if (!ps) {
            std::size_t best = static_cast<std::size_t>(-1);
            for (std::size_t i = 0; i < objects.size(); ++i) {
                const ObjEntry& o = objects[i];
                const std::size_t base = static_cast<std::size_t>(dataOffset) + static_cast<std::size_t>(o.byteStart);
                if (!InRange(n, base, o.byteSize)) continue;
                std::vector<Candidate> v;
                std::vector<std::string> ids;
                ScanSerializedStrings(b, base, o.byteSize, &v, &ids);
                if (v.empty() || ids.empty()) continue;
                if (v.size() < best) { best = v.size(); ps = &objects[i]; }
            }
        }
        if (!ps || ps->byteSize == 0) return false;

        const std::size_t base = static_cast<std::size_t>(dataOffset) + static_cast<std::size_t>(ps->byteStart);
        const std::size_t size = ps->byteSize;
        if (!InRange(n, base, size)) return false;

        // ---- L5: 定位字段（布局表 + 校验；失败则对象内扫描）----
        if (const std::size_t* rel = FindLayoutOffset(engineVersion)) {
            std::string val;
            if (ReadFieldAt(b, base, size, *rel, val)) { out = WidenAscii(val); return !out.empty(); }
        }

        std::vector<Candidate> cands;
        ScanSerializedStrings(b, base, size, &cands, nullptr);
        if (cands.empty()) return false;

        out = WidenAscii(cands.back().value);      // 观测: bundleVersion 是该对象内最后一个版本样串
        return !out.empty();
    }

} // namespace game_version_detail

// 传入游戏根目录（如 L"D:\\Games\\AliceInCradle"），返回版本号（如 L"0.29b"）。
// 任何失败返回空 wstring；全程不抛异常、不输出、不退出进程。
inline std::wstring GetGameVersionFromFile(const std::wstring& gameFolder) {
    try {
        if (gameFolder.empty()) return std::wstring();

        std::wstring path = gameFolder;
        const wchar_t last = path[path.size() - 1];
        if (last != L'\\' && last != L'/') path.push_back(L'\\');
        path += game_version_detail::kDataFolder;
        path.push_back(L'\\');
        path += game_version_detail::kFileName;

        std::vector<game_version_detail::u8> buf;
        if (!game_version_detail::ReadWholeFile(path, buf)) return std::wstring();

        std::wstring ver;
        if (!game_version_detail::ExtractVersion(buf, ver)) return std::wstring();
        return ver;                                  // 局部量移动返回，不抛
    }
    catch (...) {
        return std::wstring();                       // 任何意外一律吞掉
    }
}

std::wstring GetVer(const std::wstring& folderPath)
{
    std::wstring aabbcc = GetGameVersionFromFile(folderPath);
    if (aabbcc != L"")return aabbcc;//法1返回读取到的版本号
    // 法2找到最后一个斜杠，取最后的文件夹名
    size_t lastSlash = folderPath.find_last_of(L"\\/");
    std::wstring folderName;
    if (lastSlash != std::wstring::npos)
        folderName = folderPath.substr(lastSlash + 1);
    else
        folderName = folderPath; // 没有斜杠，整个字符串就是文件夹名

    // 统计下划线数量
    size_t firstUnderscore = folderName.find(L'_');
    if (firstUnderscore == std::wstring::npos)
        return L"未知"; // 没有下划线

    size_t lastUnderscore = folderName.rfind(L'_');
    if (firstUnderscore != lastUnderscore)
        return L"未知"; // 多个下划线

    if (firstUnderscore + 1 >= folderName.size())
        return L"未知"; // 下划线在末尾

    // 返回下划线后的部分
    return folderName.substr(firstUnderscore + 1);
}

#endif // GAME_VERSION_H_INCLUDED
