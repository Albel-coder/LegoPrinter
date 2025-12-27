namespace WindowsForms
{
    partial class DeviceUserControl
    {
        /// <summary> 
        /// Обязательная переменная конструктора.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary> 
        /// Освободить все используемые ресурсы.
        /// </summary>
        /// <param name="disposing">истинно, если управляемый ресурс должен быть удален; иначе ложно.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Код, автоматически созданный конструктором компонентов

        /// <summary> 
        /// Требуемый метод для поддержки конструктора — не изменяйте 
        /// содержимое этого метода с помощью редактора кода.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.panelConnection = new System.Windows.Forms.Panel();
            this.labelBattery = new System.Windows.Forms.Label();
            this.panelConnectionHead = new System.Windows.Forms.Panel();
            this.labelConnectionHead = new System.Windows.Forms.Label();
            this.connectButton = new System.Windows.Forms.Button();
            this.panelConsole = new System.Windows.Forms.Panel();
            this.textBoxConsole = new System.Windows.Forms.TextBox();
            this.panel2 = new System.Windows.Forms.Panel();
            this.labelConsoleHead = new System.Windows.Forms.Label();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.panelTool = new System.Windows.Forms.Panel();
            this.panelToolHead = new System.Windows.Forms.Panel();
            this.labelTool = new System.Windows.Forms.Label();
            this.logTimer = new System.Windows.Forms.Timer(this.components);
            this.panelConnection.SuspendLayout();
            this.panelConnectionHead.SuspendLayout();
            this.panelConsole.SuspendLayout();
            this.panel2.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.panelTool.SuspendLayout();
            this.panelToolHead.SuspendLayout();
            this.SuspendLayout();
            // 
            // panelConnection
            // 
            this.panelConnection.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panelConnection.Controls.Add(this.labelBattery);
            this.panelConnection.Controls.Add(this.panelConnectionHead);
            this.panelConnection.Controls.Add(this.connectButton);
            this.panelConnection.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelConnection.Location = new System.Drawing.Point(3, 3);
            this.panelConnection.Name = "panelConnection";
            this.panelConnection.Size = new System.Drawing.Size(427, 132);
            this.panelConnection.TabIndex = 0;
            // 
            // labelBattery
            // 
            this.labelBattery.Image = global::WindowsForms.Properties.Resources.batteryImage32x32;
            this.labelBattery.Location = new System.Drawing.Point(158, 56);
            this.labelBattery.Name = "labelBattery";
            this.labelBattery.Size = new System.Drawing.Size(53, 32);
            this.labelBattery.TabIndex = 2;
            this.labelBattery.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // panelConnectionHead
            // 
            this.panelConnectionHead.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panelConnectionHead.Controls.Add(this.labelConnectionHead);
            this.panelConnectionHead.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelConnectionHead.ForeColor = System.Drawing.SystemColors.Info;
            this.panelConnectionHead.Location = new System.Drawing.Point(0, 0);
            this.panelConnectionHead.Name = "panelConnectionHead";
            this.panelConnectionHead.Size = new System.Drawing.Size(427, 38);
            this.panelConnectionHead.TabIndex = 1;
            // 
            // labelConnectionHead
            // 
            this.labelConnectionHead.Dock = System.Windows.Forms.DockStyle.Left;
            this.labelConnectionHead.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.labelConnectionHead.Image = global::WindowsForms.Properties.Resources.connectionPanelImage32x32;
            this.labelConnectionHead.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.labelConnectionHead.Location = new System.Drawing.Point(0, 0);
            this.labelConnectionHead.Name = "labelConnectionHead";
            this.labelConnectionHead.Size = new System.Drawing.Size(139, 38);
            this.labelConnectionHead.TabIndex = 2;
            this.labelConnectionHead.Text = " Connection";
            this.labelConnectionHead.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // connectButton
            // 
            this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
            this.connectButton.FlatAppearance.BorderSize = 0;
            this.connectButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.connectButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(255)))));
            this.connectButton.Location = new System.Drawing.Point(13, 56);
            this.connectButton.Name = "connectButton";
            this.connectButton.Size = new System.Drawing.Size(126, 32);
            this.connectButton.TabIndex = 0;
            this.connectButton.Text = "connect";
            this.connectButton.UseVisualStyleBackColor = false;
            this.connectButton.Click += new System.EventHandler(this.connectButton_Click);
            // 
            // panelConsole
            // 
            this.panelConsole.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panelConsole.Controls.Add(this.textBoxConsole);
            this.panelConsole.Controls.Add(this.panel2);
            this.panelConsole.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panelConsole.Location = new System.Drawing.Point(442, 3);
            this.panelConsole.Name = "panelConsole";
            this.panelConsole.Size = new System.Drawing.Size(721, 344);
            this.panelConsole.TabIndex = 2;
            // 
            // textBoxConsole
            // 
            this.textBoxConsole.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.textBoxConsole.Dock = System.Windows.Forms.DockStyle.Fill;
            this.textBoxConsole.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(150)))), ((int)(((byte)(150)))), ((int)(((byte)(150)))));
            this.textBoxConsole.Location = new System.Drawing.Point(0, 38);
            this.textBoxConsole.Multiline = true;
            this.textBoxConsole.Name = "textBoxConsole";
            this.textBoxConsole.ReadOnly = true;
            this.textBoxConsole.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.textBoxConsole.Size = new System.Drawing.Size(721, 306);
            this.textBoxConsole.TabIndex = 2;
            // 
            // panel2
            // 
            this.panel2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panel2.Controls.Add(this.labelConsoleHead);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel2.ForeColor = System.Drawing.SystemColors.Info;
            this.panel2.Location = new System.Drawing.Point(0, 0);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(721, 38);
            this.panel2.TabIndex = 1;
            // 
            // labelConsoleHead
            // 
            this.labelConsoleHead.Dock = System.Windows.Forms.DockStyle.Left;
            this.labelConsoleHead.Image = global::WindowsForms.Properties.Resources.consoleImage32x32;
            this.labelConsoleHead.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.labelConsoleHead.Location = new System.Drawing.Point(0, 0);
            this.labelConsoleHead.Name = "labelConsoleHead";
            this.labelConsoleHead.Size = new System.Drawing.Size(135, 38);
            this.labelConsoleHead.TabIndex = 3;
            this.labelConsoleHead.Text = " Console";
            this.labelConsoleHead.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 2;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 37.65009F));
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 62.34991F));
            this.tableLayoutPanel1.Controls.Add(this.panelConsole, 1, 0);
            this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 0, 0);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 47.56097F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 52.43903F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(1166, 738);
            this.tableLayoutPanel1.TabIndex = 3;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.ColumnCount = 1;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.Controls.Add(this.panelTool, 0, 1);
            this.tableLayoutPanel2.Controls.Add(this.panelConnection, 0, 0);
            this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel2.Location = new System.Drawing.Point(3, 3);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 2;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 40.86956F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 59.13044F));
            this.tableLayoutPanel2.Size = new System.Drawing.Size(433, 344);
            this.tableLayoutPanel2.TabIndex = 3;
            // 
            // panelTool
            // 
            this.panelTool.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panelTool.Controls.Add(this.panelToolHead);
            this.panelTool.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelTool.Location = new System.Drawing.Point(3, 143);
            this.panelTool.Name = "panelTool";
            this.panelTool.Size = new System.Drawing.Size(427, 132);
            this.panelTool.TabIndex = 4;
            // 
            // panelToolHead
            // 
            this.panelToolHead.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panelToolHead.Controls.Add(this.labelTool);
            this.panelToolHead.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelToolHead.ForeColor = System.Drawing.SystemColors.Info;
            this.panelToolHead.Location = new System.Drawing.Point(0, 0);
            this.panelToolHead.Name = "panelToolHead";
            this.panelToolHead.Size = new System.Drawing.Size(427, 38);
            this.panelToolHead.TabIndex = 1;
            // 
            // labelTool
            // 
            this.labelTool.Dock = System.Windows.Forms.DockStyle.Left;
            this.labelTool.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.labelTool.Image = global::WindowsForms.Properties.Resources.toolImage32x32;
            this.labelTool.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.labelTool.Location = new System.Drawing.Point(0, 0);
            this.labelTool.Name = "labelTool";
            this.labelTool.Size = new System.Drawing.Size(106, 38);
            this.labelTool.TabIndex = 2;
            this.labelTool.Text = " Tool";
            this.labelTool.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // logTimer
            // 
            this.logTimer.Tick += new System.EventHandler(this.logTimer_Tick);
            // 
            // DeviceUserControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoScroll = true;
            this.Controls.Add(this.tableLayoutPanel1);
            this.Name = "DeviceUserControl";
            this.Size = new System.Drawing.Size(1166, 738);
            this.panelConnection.ResumeLayout(false);
            this.panelConnectionHead.ResumeLayout(false);
            this.panelConsole.ResumeLayout(false);
            this.panelConsole.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel2.ResumeLayout(false);
            this.panelTool.ResumeLayout(false);
            this.panelToolHead.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelConnection;
        private System.Windows.Forms.Panel panelConnectionHead;
        private System.Windows.Forms.Label labelConnectionHead;
        private System.Windows.Forms.Button connectButton;
        private System.Windows.Forms.Panel panelConsole;
        private System.Windows.Forms.TextBox textBoxConsole;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.Label labelConsoleHead;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
        private System.Windows.Forms.Panel panelTool;
        private System.Windows.Forms.Panel panelToolHead;
        private System.Windows.Forms.Label labelTool;
        private System.Windows.Forms.Label labelBattery;
        private System.Windows.Forms.Timer logTimer;
    }
}
