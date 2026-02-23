import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * RawrXD_SecureQueryProcessor — LDOAGTIAC-compliant secure query engine.
 * Logic (template) + Data (bound only) = Logic Data Only Allows Genuine Transactions In All Cases.
 *
 * Features:
 * - 8-table whitelist: users, carts, products, orders, sessions, audit_log, system_config, app_logs
 * - Query timeouts
 * - SHA-256 audit logging
 * - Sensitive masking (password, token, secret, key fields)
 * - Cart ID parser: onecart:tablename:id or onecart:custblnamehttp
 * - PreparedStatement timeout
 */
public final class RawrXD_SecureQueryProcessor {

    private static final int DEFAULT_QUERY_TIMEOUT_SEC = 5;
    private static final int MAX_CART_ID_LENGTH = 256;

    private static final Set<String> TABLE_WHITELIST = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
            "users", "carts", "products", "orders", "sessions", "audit_log", "system_config", "app_logs"
    )));

    private static final Set<String> COLUMN_WHITELIST = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
            "id", "userid", "cartid", "custid", "productid", "orderid", "sessionid",
            "name", "email", "created_at", "updated_at", "status", "amount", "quantity"
    )));

    private static final Set<String> SENSITIVE_PATTERNS = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
            "password", "passwd", "pwd", "token", "secret", "apikey", "api_key", "auth"
    )));

    private final int queryTimeoutSec;
    private final AuditLogger auditLogger;

    public RawrXD_SecureQueryProcessor() {
        this(DEFAULT_QUERY_TIMEOUT_SEC, null);
    }

    public RawrXD_SecureQueryProcessor(int queryTimeoutSec, AuditLogger auditLogger) {
        this.queryTimeoutSec = Math.max(1, Math.min(60, queryTimeoutSec));
        this.auditLogger = auditLogger;
    }

    public interface AuditLogger {
        void log(String hashHex, String queryTemplate, String table, long durationMs);
    }

    public static boolean isTableAllowed(String table) {
        return table != null && TABLE_WHITELIST.contains(table.toLowerCase().trim());
    }

    public static boolean isColumnAllowed(String column) {
        if (column == null) return false;
        String c = column.toLowerCase().trim();
        int dot = c.indexOf('.');
        if (dot >= 0 && dot + 1 < c.length()) c = c.substring(dot + 1);
        return COLUMN_WHITELIST.contains(c);
    }

    public static String maskSensitive(String columnName, String value) {
        if (value == null) return "***";
        String col = columnName == null ? "" : columnName.toLowerCase();
        for (String p : SENSITIVE_PATTERNS) {
            if (col.contains(p)) return "***";
        }
        return value;
    }

    public static String sha256Hash(String input) {
        if (input == null) return "";
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] hash = md.digest(input.getBytes(StandardCharsets.UTF_8));
            StringBuilder sb = new StringBuilder(hash.length * 2);
            for (byte b : hash) sb.append(String.format("%02x", b));
            return sb.toString();
        } catch (NoSuchAlgorithmException e) {
            return Integer.toHexString(input.hashCode());
        }
    }

    /**
     * Parse cart ID format: onecart:tablename:id or onecart:custblnamehttp (legacy).
     */
    public static CartIdParsed parseCartId(String cartParam) {
        CartIdParsed out = new CartIdParsed();
        if (cartParam == null || cartParam.isEmpty() || cartParam.length() > MAX_CART_ID_LENGTH) {
            return out;
        }
        if (!cartParam.toLowerCase().startsWith("onecart:")) {
            out.rawId = cartParam;
            out.valid = true;
            return out;
        }
        String rest = cartParam.substring(8);
        int colon = rest.indexOf(':');
        if (colon >= 0) {
            out.tableHint = rest.substring(0, colon);
            out.idPart = rest.substring(colon + 1);
        } else {
            out.tableHint = rest;
            out.idPart = "";
        }
        out.rawId = cartParam;
        out.valid = true;  // parse always succeeds; whitelist enforced at query time
        return out;
    }

    public static class CartIdParsed {
        public String rawId;
        public String tableHint = "";
        public String idPart = "";
        public boolean valid;
    }

    /**
     * Fetch cart by ID. Handles onecart:customers:67890 and onecart:custblnamehttp.
     */
    public ResultSet fetchByCartId(Connection conn, String cartId) throws SQLException {
        CartIdParsed parsed = parseCartId(cartId);
        String table = (parsed.tableHint != null && isTableAllowed(parsed.tableHint)) ? parsed.tableHint : "carts";
        String idVal = (parsed.idPart != null && !parsed.idPart.isEmpty()) ? parsed.idPart : parsed.rawId;
        if (idVal == null || idVal.isEmpty()) idVal = parsed.rawId;

        String sql = "SELECT cartid, custid FROM " + table + " WHERE cartid = ? LIMIT 1";
        if (!isTableAllowed(table)) {
            sql = "SELECT cartid, custid FROM carts WHERE cartid = ? LIMIT 1";
        }

        PreparedStatement ps = conn.prepareStatement(sql);
        try {
            ps.setQueryTimeout(queryTimeoutSec);
            ps.setString(1, idVal);
            long start = System.currentTimeMillis();
            ResultSet rs = ps.executeQuery();
            long dur = System.currentTimeMillis() - start;
            if (auditLogger != null) {
                String hash = sha256Hash(sql + "|" + idVal);
                auditLogger.log(hash, sql, table, dur);
            }
            return rs;
        } catch (SQLException e) {
            if (auditLogger != null) {
                String hash = sha256Hash(sql + "|" + idVal + "|ERROR");
                auditLogger.log(hash, sql, table, -1);
            }
            throw e;
        }
    }

    /**
     * Execute a parameterized query with whitelist enforcement and timeout.
     */
    public ResultSet executeSecure(Connection conn, String table, String[] columns, String whereColumn, String whereValue)
            throws SQLException {
        if (!isTableAllowed(table)) {
            throw new SecurityException("Table not in whitelist: " + table);
        }
        for (String col : columns) {
            if (!isColumnAllowed(col)) {
                throw new SecurityException("Column not in whitelist: " + col);
            }
        }
        if (!isColumnAllowed(whereColumn)) {
            throw new SecurityException("Where column not in whitelist: " + whereColumn);
        }

        StringBuilder sb = new StringBuilder("SELECT ");
        for (int i = 0; i < columns.length; i++) {
            if (i > 0) sb.append(", ");
            sb.append(columns[i]);
        }
        sb.append(" FROM ").append(table).append(" WHERE ").append(whereColumn).append(" = ? LIMIT 1");
        String sql = sb.toString();

        PreparedStatement ps = conn.prepareStatement(sql);
        ps.setQueryTimeout(queryTimeoutSec);
        ps.setString(1, whereValue);
        long start = System.currentTimeMillis();
        ResultSet rs = ps.executeQuery();
        long dur = System.currentTimeMillis() - start;
        if (auditLogger != null) {
            String hash = sha256Hash(sql + "|" + maskSensitive(whereColumn, whereValue));
            auditLogger.log(hash, sql, table, dur);
        }
        return rs;
    }

    /**
     * LDOAGTIAC: Process cart parameter via single template (CartQueryProcessor compatibility).
     */
    public void processOneCartParameter(Connection conn, String cartParam) throws SQLException {
        if (conn == null || cartParam == null || cartParam.isEmpty()) return;
        try (ResultSet rs = fetchByCartId(conn, cartParam)) {
            while (rs.next()) {
                String cartId = rs.getString("cartid");
                String custId = rs.getString("custid");
                if (auditLogger != null) {
                    String hash = sha256Hash("cart|" + maskSensitive("cartid", cartId) + "|" + maskSensitive("custid", custId));
                    auditLogger.log(hash, "SELECT cartid, custid FROM carts WHERE cartid = ? LIMIT 1", "carts", 0);
                }
            }
        }
    }
}
