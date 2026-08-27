namespace LPStudio
{
    partial class CalibrationUserControl
    {
        /// <summary> 
        /// Required constructor variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary> 
        /// Release all used resources.
        /// </summary>
        /// <param name="disposing">true if the managed resource should be disposed; otherwise, false.</param>
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
        /// Required method for constructor support - do not modify 
        /// the contents of this method using a code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.tableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanelMainMenu = new System.Windows.Forms.TableLayoutPanel();
            this.panelPrinter = new System.Windows.Forms.Panel();
            this.button1 = new System.Windows.Forms.Button();
            this.panelPrinterHead = new System.Windows.Forms.Panel();
            this.labelPrinterHead = new System.Windows.Forms.Label();
            this.tableLayoutPanel.SuspendLayout();
            this.tableLayoutPanelMainMenu.SuspendLayout();
            this.panelPrinter.SuspendLayout();
            this.panelPrinterHead.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel
            // 
            this.tableLayoutPanel.ColumnCount = 2;
            this.tableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 21.12403F));
            this.tableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 78.87597F));
            this.tableLayoutPanel.Controls.Add(this.tableLayoutPanelMainMenu, 0, 0);
            this.tableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel.Name = "tableLayoutPanel";
            this.tableLayoutPanel.RowCount = 1;
            this.tableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 257F));
            this.tableLayoutPanel.Size = new System.Drawing.Size(1032, 724);
            this.tableLayoutPanel.TabIndex = 0;
            this.tableLayoutPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.tableLayoutPanel1_Paint);
            // 
            // tableLayoutPanelMainMenu
            // 
            this.tableLayoutPanelMainMenu.ColumnCount = 1;
            this.tableLayoutPanelMainMenu.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanelMainMenu.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanelMainMenu.Controls.Add(this.panelPrinter, 0, 0);
            this.tableLayoutPanelMainMenu.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanelMainMenu.Location = new System.Drawing.Point(3, 3);
            this.tableLayoutPanelMainMenu.Name = "tableLayoutPanelMainMenu";
            this.tableLayoutPanelMainMenu.RowCount = 2;
            this.tableLayoutPanelMainMenu.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 82.7298F));
            this.tableLayoutPanelMainMenu.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 17.2702F));
            this.tableLayoutPanelMainMenu.Size = new System.Drawing.Size(211, 718);
            this.tableLayoutPanelMainMenu.TabIndex = 0;
            // 
            // panelPrinter
            // 
            this.panelPrinter.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panelPrinter.Controls.Add(this.button1);
            this.panelPrinter.Controls.Add(this.panelPrinterHead);
            this.panelPrinter.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelPrinter.Location = new System.Drawing.Point(3, 3);
            this.panelPrinter.Name = "panelPrinter";
            this.panelPrinter.Size = new System.Drawing.Size(205, 227);
            this.panelPrinter.TabIndex = 0;
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(42, 62);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(97, 42);
            this.button1.TabIndex = 1;
            this.button1.Text = "button1";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // panelPrinterHead
            // 
            this.panelPrinterHead.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panelPrinterHead.Controls.Add(this.labelPrinterHead);
            this.panelPrinterHead.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelPrinterHead.Location = new System.Drawing.Point(0, 0);
            this.panelPrinterHead.Name = "panelPrinterHead";
            this.panelPrinterHead.Size = new System.Drawing.Size(205, 43);
            this.panelPrinterHead.TabIndex = 0;
            // 
            // labelPrinterHead
            // 
            this.labelPrinterHead.ForeColor = System.Drawing.SystemColors.Info;
            this.labelPrinterHead.Location = new System.Drawing.Point(3, 0);
            this.labelPrinterHead.Name = "labelPrinterHead";
            this.labelPrinterHead.Size = new System.Drawing.Size(108, 40);
            this.labelPrinterHead.TabIndex = 0;
            this.labelPrinterHead.Text = "Printer";
            this.labelPrinterHead.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.labelPrinterHead.Click += new System.EventHandler(this.labelPrinterHead_Click);
            // 
            // CalibrationUserControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.tableLayoutPanel);
            this.Name = "CalibrationUserControl";
            this.Size = new System.Drawing.Size(1032, 724);
            this.tableLayoutPanel.ResumeLayout(false);
            this.tableLayoutPanelMainMenu.ResumeLayout(false);
            this.panelPrinter.ResumeLayout(false);
            this.panelPrinterHead.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanelMainMenu;
        private System.Windows.Forms.Panel panelPrinter;
        private System.Windows.Forms.Panel panelPrinterHead;
        private System.Windows.Forms.Label labelPrinterHead;
        private System.Windows.Forms.Button button1;
    }
}
