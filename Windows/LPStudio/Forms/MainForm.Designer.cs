using System.Drawing;

namespace LPStudio
{
    partial class MainForm
    {
        /// <summary>
        /// Required constructor variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Release all used resources.
        /// </summary>
        /// <param name="disposing">true if the managed resource should be deleted; otherwise false.</param>
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
        /// Required constructor support method - do not change 
        /// requires this method using the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            this.panelMenu = new System.Windows.Forms.Panel();
            this.minimizeWindowButton = new System.Windows.Forms.Button();
            this.restoreWindowButton = new System.Windows.Forms.Button();
            this.exitButton = new System.Windows.Forms.Button();
            this.panelSelect = new System.Windows.Forms.Panel();
            this.buttonCalibration = new System.Windows.Forms.Button();
            this.buttonDevice = new System.Windows.Forms.Button();
            this.buttonPreview = new System.Windows.Forms.Button();
            this.buttonPrepare = new System.Windows.Forms.Button();
            this.buttonHome = new System.Windows.Forms.Button();
            this.contentPanel = new System.Windows.Forms.Panel();
            this.panelMenu.SuspendLayout();
            this.panelSelect.SuspendLayout();
            this.SuspendLayout();
            // 
            // panelMenu
            // 
            this.panelMenu.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(30)))), ((int)(((byte)(33)))), ((int)(((byte)(48)))));
            this.panelMenu.Controls.Add(this.minimizeWindowButton);
            this.panelMenu.Controls.Add(this.restoreWindowButton);
            this.panelMenu.Controls.Add(this.exitButton);
            this.panelMenu.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelMenu.Location = new System.Drawing.Point(0, 0);
            this.panelMenu.Name = "panelMenu";
            this.panelMenu.Size = new System.Drawing.Size(1176, 36);
            this.panelMenu.TabIndex = 0;
            // 
            // minimizeWindowButton
            // 
            this.minimizeWindowButton.Dock = System.Windows.Forms.DockStyle.Right;
            this.minimizeWindowButton.FlatAppearance.BorderSize = 0;
            this.minimizeWindowButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.minimizeWindowButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.minimizeWindowButton.Image = global::LPStudio.Properties.Resources.minimizeWindowImage32x32;
            this.minimizeWindowButton.Location = new System.Drawing.Point(1005, 0);
            this.minimizeWindowButton.Name = "minimizeWindowButton";
            this.minimizeWindowButton.Size = new System.Drawing.Size(57, 36);
            this.minimizeWindowButton.TabIndex = 2;
            this.minimizeWindowButton.UseVisualStyleBackColor = true;
            this.minimizeWindowButton.Click += new System.EventHandler(this.minimizeWindowButton_Click);
            // 
            // restoreWindowButton
            // 
            this.restoreWindowButton.Dock = System.Windows.Forms.DockStyle.Right;
            this.restoreWindowButton.FlatAppearance.BorderSize = 0;
            this.restoreWindowButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.restoreWindowButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.restoreWindowButton.Image = global::LPStudio.Properties.Resources.maximizeWindowImage32x32;
            this.restoreWindowButton.Location = new System.Drawing.Point(1062, 0);
            this.restoreWindowButton.Name = "restoreWindowButton";
            this.restoreWindowButton.Size = new System.Drawing.Size(57, 36);
            this.restoreWindowButton.TabIndex = 1;
            this.restoreWindowButton.UseVisualStyleBackColor = true;
            this.restoreWindowButton.Click += new System.EventHandler(this.restoreWindowButton_Click);
            // 
            // exitButton
            // 
            this.exitButton.Dock = System.Windows.Forms.DockStyle.Right;
            this.exitButton.FlatAppearance.BorderSize = 0;
            this.exitButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.exitButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.exitButton.Image = global::LPStudio.Properties.Resources.rejectWindow1;
            this.exitButton.Location = new System.Drawing.Point(1119, 0);
            this.exitButton.Name = "exitButton";
            this.exitButton.Size = new System.Drawing.Size(57, 36);
            this.exitButton.TabIndex = 0;
            this.exitButton.UseVisualStyleBackColor = true;
            this.exitButton.Click += new System.EventHandler(this.exitButton_Click);
            // 
            // panelSelect
            // 
            this.panelSelect.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(36)))), ((int)(((byte)(40)))), ((int)(((byte)(59)))));
            this.panelSelect.Controls.Add(this.buttonCalibration);
            this.panelSelect.Controls.Add(this.buttonDevice);
            this.panelSelect.Controls.Add(this.buttonPreview);
            this.panelSelect.Controls.Add(this.buttonPrepare);
            this.panelSelect.Controls.Add(this.buttonHome);
            this.panelSelect.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelSelect.Location = new System.Drawing.Point(0, 36);
            this.panelSelect.Name = "panelSelect";
            this.panelSelect.Size = new System.Drawing.Size(1176, 41);
            this.panelSelect.TabIndex = 1;
            // 
            // buttonCalibration
            // 
            this.buttonCalibration.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonCalibration.FlatAppearance.BorderSize = 0;
            this.buttonCalibration.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonCalibration.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonCalibration.Image = global::LPStudio.Properties.Resources.calibrationImage32x32;
            this.buttonCalibration.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonCalibration.Location = new System.Drawing.Point(563, 0);
            this.buttonCalibration.Name = "buttonCalibration";
            this.buttonCalibration.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonCalibration.Size = new System.Drawing.Size(160, 41);
            this.buttonCalibration.TabIndex = 7;
            this.buttonCalibration.Text = " Calibration";
            this.buttonCalibration.UseVisualStyleBackColor = true;
            this.buttonCalibration.Click += new System.EventHandler(this.buttonCalibration_Click);
            // 
            // buttonDevice
            // 
            this.buttonDevice.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonDevice.FlatAppearance.BorderSize = 0;
            this.buttonDevice.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonDevice.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonDevice.Image = global::LPStudio.Properties.Resources.deviceImage32x32;
            this.buttonDevice.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonDevice.Location = new System.Drawing.Point(403, 0);
            this.buttonDevice.Name = "buttonDevice";
            this.buttonDevice.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonDevice.Size = new System.Drawing.Size(160, 41);
            this.buttonDevice.TabIndex = 6;
            this.buttonDevice.Text = " Device";
            this.buttonDevice.UseVisualStyleBackColor = true;
            this.buttonDevice.Click += new System.EventHandler(this.buttonDevice_Click);
            // 
            // buttonPreview
            // 
            this.buttonPreview.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonPreview.FlatAppearance.BorderSize = 0;
            this.buttonPreview.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonPreview.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonPreview.Image = global::LPStudio.Properties.Resources.previewIcon;
            this.buttonPreview.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonPreview.Location = new System.Drawing.Point(243, 0);
            this.buttonPreview.Name = "buttonPreview";
            this.buttonPreview.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonPreview.Size = new System.Drawing.Size(160, 41);
            this.buttonPreview.TabIndex = 5;
            this.buttonPreview.Text = " Preview";
            this.buttonPreview.UseVisualStyleBackColor = true;
            this.buttonPreview.Click += new System.EventHandler(this.buttonPreview_Click);
            // 
            // buttonPrepare
            // 
            this.buttonPrepare.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonPrepare.FlatAppearance.BorderSize = 0;
            this.buttonPrepare.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonPrepare.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonPrepare.Image = global::LPStudio.Properties.Resources.prepareImag32x32;
            this.buttonPrepare.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonPrepare.Location = new System.Drawing.Point(83, 0);
            this.buttonPrepare.Name = "buttonPrepare";
            this.buttonPrepare.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonPrepare.Size = new System.Drawing.Size(160, 41);
            this.buttonPrepare.TabIndex = 4;
            this.buttonPrepare.Text = " Prepare";
            this.buttonPrepare.UseVisualStyleBackColor = true;
            this.buttonPrepare.Click += new System.EventHandler(this.buttonPrepare_Click);
            // 
            // buttonHome
            // 
            this.buttonHome.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonHome.FlatAppearance.BorderSize = 0;
            this.buttonHome.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonHome.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonHome.Image = global::LPStudio.Properties.Resources.homeImage32x32;
            this.buttonHome.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonHome.Location = new System.Drawing.Point(0, 0);
            this.buttonHome.Name = "buttonHome";
            this.buttonHome.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonHome.Size = new System.Drawing.Size(83, 41);
            this.buttonHome.TabIndex = 3;
            this.buttonHome.Text = " ";
            this.buttonHome.UseVisualStyleBackColor = true;
            this.buttonHome.Click += new System.EventHandler(this.buttonHome_Click);
            // 
            // contentPanel
            // 
            this.contentPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.contentPanel.Location = new System.Drawing.Point(0, 77);
            this.contentPanel.Name = "contentPanel";
            this.contentPanel.Size = new System.Drawing.Size(1176, 646);
            this.contentPanel.TabIndex = 2;
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(46)))), ((int)(((byte)(51)))), ((int)(((byte)(73)))));
            this.ClientSize = new System.Drawing.Size(1176, 723);
            this.Controls.Add(this.contentPanel);
            this.Controls.Add(this.panelSelect);
            this.Controls.Add(this.panelMenu);
            this.ForeColor = System.Drawing.SystemColors.InfoText;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "LP Studio 1.0";
            this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MainForm_MouseDown);
            this.panelMenu.ResumeLayout(false);
            this.panelSelect.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelMenu;
        private System.Windows.Forms.Panel panelSelect;
        private System.Windows.Forms.Panel contentPanel;
        private System.Windows.Forms.Button buttonCalibration;
        private System.Windows.Forms.Button buttonHome;
        private System.Windows.Forms.Button buttonPrepare;
        private System.Windows.Forms.Button buttonPreview;
        private System.Windows.Forms.Button buttonDevice;
        private System.Windows.Forms.Button exitButton;
        private System.Windows.Forms.Button restoreWindowButton;
        private System.Windows.Forms.Button minimizeWindowButton;        
    }
}

