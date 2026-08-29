using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace LPStudio
{
    public partial class CalibrationUserControl : UserControl
    {
        private MotionCompilerController motionCompilerController;
        public CalibrationUserControl()
        {
            InitializeComponent();

            string dllPath = Path.Combine(
            AppContext.BaseDirectory,
                "MotionCompiler.dll");

            Console.WriteLine($"DLL: {dllPath}\n" +
                $"Exists: {File.Exists(dllPath)}");

            motionCompilerController = new MotionCompilerController();
        }

        private void tableLayoutPanel1_Paint(object sender, PaintEventArgs e)
        {

        }

        private void labelPrinterHead_Click(object sender, EventArgs e)
        {

        }

        private void button1_Click(object sender, EventArgs e)
        {
            motionCompilerController.GenerateGCode("testImage.png", "test.gcode", true);
        }
    }
}
