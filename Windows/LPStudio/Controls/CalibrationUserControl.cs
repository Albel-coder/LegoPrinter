using System;
using System.Diagnostics;
using System.Windows.Forms;

namespace LPStudio
{
    public partial class CalibrationUserControl : UserControl
    {
        public CalibrationUserControl()
        {
            InitializeComponent();
        }
        private void buttonCalibratePrinter_Click(object sender, EventArgs e)
        {
            string configPath = "Printer.cfg";
            Process.Start(new ProcessStartInfo
            {
                FileName = configPath,
                UseShellExecute = true
            });
        }
    }
}
