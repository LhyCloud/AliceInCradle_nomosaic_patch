# AliceInCradle_nomosaic_patch 优化清单

> 来源：2026-10-23 对 `LhyCloud/AliceInCradle_nomosaic_patch` 的完整审计（结论：无恶意代码）+ 代码审查。
> 状态标记：✅ 已实施于本次重写 / 🔜 未实施（保留为后续项）

## 一、应修的小 Bug

| # | 问题 | 状态 |
|---|------|------|
| 1 | `Program.cs` 参数解析：`-out` 分支条件重复 `i+1<args.Length && i+1<args.Length`；`-dll` 缺参时仍 `i++`；flag 会被当路径吞掉 | ✅ 单文件重写后无子进程/参数解析 |
| 2 | GUI ↔ CLI 用 stdout 字符串匹配（`"TRUE" / "notfound" / "repeat"`）做 IPC，前端 `find` 全文易误判 | ✅ dnlib 进程内直接调用，无 IPC |
| 3 | 中文输出按系统代码页，非中文系统 MessageBox 乱码 | ✅ 全部进程内 + Unicode API |
| 4 | `define.h` 头文件里定义全局变量（非 `extern`）、`NET_SDK.h` 带实现函数无 `static` —— 多 TU 即链接错误 | ✅ 重写为纯 C#，无此问题 |

## 二、健壮性

| # | 问题 | 状态 |
|---|------|------|
| 5 | 写回不原子（CopyFileW 覆盖 + 中途崩溃导致游戏损坏），README "请勿关闭工具否则游戏可能损坏" 即是此隐患 | ✅ 临时文件 + `File.Replace` 同卷原子替换 |
| 6 | 无备份，卸载是"假还原"（假设初值恒为 1） | ✅ 首次安装自动备份 `Assembly-CSharp.dll.bak`，卸载优先还原备份 |
| 7 | 子进程 `WaitForSingleObject(INFINITE)` 无超时，dnlib 挂死按钮永久禁用 | ✅ 无子进程；dnlib 操作同步函数，失败即返回 |
| 8 | 方法定位脆弱：仅按名字 + 只看最后一个 `ret`；无返回类型校验；大小写敏感；不搜嵌套类型 | ✅ 主返回点=最后一个 `ret`（0.30i 实测多返回点对齐上游语义）、校验返回类型 `Boolean`、大小写不敏感、`GetTypes()` 含嵌套类型 |
| 9 | Program Files 下写文件 ACCESS_DENIED 只报"写入失败" | ✅ 捕获未授权异常 → 提供 UAC 提权重启 |
| 10 | `ExtractResourceToFile` 不校验写结果即 `return true` | ✅ 不再释放资源，无此步骤 |

## 三、架构

| # | 问题 | 状态 |
|---|------|------|
| 11 | C++ 壳 + %TEMP% 释放 + 隐藏子进程，仅为"窗口+单文件"；带来可预测临时路径竞态、双消息循环、字符串 IPC | ✅ **C# WinForms 单文件工具**（`src/AIC_nomosaic_tool`），dnlib 以嵌入式资源经 `AssemblyResolve` 内存加载，零外部依赖 |
| 12 | `patch()` 安装/卸载两分支 ~80 行重复 | ✅ 合并为一个函数 |
| 13 | 参数名 `aaa`；函数体内用全局 `AssemblyDllPath` 而非入参 | ✅ 重写 |

## 四、仓库卫生

| # | 问题 | 状态 |
|---|------|------|
| 14 | `bin/ obj/ .tlog .aps .pdb packages/` 及 545KB 的 ildasm 残留 `~VCACBA.tmp` 全部入库 | ✅ 根目录新增 `.gitignore`（历史文件已归档至 `legacy/`，未删除） |
| 15 | 发布无可验证性（发行二进制与源码需手动比对） | ✅ 新增 GitHub Actions：Windows 构建 + 对 `legacy/patches/` 真实 DLL 跑验证器 + 自动产出 `SHA256SUMS` |
| 16 | 无 LICENSE | 🔜 建议上游作者自行添加（补丁对游戏本体版权有衍生关系，许可证选择应由作者决定） |

## 五、功能升级

| # | 问题 | 状态 |
|---|------|------|
| 17 | 嵌入自身载荷的哈希校验（防 %TEMP% 劫持） | ✅ 不再落地临时文件，从根源消除攻击面 |
| 18 | 链式删除 `.res/.pdb/.mod.il` 稍激进 | 🔜 未移植（新工具不产生这些文件） |
| 19 | **BepInEx/Harmony 运行时插件**：游戏更新后无需重打补丁 | ✅ **已实现**（`src/AIC_nomosaic_bepinex`），配置文件可热开关 |
| 20 | 拖拽支持 / 记住路径 / 安装进度提示 | ✅ 拖拽 + 记住路径（配置文件）已做；进度条留作后续（当前显示耗时） |

## 目录结构

```
AliceInCradle_nomosaic_patch/
├── README.md                    # 上游说明
├── AIC_Nomosaic.sln             # 活跃项目一键构建（dotnet build AIC_Nomosaic.sln -c Release）
├── .github/workflows/build.yml  # CI：构建 + legacy 真实 DLL 验证 + SHA256SUMS
├── .gitignore
├── docs/
│   └── OPTIMIZATION.md          # 本清单
├── src/                         # 活跃开发（详见 src/README.md）
│   ├── AIC_nomosaic_tool/       # C# WinForms 单文件工具（net48，dnlib 内嵌）
│   ├── AIC_nomosaic_bepinex/    # BepInEx 5 + HarmonyX 运行时插件（net48）
│   ├── AIC_patch_tester/        # 补丁逻辑验证器（net10，CI/本地回归用）
│   └── NuGet.config             # nuget.org + BepInEx 官方 feed
└── legacy/                      # 上游历史（只读归档）
    ├── source/                  # 原「自动补丁工具源码」（C++ 壳 + C# CLI）
    └── patches/                 # 原「各版本补丁」（ver023-029 静态 DLL 补丁）
```
