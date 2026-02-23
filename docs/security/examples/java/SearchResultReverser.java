import java.util.Base64;

/**
 * Result decoding (Base64 + XOR) and vulnerability detection with severity scoring.
 */
public final class SearchResultReverser {

    public static final String VERDICT_SAFE = "safe";
    public static final String VERDICT_SUSPICIOUS = "suspicious";
    public static final String VERDICT_VULNERABLE = "vulnerable";

    public static class Result {
        public final String verdict;
        public final int severity;
        public final String decodedSnippet;

        public Result(String verdict, int severity, String decodedSnippet) {
            this.verdict = verdict;
            this.severity = severity;
            this.decodedSnippet = decodedSnippet == null ? "" : decodedSnippet;
        }
    }

    private static final String[] SQL_ERROR_SIGS = {
        "SQL syntax", "mysql_fetch", "Warning: mysql_", "pg_query()", "Microsoft SQL Server", "ORA-"
    };

    /**
     * Analyze response body: decode if needed, run vulnerability heuristics, return verdict and severity (0–10).
     */
    public static Result analyzeResult(String url, String responseBody) {
        if (responseBody == null) responseBody = "";
        String decoded = tryDecode(responseBody);
        String snippet = decoded.length() > 256 ? decoded.substring(0, 256) + "..." : decoded;

        for (String sig : SQL_ERROR_SIGS) {
            if (decoded.contains(sig)) {
                return new Result(VERDICT_VULNERABLE, 8, snippet);
            }
        }
        if (decoded.contains("error") || decoded.contains("Exception")) {
            return new Result(VERDICT_SUSPICIOUS, 4, snippet);
        }
        return new Result(VERDICT_SAFE, 0, snippet);
    }

    /**
     * Try Base64 decode; on failure return original.
     */
    private static String tryDecode(String s) {
        try {
            byte[] decoded = Base64.getDecoder().decode(s.trim());
            if (decoded != null && decoded.length > 0) {
                return new String(decoded, java.nio.charset.StandardCharsets.UTF_8);
            }
        } catch (IllegalArgumentException ignored) {
            // not valid Base64
        }
        return s;
    }
}
