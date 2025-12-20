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

        public MainForm()
        {
            InitializeComponent();
            SetupTabs();
        }   
        
        private void SetupTabs()
        {
            tabs.Add("DeviceUserControl", new DeviceUserControl());
            tabs.Add("PreviewUserControl", new PreviewUserControl());
            tabs.Add("PrepareUserControl", new PreviewUserControl());

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
    }
}