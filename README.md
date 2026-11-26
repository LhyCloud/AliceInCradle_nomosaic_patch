<div align="center">
<img width="100" alt="image" src="https://github.com/user-attachments/assets/35f4f436-2f92-4e63-861a-59ca35a06d33" /> 
 
# AliceInCradle_nomosaic_patch
Alice In Cradle(AIC)（摇篮里的爱丽丝）去除马赛克补丁 
 
[![Release](https://img.shields.io/github/v/release/LhyCloud/AliceInCradle_nomosaic_patch?style=flat-square&label=release)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases/latest) [![Downloads](https://img.shields.io/github/downloads/LhyCloud/AliceInCradle_nomosaic_patch/total?style=flat-square&logo=github)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases) [![Stars](https://img.shields.io/github/stars/LhyCloud/AliceInCradle_nomosaic_patch?style=flat-square&logo=github&logoColor=white&label=stars&color=f5c518)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/stargazers) [![Platform](https://img.shields.io/badge/platform-Windows_7%2B-0078D6?style=flat-square)](#)

![C++](https://img.shields.io/badge/C%2B%2B-v142_/_MSVC-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![C#](https://img.shields.io/badge/C%23-.NET_Framework_4.7.2-512BD4?style=flat-square)
![IL rewrite](https://img.shields.io/badge/IL_rewrite-dnlib_4.5.0-5C2D91?style=flat-square)
![Engine](https://img.shields.io/badge/Game_Engine-Unity-000000?style=flat-square&logo=unity&logoColor=white)
![IDE](https://img.shields.io/badge/IDE-Microsoft_Visual_Studio_2019-5C2D91?style=flat-square&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjEuNDAgMS40MCAyMS4yMCAyMS4yMCI%2BPHBhdGggZmlsbD0iI2ZmZmZmZiIgZD0ibTE3IDguNWwtNC43NSAzLjgyTDE3IDE2ek00LjcgMTguNEwyIDE2Ljd2LTlsMy0xbDQuMyAzLjMzTDE4IDJsNCAyLjVWMjBsLTUgMmwtNy42Ni03LjM0ek01IDE0bDEuODYtMS43Mkw1IDEwLjV6Ii8%2BPC9zdmc%2B)

[快速开始](#快速开始) · [项目结构](#项目结构) · [功能](#功能) · [实现原理](#实现原理) · [命令行用法](#命令行用法) · [常见问题](#常见问题)   
</div>

> [!WARNING]
> **此项目非官方开发，与AIC官方无任何联系**
> 
> 本项目仅用于个人学习与技术研究，不提供任何服务保证。使用者请自行确认当地法律法规，一切后果由使用者自行承担。
> 

## 快速开始

> 只想要结果：下载 → 解压 → 双击 → 选游戏目录 → 点「安装去码补丁」。

1. 到 [Releases](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases/latest) 下载 **`AIC_nomosaic_patch.exe.zip`**
   （直接下 `.exe` 可能被浏览器或杀软拦截，压缩包更省事）
2. 解压到任意位置（也可放进游戏目录，程序会自动识别）
3. 双击运行 `AIC_nomosaic_patch.exe`
   - 放在游戏目录下启动 → 自动识别路径
   - 放在其他位置 → 手动选择游戏目录
4. 点「安装去码补丁」，等 5–10 秒提示成功即可

安装前**请先关闭游戏**，否则会提示「游戏文件被占用」。

## 项目结构

```
AIC_patch.sln
├─ AIC_nomosaic_patch/        C++ 主程序（GUI 壳，v142 工具集）
├─ AIC_nomosaic_patch_cmd/    C# 命令行核心（dnlib 4.5.0，真正改写 IL 的部分）
├─ AIC_Launcher/              C# 自制启动器（用于替换带虚拟机检测的官方壳）
└─ packages/dnlib.4.5.0/      依赖包（随仓库提供，离线可编译）
```

编译：VS2019 打开 `AIC_patch.sln`，配置选 **Release**（程序按相对路径引用 `bin\Release\` 下的产物）。

## 功能
适配所有旧版本和未来新版本  
 - 下载工具，放到游戏主程序AliceInCradle.exe所在的目录运行（放在游戏目录可自动识别，也可以放在任意位置手动选择游戏目录）  
 - 选择安装补丁，等待5-10s即可安装成功 
 
<img width="486" height="413" alt="image" src="https://github.com/user-attachments/assets/2a32b8af-1b0f-4518-889b-d6ebb7af2015" />

 - 点击游戏调试设置可快速修改debug设置  
<img width="387" height="315" alt="image" src="https://github.com/user-attachments/assets/9d351c1d-0834-4cea-9c8e-a49e289b211b" />

 - 安装虚拟机补丁可绕过虚拟机检测，实现虚拟机游玩（官方不建议在虚拟机游玩，此功能仅供开放爱好者研究使用，严禁用于违规用途）
 <img width="387" height="318" alt="image" src="https://github.com/user-attachments/assets/3696145e-fd93-4688-9d9e-cae25cab0ee4" />

   
## 实现原理  
1.自动补丁工具原理 
 - 根据路径定位Assembly-CSharp.dll 
 - 使用dnlib修改FnDrawMosaic返回值实现补丁

2.版本检测  
 - 读取文件夹名称
 - 读取游戏文件globalgamemanagers

3.调试设置  
 - 解析_debug.txt
 - 修改键值

4.虚拟机补丁
 - 替换旧启动器（加了虚拟机检测壳）为自制启动器
 - 修改unsafeAssem.dll里的硬件校验  
 
## 手动安装方法（不推荐） 
下载对应版本补丁压缩包，解压后覆盖游戏里的 Assembly-CSharp.dll 文件（已停止支持，新版本请使用自动补丁工具！）

## 命令行用法

GUI 只是壳，实际改写由同目录下的 `AIC_nomosaic_patch_cmd.exe` 完成（也可单独使用）：

| 参数 | 必填 | 说明 |
| --- | --- | --- |
| `-dll <路径>` | 是 | 要处理的 DLL，去码用 `Assembly-CSharp.dll`，虚拟机补丁用 `unsafeAssem.dll` |
| `-m <true\|false>` | 是（去码） | `false` = 关闭马赛克绘制（安装补丁）；`true` = 恢复原样 |
| `-vm <install\|uninstall>` | 是（虚拟机） | 安装 / 还原虚拟机检测绕过 |
| `-out <路径>` | 否 | 输出文件；缺省直接覆盖 `-dll` 指定的文件 |

示例：

```bat
:: 安装去码
AIC_nomosaic_patch_cmd.exe -m false -dll "D:\Games\AIC\AliceInCradle_Data\Managed\Assembly-CSharp.dll"

:: 安装虚拟机绕过
AIC_nomosaic_patch_cmd.exe -vm install -dll "D:\Games\AIC\AliceInCradle_Data\Managed\unsafeAssem.dll"
```

已是目标状态时输出 `repeat`，不做任何写入。

## 常见问题

**Q：提示「游戏文件被占用」？**
游戏或残留进程还在跑。关闭游戏，并在任务管理器结束 `AliceInCradle.exe` 后重试。

**Q：提示找不到游戏目录 / notfound？**
确认选择的是**含 `AliceInCradle_Data` 文件夹的那一层**目录，而不是 `AliceInCradle_Data` 内部，也不是上一级父目录。

**Q：游戏更新后马赛克又回来了？**
更新会覆盖 `Assembly-CSharp.dll`，属正常现象，**重新运行工具安装一次**即可。

**Q：安装成功但游戏里没变化？**
1. 确认游戏是**完全退出**后重新启动的；
2. 确认安装路径与启动的客户端是同一份游戏（多份安装很常见）；
3. 少数版本把绘制入口改到了别的方法，请在 Issues 里附上游戏版本号。

**Q：提示修改失败没有具体报错？**
关闭工具后右键以管理员身份运行。

## 许可证与使用限制

本仓库**未采用开源许可证**，作者保留全部权利。允许：

- 个人下载、使用、自行编译；
- 转载**指向本仓库的链接**。

禁止：

- 二次打包、重新分发二进制（尤其是加壳/加广告后分发）；
- 任何商业用途；
- 用于违反游戏用户协议或当地法律法规的用途。

本软件按「现状」提供，不附带任何担保；使用造成的任何后果由使用者自行承担。
