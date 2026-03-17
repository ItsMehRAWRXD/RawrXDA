using System;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace CompilerStudio;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private void OpenFile_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.OpenFileDialog();
        dialog.Filter = "PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*";
        if (dialog.ShowDialog() == true)
        {
            CodeEditor.LoadFile(dialog.FileName);
            StatusText.Text = $"Loaded: {Path.GetFileName(dialog.FileName)}";
        }
    }

    private void SaveFile_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.SaveFileDialog();
        dialog.Filter = "PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*";
        if (dialog.ShowDialog() == true)
        {
            CodeEditor.SaveFile(dialog.FileName);
            StatusText.Text = $"Saved: {Path.GetFileName(dialog.FileName)}";
        }
    }

    private void Exit_Click(object sender, RoutedEventArgs e)
    {
        Application.Current.Shutdown();
    }

    private void Compile_Click(object sender, RoutedEventArgs e)
    {
        var compilerService = new PowerShellCompilerService();
        var output = compilerService.Compile(CodeEditor.Text);
        // For now, just show in status bar
        StatusText.Text = $"Compile: {(output.Success ? "Success" : "Failed")}";
        // Later: show in output panel
    }
}
