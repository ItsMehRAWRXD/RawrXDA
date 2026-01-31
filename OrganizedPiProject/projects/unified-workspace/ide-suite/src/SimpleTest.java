public class SimpleTest {
    public static void main(String[] args) {
        try {
            ProcessBuilder pb = new ProcessBuilder("openssl", "version");
            Process p = pb.start();
            p.waitFor();
            System.out.println("OpenSSL available: " + (p.exitValue() == 0));
        } catch (Exception e) {
            System.out.println("OpenSSL not found: " + e.getMessage());
        }
    }
}