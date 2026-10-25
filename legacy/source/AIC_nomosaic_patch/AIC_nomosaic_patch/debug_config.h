
// 固定 6 个键，顺序与复选框 9001-9006 一一对应：
//     <DEBUG>  mighty  nodamage  weak  allskill  announce
//
// 文件中缺失的键：读的时候 valid 为 0（由 UI 决定禁用复选框），写的时候跳过。
// 文件里其它键（nosnd / benchmark / ...）：既不读也不写，所在行原样保留。
//

#ifndef AIC_DEBUG_CONFIG_H
#define AIC_DEBUG_CONFIG_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

// -----------------------------------------------------------------------------
// 键名表 + 两个函数
// -----------------------------------------------------------------------------

struct DebugKey {
    const wchar_t* name;    // 文件中的键名
    const wchar_t* textCN;  // 注释译文（给 UI 写提示 / 生成文档用）
    const wchar_t* textEN;  // 注释原文
};

// 6 个键，按固定顺序（顺序 == 复选框 9001 + i）
static const DebugKey kDebugKeys[] = {
    { L"<DEBUG>",   L"调试总开关：置 1 允许使用调试功能",          L"use debug." },
    { L"mighty",    L"置 1 使玩家攻击力极高",                      L"Enter 1 to make player's attack very strong" },
    { L"nodamage",  L"置 1 使玩家受到的伤害降为 0",                L"Enter 1 to reduce the damage the player takes to 0" },
    { L"weak",      L"置 1 时诺艾尔受到伤害即立刻晕厥",            L"If you enter 1, Noel will be fainted immediately when taken damage." },
    { L"allskill",  L"启用全部隐藏技能",                           L"enable all hidden skills." },
    { L"announce",  L"启用日志播报：上次游玩出错时，标题画面会提示错误日志", L"enable log announce." },
};

#define AIC_DEBUG_KEY_COUNT  ((int)(sizeof(kDebugKeys) / sizeof(kDebugKeys[0])))

// 取第 i 个键名；越界返回 0
static __inline const wchar_t* DebugKeyName(int i)
{
    if (i < 0 || i >= AIC_DEBUG_KEY_COUNT) return 0;
    return kDebugKeys[i].name;
}

// 读 _debug.txt，把 6 个键的值取出来。
//   path      : 文件完整路径
//   outValues : 接收 6 个值，长度至少 AIC_DEBUG_KEY_COUNT，按上表顺序
//   outValid  : 接收 6 个标志，非 0 = 文件里有这个键，0 = 缺失
// 返回：true = 读取成功（缺某些键也算成功）；false = 打开/读取失败
static __inline bool LoadDebugConfigFromFile(const std::wstring& path,
    int* outValues, int* outValid);

// 写 _debug.txt：只改 values 里那几个键的值（valid 为 0 的跳过）。
// 注释、空行、缩进、行数、其它键所在的行全部原样保留。
// 返回：true = 已保存（或本来就一致，无需改动）；false = 失败
static __inline bool SaveDebugConfigToFile(const std::wstring& path,
    const int* values, const int* valid);


// =============================================================================
//                                  实现
// =============================================================================

// 行内空白（不含 \n，\n 单独处理）
#define AIC_SPACE_CHARS  " \t\v\f\r"

static __inline int AicIsSpace(char c)
{
    return strchr(AIC_SPACE_CHARS, c) != 0;
}

static __inline int AicIsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// 宽 -> 窄（键名都是 ASCII，直接取低 7 位）
static __inline void AicNarrowKey(const wchar_t* key, char* out, size_t outSize)
{
    size_t i = 0;
    for (; key[i] && i + 1 < outSize; ++i)
        out[i] = (char)(key[i] & 0x7F);
    out[i] = 0;
}

// 一把读进整个文件，末尾补 \0；失败返回 false
static __inline bool AicReadWholeFile(const std::wstring& path, char** outBuf, DWORD* outSize)
{
    *outBuf = 0;
    *outSize = 0;

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD size = GetFileSize(h, 0);
    if (size == INVALID_FILE_SIZE) size = 0;

    char* buf = (char*)malloc((size_t)size + 2);   // 末尾留 2 字节
    if (!buf) { CloseHandle(h); return false; }

    DWORD read = 0;
    if (size > 0 && !ReadFile(h, buf, size, &read, 0)) {
        free(buf);
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);

    buf[read] = 0;
    buf[read + 1] = 0;
    *outBuf = buf;
    *outSize = read;
    return true;
}

// 原子写回：先写同目录临时文件，再整体替换
static __inline bool AicWriteWholeFile(const std::wstring& path, const char* buf, size_t size)
{
    const std::wstring tmp = path + L".tmp_aic_dbg";

    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    if (size > 0) {
        DWORD written = 0;
        ok = (WriteFile(h, buf, (DWORD)size, &written, 0) != 0) && (written == (DWORD)size);
    }
    CloseHandle(h);

    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// 核心：一趟扫描，找出某个键的值以及值在缓冲区里的字节范围
//
// 返回  1 = 找到了（*outValue 是值，*outStart/*outEnd 是值的字节范围）
// 返回  0 = 没找到，或这一行没有合法值
// 返回 -1 = 参数错误
//
// 行格式：  <键名> <任意空白> <数字> [空白或 // 注释 ...]
// -----------------------------------------------------------------------------
static __inline int AicFindKey(const char* buf, size_t size, const char* key,
    int* outValue, size_t* outStart, size_t* outEnd)
{
    if (!buf || !key) return -1;

    const size_t keyLen = strlen(key);
    size_t i = 0;

    while (i < size) {
        // 1) 取出一行（不含换行符）
        const size_t lineStart = i;
        while (i < size && buf[i] != '\n') ++i;
        size_t lineEnd = i;
        if (lineEnd > lineStart && buf[lineEnd - 1] == '\r') --lineEnd;
        if (i < size) ++i;                       // 跳过 '\n'

        // 2) 跳过行首空白
        size_t p = lineStart;
        while (p < lineEnd && AicIsSpace(buf[p])) ++p;

        // 3) 空行 / 整行注释
        if (p >= lineEnd) continue;
        if (buf[p] == '/' && p + 1 < lineEnd && buf[p + 1] == '/') continue;

        // 4) 键名必须完全匹配，且后面是空白或行尾（避免 mighty2 命中 mighty）
        if ((size_t)(lineEnd - p) < keyLen) continue;
        if (memcmp(buf + p, key, keyLen) != 0) continue;
        size_t q = p + keyLen;
        if (q < lineEnd && !AicIsSpace(buf[q])) continue;

        // 5) 跳过键名与值之间的空白
        while (q < lineEnd && AicIsSpace(buf[q])) ++q;
        if (q >= lineEnd) return 0;              // 只有键名，缺值

        // 6) 值：连续数字，可带一个负号
        const size_t valueStart = q;
        if (buf[q] == '-') ++q;
        if (q >= lineEnd || !AicIsDigit(buf[q])) return 0;

        int value = 0;
        while (q < lineEnd && AicIsDigit(buf[q])) {
            value = value * 10 + (buf[q] - '0');
            ++q;
        }
        const size_t valueEnd = q;

        // 7) 值后面必须是空白、行尾或注释（否则不是纯值，例如 0x1F）
        if (q < lineEnd && !AicIsSpace(buf[q]) && buf[q] != '/') return 0;

        if (outValue) *outValue = (buf[valueStart] == '-') ? -value : value;
        if (outStart) *outStart = valueStart;
        if (outEnd) *outEnd = valueEnd;
        return 1;
    }
    return 0;
}

static __inline bool LoadDebugConfigFromFile(const std::wstring& path,
    int* outValues, int* outValid)
{
    if (!outValues || !outValid) return false;

    // 先全部当成"缺失 + 值 0"
    for (int i = 0; i < AIC_DEBUG_KEY_COUNT; ++i) {
        outValues[i] = 0;
        outValid[i] = 0;
    }

    char* buf = 0;
    DWORD size = 0;
    if (!AicReadWholeFile(path, &buf, &size)) return false;

    for (int i = 0; i < AIC_DEBUG_KEY_COUNT; ++i) {
        char key[64];
        AicNarrowKey(kDebugKeys[i].name, key, sizeof(key));

        int value = 0;
        if (AicFindKey(buf, (size_t)size, key, &value, 0, 0) == 1) {
            outValues[i] = value;
            outValid[i] = 1;
        }
        // == 0：文件里没有这个键（或这一行没有合法值），保持"缺失"
    }

    free(buf);
    return true;
}

static __inline bool SaveDebugConfigToFile(const std::wstring& path,
    const int* values, const int* valid)
{
    if (!values || !valid) return false;

    char* buf = 0;
    DWORD size = 0;
    if (!AicReadWholeFile(path, &buf, &size)) return false;

    // 先把要改的位置都找出来，再统一改，避免一边改一边找
    struct Change { size_t offset; char digit; };
    Change todo[AIC_DEBUG_KEY_COUNT];
    int count = 0;      // 找到的可改项
    int wanted = 0;     // 想要改的项（valid 为 1 的个数）

    for (int i = 0; i < AIC_DEBUG_KEY_COUNT; ++i) {
        if (!valid[i]) continue;     // valid 为 0 -> 不写
        ++wanted;

        int value = values[i];
        if (value < 0) value = 0;
        if (value > 9) continue;     // 只支持一位数（本文件的值都是 0/1）

        char key[64];
        AicNarrowKey(kDebugKeys[i].name, key, sizeof(key));

        size_t start = 0, end = 0;
        if (AicFindKey(buf, (size_t)size, key, 0, &start, &end) != 1) continue;
        if (end != start + 1) continue;      // 原值不是一位数，替换会改长度 -> 跳过

        todo[count].offset = start;
        todo[count].digit = (char)('0' + value);
        ++count;
    }

    if (count == 0) {
        // 一条都改不了：调用方本来就没要求写 -> 成功；有要求却写不了 -> 失败
        free(buf);
        return (wanted == 0);
    }

    // 有没有真的需要改
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (buf[todo[i].offset] != todo[i].digit) { changed = true; break; }
    }
    if (!changed) {
        free(buf);
        return true;                 // 本来就是这些值：一个字节都不动
    }

    // 就地替换：全是等长（1 字节）改动，位置不会互相影响
    for (int i = 0; i < count; ++i)
        buf[todo[i].offset] = todo[i].digit;

    const bool ok = AicWriteWholeFile(path, buf, (size_t)size);
    free(buf);
    return ok;
}

#endif // AIC_DEBUG_CONFIG_H
