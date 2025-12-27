using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class DeviceUserControl : UserControl
    {
        private PrinterController printerController;
        public DeviceUserControl()
        {
            try
            {
                printerController = new PrinterController();
                InitializeComponent();
            }
            catch (Exception)
            {
                throw;
            }
        }
        private async void connectButton_Click(object sender, EventArgs e)
        {
            if (!printerController.IsPrinterConnect())
            {
                this.connectButton.Enabled = false;
                this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(227)))), ((int)(((byte)(235)))), ((int)(((byte)(12)))));
                this.connectButton.Text = "connecting...";

                try
                {
                    bool isConnected = await Task.Run(() => printerController.Connect());

                    if (isConnected)
                    {
                        this.connectButton.Enabled = true;
                        this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(234)))), ((int)(((byte)(84)))), ((int)(((byte)(85)))));
                        this.connectButton.Text = "disconnect";
                    }
                }
                catch (Exception)
                {
                    throw;
                }
                finally
                {
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
                    this.connectButton.Enabled = true;
                    this.connectButton.Text = "Connect";
                }
            }
            else
            {
                this.connectButton.Enabled = false;
                this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(227)))), ((int)(((byte)(235)))), ((int)(((byte)(12)))));
                this.connectButton.Text = "disconnecting...";

                try
                {
                    bool isDisconnected = await Task.Run(() => printerController.Disconnect());

                    if (isDisconnected)
                    {
                        this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
                        this.connectButton.Enabled = true;
                        this.connectButton.Text = "Connect";
                    }
                }
                catch (Exception)
                {

                    throw;
                }
                finally
                {
                    this.connectButton.Enabled = true;
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(234)))), ((int)(((byte)(84)))), ((int)(((byte)(85)))));
                    this.connectButton.Text = "disconnect";
                }
            }
        }
    }
}
