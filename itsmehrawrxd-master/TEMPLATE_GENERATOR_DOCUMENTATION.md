# Eon Template Generator Documentation

## Overview

The Eon Template Generator is a powerful code generation system that allows developers to create Eon source code from templates and external data sources. It implements a Universal Intermediate Model (UIM) that can parse various data formats and generate code using a flexible template system.

## Architecture

### Core Components

1. **Template Lexer** (`eon_template_lexer.asm`)
   - Tokenizes `.eont` template files
   - Recognizes placeholders, control structures, and comments
   - Handles template syntax: `{{}}`, `{%%}`, `{# #}`

2. **Template Parser** (`eon_template_parser.asm`)
   - Builds Abstract Syntax Tree (AST) from template tokens
   - Supports control structures: `if/else/endif`, `for/endfor`, `each/endeach`
   - Manages nested template structures

3. **Template Engine** (`eon_template_engine.asm`)
   - Processes template AST and generates Eon source code
   - Handles data binding and control flow execution
   - Manages output buffer and code generation

4. **Universal Intermediate Model** (`uim_model.asm`)
   - Defines data structures for cross-language code generation
   - Supports symbols, functions, modules, and projects
   - Provides unified interface for different data sources

5. **Data Source Adapters** (`data_source_adapters.asm`)
   - Parses various file formats: JSON, YAML, SQL, C headers
   - Converts external data into UIM format
   - Handles file I/O and data transformation

6. **Main Generator** (`eon_template_generator.asm`)
   - Orchestrates the entire generation process
   - Handles command-line arguments and file operations
   - Integrates all components into a cohesive system

## Template Language

### Syntax

The `.eont` template language supports the following constructs:

#### Placeholders
```eont
Hello {{name}}!
The value is {{user.age}}.
```

#### Control Structures
```eont
{{#if condition}}
    Content when condition is true
{{#else}}
    Content when condition is false
{{/if}}

{{#for item in items}}
    Processing {{item.name}}
{{/for}}

{{#each user in users}}
    User: {{user.name}}
{{/each}}
```

#### Comments
```eont
{# This is a comment that will be ignored #}
```

### Template Examples

#### Data Access Generation
```eont
// example_data_access.eont
{{#for table in schema.tables}}
def model {{table.name}} {
    {{#for column in table.columns}}
    var {{column.name}}: {{column.eon_type}};
    {{/for}}
    
    def func save(): bool {
        return database.save_{{table.name}}(this);
    }
}
{{/for}}
```

#### API Bindings Generation
```eont
// example_api_bindings.eont
def model {{api.name}}Client {
    {{#for path in api.paths}}
    def func {{path.operation_id}}(
        {{#for param in path.parameters}}
        {{param.name}}: {{param.eon_type}}{{#unless @last}}, {{/unless}}
        {{/for}}
    ): {{path.response.eon_type}} {
        // Implementation
    }
    {{/for}}
}
```

## Data Sources

### Supported Formats

1. **JSON** - Configuration files, API specifications
2. **YAML** - Configuration files, data schemas
3. **SQL** - Database schemas, table definitions
4. **C Headers** - Function signatures, struct definitions

### Data Mapping

The system automatically maps external data types to Eon types:

- `int` → `int32`
- `long` → `int64`
- `float` → `float`
- `double` → `double`
- `char*` → `String`
- `bool` → `bool`

## Usage

### Command Line Interface

```bash
eon_template_generator <template.eont> <data_file> <output.eon> <data_type>
```

**Parameters:**
- `template.eont` - Template file path
- `data_file` - Source data file path
- `output.eon` - Generated Eon file path
- `data_type` - Data format (json, yaml, sql, c_header)

### Examples

#### Generate Data Access Code
```bash
eon_template_generator example_data_access.eont schema.json generated_models.eon json
```

#### Generate API Bindings
```bash
eon_template_generator example_api_bindings.eont api_spec.json generated_client.eon json
```

#### Generate C++ Bindings
```bash
eon_template_generator cpp_bindings.eont header.h generated_bindings.eon c_header
```

## Build System Integration

### CMake Integration

The template generator is fully integrated into the CMake build system:

```cmake
# Custom command for template generation
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_data_access.eon
    COMMAND eon_template_generator
    ARGS
        ${CMAKE_CURRENT_SOURCE_DIR}/example_data_access.eont
        ${CMAKE_CURRENT_SOURCE_DIR}/example_schema.json
        ${CMAKE_CURRENT_BINARY_DIR}/generated_data_access.eon
        json
    DEPENDS
        eon_template_generator
        example_data_access.eont
        example_schema.json
    COMMENT "Generating data access code from template"
)
```

### Build Targets

- `eon_template_generator` - Main template generator executable
- `template_generator_test` - Test suite for template generator
- `template_generation` - Custom target for running template generation

## Testing

### Test Suite

The template generator includes a comprehensive test suite (`test_template_generator.asm`) that covers:

1. **Template Lexer Tests**
   - Token recognition
   - Placeholder parsing
   - Control structure parsing

2. **Template Parser Tests**
   - AST construction
   - Nested structure handling
   - Error handling

3. **Template Engine Tests**
   - Code generation
   - Data binding
   - Control flow execution

4. **UIM Model Tests**
   - Data structure creation
   - Node relationships
   - Memory management

5. **Data Adapter Tests**
   - File parsing
   - Data transformation
   - Error handling

6. **Integration Tests**
   - End-to-end generation
   - File I/O operations
   - Error recovery

### Running Tests

```bash
# Build and run tests
make template_generator_test
./template_generator_test

# Run through CMake
cmake --build . --target template_generator_test
```

## Advanced Features

### Custom Data Types

The UIM system supports custom data types and can be extended to handle domain-specific data structures.

### Template Inheritance

Templates can inherit from base templates and override specific sections.

### Conditional Generation

Complex conditional logic allows for sophisticated code generation based on data characteristics.

### Error Handling

Comprehensive error handling provides detailed feedback for template and data parsing issues.

## Performance Considerations

### Memory Management

- Efficient memory allocation for large templates
- String pooling for repeated data
- Garbage collection for temporary objects

### Optimization

- Lazy evaluation of template expressions
- Caching of parsed templates
- Parallel processing for large datasets

## Future Enhancements

### Planned Features

1. **Template Macros** - Reusable template components
2. **Advanced Control Flow** - While loops, switch statements
3. **Template Validation** - Static analysis of templates
4. **IDE Integration** - Syntax highlighting, IntelliSense
5. **Performance Profiling** - Generation time analysis

### Extensibility

The system is designed to be easily extensible:

- New data source adapters can be added
- Custom template functions can be implemented
- Additional output formats can be supported

## Troubleshooting

### Common Issues

1. **Template Syntax Errors**
   - Check for matching braces
   - Verify control structure syntax
   - Ensure proper nesting

2. **Data Parsing Errors**
   - Validate data file format
   - Check data type mappings
   - Verify required fields

3. **Generation Failures**
   - Check template logic
   - Verify data availability
   - Review error messages

### Debug Mode

Enable debug mode for detailed logging:

```bash
eon_template_generator --debug template.eont data.json output.eon json
```

## Contributing

### Development Setup

1. Clone the repository
2. Build the template generator
3. Run the test suite
4. Make changes and test

### Code Style

- Follow assembly language conventions
- Use consistent naming patterns
- Document complex functions
- Add tests for new features

## License

This template generator is part of the RawrZApp project and follows the same licensing terms.

## Support

For questions, issues, or contributions, please refer to the main RawrZApp documentation and community resources.
