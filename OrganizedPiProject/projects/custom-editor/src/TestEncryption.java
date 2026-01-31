public class TestEncryption {
    public static void main(String[] args) {
        try {
            CopilotEncryptor enc = new CopilotEncryptor();
            String test = "Hello World";
            String encrypted = enc.encrypt(test);
            String decrypted = enc.decrypt(encrypted);
            System.out.println("Test: " + (test.equals(decrypted) ? "PASS" : "FAIL"));
        } catch (Exception e) {
            System.out.println("Error: " + e.getMessage());
        }
    }
}