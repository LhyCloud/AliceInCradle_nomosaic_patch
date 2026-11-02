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
                //     -vm install    /  -vm=install     安装补丁 A + B
                //     -vm uninstall  /  -vm=uninstall   还原补丁 A + B
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
            // 完全独立于下面的去马赛克流程，不会影响原有任何行为
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
    //  虚拟机检测绕过补丁（对应 AIC-0.30i-VM-bypass.md 里的补丁 A / 补丁 B）
    //
    //  目标：AliceInCradle_Data\Managed\unsafeAssem.dll（2,694,144 字节）
    //  位置：XX.IN::Awake 的 IL 体（0xF9A98 起 936 字节）
    //
    //    补丁 A  文件 0x0F9D51  IL_02B9  ldc.i4.0 -> ldc.i4.1   1 字节
    //            虚拟机/模拟器判据不再把 ok 标志清零
    //
    //    补丁 B  文件 0x0F9CB1  IL_0219  20 字节
    //            ldloc.2 / ldstr <ver> / call op_Inequality / brfalse ... 整段
    //            改成 br.s IL_022D + 18 个 nop（Mono 线性解码，必须填满，
    //            否则残留的 ldstr token 会变成非法指令）
    //            这一处是版本串（由 exe 的 SHA1 推出来）比较，换掉 exe 后必然
    //            走到同一个 SceneCannotLaunch_InvalidEnviroment 分支，所以短路掉
    //
    //  这里是纯字节补丁，不经过 dnlib，也不碰上面原有的去马赛克流程。
    //  输出沿用原来的约定：TRUE / repeat / notfound（失败另加一行说明）。
    // =========================================================================
    internal static class VmGuardPatch
    {
        private const long OffsetA = 0x0F9D51;   // IL_02B9
        private const long OffsetB = 0x0F9CB1;   // IL_0219

        // 补丁 A：ldc.i4.0 -> ldc.i4.1
        private static readonly byte[] A_Pristine = { 0x16 };
        private static readonly byte[] A_Patched = { 0x17 };

        // 补丁 B：整段版本比较 -> br.s IL_022D + nop 填充
        private static readonly byte[] B_Pristine =
        {
            0x08, 0x72, 0xD8, 0xE2, 0x00, 0x70, 0x28, 0xC1, 0x00, 0x00,
            0x0A, 0x2C, 0x07, 0x16, 0x0A, 0x38, 0x8E, 0x00, 0x00, 0x00
        };
        private static readonly byte[] B_Patched =
        {
            0x2B, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

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

            try
            {
                if (uninstall) Restore(dllPath, outPath);
                else Install(dllPath, outPath);
            }
            catch (Exception ex)
            {
                Console.WriteLine("修改失败: " + ex.Message);
            }
        }

        // -----------------------
        // 安装：A + B
        // -----------------------
        private static void Install(string dllPath, string outPath)
        {
            byte[] data = File.ReadAllBytes(dllPath);

            bool aPristine, aPatched, bPristine, bPatched;
            if (!Inspect(data, out aPristine, out aPatched, out bPristine, out bPatched))
            {
                Console.WriteLine("版本不符: 0x" + OffsetA.ToString("X") + " / 0x" + OffsetB.ToString("X") +
                                  " 处的字节既不是原版也不是已打补丁的状态，未做任何修改。");
                Console.WriteLine("notfound");
                return;
            }

            if (aPatched && bPatched)
            {
                Console.WriteLine("repeat");
                return;
            }

            // 只在文件还是原版时留一份基准备份（不覆盖已有的 .bak）
            string bak = dllPath + ".bak";
            if (aPristine && bPristine && !File.Exists(bak))
            {
                try { File.Copy(dllPath, bak, false); } catch { /* 备份失败不影响打补丁 */ }
            }

            WriteRegion(data, OffsetA, A_Patched);
            WriteRegion(data, OffsetB, B_Patched);
            File.WriteAllBytes(outPath, data);

            byte[] check = File.ReadAllBytes(outPath);
            bool aOk, aPatched2, bOk, bPatched2;
            if (!Inspect(check, out aOk, out aPatched2, out bOk, out bPatched2) || !aPatched2 || !bPatched2)
            {
                Console.WriteLine("修改失败: 校验不通过");
                return;
            }

            Console.WriteLine("TRUE");
        }

        // -----------------------
        // 卸载：把两处字节还原成原版
        // -----------------------
        private static void Restore(string dllPath, string outPath)
        {
            byte[] data = File.ReadAllBytes(dllPath);

            bool aPristine, aPatched, bPristine, bPatched;
            if (!Inspect(data, out aPristine, out aPatched, out bPristine, out bPatched))
            {
                Console.WriteLine("版本不符: 0x" + OffsetA.ToString("X") + " / 0x" + OffsetB.ToString("X") +
                                  " 处的字节不是已知状态，未做任何修改。");
                Console.WriteLine("notfound");
                return;
            }

            if (aPristine && bPristine)
            {
                Console.WriteLine("repeat");
                return;
            }

            WriteRegion(data, OffsetA, A_Pristine);
            WriteRegion(data, OffsetB, B_Pristine);
            File.WriteAllBytes(outPath, data);

            byte[] check = File.ReadAllBytes(outPath);
            bool aOk, aPatched2, bOk, bPatched2;
            if (!Inspect(check, out aOk, out aPatched2, out bOk, out bPatched2) || !aOk || !bOk)
            {
                Console.WriteLine("修改失败: 校验不通过");
                return;
            }

            Console.WriteLine("TRUE");
        }

        // -----------------------
        // 判定两处补丁的当前状态；返回 false 表示该位置既非原版也非已打补丁
        // -----------------------
        private static bool Inspect(byte[] data,
                                    out bool aPristine, out bool aPatched,
                                    out bool bPristine, out bool bPatched)
        {
            aPristine = RegionMatches(data, OffsetA, A_Pristine);
            aPatched = RegionMatches(data, OffsetA, A_Patched);
            bPristine = RegionMatches(data, OffsetB, B_Pristine);
            bPatched = RegionMatches(data, OffsetB, B_Patched);

            return (aPristine || aPatched) && (bPristine || bPatched);
        }

        private static bool RegionMatches(byte[] data, long offset, byte[] expected)
        {
            if (data == null || offset < 0 || expected == null || expected.Length == 0) return false;
            if (offset + expected.Length > data.LongLength) return false;

            for (int i = 0; i < expected.Length; i++)
            {
                if (data[offset + i] != expected[i]) return false;
            }
            return true;
        }

        private static void WriteRegion(byte[] data, long offset, byte[] value)
        {
            for (int i = 0; i < value.Length; i++)
            {
                data[offset + i] = value[i];
            }
        }
    }
}
