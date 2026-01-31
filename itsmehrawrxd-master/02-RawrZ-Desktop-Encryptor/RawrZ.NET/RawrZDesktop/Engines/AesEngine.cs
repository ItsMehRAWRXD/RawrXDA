using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public class AesEngine : IEngine
    {
        public string Name => "AES";
        public string Description => "Fileless Advanced Encryption Standard engine with auto-generated keys";
        public string Version => "1.0.0";
        
        private readonly int _keySize;

        public AesEngine(int keySize)
        {
            _keySize = keySize;
        }

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var startTime = DateTime.UtcNow;
            
            try
            {
                var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
                var data = (byte[])parameters["data"];
                var filelessKey = parameters["fileless_key"]?.ToString() ?? throw new ArgumentException("Fileless key parameter is required");
                var options = parameters.ContainsKey("options") ? (Dictionary<string, object>)parameters["options"] : new Dictionary<string, object>();

                byte[] result;
                
                switch (operation.ToLower())
                {
                    case "encrypt":
                        result = await EncryptAsync(data, filelessKey, options);
                        break;
                    case "decrypt":
                        result = await DecryptAsync(data, filelessKey, options);
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
                        ["algorithm"] = $"AES-{_keySize}",
                        ["key_size"] = _keySize,
                        ["operation"] = operation,
                        ["original_size"] = data.Length,
                        ["result_size"] = result.Length,
                        ["compression_ratio"] = (double)result.Length / data.Length
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

        private async Task<byte[]> EncryptAsync(byte[] data, string filelessKey, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                var key = Convert.FromBase64String(filelessKey);
                
                using var aes = Aes.Create();
                aes.Key = key;
                aes.GenerateIV();
                aes.Mode = CipherMode.CBC;
                aes.Padding = PaddingMode.PKCS7;
                
                using var encryptor = aes.CreateEncryptor();
                using var msEncrypt = new MemoryStream();
                
                // Write IV first
                msEncrypt.Write(aes.IV, 0, aes.IV.Length);
                
                using var csEncrypt = new CryptoStream(msEncrypt, encryptor, CryptoStreamMode.Write);
                csEncrypt.Write(data, 0, data.Length);
                csEncrypt.FlushFinalBlock();
                
                return msEncrypt.ToArray();
            });
        }

        private async Task<byte[]> DecryptAsync(byte[] encryptedData, string filelessKey, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                var key = Convert.FromBase64String(filelessKey);
                
                using var aes = Aes.Create();
                aes.Key = key;
                aes.Mode = CipherMode.CBC;
                aes.Padding = PaddingMode.PKCS7;
                
                // Extract IV from the beginning of the encrypted data
                var iv = new byte[16];
                Array.Copy(encryptedData, 0, iv, 0, 16);
                aes.IV = iv;
                
                using var decryptor = aes.CreateDecryptor();
                using var msDecrypt = new MemoryStream(encryptedData, 16, encryptedData.Length - 16);
                using var csDecrypt = new CryptoStream(msDecrypt, decryptor, CryptoStreamMode.Read);
                using var msPlain = new MemoryStream();
                
                csDecrypt.CopyTo(msPlain);
                return msPlain.ToArray();
            });
        }

    }
}
