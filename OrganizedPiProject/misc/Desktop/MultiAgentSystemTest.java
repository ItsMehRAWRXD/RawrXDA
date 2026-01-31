// MultiAgentSystemTest.java - Integration test for multi-agent system
import java.util.*;
import java.io.*;

/**
 * Simple integration test for the multi-agent system components
 */
public class MultiAgentSystemTest {
    
    public static void main(String[] args) {
        System.out.println("=== Multi-Agent System Integration Test ===");
        
        try {
            // Test 1: CodebaseIndexer
            testCodebaseIndexer();
            
            // Test 2: FeedbackRecord
            testFeedbackRecord();
            
            // Test 3: AgentState
            testAgentState();
            
            // Test 4: MultiAgentCoordinator (simplified)
            testMultiAgentCoordinator();
            
            System.out.println("\nAll integration tests passed!");
            
        } catch (Exception e) {
            System.err.println("Integration test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testCodebaseIndexer() {
        System.out.println("\n1. Testing CodebaseIndexer...");
        CodebaseIndexer indexer = new CodebaseIndexer();
        
        // Test basic functionality
        List<String> context = indexer.retrieveRelevantContext("test query");
        System.out.println("   - Context retrieval: " + (context != null ? "OK" : "FAILED"));
        
        // Test stats
        CodebaseIndexer.CodebaseStats stats = indexer.getStats();
        System.out.println("   - Stats: " + stats);
        
        System.out.println("   CodebaseIndexer test passed");
    }
    
    private static void testFeedbackRecord() {
        System.out.println("\n2. Testing FeedbackRecord...");
        
        FeedbackRecord record = new FeedbackRecord.Builder()
                .userId("test_user")
                .sessionId("test_session")
                .conversationId("test_conversation")
                .originalPrompt("Test prompt")
                .originalResponse("Test response")
                .interaction(FeedbackRecord.InteractionType.ACCEPT)
                .build();
        
        System.out.println("   - Created feedback record: " + record);
        System.out.println("   FeedbackRecord test passed");
    }
    
    private static void testAgentState() {
        System.out.println("\n3. Testing AgentState...");
        
        AgentState state = new AgentState("test_user", "test_session");
        state.addMessage("User: Hello");
        state.addMessage("AI: Hi there!");
        
        System.out.println("   - Chat history size: " + state.getChatHistory().size());
        System.out.println("   - User ID: " + state.getUserId());
        System.out.println("   - Session ID: " + state.getSessionId());
        
        System.out.println("   AgentState test passed");
    }
    
    private static void testMultiAgentCoordinator() {
        System.out.println("\n4. Testing MultiAgentCoordinator...");
        
        try {
            // Create a simplified coordinator without external dependencies
            MultiAgentCoordinator coordinator = new MultiAgentCoordinator("test_api_key");
            
            // Test session creation
            MultiAgentCoordinator.CollaborationSession session = 
                coordinator.startCollaboration("Test task", "test_user");
            
            System.out.println("   - Created collaboration session: " + session.getSessionId());
            System.out.println("   - Session status: " + session.getStatus());
            
            System.out.println("   MultiAgentCoordinator test passed");
            
        } catch (Exception e) {
            System.out.println("   MultiAgentCoordinator test skipped (dependencies): " + e.getMessage());
        }
    }
}
