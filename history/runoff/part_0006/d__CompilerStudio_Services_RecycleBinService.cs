using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace CompilerStudio.Services
{
    public interface IRecycleBinService
    {
        Task<bool> RestoreFilesFromRecycleBinAsync(string projectPath);
        Task<IEnumerable<string>> GetRecycleBinItemsAsync();
    }

    public class RecycleBinService : IRecycleBinService
    {
        private readonly IPowerShellCompilerService _powerShellService;
        private readonly string _restoreScriptPath;

        public RecycleBinService(IPowerShellCompilerService powerShellService)
        {
            _powerShellService = powerShellService;
            _restoreScriptPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "RestoreFromRecycleBin.ps1");
        }

        public async Task<bool> RestoreFilesFromRecycleBinAsync(string projectPath)
        {
            try
            {
                var result = await _powerShellService.ExecuteScriptAsync(_restoreScriptPath, new[] 
                { 
                    "-GitRepo", $"\"{projectPath}\"" 
                });

                return result.Success;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error restoring files from Recycle Bin: {ex.Message}");
                return false;
            }
        }

        public async Task<IEnumerable<string>> GetRecycleBinItemsAsync()
        {
            try
            {
                var script = @"
                    $recycleBinPath = '$env:SystemDrive\$Recycle.Bin'
                    $userSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
                    $userRecycleBin = Join-Path $recycleBinPath $userSid
                    
                    if (Test-Path $userRecycleBin) {
                        Get-ChildItem $userRecycleBin -File | Where-Object { $_.Name -like '$I*' } | Select-Object -ExpandProperty Name
                    }
                ";

                var result = await _powerShellService.ExecuteScriptAsync(script);
                
                if (result.Success && result.Output != null)
                {
                    return result.Output.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error getting Recycle Bin items: {ex.Message}");
            }

            return Enumerable.Empty<string>();
        }
    }
}
