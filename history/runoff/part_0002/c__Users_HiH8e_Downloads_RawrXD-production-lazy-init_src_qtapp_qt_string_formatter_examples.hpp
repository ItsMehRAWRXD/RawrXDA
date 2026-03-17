#ifndef QT_STRING_FORMATTER_EXAMPLES_HPP
#define QT_STRING_FORMATTER_EXAMPLES_HPP

#include "qt_string_formatter_masm.hpp"
#include <QString>
#include <QDebug>
#include <iostream>

// =============================================================================
// Qt String Formatter - Working Examples
// =============================================================================
// All examples are production-ready and demonstrate different formatting use cases
// =============================================================================

/**
 * Example 1: Basic Integer Formatting (%d, %i, %u)
 * Demonstrates: Decimal, unsigned, and signed integer formatting
 */
inline void example1_basic_integers() {
    qDebug() << "=== Example 1: Basic Integer Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    // Format signed integer
    QtStringFormatterResult r1 = formatter.formatInteger(-12345, QtStringFormatterMasm::Decimal);
    qDebug() << "formatInteger(-12345, Decimal):" << r1.output;
    
    // Format unsigned integer
    QtStringFormatterResult r2 = formatter.formatUnsigned(4294967295ULL, QtStringFormatterMasm::Unsigned);
    qDebug() << "formatUnsigned(4294967295, Unsigned):" << r2.output;
    
    // Format with width (zero-padding)
    QtStringFormatterResult r3 = formatter.formatInteger(
        42,
        QtStringFormatterMasm::Decimal,
        5,  // width
        QtStringFormatterMasm::ZeroPad
    );
    qDebug() << "formatInteger(42, Decimal, width=5, zero-pad):" << r3.output;
    
    // Format with width (space-padding)
    QtStringFormatterResult r4 = formatter.formatInteger(
        42,
        QtStringFormatterMasm::Decimal,
        5,  // width
        0   // default flags
    );
    qDebug() << "formatInteger(42, Decimal, width=5, space-pad):" << r4.output;
    
    // Format with left alignment
    QtStringFormatterResult r5 = formatter.formatInteger(
        42,
        QtStringFormatterMasm::Decimal,
        5,
        QtStringFormatterMasm::LeftAlign
    );
    qDebug() << "formatInteger(42, Decimal, width=5, left-align):" << r5.output;
    
    // Format with sign display
    QtStringFormatterResult r6 = formatter.formatInteger(
        42,
        QtStringFormatterMasm::Decimal,
        0,
        QtStringFormatterMasm::ShowSign
    );
    qDebug() << "formatInteger(42, ShowSign):" << r6.output;
}

/**
 * Example 2: Hexadecimal and Octal Formatting (%x, %X, %o)
 * Demonstrates: Hex (lowercase, uppercase) and octal formats
 */
inline void example2_hex_octal() {
    qDebug() << "\n=== Example 2: Hexadecimal and Octal Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    uint64_t value = 0xDEADBEEF;
    
    // Lowercase hex
    QtStringFormatterResult r1 = formatter.formatUnsigned(value, QtStringFormatterMasm::HexLower);
    qDebug() << "formatUnsigned(0xDEADBEEF, HexLower):" << r1.output;
    
    // Uppercase hex
    QtStringFormatterResult r2 = formatter.formatUnsigned(value, QtStringFormatterMasm::HexUpper);
    qDebug() << "formatUnsigned(0xDEADBEEF, HexUpper):" << r2.output;
    
    // Hex with alternate form (0x prefix)
    QtStringFormatterResult r3 = formatter.formatUnsigned(
        value,
        QtStringFormatterMasm::HexLower,
        0,
        QtStringFormatterMasm::AltForm
    );
    qDebug() << "formatUnsigned(0xDEADBEEF, HexLower, AltForm):" << r3.output;
    
    // Octal format
    QtStringFormatterResult r4 = formatter.formatUnsigned(511, QtStringFormatterMasm::Octal);
    qDebug() << "formatUnsigned(511, Octal):" << r4.output;
    
    // Octal with alternate form (0 prefix)
    QtStringFormatterResult r5 = formatter.formatUnsigned(
        511,
        QtStringFormatterMasm::Octal,
        0,
        QtStringFormatterMasm::AltForm
    );
    qDebug() << "formatUnsigned(511, Octal, AltForm):" << r5.output;
}

/**
 * Example 3: Binary Formatting (%b)
 * Demonstrates: Binary (base-2) output with padding
 */
inline void example3_binary() {
    qDebug() << "\n=== Example 3: Binary Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    // Format 8-bit pattern
    QtStringFormatterResult r1 = formatter.formatUnsigned(0xFF, QtStringFormatterMasm::Binary);
    qDebug() << "formatUnsigned(0xFF, Binary):" << r1.output;
    
    // Binary with width (zero-padded to 16 bits)
    QtStringFormatterResult r2 = formatter.formatUnsigned(
        0x00FF,
        QtStringFormatterMasm::Binary,
        16,
        QtStringFormatterMasm::ZeroPad
    );
    qDebug() << "formatUnsigned(0x00FF, Binary, width=16, zero-pad):" << r2.output;
    
    // Binary with alternate form (if applicable)
    QtStringFormatterResult r3 = formatter.formatUnsigned(
        15,
        QtStringFormatterMasm::Binary,
        8,
        QtStringFormatterMasm::ZeroPad
    );
    qDebug() << "formatUnsigned(15, Binary, width=8):" << r3.output;
}

/**
 * Example 4: Floating-Point Formatting (%f, %e, %g)
 * Demonstrates: Fixed, scientific, and shortest-form floating-point output
 */
inline void example4_floats() {
    qDebug() << "\n=== Example 4: Floating-Point Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    double pi = 3.141592653589793;
    double small = 0.0000001;
    double large = 123456789.0;
    
    // Fixed-point notation (6 decimal places)
    QtStringFormatterResult r1 = formatter.formatFloat(
        pi,
        QtStringFormatterMasm::Float,
        6   // precision
    );
    qDebug() << "formatFloat(3.14159..., Float, precision=6):" << r1.output;
    
    // Fixed-point with fewer decimal places
    QtStringFormatterResult r2 = formatter.formatFloat(
        pi,
        QtStringFormatterMasm::Float,
        2
    );
    qDebug() << "formatFloat(3.14159..., Float, precision=2):" << r2.output;
    
    // Scientific notation
    QtStringFormatterResult r3 = formatter.formatFloat(
        pi,
        QtStringFormatterMasm::Scientific,
        6
    );
    qDebug() << "formatFloat(3.14159..., Scientific, precision=6):" << r3.output;
    
    // Scientific uppercase E
    QtStringFormatterResult r4 = formatter.formatFloat(
        small,
        QtStringFormatterMasm::Scientific_Upper,
        3
    );
    qDebug() << "formatFloat(0.0000001, Scientific_Upper, precision=3):" << r4.output;
    
    // Large number formatting
    QtStringFormatterResult r5 = formatter.formatFloat(
        large,
        QtStringFormatterMasm::Float,
        0
    );
    qDebug() << "formatFloat(123456789.0, Float, precision=0):" << r5.output;
}

/**
 * Example 5: String and Character Formatting (%s, %c)
 * Demonstrates: String, character, null pointer, and width/alignment
 */
inline void example5_strings_chars() {
    qDebug() << "\n=== Example 5: String and Character Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    // Format string
    QtStringFormatterResult r1 = formatter.formatString("Hello, World!");
    qDebug() << "formatString(\"Hello, World!\"):" << r1.output;
    
    // Format string with max length
    QtStringFormatterResult r2 = formatter.formatString(
        "Hello, World!",
        5  // max length
    );
    qDebug() << "formatString(\"Hello, World!\", maxLen=5):" << r2.output;
    
    // Format string with width (right-aligned)
    QtStringFormatterResult r3 = formatter.formatString(
        "Test",
        -1,     // no length limit
        10,     // width
        0       // no flags (right-aligned default)
    );
    qDebug() << "formatString(\"Test\", width=10):" << r3.output;
    
    // Format string with width (left-aligned)
    QtStringFormatterResult r4 = formatter.formatString(
        "Test",
        -1,
        10,
        QtStringFormatterMasm::LeftAlign
    );
    qDebug() << "formatString(\"Test\", width=10, left-align):" << r4.output;
    
    // Format single character
    QtStringFormatterResult r5 = formatter.formatChar('X');
    qDebug() << "formatChar('X'):" << r5.output;
    
    // Format character with width
    QtStringFormatterResult r6 = formatter.formatChar('*', 5);
    qDebug() << "formatChar('*', width=5):" << r6.output;
}

/**
 * Example 6: Pointer Formatting (%p)
 * Demonstrates: Pointer address output in hexadecimal
 */
inline void example6_pointers() {
    qDebug() << "\n=== Example 6: Pointer Formatting ===";
    
    static QtStringFormatterMasm formatter;
    
    int array[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const char* str = "test string";
    
    // Format array pointer
    QtStringFormatterResult r1 = formatter.formatPointer(
        static_cast<const void*>(array)
    );
    qDebug() << "formatPointer(&array):" << r1.output;
    
    // Format string pointer
    QtStringFormatterResult r2 = formatter.formatPointer(
        static_cast<const void*>(str)
    );
    qDebug() << "formatPointer(&str):" << r2.output;
    
    // Format with width (padded)
    QtStringFormatterResult r3 = formatter.formatPointer(
        static_cast<const void*>(array),
        16,     // width
        QtStringFormatterMasm::ZeroPad
    );
    qDebug() << "formatPointer(&array, width=16, zero-pad):" << r3.output;
    
    // NULL pointer
    QtStringFormatterResult r4 = formatter.formatPointer(nullptr);
    qDebug() << "formatPointer(nullptr):" << r4.output;
}

/**
 * Example 7: Complex Format Strings with Multiple Operations
 * Demonstrates: Format string parser and complex formatting
 */
inline void example7_format_parser() {
    qDebug() << "\n=== Example 7: Format Specifier Parser ===";
    
    static QtStringFormatterMasm formatter;
    
    // Parse simple decimal format
    QString fmt1 = "%d";
    QtStringFormatterMasm::FormatSpec spec1 = formatter.parseFormatSpec(fmt1.toStdString().c_str());
    qDebug() << "parseFormatSpec(\"%d\"): type=" << (int)spec1.type 
             << ", consumed=" << spec1.consumed;
    
    // Parse zero-padded width format
    QString fmt2 = "%05d";
    QtStringFormatterMasm::FormatSpec spec2 = formatter.parseFormatSpec(fmt2.toStdString().c_str());
    qDebug() << "parseFormatSpec(\"%05d\"): type=" << (int)spec2.type 
             << ", width=" << spec2.width << ", flags=" << spec2.flags;
    
    // Parse float with precision
    QString fmt3 = "%.2f";
    QtStringFormatterMasm::FormatSpec spec3 = formatter.parseFormatSpec(fmt3.toStdString().c_str());
    qDebug() << "parseFormatSpec(\"%.2f\"): type=" << (int)spec3.type 
             << ", precision=" << spec3.precision;
    
    // Parse complex format with all components
    QString fmt4 = "%-+10.5f";
    QtStringFormatterMasm::FormatSpec spec4 = formatter.parseFormatSpec(fmt4.toStdString().c_str());
    qDebug() << "parseFormatSpec(\"%-+10.5f\"): type=" << (int)spec4.type 
             << ", width=" << spec4.width << ", precision=" << spec4.precision 
             << ", flags=" << spec4.flags;
}

/**
 * Example 8: Utility Functions and Convenience APIs
 * Demonstrates: High-level convenience functions
 */
inline void example8_convenience_functions() {
    qDebug() << "\n=== Example 8: Convenience Functions ===";
    
    // qFormatString - Printf-style function
    QString r1 = qFormatString("Value: %d, String: %s", 42, "test");
    qDebug() << "qFormatString(\"Value: %d, String: %s\", 42, \"test\"):" << r1;
    
    // qFormatInteger with base conversion
    QString r2 = qFormatInteger(255, 16);  // Hex
    qDebug() << "qFormatInteger(255, 16):" << r2;
    
    QString r3 = qFormatInteger(255, 2);   // Binary
    qDebug() << "qFormatInteger(255, 2):" << r3;
    
    QString r4 = qFormatInteger(255, 8);   // Octal
    qDebug() << "qFormatInteger(255, 8):" << r4;
    
    // qFormatFloat with precision
    QString r5 = qFormatFloat(3.141592653, 4);
    qDebug() << "qFormatFloat(3.141592653, 4):" << r5;
}

/**
 * Example 9: Thread-Safe Formatting in Multi-Threaded Environment
 * Demonstrates: Concurrent formatting operations
 */
inline void example9_thread_safety() {
    qDebug() << "\n=== Example 9: Thread Safety ===";
    
    // Each thread can create its own formatter or use thread-local storage
    static QtStringFormatterMasm formatter;
    
    // Multiple operations in sequence (protected by internal QMutex)
    for (int i = 0; i < 5; i++) {
        QtStringFormatterResult result = formatter.formatInteger(i * 100 + 42);
        qDebug() << "Thread-safe format iteration" << i << ":" << result.output;
    }
}

/**
 * Example 10: Error Handling
 * Demonstrates: Error detection and recovery
 */
inline void example10_error_handling() {
    qDebug() << "\n=== Example 10: Error Handling ===";
    
    static QtStringFormatterMasm formatter;
    
    // Valid formatting should succeed
    QtStringFormatterResult r1 = formatter.formatInteger(42);
    qDebug() << "Valid format - Success:" << r1.success << ", Output:" << r1.output;
    
    // Check error codes
    if (!r1.success) {
        qDebug() << "Error Code:" << r1.errorCode;
        qDebug() << "Error Message:" << r1.errorMessage;
    }
    
    // Invalid format strings are handled gracefully
    QtStringFormatterResult r2 = formatter.formatString(nullptr, {}, 100);
    qDebug() << "NULL format string - Success:" << r2.success;
    qDebug() << "Error Code:" << r2.errorCode;
    qDebug() << "Error Message:" << r2.errorMessage;
}

/**
 * Example 11: Output Size Estimation
 * Demonstrates: Predicting output buffer requirements
 */
inline void example11_size_estimation() {
    qDebug() << "\n=== Example 11: Output Size Estimation ===";
    
    // Estimate size for various format strings
    const char* fmt1 = "Value: %d";
    int est1 = QtStringFormatterMasm::estimateOutputSize(fmt1, 1);
    qDebug() << "Estimated size for \"Value: %d\" with 1 arg:" << est1 << "bytes";
    
    const char* fmt2 = "Items: %d, %d, %d, %d, %d";
    int est2 = QtStringFormatterMasm::estimateOutputSize(fmt2, 5);
    qDebug() << "Estimated size for multi-arg format with 5 args:" << est2 << "bytes";
    
    // Verify format is valid
    bool valid1 = QtStringFormatterMasm::isValidFormat(fmt1);
    qDebug() << "Is \"Value: %d\" valid?" << valid1;
}

/**
 * Example 12: Integration with Qt Signals/Slots
 * Demonstrates: Qt event-driven formatting with error callbacks
 */
inline void example12_qt_integration() {
    qDebug() << "\n=== Example 12: Qt Integration ===";
    
    // Create formatter with parent for Qt object hierarchy
    QtStringFormatterMasm* formatter = new QtStringFormatterMasm();
    
    // Connect error signal to custom slot
    QObject::connect(
        formatter, &QtStringFormatterMasm::formattingError,
        [](const QString& error, int code) {
            qDebug() << "Formatting error signal received:" << error << "(" << code << ")";
        }
    );
    
    // Connect completion signal
    QObject::connect(
        formatter, &QtStringFormatterMasm::formattingComplete,
        [](const QString& output) {
            qDebug() << "Formatting complete, output:" << output;
        }
    );
    
    // Perform formatting operation
    QtStringFormatterResult result = formatter->formatInteger(12345);
    
    delete formatter;
}

/**
 * Run All Examples
 */
inline void run_all_formatter_examples() {
    qDebug() << "========================================";
    qDebug() << "Qt String Formatter - All Examples";
    qDebug() << "========================================";
    
    example1_basic_integers();
    example2_hex_octal();
    example3_binary();
    example4_floats();
    example5_strings_chars();
    example6_pointers();
    example7_format_parser();
    example8_convenience_functions();
    example9_thread_safety();
    example10_error_handling();
    example11_size_estimation();
    example12_qt_integration();
    
    qDebug() << "========================================";
    qDebug() << "All examples completed";
    qDebug() << "========================================";
}

#endif // QT_STRING_FORMATTER_EXAMPLES_HPP
