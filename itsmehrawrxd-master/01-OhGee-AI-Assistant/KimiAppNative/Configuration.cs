using System;
using System.IO;
using System.Text.Json;
using System.Collections.Generic;

namespace KimiAppNative
{
    public class AppConfiguration
    {
        public string OpenAIApiKey { get; set; } = "";
        public string AnthropicApiKey { get; set; } = "";
        public string GoogleApiKey { get; set; } = "";
        public string KimiApiKey { get; set; } = "";
        public string DefaultModel { get; set; } = "openai";
        public bool EnableSystemTray { get; set; } = true;
        public bool StartMinimized { get; set; } = true;
        public bool EnableHotkeys { get; set; } = true;
        public int MaxTokens { get; set; } = 2000;
        public double Temperature { get; set; } = 0.7;
        public string Theme { get; set; } = "dark";
        public bool EnableMarkdown { get; set; } = true;
        public bool EnableStreaming { get; set; } = true;
        public int ChatHistoryLimit { get; set; } = 100;
        public bool AutoSaveChats { get; set; } = true;
        public string ChatSaveDirectory { get; set; } = "Chats";
        public bool EnableNotifications { get; set; } = true;
        public bool EnableSoundEffects { get; set; } = false;
        public int WindowWidth { get; set; } = 800;
        public int WindowHeight { get; set; } = 600;
        public int ChatWindowWidth { get; set; } = 1000;
        public int ChatWindowHeight { get; set; } = 700;
        public bool RememberWindowPosition { get; set; } = true;
        public int WindowX { get; set; } = 100;
        public int WindowY { get; set; } = 100;
        public int ChatWindowX { get; set; } = 150;
        public int ChatWindowY { get; set; } = 150;
    }

    public static class ConfigurationManager
    {
        private static readonly string ConfigPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "KimiAppNative",
            "config.json"
        );

        private static AppConfiguration _config;
        private static readonly object _lock = new object();

        public static AppConfiguration Config
        {
            get
            {
                if (_config == null)
                {
                    lock (_lock)
                    {
                        if (_config == null)
                        {
                            LoadConfiguration();
                        }
                    }
                }
                return _config;
            }
        }

        public static void LoadConfiguration()
        {
            try
            {
                if (File.Exists(ConfigPath))
                {
                    var json = File.ReadAllText(ConfigPath);
                    _config = JsonSerializer.Deserialize<AppConfiguration>(json) ?? new AppConfiguration();
                }
                else
                {
                    _config = new AppConfiguration();
                    SaveConfiguration();
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Error loading configuration: {ex.Message}");
                _config = new AppConfiguration();
            }
        }

        public static void SaveConfiguration()
        {
            try
            {
                var directory = Path.GetDirectoryName(ConfigPath);
                if (!Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }

                var options = new JsonSerializerOptions
                {
                    WriteIndented = true,
                    PropertyNamingPolicy = JsonNamingPolicy.CamelCase
                };

                var json = JsonSerializer.Serialize(_config, options);
                File.WriteAllText(ConfigPath, json);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Error saving configuration: {ex.Message}");
            }
        }

        public static void ResetToDefaults()
        {
            _config = new AppConfiguration();
            SaveConfiguration();
        }

        public static string GetApiKey(string modelName)
        {
            return modelName.ToLower() switch
            {
                "openai" => Config.OpenAIApiKey,
                "anthropic" => Config.AnthropicApiKey,
                "google" => Config.GoogleApiKey,
                "kimi" => Config.KimiApiKey,
                _ => ""
            };
        }

        public static void SetApiKey(string modelName, string apiKey)
        {
            switch (modelName.ToLower())
            {
                case "openai":
                    Config.OpenAIApiKey = apiKey;
                    break;
                case "anthropic":
                    Config.AnthropicApiKey = apiKey;
                    break;
                case "google":
                    Config.GoogleApiKey = apiKey;
                    break;
                case "kimi":
                    Config.KimiApiKey = apiKey;
                    break;
            }
            SaveConfiguration();
        }

        public static bool HasApiKey(string modelName)
        {
            var apiKey = GetApiKey(modelName);
            return !string.IsNullOrWhiteSpace(apiKey);
        }

        public static void CreateDefaultConfigFile()
        {
            try
            {
                var directory = Path.GetDirectoryName(ConfigPath);
                if (!Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }

                if (!File.Exists(ConfigPath))
                {
                    SaveConfiguration();
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Error creating default config: {ex.Message}");
            }
        }
    }
}
