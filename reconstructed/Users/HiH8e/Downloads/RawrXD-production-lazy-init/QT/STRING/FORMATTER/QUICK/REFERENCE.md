# Qt String Formatter - Quick Reference Guide

**Fast lookup for Phase 2 String Formatting implementation**

---

## Format Specifiers (Quick Lookup)

```
%d, %i     → Signed integer           Example: formatInteger(-42) → "-42"
%u         → Unsigned integer         Example: formatUnsigned(42) → "42"
%x         → Hex lowercase            Example: formatUnsigned(255, HexLower) → "ff"
%X         → Hex UPPERCASE            Example: formatUnsigned(255, HexUpper) → "FF"
%o         → Octal                    Example: formatUnsigned(8, Octal) → "10"
%b         → Binary                   Example: formatUnsigned(5, Binary) → "101"
%f         → Fixed float              Example: formatFloat(3.14, Float, 2) → "3.14"
%e         → Scientific (e)           Example: formatFloat(1000, Scientific, 2) → "1.00e+03"
%E         → Scientific (E)           Example: formatFloat(1000, Scientific_Upper) → "1.00E+03"
%s         → String                   Example: formatString("hello") → "hello"
%c         → Character                Example: formatChar('A') → "A"
%p         → Pointer                  Example: formatPointer(&var) → "0x7fff5fbff8a0"
%%         → Literal %                Example: formatString("100%%") → "100%"
```

---

## Formatting Flags

```
-          Left-align       %10d for 42 → "42        "
+          Show sign        %+d for 42 → "+42"
(space)    Space for +      % d for 42 → " 42"
0          Zero-pad         %05d for 42 → "00042"
#          Alternate form   %#x for 15 → "0xf"
```

**Combined Examples:**
```
%-10d      Left-align, width 10: "42        "
%+05d      Zero-pad with sign: "+0042"
%#10x      Alternate form with width: "         0x2a"
```

---

## API Quick Reference

### Creating a Formatter
```cpp
QtStringFormatterMasm formatter;  // Qt object, thread-safe
```

### Formatting Integers
```cpp
// Signed integer
auto r = formatter.formatInteger(42);                    // "-42"
auto r = formatter.formatInteger(42, Decimal, 5);       // "   42"
auto r = formatter.formatInteger(42, Decimal, 5, ZeroPad); // "00042"
auto r = formatter.formatInteger(42, Decimal, 5, LeftAlign); // "42   "

// With sign
auto r = formatter.formatInteger(42, Decimal, 0, ShowSign); // "+42"
```

### Formatting Unsigned Integers
```cpp
auto r = formatter.formatUnsigned(255, HexLower);   // "ff"
auto r = formatter.formatUnsigned(255, HexUpper);   // "FF"
auto r = formatter.formatUnsigned(255, Octal);      // "377"
auto r = formatter.formatUnsigned(255, Binary);     // "11111111"
```

### Formatting Floats
```cpp
auto r = formatter.formatFloat(3.14159, Float, 2);        // "3.14"
auto r = formatter.formatFloat(0.0001, Scientific, 2);    // "1.00e-04"
auto r = formatter.formatFloat(12345, Scientific_Upper, 1); // "1.2E+04"
```

### Formatting Strings
```cpp
auto r = formatter.formatString("hello");             // "hello"
auto r = formatter.formatString("hello", 3);         // "hel"
auto r = formatter.formatString("hi", 10);           // "        hi"
auto r = formatter.formatString("hi", -1, 10, LeftAlign); // "hi        "
```

### Formatting Characters
```cpp
auto r = formatter.formatChar('X');                   // "X"
auto r = formatter.formatChar('*', 5);                // "    *"
auto r = formatter.formatChar('*', 5, LeftAlign);     // "*    "
```

### Formatting Pointers
```cpp
int x;
auto r = formatter.formatPointer(&x);  // "0x7fff5fbff8a0"
```

---

## Convenience Functions

```cpp
// Printf-style (variadic arguments)
QString qFormatString("Value: %d", 42);              // "Value: 42"
QString qFormatString("%.2f", 3.14159);              // "3.14"

// Integer with base conversion
QString qFormatInteger(255, 10);  // "255"
QString qFormatInteger(255, 16);  // "ff"
QString qFormatInteger(255, 2);   // "11111111"
QString qFormatInteger(255, 8);   // "377"

// Float with precision
QString qFormatFloat(3.14159, 2);  // "3.14"
```

---

## Enum Values

### FormatType
```cpp
Decimal = 2,         // %d
Unsigned = 3,        // %u
HexLower = 4,        // %x
HexUpper = 5,        // %X
Octal = 6,           // %o
Binary = 7,          // %b
Float = 8,           // %f
Scientific = 9,      // %e
Scientific_Upper = 10, // %E
String = 11,         // %s
Char = 12,           // %c
Pointer = 13         // %p
```

### FormatFlag (Bitwise OR to combine)
```cpp
LeftAlign = 0x01,    // -
ShowSign = 0x02,     // +
SpaceSign = 0x04,    // space
ZeroPad = 0x08,      // 0
AltForm = 0x10,      // #
UpperCase = 0x20     // For %X, %E, %G
```

---

## Result Handling

```cpp
QtStringFormatterResult result = formatter.formatInteger(42);

// Check success
if (result.success) {
    QString output = result.output;
    int bytes = result.charactersWritten;
} else {
    int errorCode = result.errorCode;
    QString errorMsg = result.errorMessage;
}
```

### Error Codes
```cpp
0   FORMAT_ERROR_NONE
1   FORMAT_ERROR_NULL_OUTPUT
2   FORMAT_ERROR_NULL_FORMAT
3   FORMAT_ERROR_INVALID_SPEC
4   FORMAT_ERROR_BUFFER_OVERFLOW
5   FORMAT_ERROR_INSUFFICIENT_ARGS
6   FORMAT_ERROR_INVALID_CONVERSION
7   FORMAT_ERROR_UNSUPPORTED_TYPE
8   FORMAT_ERROR_PRECISION_OVERFLOW
9   FORMAT_ERROR_WIDTH_OVERFLOW
```

---

## Common Patterns

### Format with Default Width
```cpp
// Right-aligned (default), 10 characters
auto r = formatter.formatInteger(42, Decimal, 10);
```

### Format with Zero-Padding
```cpp
// Right-aligned, 10 chars, zero-padded
auto r = formatter.formatInteger(42, Decimal, 10, ZeroPad);
```

### Format with Left-Alignment
```cpp
// Left-aligned, 10 characters
auto r = formatter.formatInteger(42, Decimal, 10, LeftAlign);
```

### Multiple Flags Combined
```cpp
// Zero-pad + show sign + alternate form
int flags = ZeroPad | ShowSign | AltForm;
auto r = formatter.formatInteger(42, Decimal, 10, flags);
```

### Handling Nulls
```cpp
// Automatically converts nullptr to "(null)"
auto r = formatter.formatString(nullptr);  // "(null)"

// Or check first
const char* str = getData();
if (str) {
    auto r = formatter.formatString(str);
} else {
    // Handle null case
}
```

---

## Performance Tips

1. **Reuse formatters** - Create once, use many times
2. **Use convenience functions** - Slightly optimized versions
3. **Batch operations** - Format multiple values together
4. **Pre-allocate space** - Use estimateOutputSize()

```cpp
// Good: Reuse formatter
static QtStringFormatterMasm formatter;
auto r1 = formatter.formatInteger(x);
auto r2 = formatter.formatInteger(y);

// Estimate size before bulk formatting
int est = QtStringFormatterMasm::estimateOutputSize("%d %d %d", 3);
```

---

## Integration Examples

### With Qt Widgets
```cpp
class MyWidget : public QWidget {
    QtStringFormatterMasm formatter;
    
    void updateLabel(int value) {
        auto r = formatter.formatInteger(value, Decimal, 5, ZeroPad);
        ui->label->setText(r.output);
    }
};
```

### With Signals/Slots
```cpp
QtStringFormatterMasm formatter;

QObject::connect(&formatter, &QtStringFormatterMasm::formattingError,
    this, &MyClass::onFormatError);

QObject::connect(&formatter, &QtStringFormatterMasm::formattingComplete,
    this, &MyClass::onFormatComplete);
```

### With QDebug
```cpp
auto r = formatter.formatFloat(3.14159, Float, 3);
qDebug() << "Result:" << r.output;

if (!r.success) {
    qWarning() << "Error:" << r.errorMessage;
}
```

---

## Format Specification Parsing

```cpp
auto spec = formatter.parseFormatSpec("%05d");

// Results:
spec.type           // Decimal
spec.flags          // ZeroPad
spec.width          // 5
spec.precision      // (unspecified)
spec.lengthModifier // 'l' or 'd'
spec.consumed       // 4 (bytes consumed)
```

---

## Common Mistakes & Fixes

### ❌ Wrong: Creating new formatter each time
```cpp
for (int i = 0; i < 1000; i++) {
    QtStringFormatterMasm fmt;  // Don't do this!
    auto r = fmt.formatInteger(i);
}
```

### ✅ Right: Reuse formatter
```cpp
static QtStringFormatterMasm fmt;
for (int i = 0; i < 1000; i++) {
    auto r = fmt.formatInteger(i);
}
```

---

### ❌ Wrong: Not checking success
```cpp
auto r = formatter.formatInteger(value);
QString output = r.output;  // Could be empty on error!
```

### ✅ Right: Always check
```cpp
auto r = formatter.formatInteger(value);
if (r.success) {
    QString output = r.output;
} else {
    qWarning() << "Format failed:" << r.errorMessage;
}
```

---

### ❌ Wrong: Ignoring null pointers
```cpp
auto r = formatter.formatString(userInput);  // Could be nullptr!
```

### ✅ Right: Validate first
```cpp
const char* str = userInput;
if (str) {
    auto r = formatter.formatString(str);
} else {
    // Handle null
}
```

---

## Base Conversion Quick Chart

```
Decimal (10)     Binary (2)         Hex (16)    Octal (8)
0                0                  0           0
1                1                  1           1
2                10                 2           2
4                100                4           4
8                1000               8           10
15               1111               f           17
16               10000              10          20
255              11111111           ff          377
256              100000000          100         400
4095             111111111111       fff         7777
65535            1111111111111111   ffff        177777
```

---

## Files & Locations

```
src/masm/qt_string_wrapper/
  qt_string_formatter.inc          - Constants & structures
  qt_string_formatter.asm          - MASM implementation

src/qtapp/
  qt_string_formatter_masm.hpp     - C++ header
  qt_string_formatter_masm.cpp     - C++ implementation
  qt_string_formatter_examples.hpp - 12 working examples

docs/
  QT_STRING_FORMATTER_PHASE2_COMPLETE.md
  QT_STRING_FORMATTER_QUICK_REFERENCE.md (this file)
```

---

**Last Updated**: December 4, 2025
**Status**: Complete ✅
**Phase**: 2 (String Formatting)
