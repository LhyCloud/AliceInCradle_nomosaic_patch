using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using dnlib.DotNet;
using dnlib.DotNet.Emit;


namespace AIC_nomosaic_patch_cmd
{
    class Program
    {
        static void Main(string[] args)
        {
            string dllPath = null;
            string outPath = null;
            bool? targetValue = null;
            string vmMode = null;   // 新增：-vm install / -vm uninstall

            // -----------------------
            // 参数解析
            // -----------------------
            for (int i = 0; i < args.Length; i++)
            {
                string arg = args[i].Trim().ToLower();

                // =========================================================
                // 新增指令（虚拟机检测绕过），原有参数分支保持原样不动
                //     -vm install    /  -vm=install     安装
                //     -vm uninstall  /  -vm=uninstall   还原
                // 目标文件同样用 -dll 指定（unsafeAssem.dll），-out 可选
                // =========================================================
                if (arg == "-vm" || arg == "-v")
                {
                    if (i + 1 < args.Length)
                    {
                        vmMode = args[i + 1].Trim().ToLower();
                        i++;
                    }
                    else
                    {
                        Console.WriteLine("请指定 -vm=install 或 -vm=uninstall");
                        return;
                    }
                    continue;
                }
                if (arg.StartsWith("-vm=") || arg.StartsWith("-v="))
                {
                    vmMode = arg.Split('=')[1].Trim().ToLower();
                    continue;
                }

                // -----------------------
                // 以下为原有的调用约定，一行未改
                // -----------------------

                // -m=false / -mode=false 或 -m false
                if (arg == "-m" || arg == "-mode")
                {
                    if (i + 1 < args.Length)
                    {
                        string val = args[i + 1].Trim().ToLower();
                        if (val == "true") targetValue = true;
                        else if (val == "false") targetValue = false;
                        else
                        {
                            Console.WriteLine("请指定 -m/--mode=true 或 false");
                            return;
                        }
                        i++;
                    }
                    else
                    {
                        Console.WriteLine("请指定 -m/--mode=true 或 false");
                        return;
                    }
                }
                else if (arg.StartsWith("-m=") || arg.StartsWith("-mode="))
                {
                    string val = arg.Split('=')[1].Trim().ToLower();
                    if (val == "true") targetValue = true;
                    else if (val == "false") targetValue = false;
                    else
                    {
                        Console.WriteLine("请指定 -m/--mode=true 或 false");
                        return;
                    }
                }
                else if (arg.StartsWith("-dll="))
                {
                    dllPath = arg.Substring(5).Trim();
                }
                else if (arg == "-dll")
                {
                    if (i + 1 < args.Length) dllPath = args[i + 1].Trim();
                    i++;
                }
                else if (arg.StartsWith("-out="))
                {
                    outPath = arg.Substring(5).Trim();
                }
                else if (arg == "-out")
                {
                    if (i + 1 < args.Length && i + 1 < args.Length) outPath = args[i + 1].Trim();
                    i++;
                }
            }

            // -----------------------
            // 新指令入口：虚拟机检测绕过
            // 完全独立于下面的去马赛克流程，不影响原有任何行为
            // -----------------------
            if (!string.IsNullOrEmpty(vmMode))
            {
                if (string.IsNullOrEmpty(dllPath))
                {
                    Console.WriteLine("请指定 -dll=<dll路径>");
                    return;
                }

                if (string.IsNullOrEmpty(outPath))
                    outPath = dllPath; // 默认覆盖原 DLL

                VmGuardPatch.Run(dllPath, outPath, vmMode);
                return;
            }

            if (dllPath == null)
            {
                Console.WriteLine("请指定 -dll=<dll路径>");
                return;
            }

            if (targetValue == null)
            {
                Console.WriteLine("请指定 -m/--mode=true 或 false");
                return;
            }

            if (string.IsNullOrEmpty(outPath))
                outPath = dllPath; // 默认覆盖原 DLL

            try
            {
                // -----------------------
                // 加载 DLL（无依赖模式）
                // -----------------------
                var module = ModuleDefMD.Load(dllPath);

                // -----------------------
                // 找到 MosaicShower.FnDrawMosaic
                // -----------------------
                var type = module.Types.FirstOrDefault(t => t.Name == "MosaicShower");
                if (type == null)
                {
                    Console.WriteLine("notfound");
                    return;
                }

                var method = type.Methods.FirstOrDefault(m => m.Name == "FnDrawMosaic");
                if (method == null || !method.HasBody)
                {
                    Console.WriteLine("notfound");
                    return;
                }

                // -----------------------
                // 修改方法 IL（只改最终返回值）
                // -----------------------
                var instrs = method.Body.Instructions;

                int retIndex = -1;

                // 找最后一个 ret
                for (int i = instrs.Count - 1; i >= 0; i--)
                {
                    if (instrs[i].OpCode == OpCodes.Ret)
                    {
                        retIndex = i;
                        break;
                    }
                }

                if (retIndex <= 0)
                {
                    Console.WriteLine("notfound");
                    return;
                }

                // 找 ret 前一条指令
                var prev = instrs[retIndex - 1];

                // 必须是 ldc.i4.0 或 ldc.i4.1
                if (prev.OpCode != OpCodes.Ldc_I4_0 && prev.OpCode != OpCodes.Ldc_I4_1)
                {
                    Console.WriteLine("notfound");
                    return;
                }

                // 判断是否已经是目标值
                bool currentValue = prev.OpCode == OpCodes.Ldc_I4_1;

                if (currentValue == targetValue)
                {
                    Console.WriteLine("repeat");
                    return;
                }

                // 修改最终返回值
                prev.OpCode = targetValue.Value ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0;

                // -----------------------
                // 保存 DLL
                // -----------------------
                module.Write(outPath);

                // -----------------------
                // 校验：重新读取生成的文件，确认该位置已是新值
                // -----------------------
                bool verified = false;
                try
                {
                    var checkModule = ModuleDefMD.Load(outPath);
                    var checkType = checkModule.Types.FirstOrDefault(t => t.Name == "MosaicShower");
                    var checkMethod = checkType == null ? null : checkType.Methods.FirstOrDefault(m => m.Name == "FnDrawMosaic");
                    if (checkMethod != null && checkMethod.HasBody)
                    {
                        var checkInstrs = checkMethod.Body.Instructions;

                        // 同样取最后一个 ret，检查其前一条指令是否为期望的新值
                        for (int i = checkInstrs.Count - 1; i >= 1; i--)
                        {
                            if (checkInstrs[i].OpCode == OpCodes.Ret)
                            {
                                var checkPrev = checkInstrs[i - 1];
                                verified = (targetValue.Value && checkPrev.OpCode == OpCodes.Ldc_I4_1)
                                        || (!targetValue.Value && checkPrev.OpCode == OpCodes.Ldc_I4_0);
                                break;
                            }
                        }
                    }
                }
                catch
                {
                    verified = false;
                }
                if (!verified)
                {
                    Console.WriteLine("修改失败: 校验不通过");
                    return;
                }

                Console.WriteLine("TRUE");
            }
            catch (Exception ex)
            {
                Console.WriteLine("修改失败: " + ex.Message);
            }
        }
    }

    // =========================================================================
    //  虚拟机检测绕过补丁（对应 AIC-0.30i-VM-bypass.md 的补丁 A / 补丁 B）
    //
    //  目标：AliceInCradle_Data\Managed\unsafeAssem.dll
    //  做法与上面的去马赛克完全一致：dnlib 定位方法 -> 改 IL -> module.Write -> 重新加载校验
    //  区别只是这里不按固定偏移，而是按代码结构找，所以 0.29 / 0.30i 都能用。
    //
    //  XX.IN::Awake() 是唯一做环境自检的方法，两个版本里形状完全一样：
    //
    //      ldc.i4.1 ; stloc.<ok>              // ok = true
    //      ...版本串比较...                    // 不匹配时：
    //          ldc.i4.0 ; stloc.<ok> ; br     //     ok = false，跳去判定
    //      ...显卡名 Contains(...) 特征链...   // 命中时：
    //          ldc.i4.0 ; stloc.<ok>          //     ok = false
    //      ldloc.<ok> ; brtrue/brfalse        // ok 为 false 就 LoadScene(
    //      ldstr "SceneCannotLaunch..."       //     "SceneCannotLaunch_InvalidEnviroment")
    //
    //  把"给 ok 赋 false"的那几条 ldc.i4.0 改成 ldc.i4.1，ok 就再也回不到 false，
    //  版本串比较和虚拟机特征检测一起失效 —— 相当于原来补丁 A + 补丁 B 的效果。
    //
    //  输出沿用原来的约定：TRUE / repeat / notfound（失败另加一行说明）。
    // =========================================================================
    internal static class VmGuardPatch
    {
        public static void Run(string dllPath, string outPath, string mode)
        {
            bool uninstall;

            if (mode == "install" || mode == "apply" || mode == "on")
                uninstall = false;
            else if (mode == "uninstall" || mode == "remove" || mode == "restore" || mode == "off")
                uninstall = true;
            else
            {
                Console.WriteLine("未知指令: -vm=" + mode + "（可用: install / uninstall）");
                return;
            }

            // -----------------------
            // 输出去处：和去马赛克一样，先写临时文件，校验通过后再替换原文件
            // （写的时候 module 还映射着原 dll，直接写回同一路径会被系统拒绝）
            // -----------------------
            string writePath = outPath;
            bool inPlace = false;
            try
            {
                inPlace = string.Equals(Path.GetFullPath(outPath), Path.GetFullPath(dllPath),
                                        StringComparison.OrdinalIgnoreCase);
            }
            catch
            {
                inPlace = false;
            }
            if (inPlace) writePath = outPath + ".tmp";

            try
            {
                int siteCount = 0;

                // -----------------------
                // 加载 DLL（无依赖模式）
                // -----------------------
                var module = ModuleDefMD.Load(dllPath);
                try
                {
                    // -----------------------
                    // 找到做环境自检的那个方法
                    // -----------------------
                    var method = FindAwake(module);
                    if (method == null || !method.HasBody)
                    {
                        Console.WriteLine("notfound");
                        return;
                    }

                    // -----------------------
                    // 找出所有"把 ok 置为 false"的指令
                    // -----------------------
                    var sites = FindSites(method);
                    if (sites == null || sites.Count == 0)
                    {
                        Console.WriteLine("notfound");
                        return;
                    }

                    bool anyPristine = sites.Any(s => s.OpCode == OpCodes.Ldc_I4_0);
                    bool anyPatched = sites.Any(s => s.OpCode == OpCodes.Ldc_I4_1);

                    // -----------------------
                    // 判断是否已经处于目标状态
                    // -----------------------
                    if (!uninstall)
                    {
                        if (!anyPristine)
                        {
                            Console.WriteLine("repeat");
                            return;
                        }
                        foreach (var s in sites) s.OpCode = OpCodes.Ldc_I4_1;
                    }
                    else
                    {
                        if (!anyPatched)
                        {
                            Console.WriteLine("repeat");
                            return;
                        }
                        foreach (var s in sites) s.OpCode = OpCodes.Ldc_I4_0;
                    }

                    siteCount = sites.Count;

                    // -----------------------
                    // 保存到临时文件
                    // -----------------------
                    if (File.Exists(writePath)) File.Delete(writePath);
                    module.Write(writePath);
                }
                finally
                {
                    // 解除对原 dll 的映射，否则后面替换不掉原文件
                    module.Dispose();
                }

                // -----------------------
                // 校验：重新读取生成的文件，确认每个检测点都已是目标状态
                // -----------------------
                bool verified = false;
                try
                {
                    var checkModule = ModuleDefMD.Load(writePath);
                    try
                    {
                        var checkMethod = FindAwake(checkModule);
                        var checkSites = checkMethod == null ? null : FindSites(checkMethod);

                        if (checkSites != null && checkSites.Count == siteCount)
                        {
                            verified = checkSites.All(s => uninstall
                                ? s.OpCode == OpCodes.Ldc_I4_0
                                : s.OpCode == OpCodes.Ldc_I4_1);
                        }
                    }
                    finally
                    {
                        checkModule.Dispose();
                    }
                }
                catch
                {
                    verified = false;
                }

                if (!verified)
                {
                    if (inPlace) TryDelete(writePath);
                    Console.WriteLine("修改失败: 校验不通过");
                    return;
                }

                // -----------------------
                // 校验通过，替换回原位置
                // -----------------------
                if (inPlace)
                {
                    if (File.Exists(outPath))
                    {
                        try
                        {
                            File.Replace(writePath, outPath, null);
                        }
                        catch
                        {
                            // 个别环境（受限沙箱、不支持 ReplaceFile 的存储）不允许原子替换，
                            // 退回删除 + 改名，结果一样
                            File.Delete(outPath);
                            File.Move(writePath, outPath);
                        }
                    }
                    else
                    {
                        File.Move(writePath, outPath);
                    }
                }

                Console.WriteLine("TRUE");
            }
            catch (Exception ex)
            {
                if (inPlace) TryDelete(writePath);
                Console.WriteLine("修改失败: " + ex.Message);
            }
        }

        private static void TryDelete(string path)
        {
            try
            {
                if (File.Exists(path)) File.Delete(path);
            }
            catch
            {
            }
        }

        // -----------------------
        // 找环境自检方法：
        //   1) XX.IN::Awake（0.29 / 0.30i 都是这个名字）
        //   2) 退化：任何方法体里出现 ldstr "SceneCannotLaunch"
        // -----------------------
        private static MethodDef FindAwake(ModuleDef module)
        {
            MethodDef byName = null;

            foreach (var t in module.GetTypes())
            {
                foreach (var m in t.Methods)
                {
                    if (m.Name != "Awake" || !m.HasBody) continue;
                    if (t.Name == "IN") return m;
                    if (byName == null) byName = m;
                }
            }
            if (byName != null) return byName;

            foreach (var t in module.GetTypes())
            {
                foreach (var m in t.Methods)
                {
                    if (!m.HasBody) continue;
                    foreach (var i in m.Body.Instructions)
                    {
                        if (i.OpCode != OpCodes.Ldstr) continue;
                        string s = i.Operand as string;
                        if (s != null && s.StartsWith("SceneCannotLaunch", StringComparison.Ordinal)) return m;
                    }
                }
            }
            return null;
        }

        // -----------------------
        // 收集补丁点：ldc.i4.0/1 + stloc.<ok>，且前一条必须是跳转指令
        // （开头那句 "ok = true" 前面是 call/ret，所以不会被误选）
        // -----------------------
        private static List<Instruction> FindSites(MethodDef method)
        {
            var ins = method.Body.Instructions;
            var hits = new List<Instruction>();
            if (ins == null || ins.Count < 3) return hits;

            int okLocal = FindOkLocal(ins);

            for (int i = 1; i < ins.Count - 1; i++)
            {
                if (ins[i].OpCode != OpCodes.Ldc_I4_0 && ins[i].OpCode != OpCodes.Ldc_I4_1) continue;
                if (!IsStoreToLocal(ins[i + 1], okLocal)) continue;

                var fc = ins[i - 1].OpCode.FlowControl;
                if (fc != FlowControl.Cond_Branch && fc != FlowControl.Branch) continue;

                hits.Add(ins[i]);
            }
            return hits;
        }

        // -----------------------
        // ok 是哪个局部变量：判定点前面一定是 "ldloc.<ok> ; brtrue/brfalse"
        // -----------------------
        private static int FindOkLocal(IList<Instruction> ins)
        {
            for (int i = 0; i < ins.Count; i++)
            {
                if (ins[i].OpCode != OpCodes.Ldstr) continue;
                string s = ins[i].Operand as string;
                if (s == null || !s.StartsWith("SceneCannotLaunch", StringComparison.Ordinal)) continue;

                for (int k = i - 1; k >= 0 && k >= i - 6; k--)
                {
                    var fc = ins[k].OpCode.FlowControl;
                    if (fc != FlowControl.Cond_Branch && fc != FlowControl.Branch) continue;

                    int idx;
                    if (TryGetLoadLocalIndex(ins[k - 1], out idx)) return idx;
                }
            }
            return 0;   // 兜底：0.29 / 0.30i 里 ok 都是 local 0
        }

        private static bool IsStoreToLocal(Instruction instr, int index)
        {
            int idx;
            return TryGetStoreLocalIndex(instr, out idx) && idx == index;
        }

        private static bool TryGetStoreLocalIndex(Instruction instr, out int index)
        {
            index = -1;
            var c = instr.OpCode.Code;
            if (c == Code.Stloc_0) { index = 0; return true; }
            if (c == Code.Stloc_1) { index = 1; return true; }
            if (c == Code.Stloc_2) { index = 2; return true; }
            if (c == Code.Stloc_3) { index = 3; return true; }
            if (c == Code.Stloc_S || c == Code.Stloc)
            {
                var l = instr.Operand as Local;
                if (l != null) { index = l.Index; return true; }
            }
            return false;
        }

        private static bool TryGetLoadLocalIndex(Instruction instr, out int index)
        {
            index = -1;
            var c = instr.OpCode.Code;
            if (c == Code.Ldloc_0) { index = 0; return true; }
            if (c == Code.Ldloc_1) { index = 1; return true; }
            if (c == Code.Ldloc_2) { index = 2; return true; }
            if (c == Code.Ldloc_3) { index = 3; return true; }
            if (c == Code.Ldloc_S || c == Code.Ldloc)
            {
                var l = instr.Operand as Local;
                if (l != null) { index = l.Index; return true; }
            }
            return false;
        }
    }
}
