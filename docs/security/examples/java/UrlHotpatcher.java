import java.util.ArrayList;
import java.util.List;

/**
 * URL hotpatching for test variant discovery: inject [t], [d], _test, _bak markers.
 */
public final class UrlHotpatcher {

    private static final String[] DEFAULT_MARKERS = { "_test", "_bak", ".bak", "[t]", "[d]" };

    /**
     * Apply a single marker to the URL (e.g. before query string or after path).
     */
    public static String applyHotpatch(String url, String marker) {
        if (url == null || url.isEmpty() || marker == null || marker.isEmpty()) {
            return url == null ? "" : url;
        }
        int q = url.indexOf('?');
        int insert = (q >= 0) ? q : url.length();
        String path = (q >= 0) ? url.substring(0, insert) : url;
        String rest = (q >= 0) ? url.substring(insert) : "";
        int slash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (slash >= 0 && slash + 1 < path.length()) {
            String base = path.substring(0, slash + 1);
            String file = path.substring(slash + 1);
            return base + file + marker + rest;
        }
        return path + marker + rest;
    }

    /**
     * Return default markers for variant discovery.
     */
    public static List<String> getDefaultMarkers() {
        List<String> list = new ArrayList<>();
        for (String m : DEFAULT_MARKERS) {
            list.add(m);
        }
        return list;
    }
}
