using System;
using System.Windows;
using System.Security.Cryptography;
using System.Text;
using BotBuilder.Models;

namespace BotBuilder
{
  public partial class MainWindow : Window
  {
    private BotConfiguration _config;

    public MainWindow()
    {
      InitializeComponent();
      _config = new BotConfiguration();
      this.DataContext = _config;
    }

    private void OnBuildClick(object sender, RoutedEventArgs e)
    {
      _config.IsBuilding = true;
      _config.BuildStatus = "Building...";
      _config.BuildProgress = 0;

      // Simulate build progress
      for (int i = 0; i <= 100; i += 10)
      {
        System.Threading.Thread.Sleep(200);
        _config.BuildProgress = i;
        _config.BuildStatus = $"Building... {i}%";
      }

      // Calculate payload size (simplified)
      _config.EstimatedPayloadSize = CalculatePayloadSize();

      // Generate hash
      _config.PayloadHash = GeneratePayloadHash();

      // Calculate evasion score
      _config.EvadeScore = CalculateEvaseScore();

      _config.BuildStatus = "✓ Build Complete!";
      _config.IsBuilding = false;

      MessageBox.Show("Build completed successfully!", "BotBuilder", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void OnExportClick(object sender, RoutedEventArgs e)
    {
      if (_config.EstimatedPayloadSize == 0)
      {
        MessageBox.Show("Please complete a build first!", "Error", MessageBoxButton.OK, MessageBoxImage.Warning);
        return;
      }

      MessageBox.Show("Payload exported successfully!", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void OnResetClick(object sender, RoutedEventArgs e)
    {
      _config = new BotConfiguration();
      this.DataContext = _config;
      MessageBox.Show("Configuration reset to defaults.", "Reset", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void OnSaveClick(object sender, RoutedEventArgs e)
    {
      MessageBox.Show($"Configuration saved:\n\nBot Name: {_config.BotName}\nC2: {_config.C2Server}:{_config.C2Port}\nFormat: {_config.OutputFormat}",
          "Save", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void OnExitClick(object sender, RoutedEventArgs e)
    {
      this.Close();
    }

    private long CalculatePayloadSize()
    {
      // Base size + compression/encryption overhead
      long baseSize = 50000; // 50KB base

      // Add compression overhead
      if (_config.CompressionMethod != "None")
        baseSize = (long)(baseSize * 0.7); // 30% compression

      // Add encryption overhead
      baseSize += 2048; // Encryption headers

      return baseSize;
    }

    private string GeneratePayloadHash()
    {
      string input = $"{_config.BotName}{_config.C2Server}{_config.C2Port}{DateTime.UtcNow:yyyyMMdd}";
      using (var sha256 = SHA256.Create())
      {
        var hashedValue = sha256.ComputeHash(Encoding.UTF8.GetBytes(input));
        return BitConverter.ToString(hashedValue).Replace("-", "").ToLower();
      }
    }

    private int CalculateEvaseScore()
    {
      int score = 0;

      if (_config.AntiVMDetection) score += 15;
      if (_config.AntiDebugging) score += 15;
      if (_config.RegistryPersistence) score += 10;
      if (_config.COMPersistence) score += 10;
      if (_config.WMIPersistence) score += 10;
      if (_config.ScheduledTaskPersistence) score += 10;

      if (_config.CompressionMethod != "None") score += 10;
      if (_config.EncryptionMethod == "AES-256") score += 15;
      else if (_config.EncryptionMethod == "AES-128") score += 10;

      score += (_config.ObfuscationLevel / 10);

      return Math.Min(score, 100);
    }
  }
}
