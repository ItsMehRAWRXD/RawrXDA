# Agent to restore deleted git files from Recycle Bin using RecycleBinService

using System;
using System.IO;
using System.Threading.Tasks;
using CompilerStudio.Services;

namespace CompilerStudio.Agents
{
    public class GitRecycleBinRestoreAgent
    {
        private readonly IRecycleBinService _recycleBinService;
        private readonly string _projectPath;

        public GitRecycleBinRestoreAgent(IRecycleBinService recycleBinService, string projectPath)
        {
            _recycleBinService = recycleBinService;
            _projectPath = projectPath;
        }

        public async Task RunAsync()
        {
            Console.WriteLine($"[Agent] Attempting to restore deleted git files for project: {_projectPath}");
            var items = await _recycleBinService.GetRecycleBinItemsAsync();
            Console.WriteLine($"[Agent] Found {items?.ToString() ?? "0"} items in Recycle Bin.");

            var success = await _recycleBinService.RestoreFilesFromRecycleBinAsync(_projectPath);
            if (success)
                Console.WriteLine("[Agent] Restore operation completed successfully.");
            else
                Console.WriteLine("[Agent] Restore operation failed or no files restored.");
        }
    }
}
