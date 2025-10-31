namespace WindowsForms
{
    partial class MainForm
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

        #region Код, автоматически созданный конструктором форм Windows

        /// <summary>
        /// Требуемый метод для поддержки конструктора — не изменяйте 
        /// содержимое этого метода с помощью редактора кода.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            this.MainTabControl = new System.Windows.Forms.TabControl();
            this.Generate = new System.Windows.Forms.TabPage();
            this.Device = new System.Windows.Forms.TabPage();
            this.ExecuteGcodeButton = new System.Windows.Forms.Button();
            this.LogTextBox = new System.Windows.Forms.TextBox();
            this.ReadConfigButton = new System.Windows.Forms.Button();
            this.DisconnectButton = new System.Windows.Forms.Button();
            this.ConnectButton = new System.Windows.Forms.Button();
            this.ForceMoveLeft_Button = new System.Windows.Forms.Button();
            this.ForceMoveUp_Button = new System.Windows.Forms.Button();
            this.ForceMoveRight_Button = new System.Windows.Forms.Button();
            this.ForceMoveDown_Button = new System.Windows.Forms.Button();
            this.HomeButton = new System.Windows.Forms.Button();
            this.MainImageList = new System.Windows.Forms.ImageList(this.components);
            this.LogTimer = new System.Windows.Forms.Timer(this.components);
            this.StatusLabel = new System.Windows.Forms.Label();
            this.InterpreterTexBox = new System.Windows.Forms.TextBox();
            this.MainTabControl.SuspendLayout();
            this.Device.SuspendLayout();
            this.SuspendLayout();
            // 
            // MainTabControl
            // 
            this.MainTabControl.Controls.Add(this.Generate);
            this.MainTabControl.Controls.Add(this.Device);
            this.MainTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.MainTabControl.Location = new System.Drawing.Point(0, 0);
            this.MainTabControl.Name = "MainTabControl";
            this.MainTabControl.SelectedIndex = 0;
            this.MainTabControl.Size = new System.Drawing.Size(867, 572);
            this.MainTabControl.TabIndex = 0;
            // 
            // Generate
            // 
            this.Generate.AutoScroll = true;
            this.Generate.Location = new System.Drawing.Point(4, 25);
            this.Generate.Name = "Generate";
            this.Generate.Padding = new System.Windows.Forms.Padding(3);
            this.Generate.Size = new System.Drawing.Size(859, 543);
            this.Generate.TabIndex = 0;
            this.Generate.Text = "Generate";
            this.Generate.UseVisualStyleBackColor = true;
            // 
            // Device
            // 
            this.Device.AutoScroll = true;
            this.Device.Controls.Add(this.InterpreterTexBox);
            this.Device.Controls.Add(this.StatusLabel);
            this.Device.Controls.Add(this.ExecuteGcodeButton);
            this.Device.Controls.Add(this.LogTextBox);
            this.Device.Controls.Add(this.ReadConfigButton);
            this.Device.Controls.Add(this.DisconnectButton);
            this.Device.Controls.Add(this.ConnectButton);
            this.Device.Controls.Add(this.ForceMoveLeft_Button);
            this.Device.Controls.Add(this.ForceMoveUp_Button);
            this.Device.Controls.Add(this.ForceMoveRight_Button);
            this.Device.Controls.Add(this.ForceMoveDown_Button);
            this.Device.Controls.Add(this.HomeButton);
            this.Device.Location = new System.Drawing.Point(4, 25);
            this.Device.Name = "Device";
            this.Device.Padding = new System.Windows.Forms.Padding(3);
            this.Device.Size = new System.Drawing.Size(859, 543);
            this.Device.TabIndex = 1;
            this.Device.Text = "Device";
            this.Device.UseVisualStyleBackColor = true;
            // 
            // ExecuteGcodeButton
            // 
            this.ExecuteGcodeButton.Location = new System.Drawing.Point(9, 280);
            this.ExecuteGcodeButton.Name = "ExecuteGcodeButton";
            this.ExecuteGcodeButton.Size = new System.Drawing.Size(161, 61);
            this.ExecuteGcodeButton.TabIndex = 9;
            this.ExecuteGcodeButton.Text = "Run";
            this.ExecuteGcodeButton.UseVisualStyleBackColor = true;
            // 
            // LogTextBox
            // 
            this.LogTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.LogTextBox.Location = new System.Drawing.Point(569, 6);
            this.LogTextBox.Multiline = true;
            this.LogTextBox.Name = "LogTextBox";
            this.LogTextBox.ReadOnly = true;
            this.LogTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.LogTextBox.Size = new System.Drawing.Size(256, 497);
            this.LogTextBox.TabIndex = 8;
            // 
            // ReadConfigButton
            // 
            this.ReadConfigButton.Location = new System.Drawing.Point(8, 199);
            this.ReadConfigButton.Name = "ReadConfigButton";
            this.ReadConfigButton.Size = new System.Drawing.Size(99, 38);
            this.ReadConfigButton.TabIndex = 7;
            this.ReadConfigButton.Text = "ReadConfig";
            this.ReadConfigButton.UseVisualStyleBackColor = true;
            // 
            // DisconnectButton
            // 
            this.DisconnectButton.Location = new System.Drawing.Point(8, 148);
            this.DisconnectButton.Name = "DisconnectButton";
            this.DisconnectButton.Size = new System.Drawing.Size(98, 35);
            this.DisconnectButton.TabIndex = 6;
            this.DisconnectButton.Text = "Disconnect";
            this.DisconnectButton.UseVisualStyleBackColor = true;
            this.DisconnectButton.Click += new System.EventHandler(this.DisconnectButton_Click);
            // 
            // ConnectButton
            // 
            this.ConnectButton.Location = new System.Drawing.Point(8, 97);
            this.ConnectButton.Name = "ConnectButton";
            this.ConnectButton.Size = new System.Drawing.Size(98, 35);
            this.ConnectButton.TabIndex = 5;
            this.ConnectButton.Text = "Connect";
            this.ConnectButton.UseVisualStyleBackColor = true;
            this.ConnectButton.Click += new System.EventHandler(this.ConnectButton_Click);
            // 
            // ForceMoveLeft_Button
            // 
            this.ForceMoveLeft_Button.Location = new System.Drawing.Point(119, 153);
            this.ForceMoveLeft_Button.Name = "ForceMoveLeft_Button";
            this.ForceMoveLeft_Button.Size = new System.Drawing.Size(30, 30);
            this.ForceMoveLeft_Button.TabIndex = 4;
            this.ForceMoveLeft_Button.Text = "←";
            this.ForceMoveLeft_Button.UseVisualStyleBackColor = true;
            // 
            // ForceMoveUp_Button
            // 
            this.ForceMoveUp_Button.Location = new System.Drawing.Point(155, 117);
            this.ForceMoveUp_Button.Name = "ForceMoveUp_Button";
            this.ForceMoveUp_Button.Size = new System.Drawing.Size(30, 30);
            this.ForceMoveUp_Button.TabIndex = 3;
            this.ForceMoveUp_Button.Text = "↑";
            this.ForceMoveUp_Button.UseVisualStyleBackColor = true;
            // 
            // ForceMoveRight_Button
            // 
            this.ForceMoveRight_Button.Location = new System.Drawing.Point(191, 153);
            this.ForceMoveRight_Button.Name = "ForceMoveRight_Button";
            this.ForceMoveRight_Button.Size = new System.Drawing.Size(30, 30);
            this.ForceMoveRight_Button.TabIndex = 2;
            this.ForceMoveRight_Button.Text = "→";
            this.ForceMoveRight_Button.UseVisualStyleBackColor = true;
            // 
            // ForceMoveDown_Button
            // 
            this.ForceMoveDown_Button.Location = new System.Drawing.Point(155, 189);
            this.ForceMoveDown_Button.Name = "ForceMoveDown_Button";
            this.ForceMoveDown_Button.Size = new System.Drawing.Size(30, 30);
            this.ForceMoveDown_Button.TabIndex = 1;
            this.ForceMoveDown_Button.Text = "↓";
            this.ForceMoveDown_Button.UseVisualStyleBackColor = true;
            // 
            // HomeButton
            // 
            this.HomeButton.Location = new System.Drawing.Point(155, 153);
            this.HomeButton.Name = "HomeButton";
            this.HomeButton.Size = new System.Drawing.Size(30, 30);
            this.HomeButton.TabIndex = 0;
            this.HomeButton.Text = "H";
            this.HomeButton.UseVisualStyleBackColor = true;
            // 
            // MainImageList
            // 
            this.MainImageList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("MainImageList.ImageStream")));
            this.MainImageList.TransparentColor = System.Drawing.Color.Transparent;
            this.MainImageList.Images.SetKeyName(0, "Logo.jpg");
            // 
            // StatusLabel
            // 
            this.StatusLabel.AutoSize = true;
            this.StatusLabel.Location = new System.Drawing.Point(199, 477);
            this.StatusLabel.Name = "StatusLabel";
            this.StatusLabel.Size = new System.Drawing.Size(44, 16);
            this.StatusLabel.TabIndex = 10;
            this.StatusLabel.Text = "label1";
            // 
            // InterpreterTexBox
            // 
            this.InterpreterTexBox.Location = new System.Drawing.Point(298, 6);
            this.InterpreterTexBox.Multiline = true;
            this.InterpreterTexBox.Name = "InterpreterTexBox";
            this.InterpreterTexBox.ReadOnly = true;
            this.InterpreterTexBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.InterpreterTexBox.Size = new System.Drawing.Size(256, 497);
            this.InterpreterTexBox.TabIndex = 11;
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(867, 572);
            this.Controls.Add(this.MainTabControl);
            this.Name = "MainForm";
            this.Text = "LP Studio 1.0";
            this.WindowState = System.Windows.Forms.FormWindowState.Maximized;
            this.MainTabControl.ResumeLayout(false);
            this.Device.ResumeLayout(false);
            this.Device.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TabControl MainTabControl;
        private System.Windows.Forms.TabPage Generate;
        private System.Windows.Forms.ImageList MainImageList;
        private System.Windows.Forms.TabPage Device;
        private System.Windows.Forms.Button HomeButton;
        private System.Windows.Forms.Button ForceMoveLeft_Button;
        private System.Windows.Forms.Button ForceMoveUp_Button;
        private System.Windows.Forms.Button ForceMoveRight_Button;
        private System.Windows.Forms.Button ForceMoveDown_Button;
        private System.Windows.Forms.Button ConnectButton;
        private System.Windows.Forms.Button DisconnectButton;
        private System.Windows.Forms.Button ReadConfigButton;
        private System.Windows.Forms.TextBox LogTextBox;
        private System.Windows.Forms.Button ExecuteGcodeButton;
        private System.Windows.Forms.Timer LogTimer;
        private System.Windows.Forms.Label StatusLabel;
        private System.Windows.Forms.TextBox InterpreterTexBox;
    }
}

