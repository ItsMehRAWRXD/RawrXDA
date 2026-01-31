namespace RawrZDesktop;

partial class Form1
{
    /// <summary>
    ///  Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    ///  Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    /// <summary>
    ///  Required method for Designer support - do not modify
    ///  the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
        this.components = new System.ComponentModel.Container();
        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
        this.ClientSize = new System.Drawing.Size(1000, 700);
        this.Text = "RawrZ Security Platform - Encryption Panel";
        this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
        
        // Initialize all form controls with null values
        this.fileLabel = null!;
        this.algorithmCombo = null!;
        this.keySizeCombo = null!;
        this.compressionCheck = null!;
        this.obfuscationCheck = null!;
        this.resultsTextBox = null!;
        this.roslynCompileButton = null!;
        this.selectEncryptedButton = null!;
        this.stealthModeCheck = null!;
        this.antiAnalysisCheck = null!;
        this.polymorphicCheck = null!;
        this.memoryProtectionCheck = null!;
        this.networkEvasionCheck = null!;
        this.persistenceCheck = null!;
        this.debugModeCheck = null!;
    }

    #endregion
}
