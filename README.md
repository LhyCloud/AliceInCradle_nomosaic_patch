# <img width="32" height="32" alt="image" src="https://github.com/user-attachments/assets/35f4f436-2f92-4e63-861a-59ca35a06d33" /> AliceInCradle_nomosaic_patch

Alice In Cradle(AIC)（摇篮里的爱丽丝）去除马赛克补丁 

## 自动全版本补丁安装工具
适配所有旧版本和未来新版本  
 - 下载工具，放到游戏主程序AliceInCradle.exe所在的目录运行（放在游戏目录可自动识别，也可以放在任意位置手动选择游戏目录）  
 - 选择安装补丁，等待5-10s即可安装成功 
 
<img width="486" height="413" alt="image" src="https://github.com/user-attachments/assets/2a32b8af-1b0f-4518-889b-d6ebb7af2015" />

 - 点击游戏调试设置可快速修改debug设置  
<img width="387" height="315" alt="image" src="https://github.com/user-attachments/assets/9d351c1d-0834-4cea-9c8e-a49e289b211b" />

   
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

## 手动安装方法（不推荐） 
下载对应版本补丁压缩包，解压后覆盖游戏里的 Assembly-CSharp.dll 文件（已停止支持，新版本请使用自动补丁工具！）
