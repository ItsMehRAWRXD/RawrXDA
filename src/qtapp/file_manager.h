/**
 * @file file_manager.h
 * @brief File management utilities and search result structures for RawrXD IDE.
 *
 * This header provides:
 * - MultiFileSearchResult: Value type representing a single search match
 * - FileManager: Static utility class for file I/O and path manipulation
 *
 * @note Thread Safety: FileManager methods are stateless and thread-safe.
 *       MultiFileSearchResult is a value type safe for concurrent access.
 *
 * @author RawrXD IDE Team
 * @version 2.0.0
 * @date 2025
 *
 * @copyright MIT License
 */
#pragma once


/**
 * @struct MultiFileSearchResult
 * @brief Represents a single search match within a file.
 *
 * Encapsulates all information needed to display and navigate to a search result,
 * including file location, position within the file, and surrounding context.
 *
 * @par Usage Example:
 * @code
 * MultiFileSearchResult result("main.cpp", 42, 15, "int main(int argc, char** argv)", "main");
 * resultFound(result);
 * @endcode
 */
struct MultiFileSearchResult {
    std::string file;        ///< Absolute or relative path to the file containing the match
    int line;            ///< 1-based line number where the match was found
    int column;          ///< 0-based column offset within the line
    std::string lineText;    ///< Full text of the line containing the match (for preview)
    std::string matchedText; ///< The actual text that matched the search query

    /**
     * @brief Default constructor - creates an empty/invalid result.
     */
    MultiFileSearchResult() : line(0), column(0) {}

    /**
     * @brief Parameterized constructor for creating a fully populated result.
     * @param file_       Path to the file containing the match
     * @param line_       1-based line number of the match
     * @param column_     0-based column offset of the match
     * @param lineText_   Full line text for context display
     * @param matchedText_ The matched substring
     */
    MultiFileSearchResult(const std::string& file_, int line_, int column_,
                          const std::string& lineText_, const std::string& matchedText_)
        : file(file_)
        , line(line_)
        , column(column_)
        , lineText(lineText_)
        , matchedText(matchedText_)
    {}

    /**
     * @brief Check if this result represents a valid match.
     * @return true if the result has a valid file path and line number
     */
    bool isValid() const { return !file.isEmpty() && line > 0; }
};

/**
 * @class FileManager
 * @brief Static utility class providing file I/O and path manipulation.
 *
 * All methods are static and stateless, making them safe for concurrent use
 * from multiple threads without synchronization.
 *
 * @par Thread Safety:
 * All methods are thread-safe (stateless).
 *
 * @par Usage Example:
 * @code
 * std::string content = FileManager::readFile("/path/to/file.cpp");
 * std::string relative = FileManager::toRelativePath("/project/src/main.cpp", "/project");
 * @endcode
 */
class FileManager {
public:
    /**
     * @brief Reads the entire contents of a text file.
     * @param filePath Absolute or relative path to the file
     * @return File contents as std::string, or empty string on error
     *
     * @note Uses std::fstream with ReadOnly | Text mode for proper line ending handling.
     * @warning Returns empty string for both empty files and read errors.
     *          Check std::fstream::exists() first if distinction is needed.
     */
    static std::string readFile(const std::string& filePath) {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return std::string();
        }
        QTextStream stream(&file);
        return stream.readAll();
    }

    /**
     * @brief Converts an absolute path to a path relative to a base directory.
     * @param absolutePath The absolute file path to convert
     * @param basePath The base directory for relative path calculation
     * @return Relative path string, or original path if conversion fails
     *
     * @par Example:
     * @code
     * std::string rel = FileManager::toRelativePath("/home/user/project/src/main.cpp", "/home/user/project");
     * // Returns: "src/main.cpp"
     * @endcode
     */
    static std::string toRelativePath(const std::string& absolutePath, const std::string& basePath) {
        std::filesystem::path baseDir(basePath);
        return baseDir.relativeFilePath(absolutePath);
    }

    /**
     * @brief Extracts the filename component from a full path.
     * @param filePath Full path to extract filename from
     * @return Filename with extension, without directory components
     */
    static std::string getFileName(const std::string& filePath) {
        return std::filesystem::path(filePath).fileName();
    }

    /**
     * @brief Extracts the directory path component from a full path.
     * @param filePath Full path to extract directory from
     * @return Directory path without the filename component
     */
    static std::string getDirectory(const std::string& filePath) {
        return std::filesystem::path(filePath).absolutePath();
    }

    /**
     * @brief Checks if a file exists and is readable.
     * @param filePath Path to check
     * @return true if the file exists and can be read
     */
    static bool fileExists(const std::string& filePath) {
        std::filesystem::path info(filePath);
        return info.exists() && info.isReadable();
    }
};

