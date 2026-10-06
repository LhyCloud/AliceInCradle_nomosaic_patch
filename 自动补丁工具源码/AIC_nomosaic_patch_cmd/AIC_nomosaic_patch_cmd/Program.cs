using System;
using System.Collections.Generic;
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

            // -----------------------
            // 参数解析
            // -----------------------
            for (int i = 0; i < args.Length; i++)
            {
                string arg = args[i].Trim().ToLower();

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
}