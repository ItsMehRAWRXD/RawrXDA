import java.util.ArrayList;
import java.util.List;

/**
 * Universal PHP dork generation: 25+ attack surface patterns.
 * Can optionally XOR-obfuscate dork strings.
 */
public final class UniversalPhpDorker {

    private static final String[] BUILTIN_DORKS = {
        "inurl:.php?id=",
        "inurl:.asp?id=",
        "inurl:.jsp?id=",
        "inurl:.php?cat=",
        "inurl:.php?page=",
        "inurl:.php?item=",
        "inurl:.php?file=",
        "inurl:.php?doc=",
        "inurl:index.php?id=",
        "inurl:view.php?id=",
        "inurl:page.php?id=",
        "inurl:product.php?id=",
        "inurl:article.php?id=",
        "inurl:detail.php?id=",
        "inurl:download.php?file=",
        "inurl:include.php?file=",
        "inurl:admin/login.php",
        "inurl:administrator/login",
        "inurl:wp-content",
        "filetype:sql inurl:backup",
        "filetype:log inurl:log",
        "filetype:bak inurl:backup",
        "intitle:index.of config",
        "intitle:index.of .env",
        "inurl:phpmyadmin",
        "inurl:mysql",
        "inurl:.php?cmd=",
        "inurl:.php?action=",
        "inurl:config.php",
        "inurl:db.php"
    };

    /**
     * Generate universal PHP dorks; optionally obfuscate with XOR.
     */
    public static List<String> generateUniversalDorks(boolean obfuscate) {
        List<String> out = new ArrayList<>();
        for (String dork : BUILTIN_DORKS) {
            if (obfuscate) {
                byte[] enc = XorObfuscator.encode(dork.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                out.add(new String(enc, java.nio.charset.StandardCharsets.ISO_8859_1));
            } else {
                out.add(dork);
            }
        }
        return out;
    }

    public static List<String> generateUniversalDorks() {
        return generateUniversalDorks(false);
    }

    public static int getBuiltinCount() {
        return BUILTIN_DORKS.length;
    }
}
