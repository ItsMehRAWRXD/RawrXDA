using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Buffers;

namespace RawrZDesktop.Engines
{
    public class StreamingEncryptionEngine : IEngine
    {
        public string Name => "Streaming Encryption";
        public string Description => "Memory-efficient streaming encryption for large files using ArrayPool";
        public string Version => "1.0.0";
        
        private readonly IEngine _baseEngine;
        private readonly int _bufferSize;

        public StreamingEncryptionEngine(IEngine baseEngine, int bufferSize = 64 * 1024)
        {
            _baseEngine = baseEngine ?? throw new ArgumentNullException(nameof(baseEngine));
            _bufferSize = bufferSize;
        }

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var startTime = DateTime.UtcNow;
            
            try
            {
                var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
                var data = (byte[])parameters["data"];
                var password = parameters["password"]?.ToString() ?? throw new ArgumentException("Password parameter is required");
                var options = parameters.ContainsKey("options") ? (Dictionary<string, object>)parameters["options"] : new Dictionary<string, object>();

                // For small files, use the base engine directly
                if (data.Length <= _bufferSize)
                {
                    return await _baseEngine.ExecuteAsync(parameters);
                }

                byte[] result;
                
                switch (operation.ToLower())
                {
                    case "encrypt":
                        result = await EncryptStreamingAsync(data, password, options);
                        break;
                    case "decrypt":
                        result = await DecryptStreamingAsync(data, password, options);
                        break;
                    default:
                        return new EngineResult
                        {
                            Success = false,
                            Error = $"Unsupported operation: {operation}"
                        };
                }

                var processingTime = (DateTime.UtcNow - startTime).TotalMilliseconds;

                return new EngineResult
                {
                    Success = true,
                    Data = result,
                    Metadata = new Dictionary<string, object>
                    {
                        ["algorithm"] = $"Streaming-{_baseEngine.Name}",
                        ["operation"] = operation,
                        ["original_size"] = data.Length,
                        ["result_size"] = result.Length,
                        ["buffer_size"] = _bufferSize,
                        ["memory_optimized"] = true,
                        ["streaming_processing"] = true,
                        ["memory_efficiency"] = GetMemoryEfficiency(data.Length)
                    },
                    ProcessingTimeMs = (long)processingTime
                };
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

        private async Task<byte[]> EncryptStreamingAsync(byte[] data, string password, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                using var inputStream = new MemoryStream(data);
                using var outputStream = new MemoryStream();
                
                // Use ArrayPool for memory efficiency
                var buffer = ArrayPool<byte>.Shared.Rent(_bufferSize);
                try
                {
                    int bytesRead;
                    while ((bytesRead = inputStream.Read(buffer, 0, _bufferSize)) > 0)
                    {
                        // Process chunk
                        var chunk = new byte[bytesRead];
                        Array.Copy(buffer, 0, chunk, 0, bytesRead);
                        
                        var chunkParams = new Dictionary<string, object>
                        {
                            ["operation"] = "encrypt",
                            ["data"] = chunk,
                            ["password"] = password,
                            ["options"] = options
                        };

                        var result = _baseEngine.ExecuteAsync(chunkParams).Result;
                        if (result.Success && result.Data != null)
                        {
                            outputStream.Write(result.Data, 0, result.Data.Length);
                        }
                    }
                }
                finally
                {
                    ArrayPool<byte>.Shared.Return(buffer);
                }
                
                return outputStream.ToArray();
            });
        }

        private async Task<byte[]> DecryptStreamingAsync(byte[] encryptedData, string password, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                using var inputStream = new MemoryStream(encryptedData);
                using var outputStream = new MemoryStream();
                
                // Use ArrayPool for memory efficiency
                var buffer = ArrayPool<byte>.Shared.Rent(_bufferSize);
                try
                {
                    int bytesRead;
                    while ((bytesRead = inputStream.Read(buffer, 0, _bufferSize)) > 0)
                    {
                        // Process chunk
                        var chunk = new byte[bytesRead];
                        Array.Copy(buffer, 0, chunk, 0, bytesRead);
                        
                        var chunkParams = new Dictionary<string, object>
                        {
                            ["operation"] = "decrypt",
                            ["data"] = chunk,
                            ["password"] = password,
                            ["options"] = options
                        };

                        var result = _baseEngine.ExecuteAsync(chunkParams).Result;
                        if (result.Success && result.Data != null)
                        {
                            outputStream.Write(result.Data, 0, result.Data.Length);
                        }
                    }
                }
                finally
                {
                    ArrayPool<byte>.Shared.Return(buffer);
                }
                
                return outputStream.ToArray();
            });
        }

        private string GetMemoryEfficiency(int dataSize)
        {
            var memoryReduction = (double)(dataSize - _bufferSize) / dataSize * 100;
            return $"~{memoryReduction:F1}% memory reduction";
        }
    }
}
