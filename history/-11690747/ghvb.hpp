#ifndef QT_STRING_FORMATTER_MASM_HPP
#define QT_STRING_FORMATTER_MASM_HPP

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <cstdint>

// =============================================================================
// Qt String Formatter - Pure MASM Implementation Wrapper
// =============================================================================
// Provides printf-style string formatting via pure MASM engine
// Thread-safe, no external dependencies, cross-platform (Windows/Linux/macOS)
// =============================================================================

/**
 * @class QtStringFormatterResult
 * @brief Result wrapper for formatting operations
 */
struct QtStringFormatterResult {
    bool success;
    QString output;
    QString errorMessage;
    int charactersWritten;
    int errorCode;
    
    static QtStringFormatterResult success(const QString& output, int written) {
        return {true, output, QString(), written, 0};
    }
    
    static QtStringFormatterResult failure(const QString& error, int code) {
        return {false, QString(), error, 0, code};
    }
};

/**
 * @class QtStringFormatterMasm
 * @brief Pure MASM string formatting engine wrapper
 * 
 * Provides printf-style formatting capabilities:
 * - Integer formats: %d, %i, %u, %x, %X, %o, %b
 * - Float formats: %f, %e, %E, %g, %G
 * - String formats: %s, %c
 * - Flags: -, +, space, 0, #
 * - Width, precision, length modifiers
 */
class QtStringFormatterMasm : public QObject {
    Q_OBJECT
    
public:
    enum FormatType {
        Invalid = 0,
        Decimal = 2,
        Unsigned = 3,
        HexLower = 4,
        HexUpper = 5,
        Octal = 6,
        Binary = 7,
        Float = 8,
        Scientific = 9,
        Scientific_Upper = 10,
        String = 11,
        Char = 12,
        Pointer = 13
    };
    
    enum FormatFlag {
        LeftAlign = 0x01,      // -
        ShowSign = 0x02,       // +
        SpaceSign = 0x04,      // space
        ZeroPad = 0x08,        // 0
        AltForm = 0x10,        // #
        UpperCase = 0x20       // Used for %X, %E, %G
    };
    
    explicit QtStringFormatterMasm(QObject* parent = nullptr);
    ~QtStringFormatterMasm();
    
    // Disable copy operations
    QtStringFormatterMasm(const QtStringFormatterMasm&) = delete;
    QtStringFormatterMasm& operator=(const QtStringFormatterMasm&) = delete;
    
    // Format string with variable arguments
    QtStringFormatterResult formatString(
        const char* format,
        va_list args,
        size_t maxOutputSize = 8192
    );
    
    // Format integer value
    QtStringFormatterResult formatInteger(
        int64_t value,
        FormatType format = Decimal,
        int width = 0,
        int flags = 0
    );
    
    // Format unsigned integer value
    QtStringFormatterResult formatUnsigned(
        uint64_t value,
        FormatType format = Decimal,
        int width = 0,
        int flags = 0
    );
    
    // Format floating-point value
    QtStringFormatterResult formatFloat(
        double value,
        FormatType format = Float,
        int precision = 6,
        int width = 0,
        int flags = 0
    );
    
    // Format string value
    QtStringFormatterResult formatString(
        const char* value,
        int maxLength = -1,
        int width = 0,
        int flags = 0
    );
    
    // Format single character
    QtStringFormatterResult formatChar(
        char value,
        int width = 0,
        int flags = 0
    );
    
    // Format pointer address
    QtStringFormatterResult formatPointer(
        const void* ptr,
        int width = 0,
        int flags = 0
    );
    
    // Parse format specifier from string
    struct FormatSpec {
        FormatType type;
        int flags;
        int width;
        int precision;
        char lengthModifier;
        int consumed;
    };
    
    FormatSpec parseFormatSpec(const char* format);
    
    // Utility functions
    static bool isValidFormat(const char* format);
    static int estimateOutputSize(const char* format, int maxElements = 20);
    
signals:
    void formattingError(const QString& error, int errorCode);
    void formattingComplete(const QString& output);
    
protected slots:
    void onFormattingError(const QString& error);
    
private:
    mutable QMutex m_mutex;
    
    // Buffer for intermediate operations
    char m_formatBuffer[4096];
    
    // Helper methods
    QtStringFormatterResult validateInputs(
        const char* format,
        size_t outputSize
    );
    
    QtStringFormatterResult handleFormatError(
        int errorCode,
        const char* context
    );
    
    // MASM function declarations
    extern "C" {
        int wrapper_format_string(
            char* output,
            size_t outputSize,
            const char* format,
            va_list args
        );
        
        int wrapper_format_integer(
            char* output,
            size_t outputSize,
            int64_t value,
            const void* formatSpec
        );
        
        int wrapper_format_unsigned(
            char* output,
            size_t outputSize,
            uint64_t value,
            const void* formatSpec
        );
        
        int wrapper_format_float(
            char* output,
            size_t outputSize,
            double value,
            const void* formatSpec
        );
        
        int wrapper_format_string_arg(
            char* output,
            size_t outputSize,
            const char* value,
            const void* formatSpec
        );
        
        int wrapper_format_char(
            char* output,
            size_t outputSize,
            int value,
            const void* formatSpec
        );
        
        void wrapper_parse_format(
            const char* format,
            void* formatSpec,
            int* consumedBytes
        );
    }
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * Format a string with variable arguments (printf-like)
 * @param format Format string (printf-compatible)
 * @param ... Variable arguments
 * @return Formatted string as QString
 */
QString qFormatString(const char* format, ...);

/**
 * Format integer with specified base and flags
 * @param value Integer value to format
 * @param base Base (2-36)
 * @param width Minimum field width
 * @param flags Formatting flags (zero-pad, left-align, sign, etc.)
 * @return Formatted string
 */
QString qFormatInteger(
    int64_t value,
    int base = 10,
    int width = 0,
    uint32_t flags = 0
);

/**
 * Format floating-point number
 * @param value Float value
 * @param precision Decimal places
 * @param format Format type (fixed, scientific, shortest)
 * @return Formatted string
 */
QString qFormatFloat(
    double value,
    int precision = 6,
    QtStringFormatterMasm::FormatType format = QtStringFormatterMasm::Float
);

#endif // QT_STRING_FORMATTER_MASM_HPP
