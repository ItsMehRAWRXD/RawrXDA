/**
 * XOR encoding/decoding for dork strings (evasion of signature detection).
 * Symmetric: encode then decode with same key restores original.
 */
public final class XorObfuscator {

    private static final int DEFAULT_KEY = 0x5A;

    /**
     * Encode bytes with a single-byte key.
     */
    public static byte[] encode(byte[] input, int key) {
        if (input == null) return null;
        byte[] out = new byte[input.length];
        byte k = (byte) (key & 0xFF);
        for (int i = 0; i < input.length; i++) {
            out[i] = (byte) (input[i] ^ k);
        }
        return out;
    }

    /**
     * Decode bytes (same as encode with same key).
     */
    public static byte[] decode(byte[] input, int key) {
        return encode(input, key);
    }

    /**
     * Encode with key schedule (multi-byte key).
     */
    public static byte[] encodeWithSchedule(byte[] input, byte[] keySchedule) {
        if (input == null || keySchedule == null || keySchedule.length == 0) {
            return input == null ? null : encode(input, DEFAULT_KEY);
        }
        byte[] out = new byte[input.length];
        for (int i = 0; i < input.length; i++) {
            out[i] = (byte) (input[i] ^ keySchedule[i % keySchedule.length]);
        }
        return out;
    }

    public static byte[] encode(byte[] input) {
        return encode(input, DEFAULT_KEY);
    }

    public static byte[] decode(byte[] input) {
        return decode(input, DEFAULT_KEY);
    }
}
