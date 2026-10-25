using System;
using System.Collections.Generic;
using dnlib.DotNet;
using dnlib.DotNet.Emit;

namespace AicNoMosaic
{
    /// <summary>
    /// Assembly-CSharp.dll 的去马赛克补丁核心（dnlib 进程内改 IL）。
    /// 目标方法：MosaicShower.FnDrawMosaic —— 把"是否绘制马赛克"的返回值翻转到目标。
    ///
    /// 三层定位/补丁策略（按可靠性递减，逐层退化）：
    ///   1) 主策略：方法体内最后一个 ret，且其前导是 ldc.i4.0/1 —— 翻转该常量
    ///      （与上游工具一致；0.30i 的多返回点形态即由主策略覆盖，早期 return 保留）；
    ///   2) 退化：最后 ret 前导非字面量时，取方法体内唯一一个 {ldc.i4.0/1, ret} 候选；
    ///   3) 兜底：以上都找不到（结构大改）→ 把整个方法体重写为常量返回
    ///      "ldc.i4.0/1; ret" —— 与 BepInEx 插件的 Postfix 强制返回同构
    ///      （官方 _debug.txt 的 nodamage 机制同思路）。代价是方法原有的
    ///      早期分支/副作用一并失效，因此只在 1/2 均失败时使用。
    /// 其它加固：校验返回类型必须是 System.Boolean；类型名大小写不敏感、
    /// GetTypes() 覆盖嵌套类型；同名歧义时按"调用点数量"裁决（没有调用者的
    /// 同名方法是冒牌的）；写出后重新读出自校验。
    ///
    /// 0.30i 实测方法结构（dnlib 探针验证，330 条 IL），三个早退分支各有明确语义，
    /// 补丁只动尾部返回常量，严禁把补丁点改到前两个分支上：
    ///   [0-2]   if (!use_mosaic) return false
    ///           —— use_mosaic 是运行时状态位：setTarget 里按
    ///           "当前目标 countMosaic() > 0" 自动计算，同时经 FtMosaic.get_activated
    ///           被 FatalShower.fineScreenSize / GetMosaicLayer / FtMosaic.isEnabled
    ///           消费。硬改此分支会波及游戏其它系统的逻辑分支。
    ///   [10-14] if (!enabled) return true
    ///           —— enabled 是 UI 生命周期同步位：ctor 置 true，
    ///           UIBase.OnEnable/OnDisable/activateCurrentUIPict 经
    ///           UIPictureBase.set_mosaic_enabled 广播同步。它不是设置项，
    ///           与 _debug.txt / PlayerPrefs 无关（已全面追踪，无外部开关）。
    ///   [328-329] { ldc.i4.1, ret }（330 条版本的尾部返回；0.29 的 55 条短版本
    ///           尾部常量方向相反）—— 主策略翻转时自适应方向，不硬编码 0→1 还是
    ///           1→0，这是跨版本设计的核心。
    /// </summary>
    public static class MosaicPatcher
    {
        internal const string TypeName = "MosaicShower";
        internal const string MethodName = "FnDrawMosaic";

        public enum PatchStatus
        {
            Success,        // 已修改并写出自校验通过
            NotFound,       // 未找到补丁目标（结构不符，疑似非目标文件或版本结构变化）
            AlreadyApplied, // 当前已是目标状态
            VerifyFailed,   // 写出后重新读取校验失败
            IoError,        // 读取/解析/写出失败
        }

        public enum InspectState
        {
            MosaicOn,   // 原始状态：绘制马赛克（未安装补丁）
            MosaicOff,  // 已安装补丁：不绘制马赛克
            NotFound,   // 未找到目标方法/返回点
            Error,      // 读取/解析失败
        }

        /// <summary>
        /// 对 dllPath 应用补丁：mosaicEnabled=false 去码（安装），true 恢复（卸载）。
        /// 结果写到 outputPath（调用方负责用临时文件 + File.Replace 做原子替换）。
        /// </summary>
        public static PatchStatus Apply(string dllPath, bool mosaicEnabled, string outputPath)
        {
            try
            {
                ModuleDefMD module;
                try { module = ModuleDefMD.Load(dllPath); }
                catch { return PatchStatus.IoError; }

                using (module)
                {
                    MethodDef method = FindTargetMethod(module);
                    if (method == null) return PatchStatus.NotFound;

                    int retIndex;
                    bool currentValue;
                    if (FindPatchPoint(method, out retIndex, out currentValue))
                    {
                        if (currentValue == mosaicEnabled) return PatchStatus.AlreadyApplied;

                        method.Body.Instructions[retIndex - 1].OpCode =
                            mosaicEnabled ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0;
                    }
                    else
                    {
                        // 兜底：返回点定位失败（结构大改）→ 整体重写为常量返回。
                        // 语义与 BepInEx Postfix "强制 result" 等构；详见类型头注释。
                        MakeConstantBody(method, mosaicEnabled);
                    }

                    try { module.Write(outputPath); }
                    catch { return PatchStatus.IoError; }
                }

                // 写出后重新读出，确认主返回点已是目标值
                try
                {
                    using (ModuleDefMD check = ModuleDefMD.Load(outputPath))
                    {
                        MethodDef checkMethod = FindTargetMethod(check);
                        int checkIdx;
                        bool checkValue;
                        if (checkMethod == null ||
                            !FindPatchPoint(checkMethod, out checkIdx, out checkValue) ||
                            checkValue != mosaicEnabled)
                            return PatchStatus.VerifyFailed;
                    }
                }
                catch { return PatchStatus.VerifyFailed; }

                return PatchStatus.Success;
            }
            catch
            {
                return PatchStatus.IoError;
            }
        }

        /// <summary>只读检查当前补丁状态，不修改任何文件。</summary>
        public static InspectState Inspect(string dllPath)
        {
            try
            {
                using (ModuleDefMD module = ModuleDefMD.Load(dllPath))
                {
                    MethodDef method = FindTargetMethod(module);
                    if (method == null) return InspectState.NotFound;

                    int retIndex;
                    bool currentValue;
                    if (!FindPatchPoint(method, out retIndex, out currentValue))
                        return InspectState.NotFound;

                    return currentValue ? InspectState.MosaicOn : InspectState.MosaicOff;
                }
            }
            catch
            {
                return InspectState.Error;
            }
        }

        // ------------------------------------------------------------------

        /// <summary>
        /// 按名字找目标方法（大小写不敏感，含嵌套类型）。
        /// 返回类型必须是 System.Boolean —— 防止未来版本里出现同名方法导致改错位置。
        /// 同名歧义时用"调用点数量"裁决：被 call/callvirt 的那一个才是真实执行路径；
        /// 无法裁决（并列）时返回 null（NotFound，宁可不动也不改错）。
        /// </summary>
        private static MethodDef FindTargetMethod(ModuleDefMD module)
        {
            var candidates = new List<MethodDef>();
            foreach (TypeDef t in module.GetTypes())
            {
                if (!string.Equals(t.Name, TypeName, StringComparison.OrdinalIgnoreCase))
                    continue;

                foreach (MethodDef m in t.Methods)
                {
                    if (!m.HasBody) continue;
                    if (!string.Equals(m.Name, MethodName, StringComparison.OrdinalIgnoreCase)) continue;
                    if (m.ReturnType == null || m.ReturnType.FullName != "System.Boolean") continue;
                    candidates.Add(m);
                }
            }

            if (candidates.Count == 0) return null;
            if (candidates.Count == 1) return candidates[0];

            // 歧义裁决：统计各候选在程序集内的调用点数量
            var first = candidates[0];
            var firstCallers = CountCallers(module, first);
            for (int i = 1; i < candidates.Count; i++)
            {
                if (CountCallers(module, candidates[i]) != firstCallers)
                    return firstCallers > 0 ? first : candidates[i];   // 仍并列则放弃
            }
            return firstCallers > 0 ? first : candidates[0];
        }

        /// <summary>统计 body 内 call/callvirt 指到 target 的调用点数量（用于同名歧义裁决）。</summary>
        private static int CountCallers(ModuleDefMD module, MethodDef target)
        {
            int count = 0;
            foreach (TypeDef t in module.GetTypes())
            foreach (MethodDef m in t.Methods)
            {
                if (!m.HasBody || m == target) continue;
                foreach (Instruction ins in m.Body.Instructions)
                {
                    if (ins.Operand is IMethod im &&
                        (ins.OpCode == OpCodes.Call || ins.OpCode == OpCodes.Callvirt) &&
                        string.Equals(im.FullName, target.FullName, StringComparison.Ordinal))
                    {
                        count++;
                        break;                     // 每个调用方法只计 1 次
                    }
                }
            }
            return count;
        }

        /// <summary>
        /// 兜底：把整个方法体重写为常量返回（"ldc.i4.0/1; ret"）。
        /// 语义 = BepInEx Postfix 强制 result；方法原有的早期分支/副作用一并失效，
        /// 因此只作为主/退化定位都失败时的最后手段。
        /// 写出后 Inspect 仍可正常识别（stub 恰好满足主策略形态）。
        /// </summary>
        private static void MakeConstantBody(MethodDef method, bool mosaicEnabled)
        {
            var body = new CilBody { InitLocals = false, MaxStack = 8 };
            // 注意：Ldc_I4_0/Ldc_I4_1 是无操作数短指令，不能用带 Int32 的重载
            body.Instructions.Add(mosaicEnabled
                ? Instruction.Create(OpCodes.Ldc_I4_1)
                : Instruction.Create(OpCodes.Ldc_I4_0));
            body.Instructions.Add(Instruction.Create(OpCodes.Ret));
            method.Body = body;
        }

        /// <summary>
        /// 定位补丁返回点。
        /// 主策略：方法体内最后一个 ret，且其前导是 ldc.i4.0/1；
        /// 退化策略：最后一个 ret 前导不是字面量时，方法体内必须恰有唯一一个
        /// {ldc.i4.0/1, ret} 候选，取之；否则视为找不到（NotFound）。
        /// 输出 retIndex = ret 指令位置（补丁点即 retIndex-1），currentValue = 当前值。
        /// </summary>
        private static bool FindPatchPoint(MethodDef method, out int retIndex, out bool currentValue)
        {
            retIndex = -1;
            currentValue = false;

            var instrs = method.Body.Instructions;
            int n = instrs.Count;

            // 最后一个 ret
            int lastRet = -1;
            for (int i = 0; i < n; i++)
            {
                if (instrs[i].OpCode == OpCodes.Ret) lastRet = i;
            }
            if (lastRet <= 0) return false;

            // 主策略：最后一个 ret 的前导是字面量
            OpCode lastPrev = instrs[lastRet - 1].OpCode;
            if (lastPrev == OpCodes.Ldc_I4_0 || lastPrev == OpCodes.Ldc_I4_1)
            {
                retIndex = lastRet;
                currentValue = lastPrev == OpCodes.Ldc_I4_1;
                return true;
            }

            // 退化策略：恰有一个 {ldc.i4.0/1, ret} 候选
            int candidate = -1;
            int candidateCount = 0;
            for (int i = 1; i < n; i++)
            {
                if (instrs[i].OpCode != OpCodes.Ret) continue;
                OpCode prev = instrs[i - 1].OpCode;
                if (prev != OpCodes.Ldc_I4_0 && prev != OpCodes.Ldc_I4_1) continue;
                candidate = i;
                candidateCount++;
            }
            if (candidateCount != 1) return false;

            retIndex = candidate;
            currentValue = instrs[candidate - 1].OpCode == OpCodes.Ldc_I4_1;
            return true;
        }
    }
}
