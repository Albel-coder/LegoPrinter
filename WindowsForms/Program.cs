using System;
using System.Windows.Forms;

namespace WindowsForms
{
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            // Adding handlers for unhandled exceptions
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
            Application.ThreadException += Application_ThreadException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

            try
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new MainForm());
            }
            catch (Exception ex)
            {
                HandleException(ex);
            }
        }

        private static void Application_ThreadException(object sender, System.Threading.ThreadExceptionEventArgs e)
        {
            HandleException(e.Exception);
        }

        private static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            HandleException(e.ExceptionObject as Exception);
        }

        private static void HandleException(Exception ex)
        {
            string errorMessage = $"Critical application error:\n\n{ex}";

            Console.WriteLine($"FATAL ERROR: {errorMessage}");

            MessageBox.Show(errorMessage, "Fatal Error",
                          MessageBoxButtons.OK, MessageBoxIcon.Error);

            Environment.Exit(1);
        }
    }
}