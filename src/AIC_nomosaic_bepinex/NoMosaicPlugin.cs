using System;
using System.Reflection;
using BepInEx;
using BepInEx.Configuration;
using BepInEx.Logging;
using HarmonyLib;

namespace AicNoMosaic.BepInEx
{
    /// <summary>
    /// Alice In Cradle 去马赛克 BepInEx 插件（v2.3 精简版）。
    /// 运行时拦截 nel.MosaicShower.FnDrawMosaic —— 游戏更新免重打，删除即还原。
    /// </summary>
    [BepInPlugin(PluginGuid, PluginName, PluginVersion)]
    public sealed class NoMosaicPlugin : BaseUnityPlugin
    {
        public const string PluginGuid = "com.lhycloud.aic.nomosaic";
        public const string PluginName = "AIC NoMosaic";
        public const string PluginVersion = "2.3.0";

        internal ConfigEntry<bool> Enabled;
        internal static NoMosaicPlugin Instance { get; private set; }

        private void Awake()
        {
            Instance = this;

            Enabled = Config.Bind(
                "General",
                "Enabled",
                true,
                "去马赛克总开关。可直接编辑 BepInEx/config/" + PluginGuid + ".cfg，" +
                "或用 BepInEx ConfigurationManager（F1）热切换。");
            NoMosaicPatch.SkipOriginal = Enabled.Value;
            Enabled.SettingChanged += (s, e) => NoMosaicPatch.SkipOriginal = Enabled.Value;

            // 依赖缺失补装钩子：游戏自研组件（unsafeAssem 等）走 Managed/ 目录
            AppDomain.CurrentDomain.AssemblyResolve += (s, e) =>
            {
                if (e.Name.StartsWith("AIC_nomosaic_bepinex")) return null;
                string probe = System.IO.Path.Combine(
                    UnityEngine.Application.dataPath, "Managed",
                    new AssemblyName(e.Name).Name + ".dll");
                return System.IO.File.Exists(probe)
                    ? Assembly.LoadFrom(probe)
                    : null;
            };

            ApplyPatch();
        }

        private void ApplyPatch()
        {
            var logger = Logger;
            try
            {
                Assembly asm = Assembly.Load("Assembly-CSharp");
                Type type = asm.GetType("nel.MosaicShower", true);
                MethodBase target = AccessTools.Method(type, "FnDrawMosaic")
                    ?? throw new MissingMethodException(type.FullName, "FnDrawMosaic");

                NoMosaicPatch.SkipOriginal = Enabled.Value;
                new Harmony(PluginGuid).Patch(target,
                    prefix: new HarmonyMethod(AccessTools.Method(typeof(NoMosaicPatch), nameof(NoMosaicPatch.Prefix))));

                logger.LogInfo(PluginName + " " + PluginVersion + " 已注入，去码" +
                    (Enabled.Value ? "生效" : "已停用"));
            }
            catch (Exception ex)
            {
                logger.LogError("注入失败（游戏版本可能不兼容）：" + ex.Message);
            }
        }
    }

    internal static class NoMosaicPatch
    {
        public static volatile bool SkipOriginal;

        public static bool Prefix()
        {
            return !SkipOriginal;   // 去码开 → false 跳过原方法（不画码）；关 → 放行
        }
    }
}
