// AIPersonaManager.java - Customizable AI personas and behavior management
package com.aicli;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import java.io.IOException;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.time.Instant;

/**
 * AI Persona Manager for creating and managing customizable AI personas.
 * Allows users to define different AI personalities, expertise areas, and behavior patterns
 * to tailor the agent's responses to specific needs and preferences.
 */
public class AIPersonaManager {
    private final Path personasDir;
    private final ObjectMapper objectMapper;
    private final ReadWriteLock lock;
    private final Map<String, AIPersona> activePersonas;
    private final Map<String, AIPersona> defaultPersonas;
    private final Map<String, String> userPersonaPreferences;

    public AIPersonaManager() {
        this(Paths.get("personas"));
    }

    public AIPersonaManager(Path personasDir) {
        this.personasDir = personasDir;
        this.objectMapper = new ObjectMapper();
        this.objectMapper.registerModule(new JavaTimeModule());
        this.objectMapper.disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS);
        this.lock = new ReentrantReadWriteLock();
        this.activePersonas = new ConcurrentHashMap<>();
        this.defaultPersonas = new ConcurrentHashMap<>();
        this.userPersonaPreferences = new ConcurrentHashMap<>();
        
        // Create personas directory
        try {
            Files.createDirectories(personasDir);
        } catch (IOException e) {
            System.err.println("Failed to create personas directory: " + e.getMessage());
        }
        
        // Initialize default personas
        initializeDefaultPersonas();
    }

    /**
     * Initialize default AI personas
     */
    private void initializeDefaultPersonas() {
        // Senior Software Engineer Persona
        AIPersona seniorEngineer = new AIPersona.Builder()
                .id("senior_engineer")
                .name("Senior Software Engineer")
                .description("Experienced software engineer focused on best practices, clean code, and architecture")
                .systemPrompt("You are a senior software engineer with 10+ years of experience. " +
                        "You focus on clean code, best practices, design patterns, and scalable architecture. " +
                        "You provide practical, production-ready solutions and always consider maintainability and performance.")
                .expertiseAreas(Arrays.asList("architecture", "design_patterns", "clean_code", "performance", "testing"))
                .communicationStyle("professional")
                .verbosityLevel("detailed")
                .codeStyle("enterprise")
                .build();
        
        // Startup Developer Persona
        AIPersona startupDev = new AIPersona.Builder()
                .id("startup_developer")
                .name("Startup Developer")
                .description("Fast-moving developer focused on rapid prototyping and MVP development")
                .systemPrompt("You are a startup developer who moves fast and breaks things. " +
                        "You prioritize speed and functionality over perfect code. " +
                        "You provide quick, practical solutions that get the job done.")
                .expertiseAreas(Arrays.asList("rapid_prototyping", "mvp_development", "quick_fixes", "productivity"))
                .communicationStyle("casual")
                .verbosityLevel("concise")
                .codeStyle("pragmatic")
                .build();
        
        // Code Reviewer Persona
        AIPersona codeReviewer = new AIPersona.Builder()
                .id("code_reviewer")
                .name("Code Reviewer")
                .description("Thorough code reviewer focused on quality, security, and maintainability")
                .systemPrompt("You are a meticulous code reviewer who focuses on code quality, security, " +
                        "and maintainability. You identify potential issues, suggest improvements, " +
                        "and ensure code follows best practices and standards.")
                .expertiseAreas(Arrays.asList("code_quality", "security", "maintainability", "standards", "testing"))
                .communicationStyle("analytical")
                .verbosityLevel("thorough")
                .codeStyle("strict")
                .build();
        
        // Learning Assistant Persona
        AIPersona learningAssistant = new AIPersona.Builder()
                .id("learning_assistant")
                .name("Learning Assistant")
                .description("Patient teacher focused on explaining concepts and helping with learning")
                .systemPrompt("You are a patient and encouraging learning assistant. " +
                        "You explain concepts clearly, provide examples, and help users understand " +
                        "not just what to do, but why. You adapt your explanations to the user's level.")
                .expertiseAreas(Arrays.asList("education", "explanation", "examples", "concepts"))
                .communicationStyle("encouraging")
                .verbosityLevel("explanatory")
                .codeStyle("educational")
                .build();
        
        // DevOps Engineer Persona
        AIPersona devOpsEngineer = new AIPersona.Builder()
                .id("devops_engineer")
                .name("DevOps Engineer")
                .description("DevOps specialist focused on deployment, infrastructure, and automation")
                .systemPrompt("You are a DevOps engineer with expertise in deployment, infrastructure, " +
                        "automation, and monitoring. You focus on making development and deployment " +
                        "processes efficient, reliable, and scalable.")
                .expertiseAreas(Arrays.asList("deployment", "infrastructure", "automation", "monitoring", "ci_cd"))
                .communicationStyle("technical")
                .verbosityLevel("practical")
                .codeStyle("operational")
                .build();
        
        // Add default personas
        defaultPersonas.put("senior_engineer", seniorEngineer);
        defaultPersonas.put("startup_developer", startupDev);
        defaultPersonas.put("code_reviewer", codeReviewer);
        defaultPersonas.put("learning_assistant", learningAssistant);
        defaultPersonas.put("devops_engineer", devOpsEngineer);
    }

    /**
     * Create a new custom persona
     */
    public AIPersona createPersona(AIPersona persona) throws IOException {
        lock.writeLock().lock();
        try {
            // Validate persona
            if (persona.getId() == null || persona.getId().trim().isEmpty()) {
                throw new IllegalArgumentException("Persona ID cannot be empty");
            }
            
            if (persona.getSystemPrompt() == null || persona.getSystemPrompt().trim().isEmpty()) {
                throw new IllegalArgumentException("System prompt cannot be empty");
            }
            
            // Save persona to file
            savePersonaToFile(persona);
            
            // Add to active personas
            activePersonas.put(persona.getId(), persona);
            
            return persona;
            
        } finally {
            lock.writeLock().unlock();
        }
    }

    /**
     * Get a persona by ID
     */
    public AIPersona getPersona(String personaId) {
        lock.readLock().lock();
        try {
            // Check active personas first
            AIPersona persona = activePersonas.get(personaId);
            if (persona != null) {
                return persona;
            }
            
            // Check default personas
            persona = defaultPersonas.get(personaId);
            if (persona != null) {
                return persona;
            }
            
            // Try to load from file
            try {
                persona = loadPersonaFromFile(personaId);
                if (persona != null) {
                    activePersonas.put(personaId, persona);
                    return persona;
                }
            } catch (IOException e) {
                System.err.println("Error loading persona from file: " + e.getMessage());
            }
            
            return null;
            
        } finally {
            lock.readLock().unlock();
        }
    }

    /**
     * Get all available personas
     */
    public List<AIPersona> getAllPersonas() {
        lock.readLock().lock();
        try {
            List<AIPersona> allPersonas = new ArrayList<>();
            
            // Add default personas
            allPersonas.addAll(defaultPersonas.values());
            
            // Add custom personas
            allPersonas.addAll(activePersonas.values());
            
            // Load any additional personas from files
            try {
                List<AIPersona> filePersonas = loadAllPersonasFromFiles();
                for (AIPersona persona : filePersonas) {
                    if (!allPersonas.stream().anyMatch(p -> p.getId().equals(persona.getId()))) {
                        allPersonas.add(persona);
                    }
                }
            } catch (IOException e) {
                System.err.println("Error loading personas from files: " + e.getMessage());
            }
            
            return allPersonas;
            
        } finally {
            lock.readLock().unlock();
        }
    }

    /**
     * Set user's preferred persona
     */
    public void setUserPersonaPreference(String userId, String personaId) {
        userPersonaPreferences.put(userId, personaId);
    }

    /**
     * Get user's preferred persona
     */
    public AIPersona getUserPreferredPersona(String userId) {
        String personaId = userPersonaPreferences.get(userId);
        if (personaId != null) {
            return getPersona(personaId);
        }
        
        // Return default persona if no preference set
        return getPersona("senior_engineer");
    }

    /**
     * Update an existing persona
     */
    public AIPersona updatePersona(String personaId, AIPersona updatedPersona) throws IOException {
        lock.writeLock().lock();
        try {
            if (!activePersonas.containsKey(personaId) && !defaultPersonas.containsKey(personaId)) {
                throw new IllegalArgumentException("Persona not found: " + personaId);
            }
            
            // Update the persona
            updatedPersona = updatedPersona.toBuilder().id(personaId).build();
            
            // Save to file
            savePersonaToFile(updatedPersona);
            
            // Update in memory
            activePersonas.put(personaId, updatedPersona);
            
            return updatedPersona;
            
        } finally {
            lock.writeLock().unlock();
        }
    }

    /**
     * Delete a persona
     */
    public boolean deletePersona(String personaId) throws IOException {
        lock.writeLock().lock();
        try {
            // Cannot delete default personas
            if (defaultPersonas.containsKey(personaId)) {
                return false;
            }
            
            // Remove from active personas
            activePersonas.remove(personaId);
            
            // Delete file
            Path personaFile = personasDir.resolve(personaId + ".json");
            if (Files.exists(personaFile)) {
                Files.delete(personaFile);
            }
            
            // Remove from user preferences
            userPersonaPreferences.entrySet().removeIf(entry -> entry.getValue().equals(personaId));
            
            return true;
            
        } finally {
            lock.writeLock().unlock();
        }
    }

    /**
     * Clone an existing persona
     */
    public AIPersona clonePersona(String sourcePersonaId, String newPersonaId, String newName) throws IOException {
        AIPersona sourcePersona = getPersona(sourcePersonaId);
        if (sourcePersona == null) {
            throw new IllegalArgumentException("Source persona not found: " + sourcePersonaId);
        }
        
        AIPersona clonedPersona = sourcePersona.toBuilder()
                .id(newPersonaId)
                .name(newName)
                .description("Cloned from " + sourcePersona.getName())
                .build();
        
        return createPersona(clonedPersona);
    }

    /**
     * Get personas by expertise area
     */
    public List<AIPersona> getPersonasByExpertise(String expertiseArea) {
        return getAllPersonas().stream()
                .filter(persona -> persona.getExpertiseAreas().contains(expertiseArea))
                .collect(java.util.stream.Collectors.toList());
    }

    /**
     * Get personas by communication style
     */
    public List<AIPersona> getPersonasByCommunicationStyle(String communicationStyle) {
        return getAllPersonas().stream()
                .filter(persona -> persona.getCommunicationStyle().equals(communicationStyle))
                .collect(java.util.stream.Collectors.toList());
    }

    /**
     * Save persona to file
     */
    private void savePersonaToFile(AIPersona persona) throws IOException {
        Path personaFile = personasDir.resolve(persona.getId() + ".json");
        objectMapper.writeValue(personaFile.toFile(), persona);
    }

    /**
     * Load persona from file
     */
    private AIPersona loadPersonaFromFile(String personaId) throws IOException {
        Path personaFile = personasDir.resolve(personaId + ".json");
        if (!Files.exists(personaFile)) {
            return null;
        }
        
        return objectMapper.readValue(personaFile.toFile(), AIPersona.class);
    }

    /**
     * Load all personas from files
     */
    private List<AIPersona> loadAllPersonasFromFiles() throws IOException {
        List<AIPersona> personas = new ArrayList<>();
        
        try (var stream = Files.list(personasDir)) {
            stream.filter(path -> path.getFileName().toString().endsWith(".json"))
                  .forEach(path -> {
                      try {
                          AIPersona persona = objectMapper.readValue(path.toFile(), AIPersona.class);
                          personas.add(persona);
                      } catch (IOException e) {
                          System.err.println("Error loading persona file " + path + ": " + e.getMessage());
                      }
                  });
        }
        
        return personas;
    }

    /**
     * AI Persona data structure
     */
    public static class AIPersona {
        private final String id;
        private final String name;
        private final String description;
        private final String systemPrompt;
        private final List<String> expertiseAreas;
        private final String communicationStyle;
        private final String verbosityLevel;
        private final String codeStyle;
        private final Map<String, Object> customSettings;
        private final Instant createdAt;
        private final Instant lastModified;

        private AIPersona(Builder builder) {
            this.id = builder.id;
            this.name = builder.name;
            this.description = builder.description;
            this.systemPrompt = builder.systemPrompt;
            this.expertiseAreas = builder.expertiseAreas;
            this.communicationStyle = builder.communicationStyle;
            this.verbosityLevel = builder.verbosityLevel;
            this.codeStyle = builder.codeStyle;
            this.customSettings = builder.customSettings;
            this.createdAt = builder.createdAt;
            this.lastModified = builder.lastModified;
        }

        public Builder toBuilder() {
            return new Builder()
                    .id(this.id)
                    .name(this.name)
                    .description(this.description)
                    .systemPrompt(this.systemPrompt)
                    .expertiseAreas(this.expertiseAreas)
                    .communicationStyle(this.communicationStyle)
                    .verbosityLevel(this.verbosityLevel)
                    .codeStyle(this.codeStyle)
                    .customSettings(this.customSettings)
                    .createdAt(this.createdAt)
                    .lastModified(this.lastModified);
        }

        // Getters
        public String getId() { return id; }
        public String getName() { return name; }
        public String getDescription() { return description; }
        public String getSystemPrompt() { return systemPrompt; }
        public List<String> getExpertiseAreas() { return expertiseAreas; }
        public String getCommunicationStyle() { return communicationStyle; }
        public String getVerbosityLevel() { return verbosityLevel; }
        public String getCodeStyle() { return codeStyle; }
        public Map<String, Object> getCustomSettings() { return customSettings; }
        public Instant getCreatedAt() { return createdAt; }
        public Instant getLastModified() { return lastModified; }

        public static class Builder {
            private String id;
            private String name;
            private String description;
            private String systemPrompt;
            private List<String> expertiseAreas = new ArrayList<>();
            private String communicationStyle = "professional";
            private String verbosityLevel = "balanced";
            private String codeStyle = "standard";
            private Map<String, Object> customSettings = new HashMap<>();
            private Instant createdAt = Instant.now();
            private Instant lastModified = Instant.now();

            public Builder id(String id) { this.id = id; return this; }
            public Builder name(String name) { this.name = name; return this; }
            public Builder description(String description) { this.description = description; return this; }
            public Builder systemPrompt(String systemPrompt) { this.systemPrompt = systemPrompt; return this; }
            public Builder expertiseAreas(List<String> expertiseAreas) { this.expertiseAreas = expertiseAreas; return this; }
            public Builder communicationStyle(String communicationStyle) { this.communicationStyle = communicationStyle; return this; }
            public Builder verbosityLevel(String verbosityLevel) { this.verbosityLevel = verbosityLevel; return this; }
            public Builder codeStyle(String codeStyle) { this.codeStyle = codeStyle; return this; }
            public Builder customSettings(Map<String, Object> customSettings) { this.customSettings = customSettings; return this; }
            public Builder createdAt(Instant createdAt) { this.createdAt = createdAt; return this; }
            public Builder lastModified(Instant lastModified) { this.lastModified = lastModified; return this; }

            public AIPersona build() {
                return new AIPersona(this);
            }
        }
    }
}
