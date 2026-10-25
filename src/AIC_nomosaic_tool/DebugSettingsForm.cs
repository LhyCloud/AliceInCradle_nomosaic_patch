using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace AicNoMosaic
{
    internal sealed class DebugSettingsForm : Form
    {
        private readonly string _configPath;
        private readonly CheckBox[] _boxes = new CheckBox[DebugConfig.KeyCount];

        public DebugSettingsForm(string configPath)
        {
            _configPath = configPath;

            Text = "游戏调试设置";
            AutoSize = true;
            AutoSizeMode = AutoSizeMode.GrowAndShrink;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;
            Font = new Font("Microsoft YaHei", 9F);

            var root = new TableLayoutPanel
            {
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                ColumnCount = 1,
                Padding = new Padding(26, 14, 26, 14),
            };
            root.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            Controls.Add(root);

            int rowIndex = 0;
            void AddRow(Control control)
            {
                root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                root.Controls.Add(control, 0, rowIndex++);
            }

            AddRow(new Label
            {
                Text = "_debug.txt（游戏自带调试开关）",
                Font = new Font("Microsoft YaHei", 9.5F, FontStyle.Bold),
                AutoSize = true,
                Margin = new Padding(0),
            });

            for (int i = 0; i < DebugConfig.KeyCount; i++)
            {
                if (i == DebugConfig.CheatGroupCount)
                {
                    AddRow(new Label
                    {
                        Text = "── 进阶（排障 / 功能开关）──",
                        ForeColor = Color.DimGray,
                        Font = new Font("Microsoft YaHei", 8.5F, FontStyle.Bold),
                        AutoSize = true,
                        Margin = new Padding(0, 10, 0, 0),
                    });
                }
                else if (i == 1)
                {
                    AddRow(new Label
                    {
                        Text = "── 作弊开关 ──",
                        ForeColor = Color.DimGray,
                        Font = new Font("Microsoft YaHei", 8.5F, FontStyle.Bold),
                        AutoSize = true,
                        Margin = new Padding(0, 2, 0, 0),
                    });
                }

                var box = new CheckBox
                {
                    Text = DebugConfig.KeyNames[i] + "    " + DebugConfig.KeyTexts[i],
                    AutoSize = true,
                    Margin = new Padding(0, 6, 0, 0),
                    Tag = i,
                };
                AddRow(box);
                _boxes[i] = box;
            }

            // 保存/放弃：嵌套 Grid 两列 AutoSize（按按钮内容定宽），Anchor=None 居中
            var saveButton = new Button
            {
                Text = "保存设置",
                AutoSize = true,
                Margin = new Padding(0, 18, 8, 0),
            };
            saveButton.Click += (s, e) => Save();
            var cancelButton = new Button
            {
                Text = "放弃更改",
                AutoSize = true,
                Margin = new Padding(8, 18, 0, 0),
            };
            cancelButton.Click += (s, e) => Close();

            var buttonGrid = new TableLayoutPanel
            {
                ColumnCount = 2,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Margin = new Padding(0),
                Anchor = AnchorStyles.None,
            };
            buttonGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            buttonGrid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            buttonGrid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            buttonGrid.Controls.Add(saveButton, 0, 0);
            buttonGrid.Controls.Add(cancelButton, 1, 0);
            AddRow(buttonGrid);

            AddRow(new Label
            {
                Text = "只修改数值本身，注释与其它行原样保留；\n缺失的键会被禁用且不会写入文件。",
                ForeColor = Color.DimGray,
                Font = new Font("Microsoft YaHei", 8.5F),
                AutoSize = true,
                Margin = new Padding(0, 10, 0, 0),
            });

            Load += (s, e) => LoadConfig();
            FormClosing += (s, e) =>
            {
                if (DialogResult == DialogResult.OK) return;
                if (MessageBox.Show("你还未保存设置，确定放弃更改？", "提示",
                    MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK)
                    e.Cancel = true;
            };
        }

        private void LoadConfig()
        {
            var entries = new DebugConfig.Entry[DebugConfig.KeyCount];
            if (!DebugConfig.TryRead(_configPath, entries))
            {
                MessageBox.Show("无法读取 _debug.txt（文件不存在或被占用）。",
                    "_debug.txt 出错", MessageBoxButtons.OK, MessageBoxIcon.Error);
                BeginInvoke(new MethodInvoker(Close));
                return;
            }

            for (int i = 0; i < DebugConfig.KeyCount; i++)
            {
                if (!entries[i].Valid)
                {
                    _boxes[i].Enabled = false;
                    _boxes[i].Checked = false;
                    continue;
                }
                _boxes[i].Enabled = true;
                _boxes[i].Checked = entries[i].Value != 0;
            }
        }

        private void Save()
        {
            var entries = new DebugConfig.Entry[DebugConfig.KeyCount];
            for (int i = 0; i < DebugConfig.KeyCount; i++)
            {
                if (!_boxes[i].Enabled) continue;
                entries[i].Valid = true;
                entries[i].Value = _boxes[i].Checked ? 1 : 0;
            }

            if (!DebugConfig.TryWrite(_configPath, entries))
            {
                MessageBox.Show("写入 _debug.txt 失败（文件可能被占用或只读）。",
                    "_debug.txt 出错", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            MessageBox.Show("设置保存成功!", "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
            DialogResult = DialogResult.OK;
            Close();
        }
    }
}
