using System;
using System.Collections.Generic;
using System.IO.Compression;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public class CompressionEngine : IEngine
    {
        public string Name => "CompressionEngine";
        public string Description => "Data compression engine";
        public string Version => "1.0.0";

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
            var data = (byte[])parameters["data"];
            
            switch (operation.ToLower())
            {
                case "compress":
                    return await CompressAsync(data, parameters);
                case "decompress":
                    return await DecompressAsync(data, parameters);
                default:
                    return new EngineResult { Success = false, Error = "Unsupported operation" };
            }
        }

        private async Task<EngineResult> CompressAsync(byte[] data, Dictionary<string, object> parameters)
        {
            return await Task.Run(() =>
            {
                using var output = new MemoryStream();
                using (var gzip = new GZipStream(output, CompressionMode.Compress))
                {
                    gzip.Write(data, 0, data.Length);
                }
                
                return new EngineResult
                {
                    Success = true,
                    Data = output.ToArray(),
                    Metadata = new Dictionary<string, object>
                    {
                        ["compression_ratio"] = (double)output.Length / data.Length,
                        ["original_size"] = data.Length,
                        ["compressed_size"] = output.Length
                    }
                };
            });
        }

        private async Task<EngineResult> DecompressAsync(byte[] data, Dictionary<string, object> parameters)
        {
            return await Task.Run(() =>
            {
                using var input = new MemoryStream(data);
                using var gzip = new GZipStream(input, CompressionMode.Decompress);
                using var output = new MemoryStream();
                
                gzip.CopyTo(output);
                
                return new EngineResult
                {
                    Success = true,
                    Data = output.ToArray()
                };
            });
        }
    }
}
