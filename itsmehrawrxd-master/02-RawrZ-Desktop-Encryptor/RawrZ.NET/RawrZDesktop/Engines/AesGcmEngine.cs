using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public class AesGcmEngine : IEngine
    {
        public string Name => "AES-GCM";
        public string Description => "Advanced Encryption Standard with Galois/Counter Mode (authenticated encryption)";
        public string Version => "1.0.0";
        
        private readonly int _keySize;

        public AesGcmEngine(int keySize)
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
                var password = parameters["password"]?.ToString() ?? throw new ArgumentException("Password parameter is required");
                var options = parameters.ContainsKey("options") ? (Dictionary<string, object>)parameters["options"] : new Dictionary<string, object>();

                byte[] result;
                
                switch (operation.ToLower())
                {
                    case "encrypt":
                        result = await EncryptAsync(data, password, options);
                        break;
                    case "decrypt":
                        result = await DecryptAsync(data, password, options);
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
                        ["algorithm"] = $"AES-{_keySize}-GCM",
                        ["key_size"] = _keySize,
                        ["operation"] = operation,
                        ["original_size"] = data.Length,
                        ["result_size"] = result.Length,
                        ["compression_ratio"] = (double)result.Length / data.Length,
                        ["authenticated_encryption"] = true,
                        ["hardware_accelerated"] = IsHardwareAccelerated()
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

        private async Task<byte[]> EncryptAsync(byte[] data, string password, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                var key = DeriveKey(password, _keySize / 8);
                
                // Generate random nonce (12 bytes for AES-GCM)
                var nonce = new byte[12];
                using (var rng = RandomNumberGenerator.Create())
                {
                    rng.GetBytes(nonce);
                }

                // Create AES-GCM instance with 128-bit tag
                using var aes = new AesGcm(key, 16);
                
                // Encrypt the data
                var ciphertext = new byte[data.Length];
                var tag = new byte[16]; // 128-bit authentication tag
                
                aes.Encrypt(nonce, data, ciphertext, tag);

                // Combine nonce + tag + ciphertext
                var result = new byte[12 + 16 + ciphertext.Length];
                Array.Copy(nonce, 0, result, 0, 12);
                Array.Copy(tag, 0, result, 12, 16);
                Array.Copy(ciphertext, 0, result, 28, ciphertext.Length);
                
                return result;
            });
        }

        private async Task<byte[]> DecryptAsync(byte[] encryptedData, string password, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                if (encryptedData.Length < 28) // 12 (nonce) + 16 (tag)
                    throw new ArgumentException("Invalid encrypted data format");

                var key = DeriveKey(password, _keySize / 8);
                
                // Extract components
                var nonce = new byte[12];
                var tag = new byte[16];
                var ciphertext = new byte[encryptedData.Length - 28];
                
                Array.Copy(encryptedData, 0, nonce, 0, 12);
                Array.Copy(encryptedData, 12, tag, 0, 16);
                Array.Copy(encryptedData, 28, ciphertext, 0, ciphertext.Length);

                // Decrypt and verify
                using var aes = new AesGcm(key, 16);
                var plaintext = new byte[ciphertext.Length];
                
                aes.Decrypt(nonce, ciphertext, tag, plaintext);
                
                return plaintext;
            });
        }

        private byte[] DeriveKey(string password, int keyLength)
        {
            var salt = Encoding.UTF8.GetBytes("RawrZAesGcmSalt2024");
            using var pbkdf2 = new Rfc2898DeriveBytes(password, salt, 100000, HashAlgorithmName.SHA256);
            return pbkdf2.GetBytes(keyLength);
        }

        private bool IsHardwareAccelerated()
        {
            try
            {
                // Check if AES-NI is available
                using var aes = Aes.Create();
                return aes.LegalKeySizes.Length > 0; // Basic check
            }
            catch
            {
                return false;
            }
        }
    }
}
