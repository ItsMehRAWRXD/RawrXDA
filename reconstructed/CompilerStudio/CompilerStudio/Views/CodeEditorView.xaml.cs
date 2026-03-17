using System.IO;
using System.Windows.Controls;

namespace CompilerStudio.Views;

public partial class CodeEditorView : UserControl
{
    public CodeEditorView()
    {
        InitializeComponent();
    }

    public string Text
    {
        get => EditorTextBox.Text;
        set => EditorTextBox.Text = value;
    }

    public void LoadFile(string filePath)
    {
        if (File.Exists(filePath))
        {
            Text = File.ReadAllText(filePath);
        }
    }

    public void SaveFile(string filePath)
    {
        File.WriteAllText(filePath, Text);
    }
}
