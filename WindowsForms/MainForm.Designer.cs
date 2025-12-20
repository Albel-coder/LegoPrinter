using System.Drawing;

namespace WindowsForms
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
            this.panelMenu = new System.Windows.Forms.Panel();
            this.panelSelect = new System.Windows.Forms.Panel();
            this.contentPanel = new System.Windows.Forms.Panel();
            this.buttonDevice = new System.Windows.Forms.Button();
            this.buttonPreview = new System.Windows.Forms.Button();
            this.buttonPrepare = new System.Windows.Forms.Button();
            this.buttonCalibration = new System.Windows.Forms.Button();
            this.panelSelect.SuspendLayout();
            this.SuspendLayout();
            // 
            // panelMenu
            // 
            this.panelMenu.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(30)))), ((int)(((byte)(33)))), ((int)(((byte)(48)))));
            this.panelMenu.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelMenu.Location = new System.Drawing.Point(0, 0);
            this.panelMenu.Name = "panelMenu";
            this.panelMenu.Size = new System.Drawing.Size(1176, 36);
            this.panelMenu.TabIndex = 0;
            // 
            // panelSelect
            // 
            this.panelSelect.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(36)))), ((int)(((byte)(40)))), ((int)(((byte)(59)))));
            this.panelSelect.Controls.Add(this.buttonCalibration);
            this.panelSelect.Controls.Add(this.buttonDevice);
            this.panelSelect.Controls.Add(this.buttonPreview);
            this.panelSelect.Controls.Add(this.buttonPrepare);
            this.panelSelect.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelSelect.Location = new System.Drawing.Point(0, 36);
            this.panelSelect.Name = "panelSelect";
            this.panelSelect.Size = new System.Drawing.Size(1176, 41);
            this.panelSelect.TabIndex = 1;
            // 
            // contentPanel
            // 
            this.contentPanel.Location = new System.Drawing.Point(299, 263);
            this.contentPanel.Name = "contentPanel";
            this.contentPanel.Size = new System.Drawing.Size(708, 312);
            this.contentPanel.TabIndex = 2;
            // 
            // buttonDevice
            // 
            this.buttonDevice.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonDevice.FlatAppearance.BorderSize = 0;
            this.buttonDevice.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonDevice.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonDevice.Image = global::WindowsForms.Properties.Resources.deviceImage32x32;
            this.buttonDevice.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonDevice.Location = new System.Drawing.Point(320, 0);
            this.buttonDevice.Name = "buttonDevice";
            this.buttonDevice.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonDevice.Size = new System.Drawing.Size(160, 41);
            this.buttonDevice.TabIndex = 5;
            this.buttonDevice.Text = " Device";
            this.buttonDevice.UseVisualStyleBackColor = true;
            // 
            // buttonPreview
            // 
            this.buttonPreview.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonPreview.FlatAppearance.BorderSize = 0;
            this.buttonPreview.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonPreview.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonPreview.Image = global::WindowsForms.Properties.Resources.previewIcon;
            this.buttonPreview.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonPreview.Location = new System.Drawing.Point(160, 0);
            this.buttonPreview.Name = "buttonPreview";
            this.buttonPreview.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonPreview.Size = new System.Drawing.Size(160, 41);
            this.buttonPreview.TabIndex = 4;
            this.buttonPreview.Text = " Preview";
            this.buttonPreview.UseVisualStyleBackColor = true;
            // 
            // buttonPrepare
            // 
            this.buttonPrepare.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonPrepare.FlatAppearance.BorderSize = 0;
            this.buttonPrepare.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonPrepare.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonPrepare.Image = global::WindowsForms.Properties.Resources.prepareImag32x32;
            this.buttonPrepare.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonPrepare.Location = new System.Drawing.Point(0, 0);
            this.buttonPrepare.Name = "buttonPrepare";
            this.buttonPrepare.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonPrepare.Size = new System.Drawing.Size(160, 41);
            this.buttonPrepare.TabIndex = 3;
            this.buttonPrepare.Text = " Prepare";
            this.buttonPrepare.UseVisualStyleBackColor = true;
            // 
            // buttonCalibration
            // 
            this.buttonCalibration.Dock = System.Windows.Forms.DockStyle.Left;
            this.buttonCalibration.FlatAppearance.BorderSize = 0;
            this.buttonCalibration.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonCalibration.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonCalibration.Image = global::WindowsForms.Properties.Resources.calibrationImage32x32;
            this.buttonCalibration.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonCalibration.Location = new System.Drawing.Point(480, 0);
            this.buttonCalibration.Name = "buttonCalibration";
            this.buttonCalibration.Padding = new System.Windows.Forms.Padding(20, 0, 0, 0);
            this.buttonCalibration.Size = new System.Drawing.Size(160, 41);
            this.buttonCalibration.TabIndex = 6;
            this.buttonCalibration.Text = " Calibration";
            this.buttonCalibration.UseVisualStyleBackColor = true;
            // 
            // MainForm
            // 
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.None;
            this.AutoSize = true;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(46)))), ((int)(((byte)(51)))), ((int)(((byte)(73)))));
            this.ClientSize = new System.Drawing.Size(1176, 723);
            this.Controls.Add(this.contentPanel);
            this.Controls.Add(this.panelSelect);
            this.Controls.Add(this.panelMenu);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "LP Studio 1.0";
            this.panelSelect.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelMenu;
        private System.Windows.Forms.Panel panelSelect;
        private System.Windows.Forms.Panel contentPanel;
        private System.Windows.Forms.Button buttonPrepare;
        private System.Windows.Forms.Button buttonPreview;
        private System.Windows.Forms.Button buttonDevice;
        private System.Windows.Forms.Button buttonCalibration;
    }
}

