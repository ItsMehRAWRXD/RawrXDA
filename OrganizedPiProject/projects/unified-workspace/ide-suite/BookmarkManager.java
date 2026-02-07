import java.util.*;
import java.io.*;
import java.nio.file.*;

public class BookmarkManager {
    private final Map<String, List<Integer>> bookmarks = new HashMap<>();
    private final String bookmarksFile = "bookmarks.dat";
    
    public BookmarkManager() {
        loadBookmarks();
    }
    
    public void toggleBookmark(String file, int line) {
        bookmarks.computeIfAbsent(file, k -> new ArrayList<>());
        List<Integer> fileBookmarks = bookmarks.get(file);
        if (fileBookmarks.contains(line)) {
            fileBookmarks.remove(Integer.valueOf(line));
        } else {
            fileBookmarks.add(line);
            Collections.sort(fileBookmarks);
        }
        saveBookmarks();
    }
    
    public List<Integer> getBookmarks(String file) {
        return bookmarks.getOrDefault(file, new ArrayList<>());
    }
    
    private void loadBookmarks() {
        try {
            if (Files.exists(Paths.get(bookmarksFile))) {
                // Load implementation
            }
        } catch (Exception e) {}
    }
    
    private void saveBookmarks() {
        try {
            // Save implementation
        } catch (Exception e) {}
    }
    
    public static void saveBookmarks(Map<String, List<Integer>> bookmarksMap) {
        try (ObjectOutputStream oos = new ObjectOutputStream(new FileOutputStream("bookmarks.dat"))) {
            oos.writeObject(bookmarksMap);
        } catch (Exception e) {
            System.err.println("Failed to save bookmarks: " + e.getMessage());
        }
    }
}