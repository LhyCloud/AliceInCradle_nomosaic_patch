# src/ — Alice In Cradle 去马赛克补丁 · 现代化重写

对上游 C++ 壳 + C# 子进程方案的**两个并行升级**（优化清单 #11 与 #19）：

| 子项目 | 形态 | 面向人群 |
|---|---|---|
| `AIC_nomosaic_tool/` | **C# WinForms 单文件工具**（net48，dnlib 内嵌进程内加载） | 想一键安装/卸载、带 GUI 的普通玩家 |
| `AIC_nomosaic_bepinex/` | **BepInEx 5 + HarmonyX 运行时插件**（net48） | 已装 BepInEx / 想要游戏更新免重装的玩家 |
| `AIC_patch_tester/` | 控制台验证器（net10，可直接在 Linux/CI 上跑） | CI 与开发者 |

## 为什么这两个产物都不是 .NET 10？

- **工具 → net48**：目标是玩家的 Windows 机器，Win10/11 自带 .NET Framework 4.8，双击即用零安装；换 net10 需要用户先装 Desktop Runtime 或自包含发布 100MB+。构建 SDK 用 .NET 10 编译 net48 目标完全没问题（SDK 版本 ≠ 目标框架）。
- **插件 → net48**：插件要被**游戏内的 Unity Mono 运行时**加载，Mono 无法加载 .NET 8/10 程序集，这是硬约束（Unity 2022.3 Mono ≈ netstandard2.1 级别）。
- **测试器 → net10.0**：跑在开发/CI 机器上，没有理由不用新版本。

## AIC_nomosaic_tool（friendly GUI 单文件）

```
AIC_nomosaic_tool.csproj   net48，dnlib.dll 作为 EmbeddedResource 打进 exe
Program.cs                 入口：单实例互斥 + AssemblyResolve 内存加载 dnlib
MosaicPatcher.cs           补丁核心（dnlib 进程内改 IL）
GameVersion.cs             globalgamemanagers 版本解析（逐行移植上游 version.h）
DebugConfig.cs             _debug.txt 编辑（逐行移植上游 debug_config.h）
GameFiles.cs               路径/占用检测/设置持久化/UAC 提权重启
MainForm.cs                主窗口（拖拽支持、记住路径、补丁状态显示）
DebugSettingsForm.cs       游戏调试设置窗口
```

相比上游的改进：dnlib 进程内调用（无临时文件释放、无 stdout 字符串 IPC）、
`File.Replace` 原子写回 + 自动备份 `.bak`、遍历全部 `ret` 返回点 + 返回类型校验、
大小写不敏感 + 嵌套类型搜索、Program Files 拒绝访问时 UAC 提权重启。

构建（任何装有 .NET 8/10 SDK 的机器，含 Linux）：
```bash
dotnet build src/AIC_nomosaic_tool -c Release
# 产物: src/AIC_nomosaic_tool/bin/Release/net48/AIC_nomosaic_tool.exe
# 或仓库根目录一键构建: dotnet build AIC_Nomosaic.sln -c Release
```

## AIC_nomosaic_bepinex（运行时插件）

原理：HarmonyX Postfix 拦截 `MosaicShower.FnDrawMosaic` 的 `bool` 返回值改为 `false`。
不做文件修改 → **游戏更新后不用重打**，删掉 dll 即还原；也能与静态补丁共存
（Harmony 层优先生效，卸载插件后静态补丁仍继续工作）。

```bash
dotnet build src/AIC_nomosaic_bepinex -c Release
# 产物: src/AIC_nomosaic_bepinex/bin/Release/net48/AIC_nomosaic_bepinex.dll
```

安装：
1. 安装 [BepInEx 5.4.22 x64](https://github.com/BepInEx/BepInEx/releases)（Unity Mono 版）到游戏目录，先启动一次游戏完成初始化；
2. 把 `AIC_nomosaic_bepinex.dll` 放进 `BepInEx/plugins/`；
3. 开关：`BepInEx/config/com.lhycloud.aic.nomosaic.cfg`（或 F1 配置管理器热切换）。

找不到 `MosaicShower.FnDrawMosaic` 时插件只写日志、不打补丁、不崩溃（`TargetMethod()`/`Prepare` 防护）。

## AIC_patch_tester（补丁逻辑验证器）

对真实 `Assembly-CSharp.dll` 执行 Inspect → 翻转 → 幂等 → round-trip 校验，非 0 退出码：
```bash
dotnet run --project src/AIC_patch_tester -c Release -- path/to/Assembly-CSharp.dll
```
CI（`.github/workflows/build.yml`）会在 windows-latest 上构建全部三个项目，
并自动展开仓库内 ver027b/ver028b/ver029b2 三个补丁 DLL 跑验证，同时产出 `SHA256SUMS`
供使用者核对发布完整性。
