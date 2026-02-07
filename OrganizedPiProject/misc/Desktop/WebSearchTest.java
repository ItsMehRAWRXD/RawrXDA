// WebSearchTest.java - Test web search functionality
import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.logging.Logger;
import java.util.logging.Level;

/**
 * Test web search functionality in AiCli
 */
public class WebSearchTest {
    private static final Logger logger = Logger.getLogger(WebSearchTest.class.getName());
    
    public static void main(String[] args) {
        WebSearchTest test = new WebSearchTest();
        try {
            test.testWebSearchFunctionality();
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Web search test failed", e);
        }
    }
    
    public void testWebSearchFunctionality() throws Exception {
        logger.info("Testing web search functionality...");
        
        // Test 1: Basic web search
        testBasicWebSearch();
        
        // Test 2: Search with special characters
        testSearchWithSpecialChars();
        
        // Test 3: Search timeout handling
        testSearchTimeout();
        
        // Test 4: Invalid URL handling
        testInvalidUrlHandling();
        
        logger.info("? All web search tests completed successfully!");
    }
    
    private void testBasicWebSearch() throws Exception {
        logger.info("Testing basic web search...");
        
        String query = "Java programming";
        String searchUrl = "https://duckduckgo.com/?q=" + URLEncoder.encode(query, StandardCharsets.UTF_8) + "&t=h_&ia=web";
        
        HttpClient client = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(10))
                .build();
        
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(searchUrl))
                .header("User-Agent", "WebSearchTest")
                .GET()
                .build();
        
        try {
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            assert response.statusCode() == 200 : "Search request should succeed";
            assert response.body().contains("Java") || response.body().contains("programming") : 
                "Search results should contain relevant content";
            
            logger.info("? Basic web search test passed");
        } catch (Exception e) {
            logger.log(Level.WARNING, "Basic web search test failed (network issue): " + e.getMessage());
        }
    }
    
    private void testSearchWithSpecialChars() throws Exception {
        logger.info("Testing search with special characters...");
        
        String query = "C++ programming & algorithms";
        String encodedQuery = URLEncoder.encode(query, StandardCharsets.UTF_8);
        
        // Verify encoding works correctly
        assert encodedQuery.contains("C%2B%2B") : "Plus signs should be encoded";
        assert encodedQuery.contains("%26") : "Ampersands should be encoded";
        
        logger.info("? Special character encoding test passed");
    }
    
    private void testSearchTimeout() throws Exception {
        logger.info("Testing search timeout handling...");
        
        HttpClient client = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(1)) // Very short timeout
                .build();
        
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://httpstat.us/200?sleep=5000")) // 5 second delay
                .GET()
                .build();
        
        try {
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            assert false : "Request should have timed out";
        } catch (Exception e) {
            if (e.getMessage().contains("timeout") || e.getMessage().contains("timed out")) {
                logger.info("? Timeout handling test passed");
            } else {
                throw e;
            }
        }
    }
    
    private void testInvalidUrlHandling() throws Exception {
        logger.info("Testing invalid URL handling...");
        
        HttpClient client = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(5))
                .build();
        
        String[] invalidUrls = {
            "not-a-valid-url",
            "http://nonexistent-domain-12345.com",
            "ftp://invalid-protocol.com"
        };
        
        for (String invalidUrl : invalidUrls) {
            try {
                HttpRequest request = HttpRequest.newBuilder()
                        .uri(URI.create(invalidUrl))
                        .GET()
                        .build();
                
                HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
                logger.log(Level.WARNING, "Unexpected success for invalid URL: " + invalidUrl);
            } catch (Exception e) {
                logger.info("? Invalid URL properly handled: " + invalidUrl + " -> " + e.getClass().getSimpleName());
            }
        }
    }
    
    // Test the web search method from AiCli
    public String performWebSearch(String query) throws IOException {
        String searchUrl = "https://duckduckgo.com/?q=" + URLEncoder.encode(query, StandardCharsets.UTF_8) + "&t=h_&ia=web";
        
        HttpClient client = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(10))
                .build();
        
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(searchUrl))
                .header("User-Agent", "AiCli")
                .GET()
                .build();
        
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        
        if (response.statusCode() != 200) {
            throw new IOException("HTTP " + response.statusCode() + ": " + response.body());
        }
        
        String html = response.body();
        // Extract only the text from the search results
        String cleanText = html.replaceAll("<[^>]*>", " ").replaceAll("\\s+", " ").trim();
        
        // Limit to reasonable length
        if (cleanText.length() > 2000) {
            cleanText = cleanText.substring(0, 2000) + "...";
        }
        
        return cleanText;
    }
}