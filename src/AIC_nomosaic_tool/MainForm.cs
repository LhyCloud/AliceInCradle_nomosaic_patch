using System;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace AicNoMosaic
{
    /// <summary>
    /// 主窗口：选择游戏目录 → 安装/卸载补丁 → 调试设置。
    /// 动识别/exe 选择/调试编辑，并新增：
    /// 拖拽支持、记住上次目录、原子写回、自动备份、补丁状态显示。
    /// </summary>
    internal sealed class MainForm : Form
    {
        private const string AppVersion = "v2.0.0";
        private const string ReleasesUrl = "https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/releases";
        private const string ReadmeUrl = "https://github.com/LhyCloud/AliceInCradle_nomosaic_patch/blob/main/README.md";

        private Label _titleLabel;
        private Label _versionLabel;
        private Label _pathLabel;
        private Label _stateLabel;
        private Label _statusLabel;
        private Button _chooseButton;
        private Button _installButton;
        private Button _uninstallButton;
        private string _gameRoot = "";

        public MainForm()
        {
            InitUi();
            InitWiring();
            LoadOrDetectGame();
        }

        // ================================ UI ================================

        private void InitUi()
        {
            Font baseFont = new Font("Microsoft YaHei", 10F);
            Font bigFont = new Font("Microsoft YaHei", 17F, FontStyle.Bold);
            Font smallFont = new Font("Microsoft YaHei", 8.5F);

            Text = "Alice In Cradle 补丁工具 " + AppVersion;
            AutoSize = true;
            AutoSizeMode = AutoSizeMode.GrowAndShrink;
            // 标题栏：无图标、无最大化/最小化（仅留 ✕；退出另有 文件→退出）
            MaximizeBox = false;
            MinimizeBox = false;
            ShowIcon = false;
            StartPosition = FormStartPosition.CenterScreen;
            Font = baseFont;
            AllowDrop = true;

            var menu = new MenuStrip();
            var toolsMenu = new ToolStripMenuItem("工具(&T)");
            toolsMenu.DropDownItems.Add(new ToolStripMenuItem("游戏调试设置(&D)", null, (s, e) => ShowDebugSettings()));
            var helpMenu = new ToolStripMenuItem("帮助(&H)");
            helpMenu.DropDownItems.Add(new ToolStripMenuItem("教程文档（工具说明）(&A)", null, (s, e) => OpenUrl(ReadmeUrl)));
            helpMenu.DropDownItems.Add(new ToolStripMenuItem("更新本工具(&U)", null, (s, e) => OpenUrl(ReleasesUrl)));
            helpMenu.DropDownItems.Add(new ToolStripMenuItem("版本信息(&V)", null, (s, e) => ShowAbout()));
            helpMenu.DropDownItems.Add(new ToolStripMenuItem("版本检测诊断(&G)", null, (s, e) => ShowVersionDiagnostics()));
            menu.Items.Add(toolsMenu);
            menu.Items.Add(helpMenu);
            MainMenuStrip = menu;   // 挂菜单源与 Alt 快捷键；控件本体作 root 首行（见下）

            var root = new TableLayoutPanel
            {
                // 官方 AutoSize 测量链要求：容器自带 AutoSize+GrowAndShrink（不能被 Dock=Fill 短路）
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                ColumnCount = 1,
                Padding = new Padding(24, 8, 24, 12),   // 左右对称 —— 内容居中不偏轴
            };
            root.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            Controls.Add(menu);
            Controls.Add(root);

            int rowIndex = 0;
            void AddRow(Control control)
            {
                root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                root.Controls.Add(control, 0, rowIndex++);
            }

            // ── 标题区（Anchor=None → 在 100% 宽单元格里水平居中）──
            _titleLabel = new Label
            {
                Text = "Alice In Cradle",
                Font = bigFont,
                ForeColor = Color.Black,
                AutoSize = true,
                Anchor = AnchorStyles.None,
                Margin = new Padding(0, 20, 0, 0),
            };
            AddRow(_titleLabel);

            var subtitle = new Label
            {
                Text = "全版本通用补丁",
                ForeColor = Color.DimGray,
                AutoSize = true,
                Anchor = AnchorStyles.None,
                Margin = new Padding(0),
            };
            AddRow(subtitle);

            // ── ① 第一步：选游戏目录 ──
            _chooseButton = new Button
            {
                Text = "① 选择游戏目录（或把游戏 exe 拖进来）",
                AutoSize = true,
                Anchor = AnchorStyles.None,
                Margin = new Padding(0, 12, 0, 6),
            };
            _chooseButton.Click += (s, e) => ChooseGameFolder();
            AddRow(_chooseButton);

            _versionLabel = InfoValue("未知", baseFont);
            _pathLabel = InfoValue("", smallFont);
            _pathLabel.ForeColor = Color.DimGray;
            _stateLabel = InfoValue("未选择游戏目录", baseFont);

            var infoGrid = new TableLayoutPanel
            {
                ColumnCount = 2,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Margin = new Padding(0),
                Anchor = AnchorStyles.None,       // 所有块都在列内居中，同一参考系
            };
            infoGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            infoGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 320));   // 数值列固定宽（检测出的最长路径）
            infoGrid.Controls.Add(InfoKey("游戏版本：", baseFont), 0, 0);
            infoGrid.Controls.Add(_versionLabel, 1, 0);
            infoGrid.Controls.Add(InfoKey("游戏路径：", baseFont), 0, 1);
            infoGrid.Controls.Add(_pathLabel, 1, 1);
            infoGrid.Controls.Add(InfoKey("补丁状态：", baseFont), 0, 2);
            infoGrid.Controls.Add(_stateLabel, 1, 2);
            AddRow(infoGrid);

            // ── ③ 操作按钮：两列固定等宽 + 按钮 Dock.Fill（文字完整、两按钮严格同宽）──
            _installButton = new Button
            {
                Text = "② 安装去码补丁",
                Dock = DockStyle.Fill,
                Height = 42,
                Margin = new Padding(0, 8, 8, 0),
                Enabled = false,
            };
            _uninstallButton = new Button
            {
                Text = "③ 卸载去码补丁",
                Dock = DockStyle.Fill,
                Height = 42,
                Margin = new Padding(8, 8, 0, 0),
                Enabled = false,
            };
            var buttonGrid = new TableLayoutPanel
            {
                ColumnCount = 2,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Margin = new Padding(0),
                Anchor = AnchorStyles.None,       // 与 infoGrid 同参考系，两块中轴对齐
            };
            buttonGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
            buttonGrid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
            buttonGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            buttonGrid.Controls.Add(_installButton, 0, 0);
            buttonGrid.Controls.Add(_uninstallButton, 1, 0);
            AddRow(buttonGrid);

            _statusLabel = new Label
            {
                Text = "",
                Font = smallFont,
                ForeColor = Color.DimGray,
                AutoSize = true,
                Margin = new Padding(0, 10, 0, 0),
            };
            AddRow(_statusLabel);

            // ── ⑤ 注意事项（流末尾）──
            var tip = new Label
            {
                Text = "安装/卸载均需约 5~10 秒，请勿重复操作\n" +
                    "检测到 BepInEx 时自动改用插件方式（免改游戏文件，删除插件即还原）\n" +
                    "无 BepInEx 时安装自动备份原始 DLL（Assembly-CSharp.dll.bak）",
                Font = smallFont,
                ForeColor = Color.Firebrick,
                AutoSize = true,
                Margin = new Padding(0, 6, 0, 0),
            };
            AddRow(tip);
        }

        private static Label InfoKey(string text, Font font)
        {
            return new Label
            {
                Text = text,
                Font = font,
                AutoSize = true,                  // 标签贴内容宽度（"游戏版本："≈5字），绝不留大空白
                Margin = new Padding(0, 7, 0, 2),
            };
        }

        private static Label InfoValue(string text, Font font)
        {
            return new Label
            {
                Text = text,
                Font = font,
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleLeft,
                AutoEllipsis = true,
                Margin = new Padding(4, 7, 0, 2),
            };
        }

        private void InitWiring()
        {
            DragEnter += OnDragEnter;
            DragDrop += OnDragDrop;
            _installButton.Click += (s, e) => InstallPatch();
            _uninstallButton.Click += (s, e) => UninstallPatch();
        }

        // ============================== 逻辑 ================================

        private void LoadOrDetectGame()
        {
            // 1) 记住的上次目录
            AppSettings.Load();
            if (TrySelectGameRoot(AppSettings.GamePath)) return;

            // 2) 工具所在目录即游戏目录
            try
            {
                string exeDir = Path.GetDirectoryName(Application.ExecutablePath);
                if (TrySelectGameRoot(exeDir)) return;
            }
            catch { }
        }

        private bool TrySelectGameRoot(string dir)
        {
            if (!GameFiles.IsGameRoot(dir)) return false;
            SelectGameRoot(Path.GetFullPath(dir));
            return true;
        }

        private void SelectGameRoot(string dir)
        {
            _gameRoot = dir;
            AppSettings.GamePath = dir;
            AppSettings.Save();

            _pathLabel.Text = dir;
            _versionLabel.Text = string.IsNullOrEmpty(GameVersion.FromFolder(dir))
                ? "未知"
                : GameVersion.FromFolder(dir);
            _installButton.Enabled = true;
            _uninstallButton.Enabled = true;
            _statusLabel.Text = "";

            if (GameFiles.IsFileInUse(GameFiles.AssemblyPath(_gameRoot)))
                MessageBox.Show("游戏文件被占用，请先关闭游戏。", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);

            RefreshState();
        }

        private void ChooseGameFolder()
        {
            using (var dialog = new OpenFileDialog
            {
                Title = "请选择游戏主程序",
                Filter = "可执行文件 (*.exe)|*.exe",
                FileName = "AliceInCradle.exe",
                CheckFileExists = true,
            })
            {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                string root;
                try { root = Path.GetDirectoryName(dialog.FileName); }
                catch { return; }
                if (!GameFiles.IsGameRoot(root))
                {
                    MessageBox.Show("未找到游戏文件，请确认选择的是游戏主程序。", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                SelectGameRoot(root);
            }
        }

        private void OnDragEnter(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.Copy;
        }

        private void OnDragDrop(object sender, DragEventArgs e)
        {
            string[] files = e.Data.GetData(DataFormats.FileDrop) as string[];
            if (files == null || files.Length == 0) return;
            string first = files[0];
            try
            {
                string root = Directory.Exists(first)
                    ? first
                    : Path.GetDirectoryName(first);
                if (!GameFiles.IsGameRoot(root))
                {
                    MessageBox.Show("未找到游戏文件，请拖入游戏主程序或游戏目录。", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                SelectGameRoot(Path.GetFullPath(root));
            }
            catch { }
        }

        // ----------------------------- 安装/卸载 -----------------------------

        /// <summary>补丁执行结果（工作函数纯返回值，UI 层据此弹窗/更新状态栏）。</summary>
        private enum PatchOutcome
        {
            Succeeded,           // dnlib 修改 + 自校验通过 + 已原子替换
            RestoredFromBackup,  // 从 .bak 直接还原
            AlreadyApplied,      // 已是目标状态
            NotFound,            // 未找到补丁位置
            VerifyOrIoFailed,    // 内部校验/IO 失败
            BlockedInUse,        // 游戏文件被占用
            NoGameRoot,          // 未选择游戏目录
            PluginDeployed,      // 已释放/更新 BepInEx 插件（免改游戏文件）
            PluginFresh,         // 插件已是当前版本（无需重复写入）
            PluginRemoved,       // 已移除 BepInEx 插件
        }

        /// <summary>
        /// 补丁工作结果：Outcome 决定弹窗/状态行；StaticRestored 说明本次操作
        /// 是否顺带还原了残留的静态补丁（BepInEx 插件方式的附加动作）。
        /// </summary>
        private sealed class PatchResult
        {
            public PatchOutcome Outcome;
            public bool StaticRestored;
        }

        private void InstallPatch()
        {
            RunPatchingAsync(install: true, verb: "安装");
        }

        private void UninstallPatch()
        {
            RunPatchingAsync(install: false, verb: "卸载");
        }

        /// <summary>
        /// 补丁执行（async）：dnlib 加载/改 IL/写盘的 5~10 秒放在线程池上执行，
        /// UI 线程保持响应（窗口可拖动/重绘；上游"窗口卡顿属正常现象"的根源
        /// 就是这段同步跑在 UI 线程上）。UI 副作用（弹窗/状态栏）全部留在
        /// await 之后的主线程，工作函数 PerformPatchWork 纯计算/IO 无弹窗。
        /// </summary>
        private async void RunPatchingAsync(bool install, string verb)
        {
            SetButtonsEnabled(false);
            _statusLabel.Text = "正在" + verb + "…";
            Cursor = Cursors.WaitCursor;
            var stopwatch = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                PatchResult result = await Task.Run(() => PerformPatchWork(install));
                stopwatch.Stop();
                string elapsed = verb + "完成，耗时 " + stopwatch.Elapsed.TotalSeconds.ToString("0.0") + " 秒";
                string staticNote = result.StaticRestored
                    ? "\n此前安装的静态补丁已一并还原。"
                    : "";

                switch (result.Outcome)
                {
                    case PatchOutcome.PluginDeployed:
                        MessageBox.Show(this,
                            "检测到 BepInEx，已释放去码插件到 BepInEx\\plugins\\。" +                            "重启游戏后由 BepInEx 自动注入生效（游戏内左上角会短暂显示注入状态）。\n" +                            "\n免改游戏文件：游戏更新无需重打，卸载时删除插件即完全还原。" + staticNote,
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        _statusLabel.Text = elapsed;
                        break;

                    case PatchOutcome.PluginFresh:
                        MessageBox.Show(this, "去码插件已是当前版本，无需重新释放。",
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        _statusLabel.Text = verb + "中止（已是目标状态）";
                        break;

                    case PatchOutcome.PluginRemoved:
                        MessageBox.Show(this, "已移除 BepInEx 去码插件，游戏完全还原原状。" + staticNote,
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        _statusLabel.Text = elapsed;
                        break;

                    case PatchOutcome.Succeeded:
                        MessageBox.Show(this,
                            (install ? "安装补丁成功！" : "卸载补丁成功！")
                            + (result.StaticRestored ? "\n此前残留的静态补丁状态已一并处理。" : ""),
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        _statusLabel.Text = elapsed;
                        break;

                    case PatchOutcome.AlreadyApplied:
                        MessageBox.Show(this, install ? "补丁已安装，请勿重复安装！" : "未找到已安装的补丁！",
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        _statusLabel.Text = verb + "中止（已是目标状态）";
                        break;

                    case PatchOutcome.NotFound:
                        MessageBox.Show(this, "游戏文件异常！找不到补丁安装位置。\n" +
                            "这可能不是对应的游戏本体，或版本结构已变化。",
                            "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        _statusLabel.Text = verb + "失败（未找到补丁位置）";
                        break;

                    case PatchOutcome.VerifyOrIoFailed:
                        MessageBox.Show(this, "补丁生成失败\n错误信息：内部校验未通过，请重新安装。",
                            "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        _statusLabel.Text = verb + "失败（校验未通过）";
                        break;

                    case PatchOutcome.BlockedInUse:
                        MessageBox.Show(this, "游戏文件被占用，请先关闭游戏。",
                            "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        _statusLabel.Text = verb + "中止（游戏文件被占用）";
                        break;

                    case PatchOutcome.NoGameRoot:
                        MessageBox.Show(this, "请先选择游戏目录。",
                            "错误", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        _statusLabel.Text = "";
                        break;
                }
            }
            catch (UnauthorizedAccessException)
            {
                if (MessageBox.Show(
                        "写入游戏目录被拒绝（通常游戏在 Program Files 等受保护目录）。\n" +
                        "是否以管理员身份重新运行本工具？",
                        "需要管理员权限", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                {
                    if (Elevation.RestartAsAdmin()) Close();
                }
                _statusLabel.Text = verb + "失败（无写入权限）";
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, verb + "补丁失败\n错误信息：" + ex.Message,
                    "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                _statusLabel.Text = verb + "失败";
            }
            finally
            {
                Cursor = Cursors.Default;
                SetButtonsEnabled(true);   // 先全部恢复，随后 RefreshState 按实际状态校正
                RefreshState();
            }
        }

        /// <summary>
        /// 补丁工作（纯 IO/dnlib，无 UI 副作用；在线程池上执行）。
        /// 方式选择：检测到 BepInEx 框架（BepInEx/core/BepInEx.dll）时自动切换
        /// 为插件方式 —— 释放内嵌插件到 BepInEx/plugins/，不改动游戏文件；
        /// 此前若装过静态补丁则顺带还原。未检测到时走 dnlib 静态补丁流程。
        /// 附加职责：维持 _sha.txt（游戏防篡改校验清单）与 DLL 实际状态一致。
        /// </summary>
        private PatchResult PerformPatchWork(bool install)
        {
            if (!GameFiles.IsGameRoot(_gameRoot))
                return new PatchResult { Outcome = PatchOutcome.NoGameRoot };

            string dll = GameFiles.AssemblyPath(_gameRoot);
            string backup = GameFiles.BackupPath(_gameRoot);
            string temp = dll + ".tmp_aic";
            string sha = GameFiles.ShaManifestPath(_gameRoot);
            TryDeleteFile(temp);

            if (GameFiles.IsFileInUse(dll))
                return new PatchResult { Outcome = PatchOutcome.BlockedInUse };

            bool hasBepInEx = BepInExDeployer.IsFrameworkPresent(_gameRoot);

            if (install)
            {
                // ---- 安装 · BepInEx 存在 → 插件方式（内容一致则幂等跳过）----
                if (hasBepInEx)
                {
                    bool wrote = BepInExDeployer.ExtractPlugin(_gameRoot);
                    // 顺带还原残留静态补丁：MosaicOn=原版无需处理；
                    // NotFound/Error 也尝试还原（.bak 路径可救异常状态的 dll）
                    bool staticRestored = false;
                    if (MosaicPatcher.Inspect(dll) != MosaicPatcher.InspectState.MosaicOn)
                        staticRestored = TryRestoreStaticPatch(dll, backup, sha, temp);
                    return new PatchResult
                    {
                        Outcome = wrote ? PatchOutcome.PluginDeployed : PatchOutcome.PluginFresh,
                        StaticRestored = staticRestored,
                    };
                }

                // ---- 安装 · 静态方式：dnlib 进程内改 IL，输出到同目录临时文件 ----
                var status = MosaicPatcher.Apply(dll, false, temp);   // false=马赛克禁用（去码）
                switch (status)
                {
                    case MosaicPatcher.PatchStatus.NotFound:
                        TryDeleteFile(temp);
                        return new PatchResult { Outcome = PatchOutcome.NotFound };
                    case MosaicPatcher.PatchStatus.AlreadyApplied:
                        TryDeleteFile(temp);
                        return new PatchResult { Outcome = PatchOutcome.AlreadyApplied };
                    case MosaicPatcher.PatchStatus.IoError:
                    case MosaicPatcher.PatchStatus.VerifyFailed:
                        TryDeleteFile(temp);
                        return new PatchResult { Outcome = PatchOutcome.VerifyOrIoFailed };
                }

                // Success：先备份原始 DLL（幂等，只备份一次），再原子替换。
                // 只在「确实是原版状态」时备份，避免把已打补丁的文件误存为 .bak。
                if (!File.Exists(backup)
                    && MosaicPatcher.Inspect(dll) == MosaicPatcher.InspectState.MosaicOn)
                {
                    File.Copy(dll, backup, false);
                }

                ReplaceAtomically(temp, dll);

                // ---- 维持 _sha.txt 一致性：登记打完补丁后的新 SHA256（非致命，失败不影响补丁） ----
                string newSha = ShaManifest.ComputeFileSha256(dll);
                if (newSha != null)
                    ShaManifest.TryUpdateEntry(sha, ShaManifest.ManagedEntry, newSha);

                return new PatchResult { Outcome = PatchOutcome.Succeeded };
            }
            else
            {
                // ---- 卸载：插件移除与静态还原两个维度各自判净，互不排斥 ----
                bool removedPlugin = hasBepInEx && BepInExDeployer.RemovePlugin(_gameRoot);

                // 静态：MosaicOn=已是原版无需处理；NotFound/Error 同样尝试还原（.bak 可救砖）
                bool staticNeedsRestore = MosaicPatcher.Inspect(dll) != MosaicPatcher.InspectState.MosaicOn;
                bool staticRestored = staticNeedsRestore && TryRestoreStaticPatch(dll, backup, sha, temp);

                if (removedPlugin)
                    return new PatchResult { Outcome = PatchOutcome.PluginRemoved, StaticRestored = staticRestored };
                if (staticRestored)
                    return new PatchResult { Outcome = PatchOutcome.Succeeded };
                return new PatchResult { Outcome = PatchOutcome.AlreadyApplied };
            }
        }

        /// <summary>
        /// 还原静态补丁到原始 DLL（供插件安装顺带清理 / 卸载路径复用）。
        /// 优先进 .bak 直接还原（对未知/损坏状态的 dll 也能救砖）；
        /// 无 .bak 时用 dnlib 把补丁点翻回马赛克开启。返回是否做了实质还原。
        /// </summary>
        private static bool TryRestoreStaticPatch(string dll, string backup, string sha, string temp)
        {
            if (File.Exists(backup))
            {
                File.Copy(backup, temp, true);
                ReplaceAtomically(temp, dll);          // temp 被消费
                TryDeleteFile(backup);
                ShaManifest.TryRestoreOriginal(sha);   // 校验清单同参还原（原始哈希表）
                return true;
            }

            var status = MosaicPatcher.Apply(dll, true, temp);   // true=马赛克恢复
            switch (status)
            {
                case MosaicPatcher.PatchStatus.Success:
                    ReplaceAtomically(temp, dll);
                    string newSha = ShaManifest.ComputeFileSha256(dll);
                    if (newSha != null)
                        ShaManifest.TryUpdateEntry(sha, ShaManifest.ManagedEntry, newSha);
                    return true;
                default:
                    TryDeleteFile(temp);
                    return false;
            }
        }

        /// <summary>关键优化 #5：用 File.Replace 做同卷近原子替换，中途断电也不会损坏游戏文件。</summary>
        private static void ReplaceAtomically(string tempFile, string targetFile)
        {
            if (File.Exists(targetFile))
                File.Replace(tempFile, targetFile, null, true);
            else
                File.Move(tempFile, targetFile);
        }

        private static void TryDeleteFile(string path)
        {
            try { if (File.Exists(path)) File.Delete(path); } catch { }
        }

        private void SetButtonsEnabled(bool enabled)
        {
            _installButton.Enabled = enabled;
            _uninstallButton.Enabled = enabled;
            _chooseButton.Enabled = enabled;   // 避免执行中切换游戏目录
        }

        /// <summary>
        /// 按补丁实际状态启用/禁用对应按钮。
        /// 静态方式：已补丁→只能卸载，原版→只能安装，未知→两个都允许尝试。
        /// BepInEx 方式：安装=释放/更新插件（幂等，可重复）、卸载=移除插件+还原静态；
        /// 两个维度互不排斥 → 两按钮一律可用，安装按钮换文案以提示方式差异。
        /// </summary>
        private void ApplyButtonStates(MosaicPatcher.InspectState state, bool hasBepInEx)
        {
            if (hasBepInEx)
            {
                _installButton.Text = "② 释放去码插件";
                _installButton.Enabled = true;
                _uninstallButton.Enabled = true;
                return;
            }
            _installButton.Text = "② 安装去码补丁";
            switch (state)
            {
                case MosaicPatcher.InspectState.MosaicOff:    // 已补丁
                    _installButton.Enabled = false;
                    _uninstallButton.Enabled = true;
                    break;
                case MosaicPatcher.InspectState.MosaicOn:     // 原版
                    _installButton.Enabled = true;
                    _uninstallButton.Enabled = false;
                    break;
                default:                                      // 未知 → 不预判
                    _installButton.Enabled = true;
                    _uninstallButton.Enabled = true;
                    break;
            }
        }

        // ----------------------------- 状态/杂项 -----------------------------

        /// <summary>刷新补丁状态与按钮可用性（dnlib 检测在线程池上，await 后回 UI 线程）。</summary>
        private async void RefreshState()
        {
            if (!GameFiles.IsGameRoot(_gameRoot))
            {
                _stateLabel.Text = "未找到游戏";
                _installButton.Text = "② 安装去码补丁";
                return;
            }
            _stateLabel.Text = "检测中…";
            string dll = GameFiles.AssemblyPath(_gameRoot);

            MosaicPatcher.InspectState state;
            try
            {
                state = await Task.Run(() => MosaicPatcher.Inspect(dll));
            }
            catch
            {
                state = MosaicPatcher.InspectState.Error;
            }

            // BepInEx/插件存在性 = 两个 File.Exists（毫秒级），留 UI 线程即可
            bool hasBepInEx = BepInExDeployer.IsFrameworkPresent(_gameRoot);
            bool hasPlugin = hasBepInEx && BepInExDeployer.IsPluginInstalled(_gameRoot);

            string text;
            if (hasPlugin)
            {
                text = state == MosaicPatcher.InspectState.MosaicOff
                    ? "已安装插件（BepInEx 运行时去码）+ 残留静态补丁"
                    : "已安装插件（BepInEx 运行时去码）";
            }
            else if (hasBepInEx)
            {
                text = state == MosaicPatcher.InspectState.MosaicOff
                    ? "已安装补丁（去码生效）"
                    : "未安装（检测到 BepInEx：安装=释放插件）";
            }
            else switch (state)
            {
                case MosaicPatcher.InspectState.MosaicOff: text = "已安装补丁（去码生效）"; break;
                case MosaicPatcher.InspectState.MosaicOn: text = "未安装（原始状态）"; break;
                case MosaicPatcher.InspectState.NotFound: text = "未知（未找到补丁位置）"; break;
                default: text = "未知（读取失败）"; break;
            }
            _stateLabel.Text = text;
            ApplyButtonStates(state, hasBepInEx);   // 状态和按钮可用性同步
        }

        private void ShowDebugSettings()
        {
            if (string.IsNullOrEmpty(_gameRoot) || !GameFiles.IsGameRoot(_gameRoot))
            {
                MessageBox.Show("请先选择游戏目录。", "错误", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            using (var form = new DebugSettingsForm(GameFiles.DebugConfigPath(_gameRoot)))
            {
                form.ShowDialog(this);
            }
        }

        private static void OpenUrl(string url)
        {
            try { System.Diagnostics.Process.Start(url); } catch { }
        }

        private void ShowAbout()
        {
            MessageBox.Show(
                "补丁工具版本：" + AppVersion + "（C# WinForms 单文件重写）\n" +
                "上游项目：LhyCloud/AliceInCradle_nomosaic_patch\n" +
                "补丁方式：dnlib 进程内修改 MosaicShower.FnDrawMosaic 返回值\n" +
                "兼容游戏版本：0.21 及之后的版本（自动检测）",
                "版本信息", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        /// <summary>版本检测自诊断：输出 globalgamemanagers 解析各环节状态，用于远程定位。</summary>
        private void ShowVersionDiagnostics()
        {
            string report = string.IsNullOrEmpty(_gameRoot)
                ? "请先选择游戏目录再运行诊断。"
                : GameVersion.Diagnose(_gameRoot);
            MessageBox.Show(this, report, "版本检测诊断 — " + _gameRoot,
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
