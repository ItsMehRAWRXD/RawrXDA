#include "qt_string_formatter_masm.hpp"
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <cstring>
#include <cstdarg>
#include <cstdio>

// =============================================================================
// QtStringFormatterMasm Implementation
// =============================================================================

QtStringFormatterMasm::QtStringFormatterMasm(QObject* parent)
    : QObject(parent)
{
    // Initialize format buffer
    memset(m_formatBuffer, 0, sizeof(m_formatBuffer));
}

QtStringFormatterMasm::~QtStringFormatterMasm()
{
    // Cleanup if needed
}

// =============================================================================
// Format String (Main API)
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatString(
    const char* format,
    va_list args,
    size_t maxOutputSize
)
{
    QMutexLocker lock(&m_mutex);
    
    // Validate inputs
    QtStringFormatterResult validation = validateInputs(format, maxOutputSize);
    if (!validation.success) {
        return validation;
    }
    
    // Ensure buffer is large enough
    if (maxOutputSize > sizeof(m_formatBuffer)) {
        return handleFormatError(-1, "Output buffer too large");
    }
    
    // Call MASM formatter
    int result = wrapper_format_string(
        m_formatBuffer,
        maxOutputSize,
        format,
        args
    );
    
    if (result < 0) {
        return handleFormatError(result, "MASM formatting failed");
    }
    
    // Create output string
    QString output = QString::fromLatin1(m_formatBuffer, result);
    emit formattingComplete(output);
    
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format Integer
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatInteger(
    int64_t value,
    FormatType format,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    // Build format spec
    struct {
        uint32_t flags;
        uint32_t width;
        uint32_t precision;
        uint8_t length_mod;
        uint8_t conversion_type;
        uint16_t reserved;
    } spec;
    
    spec.flags = flags;
    spec.width = width;
    spec.precision = 0;
    spec.length_mod = 'l';      // 64-bit long
    spec.conversion_type = format;
    spec.reserved = 0;
    
    // Call MASM formatter
    int result = wrapper_format_integer(
        m_formatBuffer,
        sizeof(m_formatBuffer),
        value,
        &spec
    );
    
    if (result < 0) {
        return handleFormatError(result, "Integer formatting failed");
    }
    
    QString output = QString::fromLatin1(m_formatBuffer, result);
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format Unsigned Integer
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatUnsigned(
    uint64_t value,
    FormatType format,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    // Build format spec
    struct {
        uint32_t flags;
        uint32_t width;
        uint32_t precision;
        uint8_t length_mod;
        uint8_t conversion_type;
        uint16_t reserved;
    } spec;
    
    spec.flags = flags;
    spec.width = width;
    spec.precision = 0;
    spec.length_mod = 'l';
    spec.conversion_type = format;
    spec.reserved = 0;
    
    int result = wrapper_format_unsigned(
        m_formatBuffer,
        sizeof(m_formatBuffer),
        value,
        &spec
    );
    
    if (result < 0) {
        return handleFormatError(result, "Unsigned formatting failed");
    }
    
    QString output = QString::fromLatin1(m_formatBuffer, result);
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format Float
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatFloat(
    double value,
    FormatType format,
    int precision,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    // Build format spec
    struct {
        uint32_t flags;
        uint32_t width;
        uint32_t precision;
        uint8_t length_mod;
        uint8_t conversion_type;
        uint16_t reserved;
    } spec;
    
    spec.flags = flags;
    spec.width = width;
    spec.precision = precision;
    spec.length_mod = 'L';      // Long double/double
    spec.conversion_type = format;
    spec.reserved = 0;
    
    int result = wrapper_format_float(
        m_formatBuffer,
        sizeof(m_formatBuffer),
        value,
        &spec
    );
    
    if (result < 0) {
        return handleFormatError(result, "Float formatting failed");
    }
    
    QString output = QString::fromLatin1(m_formatBuffer, result);
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format String Argument
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatString(
    const char* value,
    int maxLength,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    if (!value) {
        value = "(null)";
    }
    
    // Build format spec
    struct {
        uint32_t flags;
        uint32_t width;
        uint32_t precision;
        uint8_t length_mod;
        uint8_t conversion_type;
        uint16_t reserved;
    } spec;
    
    spec.flags = flags;
    spec.width = width;
    spec.precision = maxLength >= 0 ? maxLength : -1;
    spec.length_mod = 0;
    spec.conversion_type = String;
    spec.reserved = 0;
    
    int result = wrapper_format_string_arg(
        m_formatBuffer,
        sizeof(m_formatBuffer),
        value,
        &spec
    );
    
    if (result < 0) {
        return handleFormatError(result, "String formatting failed");
    }
    
    QString output = QString::fromLatin1(m_formatBuffer, result);
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format Character
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatChar(
    char value,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    struct {
        uint32_t flags;
        uint32_t width;
        uint32_t precision;
        uint8_t length_mod;
        uint8_t conversion_type;
        uint16_t reserved;
    } spec;
    
    spec.flags = flags;
    spec.width = width;
    spec.precision = 0;
    spec.length_mod = 0;
    spec.conversion_type = Char;
    spec.reserved = 0;
    
    int result = wrapper_format_char(
        m_formatBuffer,
        sizeof(m_formatBuffer),
        (int)(unsigned char)value,
        &spec
    );
    
    if (result < 0) {
        return handleFormatError(result, "Character formatting failed");
    }
    
    QString output = QString::fromLatin1(m_formatBuffer, result);
    return QtStringFormatterResult::success(output, result);
}

// =============================================================================
// Format Pointer
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::formatPointer(
    const void* ptr,
    int width,
    int flags
)
{
    QMutexLocker lock(&m_mutex);
    
    // Use HexUpper for pointer format (0x...)
    return formatUnsigned(
        reinterpret_cast<uint64_t>(ptr),
        HexUpper,
        width,
        flags | AltForm  // Always use 0x prefix for pointers
    );
}

// =============================================================================
// Parse Format Specifier
// =============================================================================

QtStringFormatterMasm::FormatSpec QtStringFormatterMasm::parseFormatSpec(
    const char* format
)
{
    QMutexLocker lock(&m_mutex);
    
    FormatSpec spec = {Invalid, 0, 0, 0, 0, 0};
    
    if (!format || *format != '%') {
        return spec;
    }
    
    const char* ptr = format + 1;
    
    // Skip flags
    while (*ptr && strchr("-+ 0#", *ptr)) {
        if (*ptr == '-') spec.flags |= LeftAlign;
        else if (*ptr == '+') spec.flags |= ShowSign;
        else if (*ptr == ' ') spec.flags |= SpaceSign;
        else if (*ptr == '0') spec.flags |= ZeroPad;
        else if (*ptr == '#') spec.flags |= AltForm;
        ptr++;
    }
    
    // Parse width
    if (isdigit(*ptr)) {
        spec.width = 0;
        while (isdigit(*ptr)) {
            spec.width = spec.width * 10 + (*ptr - '0');
            ptr++;
        }
    }
    
    // Parse precision
    if (*ptr == '.') {
        ptr++;
        spec.precision = 0;
        while (isdigit(*ptr)) {
            spec.precision = spec.precision * 10 + (*ptr - '0');
            ptr++;
        }
    } else {
        spec.precision = -1;
    }
    
    // Parse length modifier
    if (*ptr && strchr("hlL", *ptr)) {
        spec.lengthModifier = *ptr;
        ptr++;
    }
    
    // Parse conversion specifier
    if (*ptr) {
        char conv = *ptr;
        ptr++;
        
        switch (conv) {
            case 'd': case 'i': spec.type = Decimal; break;
            case 'u': spec.type = Unsigned; break;
            case 'x': spec.type = HexLower; break;
            case 'X': spec.type = HexUpper; break;
            case 'o': spec.type = Octal; break;
            case 'f': case 'F': spec.type = Float; break;
            case 'e': spec.type = Scientific; break;
            case 'E': spec.type = Scientific_Upper; break;
            case 'g': spec.type = HexLower; break;  // Shortened float
            case 'G': spec.type = HexUpper; break;  // Shortened float uppercase
            case 's': spec.type = String; break;
            case 'c': spec.type = Char; break;
            case 'p': spec.type = Pointer; break;
            case '%': break;  // %% literal
            default: spec.type = Invalid; break;
        }
    }
    
    spec.consumed = ptr - format;
    return spec;
}

// =============================================================================
// Validation and Error Handling
// =============================================================================

QtStringFormatterResult QtStringFormatterMasm::validateInputs(
    const char* format,
    size_t outputSize
)
{
    if (!format) {
        return handleFormatError(-1, "Format string is null");
    }
    
    if (outputSize == 0) {
        return handleFormatError(-2, "Output size is zero");
    }
    
    if (outputSize > 1024 * 1024) {  // 1 MB limit
        return handleFormatError(-3, "Output size exceeds maximum");
    }
    
    return QtStringFormatterResult::success("", 0);
}

QtStringFormatterResult QtStringFormatterMasm::handleFormatError(
    int errorCode,
    const char* context
)
{
    QString errorMsg = QString("Format error (%1): %2")
        .arg(errorCode)
        .arg(context);
    
    emit formattingError(errorMsg, errorCode);
    qWarning() << errorMsg;
    
    return QtStringFormatterResult::failure(errorMsg, errorCode);
}

void QtStringFormatterMasm::onFormattingError(const QString& error)
{
    qWarning() << "Formatting error:" << error;
}

// =============================================================================
// Static Utility Functions
// =============================================================================

bool QtStringFormatterMasm::isValidFormat(const char* format)
{
    if (!format) return false;
    
    // Basic validation: must contain valid format specifiers
    // or at least not crash
    
    return true;
}

int QtStringFormatterMasm::estimateOutputSize(
    const char* format,
    int maxElements
)
{
    if (!format) return 0;
    
    // Estimate based on format string length and number of arguments
    // This is a heuristic: typically output is 2-5x format string length
    
    int formatLen = strlen(format);
    return (formatLen * 3) + (maxElements * 20);  // 3x format + 20 chars per arg
}

// =============================================================================
// Convenience Functions
// =============================================================================

QString qFormatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    
    static QtStringFormatterMasm formatter;
    QtStringFormatterResult result = formatter.formatString(format, args);
    
    va_end(args);
    
    return result.success ? result.output : QString();
}

QString qFormatInteger(
    int64_t value,
    int base,
    int width,
    uint32_t flags
)
{
    static QtStringFormatterMasm formatter;
    
    // Map base to format type
    QtStringFormatterMasm::FormatType format = QtStringFormatterMasm::Decimal;
    switch (base) {
        case 2: format = QtStringFormatterMasm::Binary; break;
        case 8: format = QtStringFormatterMasm::Octal; break;
        case 10: format = QtStringFormatterMasm::Decimal; break;
        case 16: format = QtStringFormatterMasm::HexLower; break;
        default: return QString();
    }
    
    QtStringFormatterResult result = formatter.formatInteger(value, format, width, flags);
    return result.success ? result.output : QString();
}

QString qFormatFloat(
    double value,
    int precision,
    QtStringFormatterMasm::FormatType format
)
{
    static QtStringFormatterMasm formatter;
    
    QtStringFormatterResult result = formatter.formatFloat(value, format, precision);
    return result.success ? result.output : QString();
}
