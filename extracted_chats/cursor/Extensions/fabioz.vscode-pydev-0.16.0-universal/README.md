This extension provides an integration of PyDev (http://www.pydev.org) in VSCode,
enabling users to leverage the features of one of the leading Python IDEs inside VSCode.

Note: this extension provides a **30-day trial** after installation and **must** be **purchased** for continued use.

Check http://www.pydev.org/vscode for more details!

## Features currently available

-   Code-completion

    -   Fast
    -   Context sensitive
    -   Common tokens
    -   Context insensitive with auto import
    -   Templates (since `0.5.0`)

-   Code formatter

    -   Fast
    -   Format ranges
    -   Format on type
    -   Since `0.5.0` supports using `Black`, `Ruff` and `autopep8` as the
        formatter (see related `python.pydev.formatter` setting -- for example,
        after installing `Ruff` in the python environment enable `Ruff` to be
        the formatter used by setting `python.pydev.formatter` to `ruff`).
    -   Since `0.6.0`, supports import sorting as a part of the code formatting
        (using isort or one of the internal formatters).

-   Code analysis

    -   Real time using `PyDev` semantic analyzer.

    -   Since `0.5.0` also supports using `MyPy`, `Ruff`, `PyLint` and `Flake8` for linting
        when a file is saved (see related `python.pydev.lint.*` settings -- for example,
        after installing `MyPy` in the python environment, enable `MyPy`
        code analysis through the `python.pydev.lint.mypy.use` setting)

-   Go to definition

-   Symbols for Workspace

-   Symbols for open editor

-   Find references

-   Quick fix for undefined variables (adds missing import)

-   Code assists:

    -   Create/update docstring
    -   Surround with try..except, with, etc.
    -   Convert string to f-string
    -   Wrap text
    -   Create classes/functions/modules

-   Navigate to previous or next class or method (`Ctrl+Shift+Up`, `Ctrl+Shift+Down`)

-   Debugging (with the required `Python Debugger (PyDev)` extension (since `0.5.0`))

-   Initial integration with the `Testing view` (since `0.7.0`)
    -   Note: currently just updates tests for the currently opened file and doesn't load all workspace tests.

Note: the version of PyDev currently bundled is `13.0.2`.

**Note:** when changes are done to the Python interpreter, it's expected that the command: `PyDev: Clear caches` is
manually called to ensure that PyDev picks up the changes accordingly.

## Requirements

Java 17 must be installed -- `python.pydev.java.home` may be used to specify its location if it's not found.
See http://www.pydev.org/vscode for instructions and other settings.

## Changelog

### Release 0.16.0

-   Updated PyDev to `13.1.0`.

    -   Updates & Improvements

        -   It's now possible to use **pyright** analysis support
        -   **pydevd** (debugger) updated to **3.4.1**
        -   Preliminary support for debugging with **Python 3.14** (still not complete)
        -   Consider **@abstractclassmethod** and **@abstractstaticmethod** decorators in code analysis.
        -   Use plain **super()** in override completion.
        -   Settings mapped to Strings in the PyDev java backend now have string substitution applied to them.

    -   Bug Fixes
        -   If a default interpreter cannot be found, the latest Python 3 version is used to parse the code.
        -   Type definition in non-global scope (inside a method) should not give an error if the token is just found in the TYPE_CHECKING namespace.
        -   Fixed issues dealing with name store in match where spurious unused variable would be found. **#PyDev-1272**

### Release 0.15.4

-   Fixed an issue finding interpreter in the workspace root when that same configuration would be used in a sub-folder.

### Release 0.15.3

-   Fixes issue if code action is requested in location that's not available.
-   By default the interpreter is now set to `<auto-detect>` (previously it was set to `python`).
    -   When using `<auto-detect>`, a default interpreter may be found in the workspace root (i.e.: in a `.venv` folder).

### Release 0.15.2

-   When doing a launch, the env variable is properly merged with the env variable from the launch configuration.
-   No longer try to pip-install `pydevd` when doing `attach` / `subprocess attach` launches.

### Release 0.15.1

-   Fixed caching issue when auto-computing interpreter information (information was computed properly but not applied due to bad caching).
-   Handle case where module.body could be null.
-   Improve type inference engine to deal with TypeAlias.
-   Fixes in code analysis to deal with TypeAlias.

### Release 0.15.0

-   Improved loading launch configuration templates.
-   Fixed issue in code analysis related to bad scoping of type variable:

    -   In the case of def `f[T](https://github.com/fabioz/vscode-pydev-info/blob/HEAD/...)`, T was actually bound to the outer scope, not to the function scope.

-   Fixed internal error building AST with type statement construct type `IntOrStr = int | str`.

### Release 0.14.1

Bugfix release:

-   When VSCode itself is run in debug, don't automatically add agentlib vmargs for debugging (python.pydev.ls.vmargs) can be used for that.
    -   This could make the extension fail to start.

### Release 0.14.0

Release focus on multi-project support!

Details:

-   Added command `PyDev: Clear caches` to clear internal caches and restart the Language Server
    -   This can be used as an escape hatch when the Language Server is returning outdated information or if the project structure has changed.
-   Multiple projects and interpreters in the workspace are now detected and used by PyDev by using the information in a `pyproject.toml`, `venv`, `conda` or `Sema4ai VSCode Extension` (which automatically creates an interpreter based on `package.yaml`/`robot.yaml` files) -- please report if you need support for other tools or if the existing heuristics don't work for your project structure.
    -   Searching for multiple projects is enabled by default but can be disabled by setting `python.pydev.autoDetectProjects` to `false` (when disabled, just a single project is used for the whole workspace using the `python.pydev.pythonInterpreter` and `python.pydev.pythonPath` settings).
    -   See: https://www.pydev.org/vscode/multiple_projects_handling.html for more details.
-   When opening a definition, PyDev will now prefer to open the actual source file instead of a file just with the definitions in `typeshed` (although if it cannot be found, `typeshed` will still be used).
-   Using `ruff check` instead of `ruff` in ruff integration (command line breakage from ruff).

### Release 0.13.0

-   Using `runfiles.py` from `Python Debugger (PyDev)` extension (when available) to run tests.
-   Along with the updated `Python Debugger (PyDev)` version `0.0.5` provides support for debugging `Python 3.13`.

### Release 0.12.0

-   Preliminary support for Python 3.13 (note: the debugger still doesn't support it, but it'll follow soon).
-   Fix in assist docstring: properly updates docstring even if the signature spans multiple lines.
-   Support for `Annotated[cls]` in type inference engine.
-   Support for type alias declaration (Python 3.12 syntax fix). i.e.: the following syntax is supported: `type Point[T] = tuple[T, T]`.
-   Support for type parameters (Python 3.12 syntax fix). i.e.: the following syntax is supported: `class A[T]: ...` / `def foo[T](https://github.com/fabioz/vscode-pydev-info/blob/HEAD/x: T): ...`.

### Release 0.11.0

-   New code assists to create classes/functions/modules
    -   Examples:
        -   If `method` is not found in `self.method()`, a code assist to create `method` in the current class is provided.
        -   If `MyClass` is undefined, a code assist to create a class such as `MyClass` is provided.
        -   If `import my_module` points to an undefined module, a code assist to create a `my_module` is provided.
-   When the interpreter is changed all open editors have code analysis re-requested.
-   Created new command: `PyDev: Force code analysis` (to force code analysis in the current file).
-   Fixed issue where AST cache was not properly refreshed when a file was removed.
-   Fixed issue where the AST cache from the current (dirty) editor contents was not properly updated.

### Release 0.10.1

-   Fixed issue which could lead to RecursionError initializing the language server when restoring the interpreter info internally.
-   When adding a local import (when setting `"python.pydev.preferredImportLocation": "topOfMethod"`), an import added for unresolved variables inside a method definition arguments is properly added as a global import.
-   When a completion for arguments in a method call is provided it's made the default.

### Release 0.10.0

-   Improved the workspace symbols matching:
    -   It now considers the match with the name + module (which can be matched in VSCode with <symbol name> <space> <module>).
    -   It now matches without having to add wildcard chars (`*`) as VSCode didn't match when wildcards were actually added (which resulted in no symbols being shown).
-   New code assist to convert a string to an f-string.
-   New code assist to assign to local or field.
-   New code assist to wrap text in a comment or string (customize through setting: `python.pydev.text.wrap.columns`).
-   The code assist for `Surround with try..except` is now `Surround with try..except Exception`.
-   Improved command `Config: Select Python Interpreter` to show previously selected interpreters and to allow to choose from conda environments.

See full changelog at: http://www.pydev.org/vscode/history.html

## License

-   PyDev for VSCode extension: Commercial (http://www.pydev.org/vscode/license.html)

Other bundled components:

-   PyDev: EPL (https://www.eclipse.org/org/documents/epl-v10.php)
-   LSP4J: EPL (https://www.eclipse.org/org/documents/epl-v10.php)
-   Eclipse core libraries: EPL (https://www.eclipse.org/org/documents/epl-v10.php)
