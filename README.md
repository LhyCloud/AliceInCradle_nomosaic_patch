<div align="center">
<img width="100" alt="image" src="https://github.com/user-attachments/assets/35f4f436-2f92-4e63-861a-59ca35a06d33" /> 
 
# AliceInCradle_nomosaic_patch
Alice In Cradle(AIC)（摇篮里的爱丽丝）去除马赛克补丁 
 
[![Release](https://img.shields.io/github/v/release/LhyCloud/AliceInCradle_nomosaic_patch?style=flat-square&label=release)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases/latest) [![Downloads](https://img.shields.io/github/downloads/LhyCloud/AliceInCradle_nomosaic_patch/total?style=flat-square&logo=github)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases) [![Stars](https://img.shields.io/github/stars/LhyCloud/AliceInCradle_nomosaic_patch?style=flat-square&logo=github&logoColor=white&label=stars&color=f5c518)](https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/stargazers) [![Platform](https://img.shields.io/badge/platform-Windows_7%2B-0078D6?style=flat-square)](#)

![C++](https://img.shields.io/badge/C%2B%2B-v142_/_MSVC-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![C#](https://img.shields.io/badge/C%23-.NET_Framework_4.7.2-512BD4?style=flat-square)
![IL rewrite](https://img.shields.io/badge/IL_rewrite-dnlib_4.5.0-5C2D91?style=flat-square)
![Engine](https://img.shields.io/badge/Game_Engine-Unity-000000?style=flat-square&logo=unity&logoColor=white)
![IDE](https://img.shields.io/badge/IDE-Microsoft_Visual_Studio_2019-5C2D91?style=flat-square&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMjggMTI4Ij48ZGVmcz48bGluZWFyR3JhZGllbnQgaWQ9ImEiIHgxPSI0OCIgeDI9IjQ4IiB5MT0iOTcuNzUiIHkyPSIyLjI1IiBncmFkaWVudFRyYW5zZm9ybT0ibWF0cml4KDEgMCAwIC0xIDAgOTcuNzUpIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSI%2BPHN0b3Agb2Zmc2V0PSIwIiBzdG9wLWNvbG9yPSIjZmZmIi8%2BPHN0b3Agb2Zmc2V0PSIxIiBzdG9wLWNvbG9yPSIjZmZmIiBzdG9wLW9wYWNpdHk9IjAiLz48L2xpbmVhckdyYWRpZW50PjwvZGVmcz48cGF0aCBmaWxsPSIjNTIyMThhIiBkPSJNMTQuMzkgMjYuMjk1YTUuMzMzIDUuMzMzIDAgMCAwLTEuNDE3LjM3M2wtOS42OTQgNEE1LjMzMyA1LjMzMyAwIDAgMCAwIDM1LjU2MXY1Ni44OGE1LjMzMyA1LjMzMyAwIDAgMCAzLjI4IDQuODkzbDkuNjkzIDQuMDY2YTUuMzMzIDUuMzMzIDAgMCAwIDUuNTIxLS44NjVsMi4xNzItMS44NjdhMi45NDcgMi45NDcgMCAwIDEtNC42NjYtMi40VjMxLjczNGEyLjk0NyAyLjk0NyAwIDAgMSA0LjY2Ni0yLjRsLTIuMTcyLTEuNzk5YTUuMzMzIDUuMzMzIDAgMCAwLTQuMTAzLTEuMjR6Ii8%2BPHBhdGggZmlsbD0iIzZjMzNhZiIgZD0iTTk0Ljc1LjQxNkE4IDggMCAwIDAgODggMi42NjhsLTgyLjY2NiA5MS40QTMuMDggMy4wOCAwIDAgMSAwIDkyLjAwMnYuNDRhNS4zMzMgNS4zMzMgMCAwIDAgMy4yOCA0Ljg5Mmw5LjY5MyA0LjA2NmE1LjMzMyA1LjMzMyAwIDAgMCA1LjUyMS0uODY1bDIuMTcyLTEuODY3IDk5LjA4LTgxLjI0QTUuMDUzIDUuMDUzIDAgMCAxIDEyOCAyMS4zMzR2LS4zMDdhOCA4IDAgMCAwLTQuNTMzLTcuMjEzTDk3LjA5NCAxLjEyMUE4IDggMCAwIDAgOTQuNzUuNDE2WiIvPjxwYXRoIGZpbGw9IiM4NTRjYzciIGQ9Ik0xNC44NzEgMjYuMjM4YTUuMzMzIDUuMzMzIDAgMCAwLTEuODk4LjQzbC05LjY5NCA0QTUuMzMzIDUuMzMzIDAgMCAwIDAgMzUuNTYxdi40NDFhMy4wOCAzLjA4IDAgMCAxIDUuMzM0LTIuMDY2TDg4IDEyNS4zMzRhOCA4IDAgMCAwIDkuMDk0IDEuNTQ3bDI2LjM3My0xMi42OTRhOCA4IDAgMCAwIDQuNTMzLTcuMjEydi0uMzA3YTUuMDUzIDUuMDUzIDAgMCAxLTguMjU0IDMuOTA2bC05OS4wOC04MS4yNC0yLjE3Mi0xLjg2NWE1LjMzMyA1LjMzMyAwIDAgMC0zLjYyMy0xLjIzeiIvPjxwYXRoIGZpbGw9IiNiMTc5ZjEiIGQ9Ik05NC43NS40MTZhOCA4IDAgMCAwLTUuNjc0IDEuNDY5QTQuNjkzIDQuNjkzIDAgMCAxIDk2IDYuMDE1djExNmE0LjY5MyA0LjY5MyAwIDAgMS04IDMuMzE5IDggOCAwIDAgMCA5LjA5NCAxLjU0N2wyNi4zNzMtMTIuNjhhOCA4IDAgMCAwIDQuNTMzLTcuMjEzVjIxLjAxNmE4IDggMCAwIDAtNC41MzMtNy4yMTVMOTcuMDk0IDEuMTJBOCA4IDAgMCAwIDk0Ljc1LjQxNlptLTUuNjc0IDEuNDY5QTQuNjkzIDQuNjkzIDAgMCAwIDg4IDIuNjY4YTggOCAwIDAgMSAxLjA3Ni0uNzgzWiIvPjxwYXRoIGZpbGw9InVybCgjYSkiIGZpbGwtcnVsZT0iZXZlbm9kZCIgZD0iTTk0LjE0NS4zNDhhOCA4IDAgMCAwLTMuMDI2LjM4NkE4IDggMCAwIDAgODggMi42NjhMNDUuNDk0IDQ5LjY4MiAyMC42NjYgMjkuMzM0bC0yLjE3Mi0xLjg2NWE1LjMzMyA1LjMzMyAwIDAgMC00LjgxNC0xLjEwOCAzLjQgMy40IDAgMCAwLS43MDcuMjRsLTkuNjk0IDQuMDY3QTUuMzMzIDUuMzMzIDAgMCAwIDAgMzUuMTYydjU3LjY3OWE1LjMzMyA1LjMzMyAwIDAgMCAzLjI4IDQuNDkzbDkuNjkzIDRhMy40IDMuNCAwIDAgMCAuNzA3LjI0IDUuMzMzIDUuMzMzIDAgMCAwIDQuODE0LTEuMTA1bDIuMTcyLTEuODAxIDI0LjgyOC0yMC4zNDZMODggMTI1LjMzNGE4IDggMCAwIDAgMy44NTQgMi4xMzUgOCA4IDAgMCAwIDUuMjQtLjU4OGwyNi4zNzMtMTIuNjhhOCA4IDAgMCAwIDQuNTMzLTcuMjEzVjIxLjAxNmE4IDggMCAwIDAtNC41MzMtNy4yMTVMOTcuMDk0IDEuMTJhOCA4IDAgMCAwLTIuOTUtLjc3M1pNOTYgMzYuOTA4djU0LjE4Nkw2Mi45NDcgNjQuMDAyWm0tODAgOC43ODcgMTYuNTQ3IDE4LjMwN0wxNiA4Mi4zMDlaIiBvcGFjaXR5PSIuMjUiLz48L3N2Zz4K)

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
