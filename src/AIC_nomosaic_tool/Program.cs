using System;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

namespace AicNoMosaic
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            bool createdNew;
            using (Mutex mutex = new Mutex(true, "LhyCloud_AIC_Nomosaic_V2_Mutex", out createdNew))
            {
                if (!createdNew)
                {
                    MessageBox.Show("程序已经在运行", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                // dnlib 以嵌入式资源随主程序发布，运行时从内存加载（零外部依赖、无临时文件落地）
                AppDomain.CurrentDomain.AssemblyResolve += OnAssemblyResolve;

                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new MainForm());
            }
        }

        private static Assembly OnAssemblyResolve(object sender, ResolveEventArgs args)
        {
            if (args.Name == null || !args.Name.StartsWith("dnlib", StringComparison.OrdinalIgnoreCase))
                return null;

            Assembly self = typeof(Program).Assembly;
            using (Stream stream = self.GetManifestResourceStream("dnlib.dll"))
            {
                if (stream == null) return null;
                byte[] buffer = new byte[stream.Length];
                stream.Read(buffer, 0, buffer.Length);
                return Assembly.Load(buffer);
            }
        }
    }
}
