// SecureKeyIntegration.java - Secure way to integrate API keys
// NEVER hardcode API keys - use environment variables and secure storage

import java.util.*;
import java.io.*;
import java.nio.file.*;

public class SecureKeyIntegration {
    
    // NEVER store API keys in source code!
    // Instead, use environment variables and secure storage
    
    public static void main(String[] args) {
        System.out.println("? Secure Key Integration - Proper Way to Handle API Keys");
        System.out.println("=" + "=".repeat(60));
        
        SecureKeyIntegration integration = new SecureKeyIntegration();
        integration.demonstrateSecureIntegration();
    }
    
    public void demonstrateSecureIntegration() {
        System.out.println("\n? SECURE APPROACHES:");
        
        // 1. Environment Variables (Recommended)
        demonstrateEnvironmentVariables();
        
        // 2. External Config Files (Never commit these!)
        demonstrateConfigFiles();
        
        // 3. Encrypted Storage
        demonstrateEncryptedStorage();
        
        // 4. Key Management Services
        demonstrateKeyManagementServices();
        
        // 5. Runtime Input (for testing only)
        demonstrateRuntimeInput();
        
        System.out.println("\n? NEVER DO THIS:");
        demonstrateWhatNotToDo();
    }
    
    private void demonstrateEnvironmentVariables() {
        System.out.println("\n1. ? Environment Variables (RECOMMENDED):");
        
        // Check if keys are set in environment
        String openaiKey = System.getenv("OPENAI_API_KEY");
        String geminiKey = System.getenv("GEMINI_API_KEY");
        
        if (openaiKey != null && !openaiKey.isEmpty()) {
            System.out.println("   ? OPENAI_API_KEY found in environment");
            System.out.println("   ? Key: " + maskKey(openaiKey));
        } else {
            System.out.println("   ??  OPENAI_API_KEY not found in environment");
            System.out.println("   ? Set it with: export OPENAI_API_KEY=your_key_here");
        }
        
        if (geminiKey != null && !geminiKey.isEmpty()) {
            System.out.println("   ? GEMINI_API_KEY found in environment");
            System.out.println("   ? Key: " + maskKey(geminiKey));
        } else {
            System.out.println("   ??  GEMINI_API_KEY not found in environment");
            System.out.println("   ? Set it with: export GEMINI_API_KEY=your_key_here");
        }
        
        System.out.println("   ? Keys never stored in source code");
        System.out.println("   ? Safe for version control");
        System.out.println("   ? Easy to rotate keys");
    }
    
    private void demonstrateConfigFiles() {
        System.out.println("\n2. ? External Config Files (NEVER COMMIT!):");
        
        // Create .env file template
        createEnvTemplate();
        
        // Show how to load from config
        loadFromConfigFile();
        
        System.out.println("   ? Keys stored in external files");
        System.out.println("   ? Add .env to .gitignore");
        System.out.println("   ? Provide .env.example template");
        System.out.println("   ? Team members create their own .env");
    }
    
    private void createEnvTemplate() {
        try {
            // Create .env.example (safe to commit)
            String envExample = """
                # API Keys - Copy this to .env and fill in your keys
                # NEVER commit .env file!
                
                # OpenAI API Key
                OPENAI_API_KEY=your_openai_key_here
                
                # Google Gemini API Key  
                GEMINI_API_KEY=your_gemini_key_here
                
                # Anthropic API Key
                ANTHROPIC_API_KEY=your_anthropic_key_here
                
                # GitHub Token
                GITHUB_TOKEN=your_github_token_here
                
                # Other keys as needed
                # AWS_ACCESS_KEY_ID=your_aws_key
                # STRIPE_SECRET_KEY=your_stripe_key
                """;
            
            Files.write(Paths.get(".env.example"), envExample.getBytes());
            System.out.println("   ? Created .env.example template");
            
            // Check if .env exists
            if (Files.exists(Paths.get(".env"))) {
                System.out.println("   ? .env file found");
            } else {
                System.out.println("   ??  .env file not found - create one from .env.example");
            }
            
        } catch (IOException e) {
            System.err.println("   ? Error creating .env template: " + e.getMessage());
        }
    }
    
    private void loadFromConfigFile() {
        try {
            if (Files.exists(Paths.get(".env"))) {
                List<String> lines = Files.readAllLines(Paths.get(".env"));
                System.out.println("   ? Loading keys from .env file...");
                
                for (String line : lines) {
                    if (line.contains("=") && !line.startsWith("#")) {
                        String[] parts = line.split("=", 2);
                        if (parts.length == 2) {
                            String keyName = parts[0].trim();
                            String keyValue = parts[1].trim();
                            
                            // Set environment variable for this session
                            System.setProperty(keyName, keyValue);
                            System.out.println("   ? Loaded: " + keyName + " = " + maskKey(keyValue));
                        }
                    }
                }
            }
        } catch (IOException e) {
            System.err.println("   ? Error reading .env file: " + e.getMessage());
        }
    }
    
    private void demonstrateEncryptedStorage() {
        System.out.println("\n3. ? Encrypted Storage:");
        
        System.out.println("   ? Encrypted Key Storage Options:");
        System.out.println("   • Java KeyStore (.jks files)");
        System.out.println("   • OS Keychain (macOS/Windows)");
        System.out.println("   • Hardware Security Modules (HSM)");
        System.out.println("   • Cloud Key Management (AWS KMS, Azure Key Vault)");
        
        System.out.println("\n   ? Example Implementation:");
        System.out.println("   • Encrypt keys with master password");
        System.out.println("   • Store encrypted keys locally");
        System.out.println("   • Decrypt only when needed");
        System.out.println("   • Clear from memory after use");
        
        // Simulate encrypted storage
        String encryptedKey = encryptKey("sk-sample-key-12345");
        System.out.println("   ? Encrypted: " + encryptedKey);
        System.out.println("   ? Decrypted: " + maskKey(decryptKey(encryptedKey)));
    }
    
    private void demonstrateKeyManagementServices() {
        System.out.println("\n4. ?? Key Management Services:");
        
        System.out.println("   ? Enterprise Solutions:");
        System.out.println("   • AWS Secrets Manager");
        System.out.println("   • Azure Key Vault");
        System.out.println("   • Google Secret Manager");
        System.out.println("   • HashiCorp Vault");
        
        System.out.println("\n   ? Integration Example:");
        System.out.println("   • Retrieve keys from cloud service");
        System.out.println("   • Cache temporarily in memory");
        System.out.println("   • Automatic key rotation");
        System.out.println("   • Audit logging");
        
        // Simulate cloud key retrieval
        System.out.println("   ?? Retrieving keys from AWS Secrets Manager...");
        System.out.println("   ? Retrieved: OPENAI_API_KEY = " + maskKey("sk-cloud-retrieved-key"));
    }
    
    private void demonstrateRuntimeInput() {
        System.out.println("\n5. ?? Runtime Input (Testing Only):");
        
        System.out.println("   ? For Testing/Development:");
        System.out.println("   • Prompt user for keys at runtime");
        System.out.println("   • Never store in files");
        System.out.println("   • Clear from memory after use");
        System.out.println("   • Only for local development");
        
        // Simulate runtime input (don't actually prompt in this demo)
        System.out.println("   ? Example: Scanner scanner = new Scanner(System.in);");
        System.out.println("   ? Example: String apiKey = scanner.nextLine();");
        System.out.println("   ??  NEVER use this in production!");
    }
    
    private void demonstrateWhatNotToDo() {
        System.out.println("\n? NEVER DO THIS:");
        
        System.out.println("   // DON'T hardcode keys in source:");
        System.out.println("   String openaiKey = \"sk-proj-your-actual-key-here\";");
        System.out.println("   String geminiKey = \"AIza-your-actual-key-here\";");
        
        System.out.println("\n   // DON'T put keys in config files you commit:");
        System.out.println("   // config.properties (committed to git)");
        System.out.println("   openai.key=sk-proj-your-actual-key-here");
        
        System.out.println("\n   // DON'T put keys in comments:");
        System.out.println("   // My API key: sk-proj-your-actual-key-here");
        
        System.out.println("\n   // DON'T put keys in URLs:");
        System.out.println("   String url = \"https://api.openai.com/v1/chat?key=sk-proj-your-actual-key-here\";");
        
        System.out.println("\n   ? These approaches are INSECURE!");
    }
    
    // Helper methods
    private String maskKey(String key) {
        if (key == null || key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
    
    private String encryptKey(String key) {
        // Simple encryption simulation (use proper encryption in real implementation)
        return "ENCRYPTED_" + Base64.getEncoder().encodeToString(key.getBytes());
    }
    
    private String decryptKey(String encryptedKey) {
        // Simple decryption simulation
        if (encryptedKey.startsWith("ENCRYPTED_")) {
            String encoded = encryptedKey.substring(10);
            return new String(Base64.getDecoder().decode(encoded));
        }
        return encryptedKey;
    }
    
    // Integration with our scanners
    public void integrateWithScanners() {
        System.out.println("\n? Integration with Our Scanners:");
        
        System.out.println("   ? Modified Scanner Code:");
        System.out.println("   ```java");
        System.out.println("   // OLD (INSECURE):");
        System.out.println("   // String openaiKey = \"sk-proj-your-key-here\";");
        System.out.println("   ");
        System.out.println("   // NEW (SECURE):");
        System.out.println("   String openaiKey = System.getenv(\"OPENAI_API_KEY\");");
        System.out.println("   if (openaiKey == null) {");
        System.out.println("       System.err.println(\"OPENAI_API_KEY not found in environment\");");
        System.out.println("       return;");
        System.out.println("   }");
        System.out.println("   ```");
        
        System.out.println("\n   ? All scanners now use environment variables");
        System.out.println("   ? No keys hardcoded in source");
        System.out.println("   ? Safe to commit and share code");
        System.out.println("   ? Easy to rotate keys");
    }
    
    // Setup instructions
    public void showSetupInstructions() {
        System.out.println("\n? Setup Instructions:");
        
        System.out.println("\n   1. ? First, rotate your compromised keys:");
        System.out.println("      • Go to OpenAI dashboard ? API Keys ? Create new key");
        System.out.println("      • Go to Google Cloud Console ? Credentials ? Create new key");
        System.out.println("      • Delete the old keys you shared");
        
        System.out.println("\n   2. ? Set environment variables:");
        System.out.println("      Windows:");
        System.out.println("      set OPENAI_API_KEY=your_new_openai_key");
        System.out.println("      set GEMINI_API_KEY=your_new_gemini_key");
        System.out.println("      ");
        System.out.println("      Linux/Mac:");
        System.out.println("      export OPENAI_API_KEY=your_new_openai_key");
        System.out.println("      export GEMINI_API_KEY=your_new_gemini_key");
        
        System.out.println("\n   3. ? Create .env file (optional):");
        System.out.println("      • Copy .env.example to .env");
        System.out.println("      • Fill in your actual keys");
        System.out.println("      • Add .env to .gitignore");
        
        System.out.println("\n   4. ? Run the scanners:");
        System.out.println("      java AIEnhancedKeyscan");
        System.out.println("      java PracticalKeyscanHunt");
        System.out.println("      java RealWebKeyscan");
    }
}
