import javax.json.*;
import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

/**
 * AiChat – Cursor-style REPL for Gemini.
 *  javac AiChat.java
 *  GEMINI_API_KEY=xxx java AiChat
 *
 * Editor binding (Linux example):
 *  xsel -bo | GEMINI_API_KEY=xxx java AiChat --stdin --quiet | xsel -bi
 */
public class AiChat {

    /* ---------- config ---------- */
    private static final HttpClient HTTP = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(20))
            .build();

    private static final String KEY   = System.getenv("GEMINI_API_KEY");
    private static final String MODEL = System.getenv().getOrDefault("GEMINI_MODEL", "gemini-1.5-flash");

    private final List<JsonObject> history = new ArrayList<>();
    private final CliArgs args;

    /* ---------- POJO for CLI flags ---------- */
    private static class CliArgs {
        boolean stdin     = false;
        boolean jsonOut   = false;
        boolean quiet     = false;
        String  system    = null;
        Double  temp      = null;
        Integer topK      = null;
        Integer maxTokens = null;
        int retries       = 3;
        int backoffBase   = 400; // ms
    }

    /* ---------- main ---------- */
    public static void main(String[] _args) {
        if (KEY == null || KEY.isBlank()) { System.err.println("Set GEMINI_API_KEY"); System.exit(1); }
        CliArgs cfg = parseCli(_args);
        try { new AiChat(cfg).run(); }
        catch (IOException | InterruptedException e) {
            System.err.println("Fatal: " + e.getMessage());
            System.exit(2);
        }
    }

    /* ---------- ctor / run ---------- */
    private AiChat(CliArgs cfg) { this.args = cfg; }

    private void run() throws IOException, InterruptedException {
        if (args.stdin) {  // one-shot pipe mode
            String prompt = readStdin();
            if (prompt.isBlank()) return;
            String reply  = ask(prompt);
            System.out.print(args.jsonOut ? reply : reply);
            return;
        }
        // interactive REPL
        if (!args.quiet) System.out.println("=== AI Chat (Gemini) ===  (empty line or Ctrl-D to quit)\n");
        BufferedReader br = new BufferedReader(new InputStreamReader(System.in));
        while (true) {
            if (!args.quiet) System.out.print("> ");
            String line = br.readLine();
            if (line == null || line.strip().isEmpty()) break;
            String reply = ask(line.strip());
            if (!args.quiet) System.out.println(reply + "\n");
            else             System.out.print(reply);
        }
        if (!args.quiet) System.out.println("Bye.");
    }

    /* ---------- core ask ---------- */
    private String ask(String prompt) throws IOException, InterruptedException {
        JsonArrayBuilder contents = Json.createArrayBuilder();
        history.forEach(contents::add);
        contents.add(Json.createObjectBuilder()
                .add("role", "user")
                .add("parts", Json.createArrayBuilder()
                        .add(Json.createObjectBuilder().add("text", prompt))));

        JsonObjectBuilder payload = Json.createObjectBuilder()
                .add("contents", contents);

        if (args.system != null)
            payload.add("systemInstruction", Json.createObjectBuilder()
                    .add("parts", Json.createArrayBuilder()
                            .add(Json.createObjectBuilder().add("text", args.system))));

        JsonObjectBuilder genCfg = Json.createObjectBuilder();
        if (args.temp      != null) genCfg.add("temperature", args.temp);
        if (args.topK      != null) genCfg.add("topK", args.topK);
        if (args.maxTokens != null) genCfg.add("maxOutputTokens", args.maxTokens);
        payload.add("generationConfig", genCfg);

        String body = payload.build().toString();
        String url  = "https://generativelanguage.googleapis.com/v1beta/models/" +
                      MODEL + ":generateContent?key=" +
                      URLEncoder.encode(KEY, StandardCharsets.UTF_8);

        int attempt = 0;
        while (true) {
            HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(body))
                    .build();
            HttpResponse<String> res = HTTP.send(req, HttpResponse.BodyHandlers.ofString());

            if (res.statusCode() == 200) break;
            if (res.statusCode() >= 500 || res.statusCode() == 429) {
                if (++attempt > args.retries) throw new IOException("HTTP " + res.statusCode() + ": " + res.body());
                Thread.sleep(args.backoffBase * (1L << (attempt - 1)));
                continue;
            }
            throw new IOException("HTTP " + res.statusCode() + ": " + res.body());
        }

        JsonObject root = Json.createReader(new StringReader(res.body())).readObject();
        JsonArray candidates = root.getJsonArray("candidates");
        if (candidates == null || candidates.isEmpty()) return "No reply.";

        String text = candidates.getJsonObject(0)
                                .getJsonObject("content")
                                .getJsonArray("parts")
                                .getJsonObject(0)
                                .getString("text", "");

        // keep history
        history.add(Json.createObjectBuilder()
                .add("role", "user")
                .add("parts", Json.createArrayBuilder()
                        .add(Json.createObjectBuilder().add("text", prompt)))
                .build());
        history.add(Json.createObjectBuilder()
                .add("role", "model")
                .add("parts", Json.createArrayBuilder()
                        .add(Json.createObjectBuilder().add("text", text)))
                .build());

        return text;
    }

    /* ---------- helpers ---------- */
    private static String readStdin() throws IOException {
        try (BufferedReader br = new BufferedReader(new InputStreamReader(System.in))) {
            return br.lines().collect(java.util.stream.Collectors.joining("\n"));
        }
    }

    private static CliArgs parseCli(String[] a) {
        CliArgs c = new CliArgs();
        for (int i = 0; i < a.length; i++) {
            switch (a[i]) {
                case "--stdin"       -> c.stdin     = true;
                case "--json"        -> c.jsonOut   = true;
                case "--quiet"       -> c.quiet     = true;
                case "--system"      -> c.system    = next(a, i++);
                case "--temperature" -> c.temp      = Double.parseDouble(next(a, i++));
                case "--topK"        -> c.topK      = Integer.parseInt(next(a, i++));
                case "--maxTokens"   -> c.maxTokens = Integer.parseInt(next(a, i++));
                case "--retries"     -> c.retries   = Integer.parseInt(next(a, i++));
                case "--backoff"     -> c.backoffBase = Integer.parseInt(next(a, i++));
                default -> {
                    System.err.println("Unknown arg: " + a[i]);
                    System.exit(1);
                }
            }
        }
        return c;
    }
    private static String next(String[] a, int idx) {
        if (idx + 1 >= a.length) throw new IllegalArgumentException("Missing value for " + a[idx]);
        return a[idx + 1];
    }
}
