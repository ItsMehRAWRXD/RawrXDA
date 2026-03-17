# Build Audit: RawrXD MASM IDE v2.0

## Error Log Summary
The build failed during the assembly of `rawrxd_ide_main.asm`. The primary issues are located in `phase1_editor_enhancement.asm`, `phase2_language_intelligence.asm`, and `editor_control.asm`.

### 1. Reserved Word Conflicts
MASM reserved words are being used as structure field names.
- **Error**: `error A2008: syntax error : length`
- **Error**: `error A2008: syntax error : offset`
- **Error**: `error A2008: syntax error : name`
- **Fix**: Rename fields to `nLength`, `nOffset`, `szName`, etc.

### 2. Undefined Macro `CSTR`
The `CSTR` macro is used for inline strings but is not defined.
- **Error**: `error A2006: undefined symbol : CSTR`
- **Fix**: Define the `CSTR` macro in a common header or at the top of the main file.

### 3. Invalid Keyword `return`
The `return` keyword is used instead of the MASM `ret` instruction.
- **Error**: `error A2008: syntax error : return`
- **Fix**: Replace `return 0` with `mov eax, 0` followed by `ret`.

### 4. Scope and Definition Issues
Symbols like `AddCommand` are undefined or have nesting errors.
- **Error**: `error A2006: undefined symbol : AddCommand`
- **Error**: `fatal error A1010: unmatched block nesting : AddCommand`
- **Fix**: Ensure `AddCommand` has a proper `proc` and `endp` block and is declared with `PROTO` if used before definition.

### 5. Multiple `.MODEL` and Include Directives
Included files contain redundant `.MODEL` and `include` statements.
- **Warning**: `warning A4011: multiple .MODEL directives found`
- **Fix**: Remove `.MODEL` and redundant `include` statements from the included `.asm` files, keeping them only in the main entry point.

## Action Plan
1. **Define `CSTR` Macro**: Add the standard MASM32 `CSTR` macro to `rawrxd_ide_main.asm`.
2. **Clean up Includes**: Remove `.MODEL`, `.386`, and redundant `include` statements from all phase files.
3. **Rename Reserved Fields**: Update `LINE_INFO`, `TOKEN`, `MINIMAP_LINE`, and `COMMAND_ITEM` structures.
4. **Fix `return` statements**: Replace with `mov eax, val` and `ret`.
5. **Fix `AddCommand` nesting**: Verify the `proc`/`endp` balance in `phase1_editor_enhancement.asm`.
6. **Update References**: Ensure all code using renamed fields is updated.
