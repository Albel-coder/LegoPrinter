using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class MainForm : Form
    {
        private PrinterController printerController;
        private GCodeInterpreter Interpreter;
        private int LastLogCount = 0;
        private int LastInterpreterLogCount = 0;
        private bool AutoScrollEnabled = true;
                
        private readonly Dictionary<string, UserControl> tabs = new Dictionary<string, UserControl>();
        private List<Panel> panels = new List<Panel>();
        private UserControl currentTab;

        private bool isDragging = false;
        private Point dragStartPoint = Point.Empty;

        public MainForm()
        {
            InitializeComponent();
            SetupTabs();

            this.Text = string.Empty;
            this.ControlBox = false;
            this.MaximizedBounds = Screen.FromHandle(this.Handle).WorkingArea;

            SubscribeAllControls(this);
        }
        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();

        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWindow, int wMessage, int wParameter, int lParameter);

        private void SetupTabs()
        {
            tabs.Add("DeviceUserControl", new DeviceUserControl());
            tabs.Add("PreviewUserControl", new PreviewUserControl());
            tabs.Add("PrepareUserControl", new PrepareUserControl());
            tabs.Add("CalibrationUserControl", new CalibrationUserControl());

            foreach(KeyValuePair<string, UserControl> tab in tabs)
            {
                tab.Value.Dock = DockStyle.Fill;
                tab.Value.Visible = false;
                contentPanel.Controls.Add(tab.Value);
            }

            ShowTab("DeviceUserControl");
        }
        private void ShowTab(string tabKey)
        {
            if (currentTab != null)
            {
                currentTab.Visible = false;
            }

            if (tabs.ContainsKey(tabKey))
            {
                currentTab = tabs[tabKey];
                currentTab.Visible = true;
            }
        }
        private void MainForm_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        private void SubscribeAllControls(Control control)
        {
            foreach (Control childControl in control.Controls)
            {
                // Launch buttons and other interactive elements
                if (!(childControl is Button) &&
                    !(childControl is TextBox) &&
                    !(childControl is ComboBox) &&
                    !(childControl is CheckBox) &&
                    !(childControl is RadioButton))
                {
                    childControl.MouseDown += MainForm_MouseDown;
                }

                if (childControl.HasChildren)
                {
                    SubscribeAllControls(childControl);
                }
            }
        }

        private void exitButton_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void buttonPreview_Click(object sender, EventArgs e)
        {
            ShowTab("PreviewUserControl");
        }

        private void restoreWindowButton_Click(object sender, EventArgs e)
        {
            if (WindowState == FormWindowState.Normal)
            {
                this.WindowState = FormWindowState.Maximized;
                this.restoreWindowButton.Image = global::WindowsForms.Properties.Resources.restoreImage32x32;
            }
            else
            {
                this.WindowState = FormWindowState.Normal;
                this.restoreWindowButton.Image = global::WindowsForms.Properties.Resources.maximizeWindowImage32x32;
            }
        }
        private void minimizeWindowButton_Click(object sender, EventArgs e)
        {
            this.WindowState = FormWindowState.Minimized;
        }
        private void buttonHome_Click(object sender, EventArgs e)
        {
            ShowTab("DeviceUserControl");
        }
        private void buttonPrepare_Click(object sender, EventArgs e)
        {
            ShowTab("PrepareUserControl");
        }

        private void buttonDevice_Click(object sender, EventArgs e)
        {
            ShowTab("DeviceUserControl");
        }
        private void buttonCalibration_Click(object sender, EventArgs e)
        {
            ShowTab("CalibrationUserControl");
        }
    }
}