import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.stream.Collectors;

public class AIGenerator {

    private static final int MAX_ITERATIONS = 5; // Limits iterations to prevent infinite loops.

    private static String askAI(String prompt, String language, String url, String auth, String previousResponse) {
        try {
            String fullPrompt;
            if (previousResponse != null && !previousResponse.isEmpty()) {
                fullPrompt = String.format(
                    "The previous code example in %s was: ```%s```. Please review, and continue or refine the example based on the following instruction: %s",
                    language,
                    previousResponse,
                    prompt
                );
            } else {
                fullPrompt = String.format(
                    "Generate a clear, commented, and concise code example in %s for the following task: %s",
                    language,
                    prompt
                );
            }

            URL apiEndpoint = new URL(url + "?key=" + URLEncoder.encode(auth, StandardCharsets.UTF_8));
            HttpURLConnection conn = (HttpURLConnection) apiEndpoint.openConnection();
            conn.setRequestMethod("POST");
            conn.setRequestProperty("Content-Type", "application/json");
            conn.setDoOutput(true);

            String jsonPayload = String.format(
                "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}",
                fullPrompt.replace("\\", "\\\\").replace("\"", "\\\"")
            );

            try (OutputStream os = conn.getOutputStream()) {
                byte[] input = jsonPayload.getBytes(StandardCharsets.UTF_8);
                os.write(input, 0, input.length);
            }

            int responseCode = conn.getResponseCode();
            if (responseCode >= 400) {
                try (BufferedReader br = new BufferedReader(new InputStreamReader(conn.getErrorStream()))) {
                    String errorResponse = br.lines().collect(Collectors.joining());
                    return String.format("AI call failed (HTTP code: %d). Details: %s", responseCode, errorResponse);
                }
            }

            try (BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream(), StandardCharsets.UTF_8))) {
                StringBuilder response = new StringBuilder();
                String responseLine;
                while ((responseLine = br.readLine()) != null) {
                    response.append(responseLine.trim());
                }

                String jsonResponse = response.toString();
                // Simple parsing for the AI's text content, assuming the response structure
                int textStart = jsonResponse.indexOf("\"text\":\"") + 8;
                if (textStart > 8) {
                    int textEnd = jsonResponse.indexOf("\"", textStart);
                    if (textEnd != -1) {
                        return jsonResponse.substring(textStart, textEnd).replace("\\n", "\n").replace("\\\"", "\"");
                    }
                }
                return "AI call failed (invalid or missing response).";
            }
        } catch (Exception e) {
            return "AI call failed: " + e.getMessage();
        }
    }

    public static void main(String[] args) {
        String prompt = null;
        String language = "java"; // Default language
        boolean loop = false;
        String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent";
        String auth = null;

        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("--prompt") && i + 1 < args.length) {
                prompt = args[++i];
            } else if (args[i].equals("--lang") && i + 1 < args.length) {
                language = args[++i];
            } else if (args[i].equals("--loop")) {
                loop = true;
            } else if (args[i].equals("--api") && i + 1 < args.length) {
                url = args[++i];
            } else if (args[i].equals("--auth") && i + 1 < args.length) {
                auth = args[++i];
            }
        }

        if (prompt == null) {
            System.out.println("Please provide a prompt using --prompt.");
            System.exit(1);
        }
        if (auth == null) {
            System.out.println("Please provide an API key using --auth.");
            System.exit(1);
        }

        System.out.println("Generating example in " + language + " for: \"" + prompt + "\"");
        System.out.println("----------------------------------------");
        
        String previousResponse = null;
        int iterations = 0;

        if (loop) {
            while (iterations < MAX_ITERATIONS) {
                System.out.println("\n--- Iteration " + (iterations + 1) + " ---");
                String aiResponse = askAI(prompt, language, url, auth, previousResponse);
                System.out.println(aiResponse);
                previousResponse = aiResponse;
                iterations++;
            }
        } else {
            String aiResponse = askAI(prompt, language, url, auth, previousResponse);
            System.out.println(aiResponse);
        }
        
        System.out.println("\n----------------------------------------");
    }
}
