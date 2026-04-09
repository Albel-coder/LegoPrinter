namespace LPStudio
{
    partial class DeviceUserControl
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
            this.panel4 = new System.Windows.Forms.Panel();
            this.browseButton = new System.Windows.Forms.Button();
            this.showCodeFile = new System.Windows.Forms.TextBox();
            this.buttonExecuteCode = new System.Windows.Forms.Button();
            this.panel5 = new System.Windows.Forms.Panel();
            this.label4 = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.homeZButton = new System.Windows.Forms.Button();
            this.moveZUpButton = new System.Windows.Forms.Button();
            this.moveZDownButton = new System.Windows.Forms.Button();
            this.homeALLButton = new System.Windows.Forms.Button();
            this.homeXButton = new System.Windows.Forms.Button();
            this.homeYButton = new System.Windows.Forms.Button();
            this.homeXYButton = new System.Windows.Forms.Button();
            this.moveYUpButton = new System.Windows.Forms.Button();
            this.moveXLeftButton = new System.Windows.Forms.Button();
            this.moveYDownButton = new System.Windows.Forms.Button();
            this.moveXRightButton = new System.Windows.Forms.Button();
            this.panel3 = new System.Windows.Forms.Panel();
            this.label2 = new System.Windows.Forms.Label();
            this.logTimer = new System.Windows.Forms.Timer(this.components);
            this.OpenFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.panelConnection.SuspendLayout();
            this.panelConnectionHead.SuspendLayout();
            this.panelConsole.SuspendLayout();
            this.panel2.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.panel4.SuspendLayout();
            this.panel5.SuspendLayout();
            this.panel1.SuspendLayout();
            this.panel3.SuspendLayout();
            this.SuspendLayout();
            // 
            // panelConnection
            // 
            this.panelConnection.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panelConnection.Controls.Add(this.labelBattery);
            this.panelConnection.Controls.Add(this.panelConnectionHead);
            this.panelConnection.Controls.Add(this.connectButton);
            this.panelConnection.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panelConnection.Location = new System.Drawing.Point(3, 3);
            this.panelConnection.Name = "panelConnection";
            this.panelConnection.Size = new System.Drawing.Size(427, 146);
            this.panelConnection.TabIndex = 0;
            // 
            // labelBattery
            // 
            this.labelBattery.Location = new System.Drawing.Point(229, 56);
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
            this.labelConnectionHead.Image = global::LPStudio.Properties.Resources.connectionPanelImage32x32;
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
            this.connectButton.Location = new System.Drawing.Point(13, 50);
            this.connectButton.Name = "connectButton";
            this.connectButton.Size = new System.Drawing.Size(151, 44);
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
            this.panelConsole.Size = new System.Drawing.Size(721, 732);
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
            this.textBoxConsole.Size = new System.Drawing.Size(721, 694);
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
            this.labelConsoleHead.Image = global::LPStudio.Properties.Resources.consoleImage32x32;
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
            this.tableLayoutPanel1.RowCount = 1;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 89.83739F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 10.1626F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(1166, 738);
            this.tableLayoutPanel1.TabIndex = 3;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.ColumnCount = 1;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.Controls.Add(this.panel4, 0, 2);
            this.tableLayoutPanel2.Controls.Add(this.panel1, 0, 1);
            this.tableLayoutPanel2.Controls.Add(this.panelConnection, 0, 0);
            this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel2.Location = new System.Drawing.Point(3, 3);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 3;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 38.98305F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 61.01695F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 341F));
            this.tableLayoutPanel2.Size = new System.Drawing.Size(433, 732);
            this.tableLayoutPanel2.TabIndex = 3;
            // 
            // panel4
            // 
            this.panel4.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panel4.Controls.Add(this.browseButton);
            this.panel4.Controls.Add(this.showCodeFile);
            this.panel4.Controls.Add(this.buttonExecuteCode);
            this.panel4.Controls.Add(this.panel5);
            this.panel4.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel4.Location = new System.Drawing.Point(3, 393);
            this.panel4.Name = "panel4";
            this.panel4.Size = new System.Drawing.Size(427, 123);
            this.panel4.TabIndex = 2;
            // 
            // browseButton
            // 
            this.browseButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.browseButton.FlatAppearance.BorderSize = 0;
            this.browseButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.browseButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.browseButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.browseButton.Location = new System.Drawing.Point(302, 65);
            this.browseButton.Name = "browseButton";
            this.browseButton.Size = new System.Drawing.Size(88, 22);
            this.browseButton.TabIndex = 9;
            this.browseButton.Text = " browse...";
            this.browseButton.UseVisualStyleBackColor = false;
            this.browseButton.Click += new System.EventHandler(this.browseButton_Click);
            // 
            // showCodeFile
            // 
            this.showCodeFile.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.showCodeFile.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(150)))), ((int)(((byte)(150)))), ((int)(((byte)(150)))));
            this.showCodeFile.Location = new System.Drawing.Point(130, 65);
            this.showCodeFile.Name = "showCodeFile";
            this.showCodeFile.ReadOnly = true;
            this.showCodeFile.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.showCodeFile.Size = new System.Drawing.Size(166, 22);
            this.showCodeFile.TabIndex = 8;
            // 
            // buttonExecuteCode
            // 
            this.buttonExecuteCode.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.buttonExecuteCode.FlatAppearance.BorderSize = 0;
            this.buttonExecuteCode.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonExecuteCode.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.buttonExecuteCode.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.buttonExecuteCode.Location = new System.Drawing.Point(13, 58);
            this.buttonExecuteCode.Name = "buttonExecuteCode";
            this.buttonExecuteCode.Size = new System.Drawing.Size(111, 36);
            this.buttonExecuteCode.TabIndex = 7;
            this.buttonExecuteCode.Text = " Execute code";
            this.buttonExecuteCode.UseVisualStyleBackColor = false;
            this.buttonExecuteCode.Click += new System.EventHandler(this.buttonExecuteCode_Click);
            // 
            // panel5
            // 
            this.panel5.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panel5.Controls.Add(this.label4);
            this.panel5.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel5.ForeColor = System.Drawing.SystemColors.Info;
            this.panel5.Location = new System.Drawing.Point(0, 0);
            this.panel5.Name = "panel5";
            this.panel5.Size = new System.Drawing.Size(427, 38);
            this.panel5.TabIndex = 1;
            // 
            // label4
            // 
            this.label4.Dock = System.Windows.Forms.DockStyle.Left;
            this.label4.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.label4.Image = global::LPStudio.Properties.Resources.cubeG_codeImage32x32;
            this.label4.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.label4.Location = new System.Drawing.Point(0, 0);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(139, 38);
            this.label4.TabIndex = 2;
            this.label4.Text = " G-code";
            this.label4.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // panel1
            // 
            this.panel1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(38)))), ((int)(((byte)(41)))));
            this.panel1.Controls.Add(this.homeZButton);
            this.panel1.Controls.Add(this.moveZUpButton);
            this.panel1.Controls.Add(this.moveZDownButton);
            this.panel1.Controls.Add(this.homeALLButton);
            this.panel1.Controls.Add(this.homeXButton);
            this.panel1.Controls.Add(this.homeYButton);
            this.panel1.Controls.Add(this.homeXYButton);
            this.panel1.Controls.Add(this.moveYUpButton);
            this.panel1.Controls.Add(this.moveXLeftButton);
            this.panel1.Controls.Add(this.moveYDownButton);
            this.panel1.Controls.Add(this.moveXRightButton);
            this.panel1.Controls.Add(this.panel3);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel1.Location = new System.Drawing.Point(3, 155);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(427, 232);
            this.panel1.TabIndex = 1;
            // 
            // homeZButton
            // 
            this.homeZButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.homeZButton.FlatAppearance.BorderSize = 0;
            this.homeZButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.homeZButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.homeZButton.Image = global::LPStudio.Properties.Resources.homeImage16x16;
            this.homeZButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.homeZButton.Location = new System.Drawing.Point(150, 86);
            this.homeZButton.Name = "homeZButton";
            this.homeZButton.Size = new System.Drawing.Size(43, 36);
            this.homeZButton.TabIndex = 12;
            this.homeZButton.Text = " Z";
            this.homeZButton.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.homeZButton.UseVisualStyleBackColor = false;
            // 
            // moveZUpButton
            // 
            this.moveZUpButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveZUpButton.FlatAppearance.BorderSize = 0;
            this.moveZUpButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveZUpButton.Image = global::LPStudio.Properties.Resources.upArrowImage32x32;
            this.moveZUpButton.Location = new System.Drawing.Point(154, 44);
            this.moveZUpButton.Name = "moveZUpButton";
            this.moveZUpButton.Size = new System.Drawing.Size(35, 35);
            this.moveZUpButton.TabIndex = 11;
            this.moveZUpButton.UseVisualStyleBackColor = false;
            // 
            // moveZDownButton
            // 
            this.moveZDownButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveZDownButton.FlatAppearance.BorderSize = 0;
            this.moveZDownButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveZDownButton.Image = global::LPStudio.Properties.Resources.downArrowImage32x32;
            this.moveZDownButton.Location = new System.Drawing.Point(154, 128);
            this.moveZDownButton.Name = "moveZDownButton";
            this.moveZDownButton.Size = new System.Drawing.Size(35, 35);
            this.moveZDownButton.TabIndex = 10;
            this.moveZDownButton.UseVisualStyleBackColor = false;
            // 
            // homeALLButton
            // 
            this.homeALLButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.homeALLButton.FlatAppearance.BorderSize = 0;
            this.homeALLButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.homeALLButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.homeALLButton.Image = global::LPStudio.Properties.Resources.homeImage16x16;
            this.homeALLButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.homeALLButton.Location = new System.Drawing.Point(204, 44);
            this.homeALLButton.Name = "homeALLButton";
            this.homeALLButton.Size = new System.Drawing.Size(58, 36);
            this.homeALLButton.TabIndex = 9;
            this.homeALLButton.Text = " ALL";
            this.homeALLButton.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.homeALLButton.UseVisualStyleBackColor = false;
            // 
            // homeXButton
            // 
            this.homeXButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.homeXButton.FlatAppearance.BorderSize = 0;
            this.homeXButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.homeXButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.homeXButton.Image = global::LPStudio.Properties.Resources.homeImage16x16;
            this.homeXButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.homeXButton.Location = new System.Drawing.Point(204, 85);
            this.homeXButton.Name = "homeXButton";
            this.homeXButton.Size = new System.Drawing.Size(43, 36);
            this.homeXButton.TabIndex = 8;
            this.homeXButton.Text = " X";
            this.homeXButton.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.homeXButton.UseVisualStyleBackColor = false;
            // 
            // homeYButton
            // 
            this.homeYButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.homeYButton.FlatAppearance.BorderSize = 0;
            this.homeYButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.homeYButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.homeYButton.Image = global::LPStudio.Properties.Resources.homeImage16x16;
            this.homeYButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.homeYButton.Location = new System.Drawing.Point(204, 127);
            this.homeYButton.Name = "homeYButton";
            this.homeYButton.Size = new System.Drawing.Size(43, 36);
            this.homeYButton.TabIndex = 7;
            this.homeYButton.Text = " Y";
            this.homeYButton.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.homeYButton.UseVisualStyleBackColor = false;
            // 
            // homeXYButton
            // 
            this.homeXYButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.homeXYButton.FlatAppearance.BorderSize = 0;
            this.homeXYButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.homeXYButton.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(212)))), ((int)(((byte)(212)))), ((int)(((byte)(212)))));
            this.homeXYButton.Image = global::LPStudio.Properties.Resources.homeImage16x16;
            this.homeXYButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.homeXYButton.Location = new System.Drawing.Point(54, 86);
            this.homeXYButton.Name = "homeXYButton";
            this.homeXYButton.Size = new System.Drawing.Size(49, 36);
            this.homeXYButton.TabIndex = 6;
            this.homeXYButton.Text = " XY";
            this.homeXYButton.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.homeXYButton.UseVisualStyleBackColor = false;
            // 
            // moveYUpButton
            // 
            this.moveYUpButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveYUpButton.FlatAppearance.BorderSize = 0;
            this.moveYUpButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveYUpButton.Image = global::LPStudio.Properties.Resources.upArrowImage32x32;
            this.moveYUpButton.Location = new System.Drawing.Point(60, 44);
            this.moveYUpButton.Name = "moveYUpButton";
            this.moveYUpButton.Size = new System.Drawing.Size(35, 35);
            this.moveYUpButton.TabIndex = 5;
            this.moveYUpButton.UseVisualStyleBackColor = false;
            this.moveYUpButton.Click += new System.EventHandler(this.moveYUpButton_Click);
            // 
            // moveXLeftButton
            // 
            this.moveXLeftButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveXLeftButton.FlatAppearance.BorderSize = 0;
            this.moveXLeftButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveXLeftButton.Image = global::LPStudio.Properties.Resources.leftArrowImage32x32;
            this.moveXLeftButton.Location = new System.Drawing.Point(13, 86);
            this.moveXLeftButton.Name = "moveXLeftButton";
            this.moveXLeftButton.Size = new System.Drawing.Size(35, 35);
            this.moveXLeftButton.TabIndex = 4;
            this.moveXLeftButton.UseVisualStyleBackColor = false;
            // 
            // moveYDownButton
            // 
            this.moveYDownButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveYDownButton.FlatAppearance.BorderSize = 0;
            this.moveYDownButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveYDownButton.Image = global::LPStudio.Properties.Resources.downArrowImage32x32;
            this.moveYDownButton.Location = new System.Drawing.Point(60, 128);
            this.moveYDownButton.Name = "moveYDownButton";
            this.moveYDownButton.Size = new System.Drawing.Size(35, 35);
            this.moveYDownButton.TabIndex = 3;
            this.moveYDownButton.UseVisualStyleBackColor = false;
            this.moveYDownButton.Click += new System.EventHandler(this.moveYDownButton_Click);
            // 
            // moveXRightButton
            // 
            this.moveXRightButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(65)))), ((int)(((byte)(65)))), ((int)(((byte)(224)))));
            this.moveXRightButton.FlatAppearance.BorderSize = 0;
            this.moveXRightButton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.moveXRightButton.Image = global::LPStudio.Properties.Resources.rigthArrowImage32x32;
            this.moveXRightButton.Location = new System.Drawing.Point(109, 86);
            this.moveXRightButton.Name = "moveXRightButton";
            this.moveXRightButton.Size = new System.Drawing.Size(35, 35);
            this.moveXRightButton.TabIndex = 2;
            this.moveXRightButton.UseVisualStyleBackColor = false;
            // 
            // panel3
            // 
            this.panel3.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(51)))), ((int)(((byte)(51)))), ((int)(((byte)(55)))));
            this.panel3.Controls.Add(this.label2);
            this.panel3.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel3.ForeColor = System.Drawing.SystemColors.Info;
            this.panel3.Location = new System.Drawing.Point(0, 0);
            this.panel3.Name = "panel3";
            this.panel3.Size = new System.Drawing.Size(427, 38);
            this.panel3.TabIndex = 1;
            // 
            // label2
            // 
            this.label2.Dock = System.Windows.Forms.DockStyle.Left;
            this.label2.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.label2.Image = global::LPStudio.Properties.Resources.toolImage32x32;
            this.label2.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.label2.Location = new System.Drawing.Point(0, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(103, 38);
            this.label2.TabIndex = 2;
            this.label2.Text = " Tool";
            this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // logTimer
            // 
            this.logTimer.Tick += new System.EventHandler(this.logTimer_Tick);
            // 
            // OpenFileDialog
            // 
            this.OpenFileDialog.FileName = "openFileDialog";
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
            this.panel4.ResumeLayout(false);
            this.panel4.PerformLayout();
            this.panel5.ResumeLayout(false);
            this.panel1.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
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
        private System.Windows.Forms.Label labelBattery;
        private System.Windows.Forms.Timer logTimer;
        private System.Windows.Forms.Panel panel4;
        private System.Windows.Forms.Panel panel5;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Panel panel3;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Button moveXRightButton;
        private System.Windows.Forms.Button homeXYButton;
        private System.Windows.Forms.Button moveYUpButton;
        private System.Windows.Forms.Button moveXLeftButton;
        private System.Windows.Forms.Button moveYDownButton;
        private System.Windows.Forms.Button homeALLButton;
        private System.Windows.Forms.Button homeXButton;
        private System.Windows.Forms.Button homeYButton;
        private System.Windows.Forms.Button homeZButton;
        private System.Windows.Forms.Button moveZUpButton;
        private System.Windows.Forms.Button moveZDownButton;
        private System.Windows.Forms.Button buttonExecuteCode;
        private System.Windows.Forms.TextBox showCodeFile;
        private System.Windows.Forms.OpenFileDialog OpenFileDialog;
        private System.Windows.Forms.Button browseButton;
    }
}
