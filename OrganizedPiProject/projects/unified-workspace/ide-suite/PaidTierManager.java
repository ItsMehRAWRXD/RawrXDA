import java.util.*;
import java.time.*;
import java.io.*;
import java.nio.file.*;

public class PaidTierManager {
    private static final String LICENSE_FILE = "license.dat";
    private static final Map<String, TierFeatures> TIERS = new HashMap<>();
    
    static {
        TIERS.put("FREE", new TierFeatures(false, 1, 10, false, false));
        TIERS.put("PRO", new TierFeatures(true, 5, 100, true, false));
        TIERS.put("ENTERPRISE", new TierFeatures(true, -1, -1, true, true));
    }
    
    private String currentTier = "FREE";
    private LocalDateTime expiryDate;
    
    public PaidTierManager() {
        loadLicense();
    }
    
    public boolean setPaidTier(String tier, String licenseKey) {
        if (!TIERS.containsKey(tier.toUpperCase())) {
            return false;
        }
        
        if (validateLicense(tier, licenseKey)) {
            currentTier = tier.toUpperCase();
            expiryDate = LocalDateTime.now().plusYears(1);
            saveLicense();
            return true;
        }
        return false;
    }
    
    private boolean validateLicense(String tier, String key) {
        // Simple validation - in production use proper cryptographic validation
        String expected = generateLicenseKey(tier);
        return expected.equals(key);
    }
    
    private String generateLicenseKey(String tier) {
        return "PIENGINE-" + tier.toUpperCase() + "-" + tier.hashCode();
    }
    
    public TierFeatures getCurrentFeatures() {
        if (expiryDate != null && LocalDateTime.now().isAfter(expiryDate)) {
            currentTier = "FREE";
            expiryDate = null;
            saveLicense();
        }
        return TIERS.get(currentTier);
    }
    
    public String getCurrentTier() {
        return currentTier;
    }
    
    public boolean canUseFeature(String feature) {
        TierFeatures features = getCurrentFeatures();
        switch (feature.toLowerCase()) {
            case "multicompiler": return features.multiCompilerSupport;
            case "cloudsync": return features.cloudSync;
            case "advancedtools": return features.advancedTools;
            default: return true;
        }
    }
    
    public boolean checkCompilationLimit(int currentCount) {
        TierFeatures features = getCurrentFeatures();
        return features.maxCompilationsPerHour == -1 || currentCount < features.maxCompilationsPerHour;
    }
    
    public boolean checkProjectLimit(int currentCount) {
        TierFeatures features = getCurrentFeatures();
        return features.maxProjects == -1 || currentCount < features.maxProjects;
    }
    
    private void loadLicense() {
        try {
            if (Files.exists(Paths.get(LICENSE_FILE))) {
                List<String> lines = Files.readAllLines(Paths.get(LICENSE_FILE));
                if (lines.size() >= 2) {
                    currentTier = lines.get(0);
                    expiryDate = LocalDateTime.parse(lines.get(1));
                }
            }
        } catch (Exception e) {
            currentTier = "FREE";
            expiryDate = null;
        }
    }
    
    private void saveLicense() {
        try {
            List<String> lines = Arrays.asList(
                currentTier,
                expiryDate != null ? expiryDate.toString() : ""
            );
            Files.write(Paths.get(LICENSE_FILE), lines);
        } catch (Exception e) {
            // Handle silently
        }
    }
    
    public String getUpgradeMessage() {
        return "Upgrade to PRO for unlimited compilations and advanced features!\n" +
               "License Key: " + generateLicenseKey("PRO");
    }
    
    public static class TierFeatures {
        public final boolean multiCompilerSupport;
        public final int maxProjects;
        public final int maxCompilationsPerHour;
        public final boolean cloudSync;
        public final boolean advancedTools;
        
        public TierFeatures(boolean multiCompiler, int maxProjects, int maxCompilations, 
                           boolean cloudSync, boolean advancedTools) {
            this.multiCompilerSupport = multiCompiler;
            this.maxProjects = maxProjects;
            this.maxCompilationsPerHour = maxCompilations;
            this.cloudSync = cloudSync;
            this.advancedTools = advancedTools;
        }
    }
}