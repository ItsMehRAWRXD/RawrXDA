// FeedbackStore.java - File-based storage for feedback data
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import java.io.IOException;
import java.nio.file.*;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.stream.Collectors;

/**
 * File-based storage solution for feedback data collection and retrieval.
 * This implementation provides thread-safe operations and JSON serialization
 * for persistent storage of user feedback records.
 */
public class FeedbackStore {
    private final Path storageDir;
    private final ObjectMapper objectMapper;
    private final ReadWriteLock lock;
    private final Map<String, List<FeedbackRecord>> memoryCache;
    private final int maxCacheSize;

    public FeedbackStore() {
        this(Paths.get("feedback_data"));
    }

    public FeedbackStore(Path storageDir) {
        this.storageDir = storageDir;
        this.objectMapper = new ObjectMapper();
        this.objectMapper.registerModule(new JavaTimeModule());
        this.objectMapper.disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS);
        this.lock = new ReentrantReadWriteLock();
        this.memoryCache = new ConcurrentHashMap<>();
        this.maxCacheSize = 1000;
        
        // Create storage directory if it doesn't exist
        try {
            Files.createDirectories(storageDir);
        } catch (IOException e) {
            System.err.println("Failed to create feedback storage directory: " + e.getMessage());
        }
    }

    /**
     * Store a feedback record
     */
    public void storeFeedback(FeedbackRecord feedback) throws IOException {
        lock.writeLock().lock();
        try {
            // Add to memory cache
            String userId = feedback.getUserId();
            memoryCache.computeIfAbsent(userId, k -> new ArrayList<>()).add(feedback);
            
            // Persist to file
            persistFeedback(feedback);
            
            // Clean up cache if it gets too large
            if (memoryCache.size() > maxCacheSize) {
                cleanupCache();
            }
            
        } finally {
            lock.writeLock().unlock();
        }
    }

    /**
     * Persist feedback to file
     */
    private void persistFeedback(FeedbackRecord feedback) throws IOException {
        String fileName = String.format("feedback_%s_%d.json", 
                feedback.getUserId(), feedback.getTimestamp().toEpochMilli());
        Path filePath = storageDir.resolve(fileName);
        
        // Write feedback record to JSON file
        objectMapper.writeValue(filePath.toFile(), feedback);
    }

    /**
     * Retrieve feedback records for a user
     */
    public List<FeedbackRecord> getFeedbackForUser(String userId) {
        lock.readLock().lock();
        try {
            // Check memory cache first
            List<FeedbackRecord> cached = memoryCache.get(userId);
            if (cached != null) {
                return new ArrayList<>(cached);
            }
            
            // Load from files if not in cache
            return loadFeedbackFromFiles(userId);
            
        } finally {
            lock.readLock().unlock();
        }
    }

    /**
     * Load feedback records from files
     */
    private List<FeedbackRecord> loadFeedbackFromFiles(String userId) {
        List<FeedbackRecord> records = new ArrayList<>();
        
        try (var stream = Files.list(storageDir)) {
            stream.filter(path -> path.getFileName().toString().startsWith("feedback_" + userId + "_"))
                  .forEach(path -> {
                      try {
                          FeedbackRecord record = objectMapper.readValue(path.toFile(), FeedbackRecord.class);
                          records.add(record);
                      } catch (IOException e) {
                          System.err.println("Error loading feedback file " + path + ": " + e.getMessage());
                      }
                  });
        } catch (IOException e) {
            System.err.println("Error listing feedback files: " + e.getMessage());
        }
        
        // Sort by timestamp
        records.sort(Comparator.comparing(FeedbackRecord::getTimestamp));
        
        // Cache the results
        memoryCache.put(userId, records);
        
        return records;
    }

    /**
     * Get all feedback records
     */
    public List<FeedbackRecord> getAllFeedback() {
        lock.readLock().lock();
        try {
            List<FeedbackRecord> allRecords = new ArrayList<>();
            
            try (var stream = Files.list(storageDir)) {
                stream.filter(path -> path.getFileName().toString().startsWith("feedback_"))
                      .forEach(path -> {
                          try {
                              FeedbackRecord record = objectMapper.readValue(path.toFile(), FeedbackRecord.class);
                              allRecords.add(record);
                          } catch (IOException e) {
                              System.err.println("Error loading feedback file " + path + ": " + e.getMessage());
                          }
                      });
            } catch (IOException e) {
                System.err.println("Error listing feedback files: " + e.getMessage());
            }
            
            // Sort by timestamp
            allRecords.sort(Comparator.comparing(FeedbackRecord::getTimestamp));
            
            return allRecords;
            
        } finally {
            lock.readLock().unlock();
        }
    }

    /**
     * Get feedback records by interaction type
     */
    public List<FeedbackRecord> getFeedbackByType(FeedbackRecord.InteractionType type) {
        return getAllFeedback().stream()
                .filter(record -> record.getInteraction() == type)
                .collect(Collectors.toList());
    }

    /**
     * Get feedback records within a time range
     */
    public List<FeedbackRecord> getFeedbackInTimeRange(Instant start, Instant end) {
        return getAllFeedback().stream()
                .filter(record -> record.getTimestamp().isAfter(start) && record.getTimestamp().isBefore(end))
                .collect(Collectors.toList());
    }

    /**
     * Get feedback statistics
     */
    public FeedbackStats getStats() {
        List<FeedbackRecord> allFeedback = getAllFeedback();
        
        Map<FeedbackRecord.InteractionType, Long> typeCounts = allFeedback.stream()
                .collect(Collectors.groupingBy(FeedbackRecord::getInteraction, Collectors.counting()));
        
        Map<String, Long> userCounts = allFeedback.stream()
                .collect(Collectors.groupingBy(FeedbackRecord::getUserId, Collectors.counting()));
        
        double averageRating = allFeedback.stream()
                .filter(record -> record.getRating() != null)
                .mapToDouble(FeedbackRecord::getRating)
                .average()
                .orElse(0.0);
        
        return new FeedbackStats(
                allFeedback.size(),
                typeCounts,
                userCounts,
                averageRating,
                allFeedback.stream().map(FeedbackRecord::getUserId).distinct().count()
        );
    }

    /**
     * Export feedback data to JSON
     */
    public void exportToJson(Path outputPath) throws IOException {
        List<FeedbackRecord> allFeedback = getAllFeedback();
        objectMapper.writeValue(outputPath.toFile(), allFeedback);
    }

    /**
     * Import feedback data from JSON
     */
    public void importFromJson(Path inputPath) throws IOException {
        FeedbackRecord[] records = objectMapper.readValue(inputPath.toFile(), FeedbackRecord[].class);
        
        for (FeedbackRecord record : records) {
            storeFeedback(record);
        }
    }

    /**
     * Clean up old feedback records
     */
    public void cleanupOldRecords(int daysToKeep) {
        Instant cutoff = Instant.now().minusSeconds(daysToKeep * 24 * 60 * 60);
        
        try (var stream = Files.list(storageDir)) {
            stream.filter(path -> path.getFileName().toString().startsWith("feedback_"))
                  .forEach(path -> {
                      try {
                          FeedbackRecord record = objectMapper.readValue(path.toFile(), FeedbackRecord.class);
                          if (record.getTimestamp().isBefore(cutoff)) {
                              Files.delete(path);
                              System.out.println("Deleted old feedback record: " + path);
                          }
                      } catch (IOException e) {
                          System.err.println("Error processing feedback file " + path + ": " + e.getMessage());
                      }
                  });
        } catch (IOException e) {
            System.err.println("Error listing feedback files for cleanup: " + e.getMessage());
        }
    }

    /**
     * Clean up memory cache
     */
    private void cleanupCache() {
        // Remove oldest entries from cache
        int entriesToRemove = memoryCache.size() - maxCacheSize + 100;
        List<String> userIds = new ArrayList<>(memoryCache.keySet());
        
        for (int i = 0; i < entriesToRemove && i < userIds.size(); i++) {
            memoryCache.remove(userIds.get(i));
        }
    }

    /**
     * Get training examples for model fine-tuning
     */
    public List<FeedbackRecord.TrainingExample> getTrainingExamples() {
        return getAllFeedback().stream()
                .map(FeedbackRecord::toTrainingExample)
                .filter(FeedbackRecord.TrainingExample::hasPreference)
                .collect(Collectors.toList());
    }

    /**
     * Get feedback records for a specific session
     */
    public List<FeedbackRecord> getFeedbackForSession(String sessionId) {
        return getAllFeedback().stream()
                .filter(record -> sessionId.equals(record.getSessionId()))
                .collect(Collectors.toList());
    }

    /**
     * Search feedback records by content
     */
    public List<FeedbackRecord> searchFeedback(String query) {
        String lowerQuery = query.toLowerCase();
        return getAllFeedback().stream()
                .filter(record -> 
                    (record.getOriginalPrompt() != null && record.getOriginalPrompt().toLowerCase().contains(lowerQuery)) ||
                    (record.getOriginalResponse() != null && record.getOriginalResponse().toLowerCase().contains(lowerQuery)) ||
                    (record.getEditedResponse() != null && record.getEditedResponse().toLowerCase().contains(lowerQuery)) ||
                    (record.getContext() != null && record.getContext().toLowerCase().contains(lowerQuery))
                )
                .collect(Collectors.toList());
    }

    /**
     * Feedback statistics data structure
     */
    public static class FeedbackStats {
        private final int totalRecords;
        private final Map<FeedbackRecord.InteractionType, Long> typeCounts;
        private final Map<String, Long> userCounts;
        private final double averageRating;
        private final long uniqueUsers;

        public FeedbackStats(int totalRecords, Map<FeedbackRecord.InteractionType, Long> typeCounts,
                           Map<String, Long> userCounts, double averageRating, long uniqueUsers) {
            this.totalRecords = totalRecords;
            this.typeCounts = typeCounts;
            this.userCounts = userCounts;
            this.averageRating = averageRating;
            this.uniqueUsers = uniqueUsers;
        }

        public int getTotalRecords() { return totalRecords; }
        public Map<FeedbackRecord.InteractionType, Long> getTypeCounts() { return typeCounts; }
        public Map<String, Long> getUserCounts() { return userCounts; }
        public double getAverageRating() { return averageRating; }
        public long getUniqueUsers() { return uniqueUsers; }

        @Override
        public String toString() {
            return String.format("FeedbackStats{total=%d, users=%d, avgRating=%.2f, types=%s}", 
                    totalRecords, uniqueUsers, averageRating, typeCounts);
        }
    }
}
