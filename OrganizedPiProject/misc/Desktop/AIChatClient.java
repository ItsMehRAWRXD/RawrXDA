import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.stream.Collectors;

public class AIChatClient {

    private static final ObjectMapper objectMapper = new ObjectMapper();

    private static void usage() {
        String u = """
            Usage:
              java AIChatClient --ask "prompt" [--auth KEY|--env] [--model gemini-pro] [--system "text"] [--json]
              java AIChatClient --search "query" [--auth KEY|--env] [--model gemini-pro] [--system "text"] [--json]
              java AIChatClient --ask "..." --search "..." ...

            Flags:
              --ask "text"        Ask the AI directly.
              --search "query"    Ask the AI to synthesize an answer as if from search results.
              --auth "KEY"        API key explicitly.
              --env               Read API key from GEMINI_API_KEY env var (ignored if --auth is present).
              --model "NAME"      Model name (default: gemini-pro). Examples: gemini-1.5-flash, gemini-1.5-pro.
              --api "URL"         Override API endpoint (default built from model).
              --system "text"     Optional system instruction.
              --timeout N         HTTP timeout in seconds (default: 30).
              --json              Print the raw JSON response instead of extracted text.

            Notes:
              - Exits with code 1 for usage errors, 2 for HTTP/parse failures.
            """;
        System.err.println(u);
    }

    private static String buildUrl(String model, String api) {
        if (api != null && !api.isEmpty()) {
            return api;
        }
        return "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    }

    private static void aiCall(String prompt, String system, String url, String apiKey, int timeout, boolean wantJson) {
        try {
            // Build the JSON body
            JsonNode body;
            if (system != null && !system.isEmpty()) {
                body = objectMapper.createObjectNode()
                    .putArray("contents").addObject().putArray("parts").addObject().put("text", prompt).end()
                    .end()
                    .set("systemInstruction", objectMapper.createObjectNode()
                        .putArray("parts").addObject().put("text", system).end()
                        .end());
            } else {
                body = objectMapper.createObjectNode()
                    .putArray("contents").addObject().putArray("parts").addObject().put("text", prompt).end()
                    .end();
            }
            String jsonPayload = objectMapper.writeValueAsString(body);

            // Append key to URL
            String finalUrl = url + "?key=" + URLEncoder.encode(apiKey, StandardCharsets.UTF_8);

            // Setup HTTP request
            HttpURLConnection conn = (HttpURLConnection) new URL(finalUrl).openConnection();
            conn.setRequestMethod("POST");
            conn.setRequestProperty("Content-Type", "application/json");
            conn.setDoOutput(true);
            conn.setConnectTimeout(timeout * 1000);
            conn.setReadTimeout(timeout * 1000);

            // Write payload
            try (OutputStream os = conn.getOutputStream()) {
                byte[] input = jsonPayload.getBytes(StandardCharsets.UTF_8);
                os.write(input, 0, input.length);
            }

            int responseCode = conn.getResponseCode();
            if (responseCode >= 400) {
                try (BufferedReader br = new BufferedReader(new InputStreamReader(conn.getErrorStream()))) {
                    String serverError = br.lines().collect(Collectors.joining());
                    System.err.printf("AI call failed (HTTP %d). Server said:%n%s%n", responseCode, serverError);
                }
                System.exit(2);
            }

            // Read response
            try (BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8))) {
                String response = br.lines().collect(Collectors.joining());
                if (wantJson) {
                    System.out.println(response);
                } else {
                    String text = extractText(objectMapper.readTree(response));
                    if (text == null) {
                        System.err.println("AI call returned no text candidates.");
                        System.exit(2);
                    }
                    System.out.println(text);
                }
            }
        } catch (Exception e) {
            System.err.printf("AI call failed (network/IO): %s%n", e.getMessage());
            System.exit(2);
        }
    }

    private static String extractText(JsonNode json) {
        if (json.has("candidates") && json.get("candidates").isArray() && !json.get("candidates").isEmpty()) {
            JsonNode candidate = json.get("candidates").get(0);
            if (candidate.has("content") && candidate.get("content").has("parts") && candidate.get("content").get("parts").isArray()) {
                StringBuilder texts = new StringBuilder();
                for (JsonNode part : candidate.get("content").get("parts")) {
                    if (part.has("text") && part.get("text").isTextual()) {
                        texts.append(part.get("text").asText());
                    }
                }
                return texts.toString();
            }
        }
        return null;
    }

    public static void main(String[] args) {
        String prompt = null;
        String search = null;
        String auth = null;
        boolean useEnv = false;
        String model = "gemini-pro";
        String api = null;
        String system = null;
        int timeout = 30;
        boolean wantJson = false;

        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--ask":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    prompt = args[++i];
                    break;
                case "--search":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    search = args[++i];
                    break;
                case "--auth":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    auth = args[++i];
                    break;
                case "--env":
                    useEnv = true;
                    break;
                case "--model":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    model = args[++i];
                    break;
                case "--api":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    api = args[++i];
                    break;
                case "--system":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    system = args[++i];
                    break;
                case "--timeout":
                    if (i + 1 >= args.length) { usage(); System.exit(1); }
                    timeout = Math.max(1, Integer.parseInt(args[++i]));
                    break;
                case "--json":
                    wantJson = true;
                    break;
                case "-h":
                case "--help":
                    usage();
                    System.exit(0);
                default:
                    System.err.println("Unknown flag: " + arg);
                    usage();
                    System.exit(1);
            }
        }

        if (prompt == null && search == null) {
            System.err.println("Please provide a prompt using --ask, a search query using --search, or both.");
            usage();
            System.exit(1);
        }

        if (auth == null && useEnv) {
            String envKey = System.getenv("GEMINI_API_KEY");
            if (envKey != null && !envKey.isEmpty()) {
                auth = envKey;
            }
        }
        if (auth == null) {
            System.err.println("Missing API key. Use --auth KEY or --env with GEMINI_API_KEY set.");
            System.exit(1);
        }

        String finalUrl = buildUrl(model, api);

        if (prompt != null) {
            System.out.println("AI reply:");
            aiCall(prompt, system, finalUrl, auth, timeout, wantJson);
            System.out.println();
        }

        if (search != null) {
            System.out.println("AI Search Results:");
            String searchPrompt = "Using your general knowledge and current reasoning, synthesize a concise answer for: " + search;
            aiCall(searchPrompt, system, finalUrl, auth, timeout, wantJson);
            System.out.println();
        }
    }
}
