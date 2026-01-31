import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public class SecurityHardening {
    
    // ✅ FIX: Environment-based credentials
    private static String getApiKey(String provider) {
        String key = System.getenv(provider.toUpperCase() + "_API_KEY");
        if (key == null) throw new SecurityException("API key not found in environment");
        return key;
    }
    
    // ✅ FIX: Secure IV generation
    private static byte[] generateSecureIV() {
        byte[] iv = new byte[12]; // GCM standard
        new SecureRandom().nextBytes(iv);
        return iv;
    }
    
    // ✅ FIX: Authenticated encryption (AES-GCM)
    public static byte[] secureEncrypt(byte[] data, byte[] key) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
        
        byte[] iv = generateSecureIV();
        GCMParameterSpec gcmSpec = new GCMParameterSpec(128, iv);
        
        cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmSpec);
        byte[] encrypted = cipher.doFinal(data);
        
        // Prepend IV to encrypted data
        byte[] result = new byte[iv.length + encrypted.length];
        System.arraycopy(iv, 0, result, 0, iv.length);
        System.arraycopy(encrypted, 0, result, iv.length, encrypted.length);
        
        return result;
    }
    
    // ✅ FIX: Process exit code validation
    private static void validateProcessResult(Process process) throws Exception {
        int exitCode = process.waitFor();
        if (exitCode != 0) {
            throw new RuntimeException("Process failed with exit code: " + exitCode);
        }
    }
}