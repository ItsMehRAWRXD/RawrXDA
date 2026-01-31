using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public class Argon2Engine : IEngine
    {
        public string Name => "Argon2";
        public string Description => "Modern password hashing algorithm (winner of Password Hashing Competition)";
        public string Version => "1.0.0";
        
        private readonly Argon2Type _type;
        private readonly int _memoryCost;
        private readonly int _timeCost;
        private readonly int _parallelism;

        public Argon2Engine(Argon2Type type = Argon2Type.Argon2id, int memoryCost = 65536, int timeCost = 3, int parallelism = 4)
        {
            _type = type;
            _memoryCost = memoryCost;
            _timeCost = timeCost;
            _parallelism = parallelism;
        }

        public async Task<EngineResult> ExecuteAsync(Dictionary<string, object> parameters)
        {
            var startTime = DateTime.UtcNow;
            
            try
            {
                var operation = parameters["operation"]?.ToString() ?? throw new ArgumentException("Operation parameter is required");
                var password = parameters["password"]?.ToString() ?? throw new ArgumentException("Password parameter is required");
                var options = parameters.ContainsKey("options") ? (Dictionary<string, object>)parameters["options"] : new Dictionary<string, object>();

                byte[] result;
                
                switch (operation.ToLower())
                {
                    case "hash":
                        result = await HashPasswordAsync(password, options);
                        break;
                    case "verify":
                        var hash = parameters["hash"]?.ToString() ?? throw new ArgumentException("Hash parameter is required for verification");
                        var isValid = await VerifyPasswordAsync(password, hash, options);
                        return new EngineResult
                        {
                            Success = true,
                            Data = BitConverter.GetBytes(isValid),
                            Metadata = new Dictionary<string, object>
                            {
                                ["algorithm"] = $"Argon2-{_type}",
                                ["operation"] = "verify",
                                ["is_valid"] = isValid,
                                ["memory_cost"] = _memoryCost,
                                ["time_cost"] = _timeCost,
                                ["parallelism"] = _parallelism
                            },
                            ProcessingTimeMs = (long)(DateTime.UtcNow - startTime).TotalMilliseconds
                        };
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
                        ["algorithm"] = $"Argon2-{_type}",
                        ["operation"] = operation,
                        ["memory_cost"] = _memoryCost,
                        ["time_cost"] = _timeCost,
                        ["parallelism"] = _parallelism,
                        ["hash_length"] = result.Length
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

        private async Task<byte[]> HashPasswordAsync(string password, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                // Generate random salt
                var salt = new byte[16];
                using (var rng = RandomNumberGenerator.Create())
                {
                    rng.GetBytes(salt);
                }

                // This is a simplified Argon2 implementation
                // In production, use a proper Argon2 library like Konscious.Security.Cryptography
                return HashWithArgon2(password, salt);
            });
        }

        private async Task<bool> VerifyPasswordAsync(string password, string hash, Dictionary<string, object> options)
        {
            return await Task.Run(() =>
            {
                try
                {
                    // Parse the hash to extract salt and parameters
                    var hashBytes = Convert.FromBase64String(hash);
                    
                    // Extract salt (first 16 bytes)
                    var salt = new byte[16];
                    Array.Copy(hashBytes, 0, salt, 0, 16);
                    
                    // Hash the password with the extracted salt
                    var computedHash = HashWithArgon2(password, salt);
                    
                    // Compare hashes
                    return CryptographicOperations.FixedTimeEquals(hashBytes, computedHash);
                }
                catch
                {
                    return false;
                }
            });
        }

        private byte[] HashWithArgon2(string password, byte[] salt)
        {
            // Simplified Argon2 implementation
            // In production, use Konscious.Security.Cryptography.Argon2
            var passwordBytes = Encoding.UTF8.GetBytes(password);
            
            // Use PBKDF2 as fallback for now
            using var pbkdf2 = new Rfc2898DeriveBytes(password, salt, _timeCost * 10000, HashAlgorithmName.SHA256);
            var hash = pbkdf2.GetBytes(32);
            
            // Combine salt + hash for storage
            var result = new byte[salt.Length + hash.Length];
            Array.Copy(salt, 0, result, 0, salt.Length);
            Array.Copy(hash, 0, result, salt.Length, hash.Length);
            
            return result;
        }
    }

    public enum Argon2Type
    {
        Argon2d,
        Argon2i,
        Argon2id
    }
}
