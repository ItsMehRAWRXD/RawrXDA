import java.security.SecureRandom;
import java.security.MessageDigest;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.util.Base64;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

public class CopilotEncryptor {
    private static final String ENCRYPTION_KEY_ENV = "COPILOT_ENCRYPTION_KEY";

    /**
     * Encrypts the given text using AES-256-GCM with a random IV and a key derived from the
     * COPILOT_ENCRYPTION_KEY environment variable.
     * Returns Base64( IV || ciphertext ).
     */
    public String encrypt(String text) throws Exception {
        if (text == null) {
            return null;
        }
        String encryptionKey = System.getenv(ENCRYPTION_KEY_ENV);
        if (encryptionKey == null || encryptionKey.isEmpty()) {
            throw new IllegalStateException("Encryption key not set. Please set "
                + ENCRYPTION_KEY_ENV + " environment variable.");
        }
        byte[] plainBytes = text.getBytes(StandardCharsets.UTF_8);
        byte[] keyHash = MessageDigest.getInstance("SHA-256")
            .digest(encryptionKey.getBytes(StandardCharsets.UTF_8));

        byte[] iv = new byte[12];
        new SecureRandom().nextBytes(iv);
        GCMParameterSpec spec = new GCMParameterSpec(128, iv);
        SecretKeySpec keySpec = new SecretKeySpec(keyHash, "AES");

        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, keySpec, spec);
        byte[] cipherText = cipher.doFinal(plainBytes);

        // Prepend IV
        byte[] output = new byte[12 + cipherText.length];
        System.arraycopy(iv, 0, output, 0, 12);
        System.arraycopy(cipherText, 0, output, 12, cipherText.length);
        return Base64.getEncoder().encodeToString(output);
    }

    /**
     * Decrypts Base64( IV || ciphertext ) from encrypt().
     */
    public String decrypt(String input) throws Exception {
        if (input == null) {
            return null;
        }
        String encryptionKey = System.getenv(ENCRYPTION_KEY_ENV);
        if (encryptionKey == null || encryptionKey.isEmpty()) {
            throw new IllegalStateException("Encryption key not set. Please set "
                + ENCRYPTION_KEY_ENV + " environment variable.");
        }
        byte[] decoded = Base64.getDecoder().decode(input);
        byte[] keyHash = MessageDigest.getInstance("SHA-256")
            .digest(encryptionKey.getBytes(StandardCharsets.UTF_8));

        byte[] iv = Arrays.copyOfRange(decoded, 0, 12);
        byte[] cipherText = Arrays.copyOfRange(decoded, 12, decoded.length);

        GCMParameterSpec spec = new GCMParameterSpec(128, iv);
        SecretKeySpec keySpec = new SecretKeySpec(keyHash, "AES");

        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, keySpec, spec);
        byte[] plainBytes = cipher.doFinal(cipherText);
        return new String(plainBytes, StandardCharsets.UTF_8);
    }
    
    /**
     * Executes a system process with the given text as input.
     * Returns the output as a string.
     */
    public String executeProcess(String command, String text) throws Exception {
        Process p = Runtime.getRuntime().exec(command);
        
        try (OutputStreamWriter writer = new OutputStreamWriter(p.getOutputStream())) {
            writer.write(text);
            writer.flush();
        }
        p.getOutputStream().close();
        
        StringBuilder result = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                result.append(line);
            }
        }
        p.waitFor();
        return result.toString();
    }
    
    /**
     * Helper to convert bytes to hex string
     */
    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }
}
