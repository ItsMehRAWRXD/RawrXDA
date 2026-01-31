using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;

namespace RawrZDesktop.Engines
{
    public class BatchProcessor
    {
        public event EventHandler<BatchProgressEventArgs>? ProgressChanged;
        public event EventHandler<BatchCompletedEventArgs>? BatchCompleted;

        public async Task<BatchResult> ProcessFilesAsync(
            List<string> filePaths, 
            string operation, 
            string password, 
            Dictionary<string, object> options,
            CancellationToken cancellationToken = default)
        {
            var result = new BatchResult
            {
                TotalFiles = filePaths.Count,
                StartTime = DateTime.UtcNow
            };

            var engineManager = new EngineManager();
            var algorithm = options.ContainsKey("algorithm") ? options["algorithm"]?.ToString() ?? "ModernCrypto" : "ModernCrypto";

            for (int i = 0; i < filePaths.Count; i++)
            {
                if (cancellationToken.IsCancellationRequested)
                {
                    result.Cancelled = true;
                    break;
                }

                var filePath = filePaths[i];
                var fileResult = new FileProcessResult
                {
                    FilePath = filePath,
                    FileName = Path.GetFileName(filePath),
                    FileSize = GetFileSize(filePath)
                };

                try
                {
                    // Update progress
                    ProgressChanged?.Invoke(this, new BatchProgressEventArgs
                    {
                        CurrentFile = i + 1,
                        TotalFiles = filePaths.Count,
                        CurrentFileName = fileResult.FileName,
                        ProgressPercentage = (double)(i + 1) / filePaths.Count * 100
                    });

                    // Process the file
                    var fileData = await File.ReadAllBytesAsync(filePath, cancellationToken);
                    
                    EngineResult engineResult;
                    if (operation.ToLower() == "encrypt")
                    {
                        engineResult = await engineManager.EncryptWithEngine(algorithm, fileData, password, options);
                    }
                    else
                    {
                        engineResult = await engineManager.DecryptWithEngine(algorithm, fileData, password, options);
                    }

                    if (engineResult.Success && engineResult.Data != null)
                    {
                        // Save the processed file
                        var outputPath = GenerateOutputPath(filePath, operation);
                        await File.WriteAllBytesAsync(outputPath, engineResult.Data, cancellationToken);
                        
                        fileResult.Success = true;
                        fileResult.OutputPath = outputPath;
                        fileResult.ProcessingTimeMs = (long)engineResult.ProcessingTimeMs;
                        result.SuccessfulFiles++;
                    }
                    else
                    {
                        fileResult.Success = false;
                        fileResult.Error = engineResult.Error ?? "Unknown error";
                        result.FailedFiles++;
                    }
                }
                catch (Exception ex)
                {
                    fileResult.Success = false;
                    fileResult.Error = ex.Message;
                    result.FailedFiles++;
                }

                result.FileResults.Add(fileResult);
            }

            result.EndTime = DateTime.UtcNow;
            result.TotalProcessingTime = result.EndTime - result.StartTime;

            BatchCompleted?.Invoke(this, new BatchCompletedEventArgs
            {
                Result = result
            });

            return result;
        }

        private long GetFileSize(string filePath)
        {
            try
            {
                if (File.Exists(filePath))
                {
                    return new FileInfo(filePath).Length;
                }
                else if (Directory.Exists(filePath))
                {
                    return Directory.GetFiles(filePath, "*", SearchOption.AllDirectories)
                        .Sum(f => new FileInfo(f).Length);
                }
            }
            catch
            {
                // Ignore errors
            }
            return 0;
        }

        private string GenerateOutputPath(string originalPath, string operation)
        {
            var directory = Path.GetDirectoryName(originalPath) ?? Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            var fileName = Path.GetFileNameWithoutExtension(originalPath);
            var extension = Path.GetExtension(originalPath);
            
            var suffix = operation.ToLower() == "encrypt" ? "_encrypted" : "_decrypted";
            var outputFileName = $"{fileName}{suffix}{extension}";
            
            return Path.Combine(directory, outputFileName);
        }
    }

    public class BatchResult
    {
        public int TotalFiles { get; set; }
        public int SuccessfulFiles { get; set; }
        public int FailedFiles { get; set; }
        public bool Cancelled { get; set; }
        public DateTime StartTime { get; set; }
        public DateTime EndTime { get; set; }
        public TimeSpan TotalProcessingTime { get; set; }
        public List<FileProcessResult> FileResults { get; set; } = new List<FileProcessResult>();
    }

    public class FileProcessResult
    {
        public string FilePath { get; set; } = string.Empty;
        public string FileName { get; set; } = string.Empty;
        public long FileSize { get; set; }
        public bool Success { get; set; }
        public string? OutputPath { get; set; }
        public string? Error { get; set; }
        public long ProcessingTimeMs { get; set; }
    }

    public class BatchProgressEventArgs : EventArgs
    {
        public int CurrentFile { get; set; }
        public int TotalFiles { get; set; }
        public string CurrentFileName { get; set; } = string.Empty;
        public double ProgressPercentage { get; set; }
    }

    public class BatchCompletedEventArgs : EventArgs
    {
        public BatchResult Result { get; set; } = new BatchResult();
    }
}
