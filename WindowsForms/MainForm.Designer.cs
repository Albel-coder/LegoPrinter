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
            this.ConnectButton = new System.Windows.Forms.Button();
            this.DisconnectButton = new System.Windows.Forms.Button();
            this.Test = new System.Windows.Forms.Button();
            this.MotorTest = new System.Windows.Forms.Button();
            this.LogTextBox = new System.Windows.Forms.TextBox();
            this.LogTimer = new System.Windows.Forms.Timer(this.components);
            this.StatusLabel = new System.Windows.Forms.Label();
            this.LoadConfigButton = new System.Windows.Forms.Button();
            this.InterpreterTexBox = new System.Windows.Forms.TextBox();
            this.ExecuteGcodeButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // ConnectButton
            // 
            this.ConnectButton.Location = new System.Drawing.Point(51, 32);
            this.ConnectButton.Name = "ConnectButton";
            this.ConnectButton.Size = new System.Drawing.Size(104, 71);
            this.ConnectButton.TabIndex = 0;
            this.ConnectButton.Text = "Connect";
            this.ConnectButton.UseVisualStyleBackColor = true;
            this.ConnectButton.Click += new System.EventHandler(this.ConnectButton_Click);
            // 
            // DisconnectButton
            // 
            this.DisconnectButton.Location = new System.Drawing.Point(51, 137);
            this.DisconnectButton.Name = "DisconnectButton";
            this.DisconnectButton.Size = new System.Drawing.Size(104, 79);
            this.DisconnectButton.TabIndex = 1;
            this.DisconnectButton.Text = "Disconnect";
            this.DisconnectButton.UseVisualStyleBackColor = true;
            this.DisconnectButton.Click += new System.EventHandler(this.DisconnectButton_Click);
            // 
            // Test
            // 
            this.Test.Location = new System.Drawing.Point(51, 255);
            this.Test.Name = "Test";
            this.Test.Size = new System.Drawing.Size(104, 84);
            this.Test.TabIndex = 2;
            this.Test.Text = "Test";
            this.Test.UseVisualStyleBackColor = true;
            this.Test.Click += new System.EventHandler(this.Test_Click);
            // 
            // MotorTest
            // 
            this.MotorTest.Location = new System.Drawing.Point(51, 379);
            this.MotorTest.Name = "MotorTest";
            this.MotorTest.Size = new System.Drawing.Size(104, 83);
            this.MotorTest.TabIndex = 3;
            this.MotorTest.Text = "MotorTest";
            this.MotorTest.UseVisualStyleBackColor = true;
            this.MotorTest.Click += new System.EventHandler(this.MotorTest_Click);
            // 
            // LogTextBox
            // 
            this.LogTextBox.Location = new System.Drawing.Point(210, 255);
            this.LogTextBox.Multiline = true;
            this.LogTextBox.Name = "LogTextBox";
            this.LogTextBox.ReadOnly = true;
            this.LogTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.LogTextBox.Size = new System.Drawing.Size(211, 217);
            this.LogTextBox.TabIndex = 4;
            // 
            // LogTimer
            // 
            this.LogTimer.Interval = 500;
            this.LogTimer.Tick += new System.EventHandler(this.LogTimer_Tick);
            // 
            // StatusLabel
            // 
            this.StatusLabel.AutoSize = true;
            this.StatusLabel.Location = new System.Drawing.Point(207, 505);
            this.StatusLabel.Name = "StatusLabel";
            this.StatusLabel.Size = new System.Drawing.Size(50, 16);
            this.StatusLabel.TabIndex = 5;
            this.StatusLabel.Text = "Status: ";
            // 
            // LoadConfigButton
            // 
            this.LoadConfigButton.Location = new System.Drawing.Point(210, 137);
            this.LoadConfigButton.Name = "LoadConfigButton";
            this.LoadConfigButton.Size = new System.Drawing.Size(112, 79);
            this.LoadConfigButton.TabIndex = 7;
            this.LoadConfigButton.Text = "Read config";
            this.LoadConfigButton.UseVisualStyleBackColor = true;
            this.LoadConfigButton.Click += new System.EventHandler(this.LoadConfigButton_Click);
            // 
            // InterpreterTexBox
            // 
            this.InterpreterTexBox.Location = new System.Drawing.Point(446, 32);
            this.InterpreterTexBox.Multiline = true;
            this.InterpreterTexBox.Name = "InterpreterTexBox";
            this.InterpreterTexBox.ReadOnly = true;
            this.InterpreterTexBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.InterpreterTexBox.Size = new System.Drawing.Size(364, 494);
            this.InterpreterTexBox.TabIndex = 8;
            // 
            // ExecuteGcodeButton
            // 
            this.ExecuteGcodeButton.Location = new System.Drawing.Point(210, 32);
            this.ExecuteGcodeButton.Name = "ExecuteGcodeButton";
            this.ExecuteGcodeButton.Size = new System.Drawing.Size(112, 71);
            this.ExecuteGcodeButton.TabIndex = 9;
            this.ExecuteGcodeButton.Text = "Run G-code";
            this.ExecuteGcodeButton.UseVisualStyleBackColor = true;
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(867, 572);
            this.Controls.Add(this.ExecuteGcodeButton);
            this.Controls.Add(this.InterpreterTexBox);
            this.Controls.Add(this.LoadConfigButton);
            this.Controls.Add(this.StatusLabel);
            this.Controls.Add(this.LogTextBox);
            this.Controls.Add(this.MotorTest);
            this.Controls.Add(this.Test);
            this.Controls.Add(this.DisconnectButton);
            this.Controls.Add(this.ConnectButton);
            this.Name = "MainForm";
            this.Text = "Form1";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button ConnectButton;
        private System.Windows.Forms.Button DisconnectButton;
        private System.Windows.Forms.Button Test;
        private System.Windows.Forms.Button MotorTest;
        private System.Windows.Forms.TextBox LogTextBox;
        private System.Windows.Forms.Timer LogTimer;
        private System.Windows.Forms.Label StatusLabel;
        private System.Windows.Forms.Button LoadConfigButton;
        private System.Windows.Forms.TextBox InterpreterTexBox;
        private System.Windows.Forms.Button ExecuteGcodeButton;
    }
}

