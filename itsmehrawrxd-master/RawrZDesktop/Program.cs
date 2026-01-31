// RawrZ Desktop Application - Main Entry Point
using System;
using System.Windows.Forms;
using System.Threading;

namespace RawrZDesktop
{
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            // Enable visual styles
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            
            // Set application properties
            Application.ApplicationExit += Application_ApplicationExit;
            
            try
            {
                // Initialize and run the main form
                var mainForm = new RawrZDesktopApp();
                Application.Run(mainForm);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"RawrZ Desktop failed to start:\n\n{ex.Message}", 
                    "RawrZ Desktop Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        
        private static void Application_ApplicationExit(object sender, EventArgs e)
        {
            // Cleanup on exit
            Console.WriteLine("RawrZ Desktop shutting down...");
        }
    }
}
