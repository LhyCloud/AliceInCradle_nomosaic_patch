using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using AicNoMosaic;
using dnlib.DotNet;
using dnlib.DotNet.Emit;

namespace AicPatchTester
{
    /// <summary>
    /// 补丁逻辑验证器：对传入的 Assembly-CSharp.dll 依次执行
    ///   1) Inspect 读取当前状态
    ///   2) Apply 翻转到相反状态 —— 应 Success
    ///      （状态自适应：仓库内已安装副本 Off→On；用户原始游戏 DLL On→Off）
    ///   3) Inspect 确认已翻转
    ///   4) Apply 同方向再次翻转           —— 应 AlreadyApplied（幂等）
    /// 返回非 0 表示有失败，用于 CI 断言。
    /// </summary>
    internal static class Program
    {
        private static int _synthFailures;

        private static int Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("用法: AIC_patch_tester <Assembly-CSharp.dll> [more.dll ...]");
                Console.WriteLine("  对每个 DLL 验证 MosaicPatcher 的定位/翻转/幂等逻辑。");
                return 2;
            }

            int failures = 0;

            // —— 合成用例：以第一个真实 DLL 为宿主改造出多返回点结构（不碰原文件）——
            if (args.Length > 0 && File.Exists(args[0]))
                failures += RunSyntheticTests(args[0]);

            // —— GameVersion：真实 128B 头部探针 + 全管线合成样本 ——
            failures += RunGameVersionTests(args);

            // —— DebugConfig 回归：对 _debug.txt 验证读取/改写/保序保留 ——
            foreach (string arg in args)
            {
                string debugTxt = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(arg)) ?? ".", "_debug.txt");
                if (File.Exists(debugTxt))
                    failures += RunDebugConfigTests(debugTxt);
            }

            // —— ShaManifest 回归：对 _sha.txt 验证更新/保留/.orig 恢复 ——
            foreach (string arg in args)
            {
                string shaTxt = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(arg)) ?? ".", "_sha.txt");
                if (File.Exists(shaTxt))
                    failures += RunShaManifestTests(shaTxt);
            }

            // —— BepInExDeployer 回归：框架判定 / 插件幂等释放与移除（与真实游戏目录无关，纯合成） ——
            failures += RunBepInExDeployerTests();

            foreach (string dll in args)
            {
                if (!File.Exists(dll))
                {
                    Console.WriteLine("[FAIL] 文件不存在: " + dll);
                    failures++;
                    continue;
                }

                Console.WriteLine("== " + Path.GetFileName(Path.GetDirectoryName(dll))
                    + Path.DirectorySeparatorChar + Path.GetFileName(dll) + " (" + new FileInfo(dll).Length.ToString("N0") + " B)");

                MosaicPatcher.InspectState state0 = MosaicPatcher.Inspect(dll);
                Console.WriteLine("   初始状态: " + state0);
                if (state0 != MosaicPatcher.InspectState.MosaicOff && state0 != MosaicPatcher.InspectState.MosaicOn)
                {
                    Console.WriteLine("   [FAIL] 初始状态异常，无法验证翻转逻辑。");
                    failures++;
                    continue;
                }

                string temp1 = dll + ".tester_flip";
                string temp2 = dll + ".tester_flip2";
                try
                {
                    // 1) 翻转到相反状态（目标方向自适应初始状态，两种来源均支持）
                    bool targetMosaic = state0 == MosaicPatcher.InspectState.MosaicOff;
                    var r1 = MosaicPatcher.Apply(dll, targetMosaic, temp1);
                    Console.WriteLine("   翻转 " + targetMosaic + " -> " + r1);
                    if (r1 != MosaicPatcher.PatchStatus.Success) { failures++; continue; }

                    MosaicPatcher.InspectState state1 = MosaicPatcher.Inspect(temp1);
                    bool expectedOn = targetMosaic;   // 翻转后应恰为目标状态
                    Console.WriteLine("   翻转后状态: " + state1);
                    if ((state1 == MosaicPatcher.InspectState.MosaicOn) != expectedOn)
                    {
                        Console.WriteLine("   [FAIL] 翻转后状态不符。");
                        failures++;
                        continue;
                    }

                    // 2) 再翻转一次（幂等性）
                    // 注意：AlreadyApplied 时不产生输出文件，状态检查回退到输入文件
                    var r2 = MosaicPatcher.Apply(temp1, targetMosaic, temp2);
                    Console.WriteLine("   重复应用 -> " + r2);
                    string secondFile;
                    if (r2 == MosaicPatcher.PatchStatus.AlreadyApplied)
                        secondFile = temp1;
                    else if (r2 == MosaicPatcher.PatchStatus.Success)
                        secondFile = temp2;
                    else
                    {
                        Console.WriteLine("   [FAIL] 幂等性校验失败。");
                        failures++;
                        continue;
                    }

                    // 校验最终产物状态与上次一致（round-trip 完整性）
                    MosaicPatcher.InspectState state2 = MosaicPatcher.Inspect(secondFile);
                    if (state2 != state1)
                    {
                        Console.WriteLine("   [FAIL] 二次写出的状态与上次不符: " + state2);
                        failures++;
                        continue;
                    }

                    Console.WriteLine("   [OK]");
                }
                finally
                {
                    TryDelete(temp1);
                    TryDelete(temp2);
                }
            }

            Console.WriteLine(failures == 0 ? "全部通过" : ("有 " + failures + " 个失败"));
            return failures == 0 ? 0 : 1;
        }

        private static void TryDelete(string path)
        {
            try { if (File.Exists(path)) File.Delete(path); } catch { }
        }

        // ==================================================================
        //  DebugConfig 回归：_debug.txt 读取/改写（UTF-8 无 BOM + LF 语义）
        // ==================================================================

        private static int RunDebugConfigTests(string debugTxt)
        {
            Console.WriteLine("== DebugConfig（" + Path.GetFileName(debugTxt) + "，样本含未知键/下行注释/超长行）");
            int failures = 0;
            string work = Path.Combine(Path.GetTempPath(), "aic_debugcfg_test");
            try
            {
                Directory.CreateDirectory(work);
                string testFile = Path.Combine(work, "_debug.txt");
                File.Copy(debugTxt, testFile, true);

                // 1) 读取：六个键全部有效（值本身随用户文件变化，不做硬编码断言——下面自记录自比对）
                var entries = new AicNoMosaic.DebugConfig.Entry[AicNoMosaic.DebugConfig.KeyCount];
                Expect("R1 读取成功", AicNoMosaic.DebugConfig.TryRead(testFile, entries));
                bool allValid = true;
                for (int i = 0; i < entries.Length; i++)
                    if (!entries[i].Valid) { allValid = false; break; }
                Expect("R2 六键全部命中", allValid);
                int[] before = new int[entries.Length];
                for (int i = 0; i < entries.Length; i++) before[i] = entries[i].Value;

                // 2) 写入：全部取反（0↔1）
                for (int i = 0; i < entries.Length; i++)
                    entries[i].Value = entries[i].Value == 0 ? 1 : 0;
                Expect("W1 写入成功", AicNoMosaic.DebugConfig.TryWrite(testFile, entries));

                // 3) 回读：全部已反转
                var recheck = new AicNoMosaic.DebugConfig.Entry[AicNoMosaic.DebugConfig.KeyCount];
                Expect("R3 回读成功", AicNoMosaic.DebugConfig.TryRead(testFile, recheck));
                bool reversed = true;
                for (int i = 0; i < recheck.Length; i++)
                    if (!recheck[i].Valid || recheck[i].Value != (before[i] == 0 ? 1 : 0)) { reversed = false; break; }
                Expect("R4 六键值已全部反转", reversed);

                // 4) 格式断言：UTF-8 无 BOM + LF（重建行为规范行；未知键/注释行原样保留）
                byte[] bytes = File.ReadAllBytes(testFile);
                Expect("F1 无 BOM", !(bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF));
                string utf8 = Encoding.UTF8.GetString(bytes);
                Expect("F2 无 CRLF 残留（统一 LF）", !utf8.Contains("\r"));

                // 5) 内容断言：未知键与玩家注释保留、目标键规范化、缺失键追加
                // nosnd 自本版起是受管理键，会被重写为规范行 "nosnd 0/1"；
                // 真正的未知键（supersensitive/nocfg/supercyclone/_player…）应原样保留
                bool unknownKeysKept =
                    utf8.Contains("supersensitive 0")
                    && utf8.Contains("nocfg 0")
                    && utf8.Contains("supercyclone 0")
                    && utf8.Contains("_player 0")
                    && utf8.Contains("benchmark 1")
                    && utf8.Contains("nocomputeshader 0");
                Expect("F3 未知键(supersensitive/nocfg/…/_player) 原样保留", unknownKeysKept);
                Expect("F3b nosnd 已升级为受管理键（规范化行）",
                    System.Text.RegularExpressions.Regex.IsMatch(utf8, @"\nnosnd [01]\n"));
                Expect("F6 游戏英文注释保留", utf8.Contains("enable log announce"));
                Expect("F7 目标键已规范化", utf8.Contains("\nmighty 1\n") || utf8.StartsWith("might 1\n") || utf8.Contains("\nmighty 0\n"));
                Expect("F8 超长注释行保留", utf8.Contains("It's not so funny"));

                // 6) 幂等：同样再写一次 → 无差异免写（mtime 不变）
                var again = new AicNoMosaic.DebugConfig.Entry[AicNoMosaic.DebugConfig.KeyCount];
                for (int i = 0; i < again.Length; i++) { again[i].Valid = true; again[i].Value = recheck[i].Value; }
                var mtimeBefore = File.GetLastWriteTimeUtc(testFile);
                Expect("W3 幂等重写成功", AicNoMosaic.DebugConfig.TryWrite(testFile, again));
                var mtimeAfter = File.GetLastWriteTimeUtc(testFile);
                Expect("W4 无差异不触碰文件（mtime 不变）", mtimeBefore == mtimeAfter);

                // 7) 部分写入：只写 weak，其余 Valid=false → 其它键行原样
                string beforePartial = File.ReadAllText(testFile, new UTF8Encoding(false));
                var partial = new AicNoMosaic.DebugConfig.Entry[AicNoMosaic.DebugConfig.KeyCount];
                partial[3].Valid = true;               // weak
                partial[3].Value = 1;
                Expect("P1 部分写入成功", AicNoMosaic.DebugConfig.TryWrite(testFile, partial));
                string afterPartial = File.ReadAllText(testFile, new UTF8Encoding(false));
                Expect("P2 目标键行已改写", afterPartial.Contains("\nweak 1\n") || afterPartial.Contains("\nweak 0\n"));
                bool othersUntouched = true;
                foreach (string known in new[] { "supersensitive 0", "nocfg 0", "supercyclone 0", "_player 0", "benchmark" })
                    if (!afterPartial.Contains(known)) { othersUntouched = false; break; }
                Expect("P3 未知键行原样（部分写入只动目标行）", othersUntouched);
            }
            catch (Exception ex)
            {
                Console.WriteLine("   [FAIL] DebugConfig 测试异常: " + ex.Message);
                failures++;
            }
            finally
            {
                try { Directory.Delete(work, true); } catch { }
            }
            return failures;
        }

        // ==================================================================
        //  BepInExDeployer 回归：框架判定 / 幂等释放 / 移除（纯合成目录）
        // ==================================================================

        private static int RunBepInExDeployerTests()
        {
            Console.WriteLine("== BepInExDeployer（合成目录：框架判定/幂等释放/移除）");
            int failures = 0;
            string work = Path.Combine(Path.GetTempPath(), "aic_bepinex_test");
            try
            {
                Directory.CreateDirectory(work);

                // 1) 框架判定：无 BepInEx → false
                Expect("E1 无框架 → false", !AicNoMosaic.BepInExDeployer.IsFrameworkPresent(work));

                // 2) 只建 plugins/（异常结构）→ 仍 false（判定只认 core/BepInEx.dll）
                Directory.CreateDirectory(Path.Combine(work, "BepInEx", "plugins"));
                Expect("E2 仅 plugins 目录 → false", !AicNoMosaic.BepInExDeployer.IsFrameworkPresent(work));

                // 3) 建立 core/BepInEx.dll → true
                Directory.CreateDirectory(Path.Combine(work, "BepInEx", "core"));
                File.WriteAllBytes(Path.Combine(work, "BepInEx", "core", "BepInEx.dll"), new byte[] { 1, 2, 3 });
                Expect("E3 core/BepInEx.dll → true", AicNoMosaic.BepInExDeployer.IsFrameworkPresent(work));

                byte[] payloadA = { 0xAA, 0xBB, 0xCC };
                byte[] payloadB = { 0x11, 0x22, 0x33, 0x44 };
                string pluginPath = AicNoMosaic.BepInExDeployer.PluginPath(work);

                // 4) 未装时插件路径不存在，Remove 是 no-op
                Expect("D1 未装 → Plugin 不存在 / Remove=no-op",
                    !AicNoMosaic.BepInExDeployer.IsPluginInstalled(work)
                    && !AicNoMosaic.BepInExDeployer.RemovePlugin(work));

                // 5) 首次释放 → 写入且内容一致（plugins/ 目录自动创建）
                Expect("D2 首次释放写入", AicNoMosaic.BepInExDeployer.ExtractPlugin(work, payloadA));
                Expect("D3 内容落位plugins/", File.ReadAllBytes(pluginPath).Length == payloadA.Length);

                // 6) 幂等：同 payload 再放 → 不写（mtime 不变）
                var mtimeBefore = File.GetLastWriteTimeUtc(pluginPath);
                Expect("D4 同内容幂等跳过", !AicNoMosaic.BepInExDeployer.ExtractPlugin(work, payloadA));
                Expect("D5 幂等未触碰（mtime 不变）", File.GetLastWriteTimeUtc(pluginPath) == mtimeBefore);

                // 7) 更新：不同 payload → 重写为新内容（temp+原子替换）
                Expect("D6 新版本重写", AicNoMosaic.BepInExDeployer.ExtractPlugin(work, payloadB));
                byte[] after = File.ReadAllBytes(pluginPath);
                Expect("D7 更新后内容一致",
                    after.Length == payloadB.Length && after[0] == payloadB[0] && after[3] == payloadB[3]);

                // 8) 移除 → 文件消失；再移除 → no-op
                Expect("D8 Remove 删除成功", AicNoMosaic.BepInExDeployer.RemovePlugin(work));
                Expect("D9 删除后不存在 / 二次 Remove=no-op",
                    !File.Exists(pluginPath) && !AicNoMosaic.BepInExDeployer.RemovePlugin(work));
            }
            catch (Exception ex)
            {
                Console.WriteLine("   [FAIL] BepInExDeployer 测试异常: " + ex.Message);
                failures++;
            }
            finally
            {
                try { Directory.Delete(work, true); } catch { }
            }
            return failures;
        }

        // ==================================================================
        //  合成用例：验证多返回点（主返回点策略）与退化策略
        // ==================================================================

        private static int RunSyntheticTests(string hostGameDll)
        {
            Console.WriteLine("== 合成用例（宿主: " + Path.GetFileName(hostGameDll) + ")");
            _synthFailures = 0;
            string work = Path.Combine(Path.GetTempPath(), "aic_tester_synth");
            Directory.CreateDirectory(work);

            try
            {
                // Case A：多返回点 —— 早期 return false + 最终 return true（0.30i 实测形态）
                // 主返回点 = 最后一个 ret → 补丁只翻主点，早期分支保持原样
                //   IL: Ldc_I4_0, Ret, Ldc_I4_1, Ret
                string a = Path.Combine(work, "caseA.dll");
                string aOut = Path.Combine(work, "caseA_out.dll");
                string aOut2 = Path.Combine(work, "caseA_out2.dll");
                if (BuildSynthetic(hostGameDll, a, b =>
                {
                    b.Instructions.Add(new Instruction(OpCodes.Ldc_I4_0));
                    b.Instructions.Add(new Instruction(OpCodes.Ret));
                    b.Instructions.Add(new Instruction(OpCodes.Ldc_I4_1));
                    b.Instructions.Add(new Instruction(OpCodes.Ret));
                }))
                {
                    Expect("A1 剩余 MosaicOn", MosaicPatcher.Inspect(a) == MosaicPatcher.InspectState.MosaicOn);
                    Expect("A2 应用 false", MosaicPatcher.Apply(a, false, aOut) == MosaicPatcher.PatchStatus.Success);
                    OpCode[] ops = ReadOpcodes(aOut);
                    Expect("A3 只翻主点（早期 return 保留）", ops.Length == 4
                        && ops[0] == OpCodes.Ldc_I4_0 && ops[1] == OpCodes.Ret
                        && ops[2] == OpCodes.Ldc_I4_0 && ops[3] == OpCodes.Ret);
                    Expect("A4 应用后 MosaicOff", MosaicPatcher.Inspect(aOut) == MosaicPatcher.InspectState.MosaicOff);
                    Expect("A5 恢复 true", MosaicPatcher.Apply(aOut, true, aOut2) == MosaicPatcher.PatchStatus.Success);
                    OpCode[] ops2 = ReadOpcodes(aOut2);
                    Expect("A6 恢复后主点=1、早期点=0", ops2.Length == 4
                        && ops2[0] == OpCodes.Ldc_I4_0 && ops2[1] == OpCodes.Ret
                        && ops2[2] == OpCodes.Ldc_I4_1 && ops2[3] == OpCodes.Ret);
                }
                else { Console.WriteLine("   [FAIL] 未能在宿主上定位 MosaicShower.FnDrawMosaic"); _synthFailures++; }

                // Case B：最后一个 ret 前导非字面量 → 退化策略取唯一 {ldc, ret} 候选
                //   IL: Ldc_I4_0, Ret, Ldc_I4_M1, Ret   (M1 不是补丁候选)
                string b2 = Path.Combine(work, "caseB.dll");
                string bOut = Path.Combine(work, "caseB_out.dll");
                if (BuildSynthetic(hostGameDll, b2, bd =>
                {
                    bd.Instructions.Add(new Instruction(OpCodes.Ldc_I4_0));
                    bd.Instructions.Add(new Instruction(OpCodes.Ret));
                    bd.Instructions.Add(new Instruction(OpCodes.Ldc_I4_M1));
                    bd.Instructions.Add(new Instruction(OpCodes.Ret));
                }))
                {
                    Expect("B1 退化策略得 MosaicOff", MosaicPatcher.Inspect(b2) == MosaicPatcher.InspectState.MosaicOff);
                    Expect("B2 已是 false → AlreadyApplied", MosaicPatcher.Apply(b2, false, bOut) == MosaicPatcher.PatchStatus.AlreadyApplied);
                    Expect("B3 翻转 true", MosaicPatcher.Apply(b2, true, bOut) == MosaicPatcher.PatchStatus.Success);
                    OpCode[] ops = ReadOpcodes(bOut);
                    Expect("B4 唯一候选被翻、M1 保留", ops.Length == 4
                        && ops[0] == OpCodes.Ldc_I4_1 && ops[1] == OpCodes.Ret
                        && ops[2] == OpCodes.Ldc_I4_M1 && ops[3] == OpCodes.Ret);
                }
                else { Console.WriteLine("   [FAIL] CaseB 构造失败"); _synthFailures++; }

                // Case C：无字面量候选（结构完全改动）
                // Inspect → NotFound（透明报告）；Apply → 常量体兜底（BepInEx 同构）
                string c = Path.Combine(work, "caseC.dll");
                string cOut = Path.Combine(work, "caseC_out.dll");
                if (BuildSynthetic(hostGameDll, c, cd =>
                {
                    cd.Instructions.Add(new Instruction(OpCodes.Ldc_I4_2));
                    cd.Instructions.Add(new Instruction(OpCodes.Ret));
                    cd.Instructions.Add(new Instruction(OpCodes.Ldc_I4_M1));
                    cd.Instructions.Add(new Instruction(OpCodes.Ret));
                }))
                {
                    Expect("C1 Inspect → NotFound（透明）", MosaicPatcher.Inspect(c) == MosaicPatcher.InspectState.NotFound);
                    Expect("C2 Apply → 常量体兜底成功", MosaicPatcher.Apply(c, false, cOut) == MosaicPatcher.PatchStatus.Success);
                    OpCode[] ops = ReadOpcodes(cOut);
                    Expect("C3 兜底体为 {ldc.i4.0, ret}", ops.Length == 2
                        && ops[0] == OpCodes.Ldc_I4_0 && ops[1] == OpCodes.Ret);
                    Expect("C4 兜底后 Inspect → MosaicOff", MosaicPatcher.Inspect(cOut) == MosaicPatcher.InspectState.MosaicOff);
                }
                else { Console.WriteLine("   [FAIL] CaseC 构造失败"); _synthFailures++; }
            }
            finally
            {
                try { Directory.Delete(work, true); } catch { }
            }

            return _synthFailures;
        }

        private static void Expect(string name, bool ok)
        {
            Console.WriteLine("   [" + (ok ? "OK" : "FAIL") + "] " + name);
            if (!ok) _synthFailures++;
        }

        // ==================================================================
        //  GameVersion 头部探针 + 目录名兜底回归
        //  （头部样本 = 用户提供的真实 globalgamemanagers 前 128 字节，hex 精确转写）
        // ==================================================================

        private const string RealHeadHex =
            "00000000000000000000001600000000" +
            "00000000000004620000000000037a7c" +
            "00000000000004a00000000000000000" +
            "323032322e332e363266320013000000" +
            "00130000008100000000ffffd00e6b78" +
            "46b7c72167b16846108ca6240d000000" +
            "00ffff592e035c5fc203d9bbeebadbc7" +
            "9487184000000000ffff3d9597d15b2b";

        private static int RunGameVersionTests(string[] args)
        {
            Console.WriteLine("== GameVersion（真实头部 128B 探针 + 全管线合成样本）");
            _synthFailures = 0;

            // —— 头部探针：128B 截断样本（不需要完整文件！）——
            byte[] head = HexToBytes(RealHeadHex.Replace(" ", ""));
            AicNoMosaic.GameVersion.HeadInfo h = AicNoMosaic.GameVersion.ParseHead(head);
            Expect("H1 头部自洽", h.Ok);
            Expect("H2 格式版本=22", h.FormatVersion == 22U);
            Expect("H3 metadataSize=1122", h.MetadataSize == 1122UL);
            Expect("H4 fileSize=227964", h.FileSize == 227964UL);
            Expect("H5 dataOffset=1184", h.DataOffset == 1184UL);
            Expect("H6 引擎=2022.3.62f2", h.EngineVersion == "2022.3.62f2");

            // —— 全管线合成样本：手工构造含 bundleVersion="0.30i" 的 globalgamemanagers ——
            //（防止"头部探针通过、L5 版本串判定失败"这类静默回归溜过测试）
            byte[] synth = BuildSyntheticGlobalGameManager("2022.3.62f2", "0.30i");
            Expect("V1 合成样本解析=0.30i",
                AicNoMosaic.GameVersion.FromBytes(synth) == "0.30i");

            byte[] broken = BuildSyntheticGlobalGameManager("2022.3.62f2", "zzzZ!");
            Expect("V2 非版本串 → 空（UI 显示未知）",
                AicNoMosaic.GameVersion.FromBytes(broken) == "");

            // —— 真实样本：参数目录旁发现 globalgamemanagers 时做实证断言 ——
            foreach (string arg in args)
            {
                string dir = Path.GetDirectoryName(Path.GetFullPath(arg)) ?? ".";
                string ggm = Path.Combine(dir, "globalgamemanagers");
                if (!File.Exists(ggm)) continue;
                string real = AicNoMosaic.GameVersion.FromBytes(File.ReadAllBytes(ggm));
                Expect("G真实样本解析非空", !string.IsNullOrEmpty(real));
                Console.WriteLine("   真实样本版本 = " + real);
                break;   // 一个真实样本足矣
            }

            Expect("G1 无文件目录 → 空（UI 显示未知）",
                AicNoMosaic.GameVersion.FromFolder(
                    Path.Combine(Path.GetTempPath(), "aic_gv_empty_missing_dir_42")) == "");

            Console.WriteLine(_synthFailures == 0 ? "   [OK] GameVersion 全部通过" : ("   有 " + _synthFailures + " 个失败"));
            return _synthFailures;
        }

        /// <summary>
        /// 构造最小但结构完全合规的 globalgamemanagers：
        /// 引擎 2022.3（无类型树）、类型表 2 项（129 PlayerSettings / 1 GameObject）、
        /// 对象表 1 项指向 PlayerSettings 数据块（byteStart=0、typeId=0）、
        /// bundleVersion 长度前缀位于对象内 612（2022.3 已知布局）。用于完整验证解析快路。
        /// </summary>
        private static byte[] BuildSyntheticGlobalGameManager(string engine, string bundleVersion)
        {
            byte[] engineBytes = Encoding.ASCII.GetBytes(engine);
            byte[] verBytes = Encoding.ASCII.GetBytes(bundleVersion);

            const int typeStart = 69;                      // AlignUp(48+12,4)=60 → +平台4 +tree 1 +count 4 = 69
            // 对象表布局与解析器约定一致：count 紧跟类型表（不额外对齐），条目起始 8 对齐；
            // 条目 = pathID(i64) + byteStart(u64) + size(u32) + typeId(i32) = 24B（实证）
            int objCountPos = typeStart + 2 * 23;          // = 115（两类型项各 23B）
            int objFirst = AlignUpTester(objCountPos + 4, 8);   // = 120
            int metaEnd = objFirst + 24;                   // 单对象条目 24B → 144
            int metaSize = metaEnd - 48;                   // = 96
            int dataOffset = 48 + ((metaSize + 15) & ~15); // 48 + AlignUp(96,16) = 144
            const int verRel = 612;                        // 2022.3 已知布局

            byte[] obj = new byte[verRel + 4 + verBytes.Length + (4 - verBytes.Length % 4) % 4];
            int fileSize = dataOffset + obj.Length;
            var b = new byte[fileSize];

            void PutInt32LE(int offset, int value)
            {
                b[offset] = (byte)value;
                b[offset + 1] = (byte)(value >> 8);
                b[offset + 2] = (byte)(value >> 16);
                b[offset + 3] = (byte)(value >> 24);
            }
            void PutInt32BE(int offset, int value)
            {
                b[offset] = (byte)(value >> 24);
                b[offset + 1] = (byte)(value >> 16);
                b[offset + 2] = (byte)(value >> 8);
                b[offset + 3] = (byte)value;
            }
            void PutInt64BE(int offset, long value)
            {
                for (int i = 0; i < 8; i++) b[offset + i] = (byte)(value >> (56 - i * 8));
            }

            // ---- L1 头（大端）----
            PutInt32BE(8, 22);
            PutInt64BE(16, metaSize);
            PutInt64BE(24, fileSize);
            PutInt64BE(32, dataOffset);

            // ---- L2 元数据头 ----
            Array.Copy(engineBytes, 0, b, 48, engineBytes.Length);
            PutInt32LE(60, 19);                            // targetPlatform
            b[64] = 0;                                     // enableTypeTree = false
            PutInt32LE(65, 2);                             // typeCount = 2

            // 类型表：classID + isStripped + scriptTypeIndex(=-1) + 16B 哈希 = 23B/项
            int p = typeStart;
            PutInt32LE(p, 129); b[p + 4] = 0; b[p + 5] = 0xFF; b[p + 6] = 0xFF; p += 7;
            for (int i = 0; i < 16; i++) b[p + i] = (byte)(0xD0 + i); p += 16;
            PutInt32LE(p, 1); b[p + 4] = 0; b[p + 5] = 0xFF; b[p + 6] = 0xFF; p += 7;
            for (int i = 0; i < 16; i++) b[p + i] = (byte)(0x10 + i); p += 16;
            // p == objCountPos（=115，紧跟类型表不额外对齐）

            // ---- 对象表：count 紧跟类型表；条目 8 对齐；24B 含 pathID ----
            PutInt32LE(objCountPos, 1);
            int e = objFirst;
            for (int i = 0; i < 8; i++) b[e + i] = (byte)((long)1 >> (i * 8) & 0xFF);   // pathID = 1
            for (int i = 0; i < 8; i++) b[e + 8 + i] = 0;              // byteStart = 0
            PutInt32LE(e + 16, obj.Length);                            // byteSize
            PutInt32LE(e + 20, 0);                                     // typeId → classIds[0] = 129

            // ---- 数据区：PlayerSettings（bundleVersion @612）----
            PutInt32LE(dataOffset + verRel, verBytes.Length);
            Array.Copy(verBytes, 0, b, dataOffset + verRel + 4, verBytes.Length);
            // 对齐填充保持 0

            return b;
        }

        private static byte[] HexToBytes(string hex)
        {
            var bytes = new byte[hex.Length / 2];
            for (int i = 0; i < bytes.Length; i++)
                bytes[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
            return bytes;
        }

        private static int AlignUpTester(int v, int align) => (v + (align - 1)) & ~(align - 1);

        // ==================================================================
        //  ShaManifest 回归：_sha.txt 更新/保留/.orig 恢复
        // ==================================================================

        private static int RunShaManifestTests(string shaTxt)
        {
            Console.WriteLine("== ShaManifest（" + Path.GetFileName(shaTxt) + "）");
            _synthFailures = 0;
            string work = Path.Combine(Path.GetTempPath(), "aic_shamanifest_test");
            try
            {
                Directory.CreateDirectory(work);
                string testFile = Path.Combine(work, "_sha.txt");
                File.Copy(shaTxt, testFile, true);
                string[] before = File.ReadAllLines(testFile);
                int beforeLines = before.Length;

                // 断言真实样本格式：路径/哈希交替，Hash 均小写 64 hex。已有 Assembly 条目。
                var fakeSha = new string('a', 64);
                bool ok = AicNoMosaic.ShaManifest.TryUpdateEntry(
                    testFile, AicNoMosaic.ShaManifest.ManagedEntry, fakeSha);
                Expect("S1 更新已有条目成功", ok);

                string[] after = File.ReadAllLines(testFile);
                Expect("S2 行数不变", after.Length == beforeLines);

                // 找到 Assembly 条目行，确认哈希已更新、其它条目保留
                int idx = -1;
                for (int i = 0; i + 1 < after.Length; i += 2)
                {
                    if (string.Equals(after[i].Trim(), AicNoMosaic.ShaManifest.ManagedEntry, StringComparison.OrdinalIgnoreCase))
                    { idx = i; break; }
                }
                Expect("S3 Assembly 条目存在", idx >= 0);
                if (idx < 0) return _synthFailures;
                Expect("S4 目标哈希已更新为 fake", after[idx + 1] == fakeSha);
                bool othersSame = true;
                for (int i = 0; i < before.Length; i++)
                {
                    if (i == idx) continue;                     // Assembly 路径行
                    if (i == idx + 1) continue;                 // 已更新的哈希行
                    if (before[i] != after[i]) { othersSame = false; break; }
                }
                Expect("S5 其余行原样保留", othersSame);

                // 格式断言
                byte[] bytes = File.ReadAllBytes(testFile);
                Expect("S6 无 BOM", !(bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF));
                string asText = Encoding.UTF8.GetString(bytes);
                Expect("S7 无 CRLF", !asText.Contains("\r"));

                // S8 真实 SHA256 端到端：让 ShaManifest 计算一个已知文件（改回自身）
                var tmpDll = Path.Combine(work, "dummy.dll");
                File.WriteAllBytes(tmpDll, new byte[] { 1, 2, 3, 4, 5 });
                string real = AicNoMosaic.ShaManifest.ComputeFileSha256(tmpDll);
                Expect("S8 SHA256 计算可用（64 hex）", real != null && real.Length == 64);

                // S9/S10 .orig 备份与恢复
                string orig = testFile + ".orig";
                Expect("S9 .orig 已自动生成", File.Exists(orig));
                string[] origLines = File.ReadAllLines(orig);
                int origIdx = -1;
                for (int i = 0; i + 1 < origLines.Length; i += 2)
                    if (string.Equals(origLines[i].Trim(), AicNoMosaic.ShaManifest.ManagedEntry, StringComparison.OrdinalIgnoreCase))
                    { origIdx = i; break; }
                Expect("S10 .orig 含原始哈希", origIdx >= 0 && origLines[origIdx + 1] != fakeSha);

                Expect("S11 TryRestoreOriginal 成功", AicNoMosaic.ShaManifest.TryRestoreOriginal(testFile));
                string[] restored = File.ReadAllLines(testFile);
                bool restoredOk = restored.Length == beforeLines;
                for (int i = 0; i < restored.Length && restoredOk; i++)
                    if (restored[i] != before[i]) restoredOk = false;
                Expect("S12 还原后与原始完全一致", restoredOk);
                Expect("S13 .orig 已被消耗删除", !File.Exists(orig));

                // S14 缺失条目的追加行为（合成清单）
                string miniFile = Path.Combine(work, "_mini.txt");
                File.WriteAllLines(miniFile, new[] { "Managed/other.dll", new string('0', 64) });
                AicNoMosaic.ShaManifest.TryUpdateEntry(miniFile, AicNoMosaic.ShaManifest.ManagedEntry, fakeSha);
                string mini = File.ReadAllText(miniFile);
                Expect("S14 清单无该条目时追加到末尾", mini.Contains("\nManaged/Assembly-CSharp.dll\n"));
            }
            catch (Exception ex)
            {
                Console.WriteLine("   [FAIL] ShaManifest 测试异常: " + ex.Message);
                _synthFailures++;
            }
            finally
            {
                try { Directory.Delete(work, true); } catch { }
            }
            Console.WriteLine(_synthFailures == 0 ? "   [OK] ShaManifest 全部通过" : ("   有 " + _synthFailures + " 个失败"));
            return _synthFailures;
        }

        private static bool BuildSynthetic(string hostDll, string outPath, Action<CilBody> filler)
        {
            try
            {
                using (ModuleDefMD module = ModuleDefMD.Load(hostDll))
                {
                    MethodDef method = FindMosaicMethod(module);
                    if (method == null) return false;
                    method.Body = new CilBody { InitLocals = false };
                    filler(method.Body);
                    module.Write(outPath);
                    return true;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("   [FAIL] 合成构造异常: " + ex.Message);
                return false;
            }
        }

        private static MethodDef FindMosaicMethod(ModuleDef module)
        {
            foreach (TypeDef t in module.GetTypes())
            {
                if (!string.Equals(t.Name, "MosaicShower", StringComparison.OrdinalIgnoreCase)) continue;
                foreach (MethodDef m in t.Methods)
                {
                    if (m.HasBody && string.Equals(m.Name, "FnDrawMosaic", StringComparison.OrdinalIgnoreCase))
                        return m;
                }
            }
            return null;
        }

        private static OpCode[] ReadOpcodes(string path)
        {
            using (ModuleDefMD module = ModuleDefMD.Load(path))
            {
                MethodDef method = FindMosaicMethod(module);
                return method.Body.Instructions.Select(i => i.OpCode).ToArray();
            }
        }
    }
}
