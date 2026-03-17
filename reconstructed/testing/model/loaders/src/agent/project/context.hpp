#ifndef PROJECT_CONTEXT_HPP
#define PROJECT_CONTEXT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <map>
#include <functional>

/**
 * @brief Struct representing a code entity (class, function, variable, etc.)
 * 
 * This struct defines a code entity discovered in the project, including
 * its type, name, location, and relationships to other entities.
 */
struct CodeEntity {
    std::string id;                       // Unique identifier for the entity
    std::string type;                     // Type of entity (class, function, variable, etc.)
    std::string name;                     // Name of the entity
    std::string filePath;                 // Path to the file containing the entity
    int lineNumber = 0;                   // Line number where the entity is defined
    std::vector<std::string> dependencies;    // List of entity IDs this entity depends on
    std::vector<std::string> references;      // List of entity IDs that reference this entity
    std::string visibility;               // Visibility (public, private, protected)
    std::string returnType;               // Return type (for functions)
    std::vector<std::string> parameters;      // Parameters (for functions)
    std::string parentClass;              // Parent class (for nested entities)
};

/**
 * @brief Struct representing a project dependency
 * 
 * This struct defines an external dependency of the project, including
 * its name, version, source, and usage information.
 */
struct ProjectDependency {
    std::string name;                 // Name of the dependency
    std::string version;              // Version of the dependency
    std::string source;               // Source (e.g., package manager, local, remote)
    std::string license;              // License information
    std::vector<std::string> usedInFiles;  // Files where this dependency is used
    bool isOptional = false;          // Whether this dependency is optional
    std::string description;          // Description of what this dependency provides
};

/**
 * @brief Struct representing a project pattern
 * 
 * This struct defines a recurring pattern identified in the project,
 * including its type, examples, and usage frequency.
 */
struct ProjectPattern {
    std::string patternId;         // Unique identifier for the pattern
    std::string patternType;       // Type of pattern (architectural, coding, etc.)
    std::string name;              // Name of the pattern
    std::string description;       // Description of the pattern
    std::vector<std::string> examples;  // Examples of where this pattern is used
    int frequency = 0;             // How often this pattern appears in the project
    std::string bestPractices;     // Best practices for this pattern
};

/**
 * @brief Code style information
 * 
 * Defines the coding style conventions used in the project,
 * including indentation, naming conventions, and formatting rules.
 */
struct CodeStyle {
    std::string indentationStyle;  // Indentation style (spaces, tabs)
    int indentationSize = 4;       // Size of indentation (e.g., 2, 4)
    std::string namingConvention;  // Naming convention (camelCase, snake_case, etc.)
    std::string braceStyle;        // Brace style (K&R, Allman, etc.)
    bool useConstCorrectness = true;  // Whether const correctness is enforced
    std::string commentStyle;      // Comment style (//, /**/, etc.)
    std::vector<std::string> fileExtensions;  // File extensions this style applies to
};

/**
 * @brief Project Context System for Deep Project Understanding
 * 
 * Provides deep understanding of the project's structure, dependencies, 
 * patterns, and coding conventions. Uses callback functions for events 
 * instead of Qt signals/slots.
 */
class ProjectContext {
public:
    // Callback function types
    using OnAnalyzedCallback = std::function<void(const std::string&)>;
    using OnEntitiesCallback = std::function<void(const std::vector<CodeEntity>&)>;
    using OnDependenciesCallback = std::function<void(const std::vector<ProjectDependency>&)>;
    using OnPatternsCallback = std::function<void(const std::vector<ProjectPattern>&)>;

    ProjectContext();
    ~ProjectContext();

    // Delete copy operations
    ProjectContext(const ProjectContext&) = delete;
    ProjectContext& operator=(const ProjectContext&) = delete;

    // Allow move operations
    ProjectContext(ProjectContext&&) = default;
    ProjectContext& operator=(ProjectContext&&) = default;

    /**
     * @brief Analyze the project structure and content
     * @param projectRootPath Path to the project root directory
     */
    void analyzeProject(const std::string& projectRootPath);

    /**
     * @brief Get code entities in the project
     * @return Vector of code entities
     */
    const std::vector<CodeEntity>& getCodeEntities() const;

    /**
     * @brief Get project dependencies
     * @return Vector of project dependencies
     */
    const std::vector<ProjectDependency>& getDependencies() const;

    /**
     * @brief Get identified project patterns
     * @return Vector of project patterns
     */
    const std::vector<ProjectPattern>& getPatterns() const;

    /**
     * @brief Get inferred code style
     * @return Code style information
     */
    const CodeStyle& getCodeStyle() const;

    /**
     * @brief Find related entities for a given entity
     * @param entityId ID of the entity to find relations for
     * @return Vector of related entity IDs
     */
    std::vector<std::string> findRelatedEntities(const std::string& entityId) const;

    /**
     * @brief Get files that use a specific dependency
     * @param dependencyName Name of the dependency
     * @return Vector of file paths
     */
    std::vector<std::string> getFilesUsingDependency(const std::string& dependencyName) const;

    /**
     * @brief Get project metrics as a string representation
     * @return Metrics string
     */
    std::string getProjectMetrics() const;

    /**
     * @brief Get recommended best practices for the project
     * @return Vector of best practice recommendations
     */
    std::vector<std::string> getBestPracticeRecommendations() const;

    // Callback setters for event notifications
    void setOnAnalyzedCallback(OnAnalyzedCallback cb) { m_onAnalyzed = cb; }
    void setOnEntitiesCallback(OnEntitiesCallback cb) { m_onEntities = cb; }
    void setOnDependenciesCallback(OnDependenciesCallback cb) { m_onDependencies = cb; }
    void setOnPatternsCallback(OnPatternsCallback cb) { m_onPatterns = cb; }

private:
    void scanProjectFiles(const std::string& projectRootPath);
    void analyzeFileContent(const std::string& filePath);
    void identifyDependencies(const std::string& projectRootPath);
    void recognizePatterns();
    void inferCodeStyle();
    void calculateMetrics();
    std::string generateUniqueId();
    std::string getFileExtension(const std::string& filePath) const;
    bool isSourceFile(const std::string& filePath) const;
    bool isConfigFile(const std::string& filePath) const;

    std::vector<CodeEntity> m_codeEntities;
    std::vector<ProjectDependency> m_dependencies;
    std::vector<ProjectPattern> m_patterns;
    CodeStyle m_codeStyle;
    std::string m_architectureOverview;
    std::string m_projectMetrics;
    std::string m_projectRootPath;
    int m_idCounter = 0;
    std::unordered_map<std::string, std::vector<std::string>> m_entityRelations;
    std::unordered_map<std::string, std::vector<std::string>> m_dependencyUsage;

    // Callbacks
    OnAnalyzedCallback m_onAnalyzed;
    OnEntitiesCallback m_onEntities;
    OnDependenciesCallback m_onDependencies;
    OnPatternsCallback m_onPatterns;
};

#endif // PROJECT_CONTEXT_HPP