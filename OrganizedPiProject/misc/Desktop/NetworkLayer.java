import com.sun.net.httpserver.*;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

public class NetworkLayer {
    private HttpServer server;
    
    public void start(int port) throws Exception {
        server = HttpServer.create(new InetSocketAddress(port), 0);
        server.setExecutor(Executors.newVirtualThreadPerTaskExecutor());
        
        server.createContext("/api", exchange -> {
            String response = switch (exchange.getRequestMethod()) {
                case "GET" -> handleGet(exchange);
                case "POST" -> handlePost(exchange);
                default -> "Method not allowed";
            };
            
            exchange.sendResponseHeaders(200, response.length());
            exchange.getResponseBody().write(response.getBytes());
            exchange.close();
        });
        
        server.start();
        System.out.println("Server running on port " + port);
    }
    
    private String handleGet(HttpExchange exchange) {
        return "{\"status\":\"running\",\"endpoints\":[\"/api\"]}";
    }
    
    private String handlePost(HttpExchange exchange) {
        return "{\"received\":\"ok\"}";
    }
    
    public void stop() { if (server != null) server.stop(0); }
}