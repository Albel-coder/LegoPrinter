using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class MainForm : Form
    {
        private PrinterController printerController = new PrinterController();

        public MainForm()
        {
            InitializeComponent();
        }

        private void ConnectButton_Click(object sender, EventArgs e)
        {
            printerController.Connect();
        }

        private void DisconnectButton_Click(object sender, EventArgs e)
        {
            printerController.Disconnect();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            printerController?.Dispose();
            base.OnFormClosing(e);
        }
    }
}


