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

[功能](#功能) · [实现原理](#实现原理) 
</div>

> [!WARNING]
> **此项目非官方开发，与AIC官方无任何联系**
> 
> 本项目仅用于个人学习与技术研究，不提供任何服务保证。使用者请自行确认当地法律法规，一切后果由使用者自行承担。
> 

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
