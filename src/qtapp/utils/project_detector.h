#pragma once
/**
 * \file project_detector.h
 * \brief Automatic project type detection and metadata management
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Detects project type based on files present in the directory:
 * - Git repositories
 * - CMake projects
 * - QMake projects
 * - Node.js/npm projects
 * - Python projects
 * - .NET/C# projects
 * - Rust/Cargo projects
 * - Go modules
 * - Visual Studio solutions
 * - MASM/assembly projects
 */


namespace RawrXD {

/**
 * \enum ProjectType
 * \brief Detected project types
 */
enum class ProjectType {
    Unknown,        ///< Could not detect project type
    Git,            ///< Git repository (has .git/)
    CMake,          ///< CMake project (has CMakeLists.txt)
    QMake,          ///< QMake project (has *.pro files)
    NodeJS,         ///< Node.js project (has package.json)
    Python,         ///< Python project (has setup.py, pyproject.toml, requirements.txt)
    DotNet,         ///< .NET project (has *.sln, *.csproj)
    Rust,           ///< Rust project (has Cargo.toml)
    Go,             ///< Go module (has go.mod)
    VisualStudio,   ///< Visual Studio solution (has *.sln)
    MASM,           ///< Assembly project (has *.asm files)
    Generic         ///< Generic project (just files)
};

/**
 * \struct ProjectMetadata
 * \brief Project configuration and state
 */
struct ProjectMetadata {
    std::string name;               ///< Project name (from config or directory name)
    std::string rootPath;           ///< Absolute path to project root
    ProjectType type;           ///< Detected project type
    std::string buildDirectory;     ///< Relative path to build directory
    std::vector<std::string> recentFiles;    ///< Recently opened files (up to 20)
    std::string gitBranch;          ///< Current git branch (if applicable)
    std::vector<std::string> includePaths;   ///< Include directories for C/C++ projects
    std::vector<std::string> sourcePaths;    ///< Source directories
    std::chrono::system_clock::time_point lastOpened;       ///< Last time project was opened
    void* customData;     ///< Custom key-value data
    
    ProjectMetadata();
    
    /**
     * \brief Serialize to JSON
     * \return JSON object representation
     */
    void* toJson() const;
    
    /**
     * \brief Deserialize from JSON
     * \param json JSON object to parse
     * \return true if successful
     */
    bool fromJson(const void*& json);
};

/**
 * \class ProjectDetector
 * \brief Automatic project detection and configuration
 * 
 * Scans a directory to determine project type based on marker files.
 * Can also save/load project metadata from .rawrxd/project.json.
 * 
 * Example usage:
 * \code
 * ProjectDetector detector;
 * ProjectMetadata meta = detector.detectProject("/path/to/project");
 * if (meta.type == ProjectType::CMake) {
 * }
 * detector.saveProjectMetadata(meta);
 * \endcode
 */
class ProjectDetector {
public:
    ProjectDetector();
    ~ProjectDetector();
    
    /**
     * \brief Detect project type and create metadata
     * \param path Absolute path to any directory in the project
     * \return ProjectMetadata with detected information
     * 
     * This will scan up the directory tree to find the project root.
     */
    ProjectMetadata detectProject(const std::string& path);
    
    /**
     * \brief Find project root directory from any subdirectory
     * \param anyPath Path to file or directory within project
     * \return Absolute path to project root, or empty string if not found
     * 
     * Searches up the tree for marker files (.git, CMakeLists.txt, etc.)
     */
    std::string findProjectRoot(const std::string& anyPath);
    
    /**
     * \brief Detect project type from root directory
     * \param rootPath Absolute path to project root
     * \return Detected project type
     */
    ProjectType detectProjectType(const std::string& rootPath);
    
    /**
     * \brief Get human-readable project type name
     * \param type Project type enum value
     * \return Localized type name (e.g., "CMake Project", "Node.js Project")
     */
    static std::string projectTypeName(ProjectType type);
    
    /**
     * \brief Get typical build directory for project type
     * \param type Project type
     * \return Relative path to build directory (e.g., "build", "bin", "target")
     */
    static std::string defaultBuildDirectory(ProjectType type);
    
    /**
     * \brief Get typical source directories for project type
     * \param type Project type
     * \return List of common source directory names
     */
    static std::vector<std::string> defaultSourceDirectories(ProjectType type);
    
    /**
     * \brief Save project metadata to .rawrxd/project.json
     * \param metadata Project metadata to save
     * \return true if successful
     */
    bool saveProjectMetadata(const ProjectMetadata& metadata);
    
    /**
     * \brief Load project metadata from .rawrxd/project.json
     * \param projectRoot Absolute path to project root
     * \return Loaded metadata, or default metadata if not found
     */
    ProjectMetadata loadProjectMetadata(const std::string& projectRoot);
    
    /**
     * \brief Check if project has saved metadata
     * \param projectRoot Absolute path to project root
     * \return true if .rawrxd/project.json exists
     */
    bool hasProjectMetadata(const std::string& projectRoot);
    
    /**
     * \brief Get path to .rawrxd directory for project
     * \param projectRoot Absolute path to project root
     * \return Absolute path to .rawrxd directory
     */
    static std::string projectConfigDirectory(const std::string& projectRoot);
    
    /**
     * \brief Get path to project metadata file
     * \param projectRoot Absolute path to project root
     * \return Absolute path to .rawrxd/project.json
     */
    static std::string projectConfigFile(const std::string& projectRoot);
    
    /**
     * \brief Detect git branch for project
     * \param projectRoot Absolute path to project root
     * \return Current branch name, or empty string if not a git repo
     */
    static std::string detectGitBranch(const std::string& projectRoot);
    
    /**
     * \brief Add file to recent files list
     * \param metadata Project metadata to modify
     * \param filePath Absolute path to file
     * \param maxRecent Maximum number of recent files to keep (default 20)
     */
    static void addRecentFile(ProjectMetadata& metadata, const std::string& filePath, int maxRecent = 20);
    
private:
    /**
     * \brief Check if directory contains marker file
     * \param dirPath Directory path to check
     * \param markerFile Marker file name (e.g., ".git", "CMakeLists.txt")
     * \return true if marker exists
     */
    static bool hasMarkerFile(const std::string& dirPath, const std::string& markerFile);
    
    /**
     * \brief Check if directory contains any file matching pattern
     * \param dirPath Directory path to check
     * \param pattern Wildcard pattern (e.g., "*.pro", "*.sln")
     * \return true if matching file found
     */
    static bool hasFilePattern(const std::string& dirPath, const std::string& pattern);
    
    /**
     * \brief Scan directory for specific project type markers
     * \param rootPath Directory to scan
     * \param type Project type to check for
     * \return true if markers for this type found
     */
    static bool checkProjectType(const std::string& rootPath, ProjectType type);
};

} // namespace RawrXD

