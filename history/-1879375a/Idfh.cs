using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace BotBuilder.Models
{
  public class BotConfiguration : INotifyPropertyChanged
  {
    private string _botName = "MyBot";
    private string _c2Server = "localhost";
    private int _c2Port = 8080;
    private string _architecture = "x64";
    private string _outputFormat = "EXE";
    private int _obfuscationLevel = 50;

    // Configuration Tab Properties
    public string BotName
    {
      get => _botName;
      set { SetProperty(ref _botName, value); }
    }

    public string C2Server
    {
      get => _c2Server;
      set { SetProperty(ref _c2Server, value); }
    }

    public int C2Port
    {
      get => _c2Port;
      set { SetProperty(ref _c2Port, value); }
    }

    public string Architecture
    {
      get => _architecture;
      set { SetProperty(ref _architecture, value); }
    }

    public string OutputFormat
    {
      get => _outputFormat;
      set { SetProperty(ref _outputFormat, value); }
    }

    public int ObfuscationLevel
    {
      get => _obfuscationLevel;
      set { SetProperty(ref _obfuscationLevel, value); }
    }

    // Advanced Tab Properties
    public bool AntiVMDetection { get; set; } = false;
    public bool AntiDebugging { get; set; } = false;
    public bool RegistryPersistence { get; set; } = false;
    public bool COMPersistence { get; set; } = false;
    public bool WMIPersistence { get; set; } = false;
    public bool ScheduledTaskPersistence { get; set; } = false;
    public string NetworkProtocol { get; set; } = "TCP";

    // Build Tab Properties
    public string CompressionMethod { get; set; } = "None";
    public string EncryptionMethod { get; set; } = "AES-256";
    public bool IsBuilding { get; set; } = false;
    public int BuildProgress { get; set; } = 0;
    public string BuildStatus { get; set; } = "Ready";

    // Preview Tab Properties
    public long EstimatedPayloadSize { get; set; } = 0;
    public string PayloadHash { get; set; } = "";
    public int EvadeScore { get; set; } = 0;

    public event PropertyChangedEventHandler PropertyChanged;

    protected void SetProperty<T>(ref T backingField, T value, [CallerMemberName] string propertyName = null)
    {
      if (!Equals(backingField, value))
      {
        backingField = value;
        OnPropertyChanged(propertyName);
      }
    }

    protected void OnPropertyChanged(string propertyName)
    {
      PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
  }
}
