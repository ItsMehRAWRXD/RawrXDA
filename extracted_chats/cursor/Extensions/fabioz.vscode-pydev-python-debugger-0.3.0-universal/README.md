This extension provides an integration of PyDev.Debugger (https://github.com/fabioz/PyDev.Debugger/) in VSCode,
enabling its usage inside of VSCode.

Note: this extension provides a **30-day trial** after installation and **must** be **purchased** for continued use.

Note: If `PyDev for VSCode` is installed and a valid license is available, this extension can be used without an additional license.

Check http://www.pydev.org/vscode for more details!

## Features currently available

-   Run with our without debugging:
    -   Run a python program (`path/to/script.py`) (set `program` in the launch configuration)
    -   Run a python module (`python -m <module>`) (set `module` in the launch configuration)
    -   Attach to a python program waiting for a connection
        -   The python program must call `import pydevd;pydevd.settrace(host="", port=port, protocol="dap")`
        -   An attach launch configuration with an `attach` mode must be run
    -   Start a server which waits for incoming connections
        -   An attach launch configuration with a `server` mode must be run
        -   The python program must call `import pydevd;pydevd.settrace(host="127.0.0.1", port=port, protocol="dap")`

## Why use this debugger?

-   `pydevd` is one of the fastest debuggers available, and while it was integrations through other extensions/IDEs (such as `debugpy` and even `PyCharm`,
    this extension is done by its author and is kept up-to date with the latest changes).
-   In Python 3.12 uses new APIs for very low overhead debugging.
-   In other Python versions it can also leverage the python `frame eval` for fast debugging (although it falls back to tracing once a breakpoint is hit).
-   Supports many features such as:
    -   Line breakpoints
        -   Expression
        -   Hit Count
        -   Log message
    -   Exception breakpoints
    -   Step in
    -   Step into target
    -   Jump to line (Set next line execution)
    -   Step over
    -   Step Return
    -   Change variable
    -   Debug Console evaluation
    -   View variables
    -   Customize variables
    -   Auto attach to subprocesses
    -   Start attach in server mode
    -   Attach to waiting program
    -   Launch in run or debug mode

## How to use this debugger

After installing, create a run configuration that has the `"type": "pydevd"` with a `"request": "attach"` or `"request": "launch"` and launch it.

If you have `PyDev for VSCode` installed, it'll be the engine used to do launches for test cases (and any other python-related launch).

### Notes

-   Currently launches will use `python.pydev.pythonExecutable` as the python executable (contributed by the `PyDev for VSCode` extension)
    unless a `pythonExecutable` is given in the launch configuration.

-   The `"request": "test-template"` is actually only used in `PyDev for VSCode` when doing a test-case launch (if customization
    for the test launch is required).

-   The `"request": "run-file-template"` is actually only used in `PyDev for VSCode` for customizing the launch done from the related custom command to
    run the current file (may also be used from other places such as code lenses).

-   The `"request": "base-template"` is used by the debugger extension to customize all launch configurations.

## License

-   PyDev.Debugger for VSCode extension: Commercial (http://www.pydev.org/vscode/license.html)

### Changelog

-   `0.3.0`:

    -   Make string replacement work for `pythonExecutable` and `cwd` in the launch configuration (so entries such as `${workspaceFolder}/.venv/bin/python` are supported).
    -   An extension is now automatically added to the `pythonExecutable` if the file cannot be found.

-   `0.2.0`:

    -   Updated to `pydevd 3.4.1`
        -   Preliminary support for debugging with **Python 3.14** (still not complete)

-   `0.1.1`:

    -   When doing a subprocess attach, the debug adapter will now pass the `pythonExecutable` from the base launch configuration (needed for launching subprocesses with the correct python interpreter when a venv is used for a project).
    -   When launching with uv, it'll now try to install pydevd using uv if the pip installation fails (`uv` is searched for in the PATH).

-   `0.1.0`:

    -   Uses `pydevd 3.3.0`

    -   It's now possible to customize the `variablePresentation` in the launch configuration.

        -   It's a json object which can customize how the following attributes: `special`, `function`, `class`, `protected` are shown with (`hide`: to hide, `group`: to create a group and show variables in the group, `inline`: to show variables inline)

    -   It's now possible to customize the `redirectOutput` in the launch configuration (default is `false`).

        -   If `true`, the debugger will redirect the output of the program to the debug console.

    -   It's now possible to customize the `stopAllThreadsOnSuspend` in the launch configuration (default is `false`).

        -   If `true`, the debugger will stop all threads when the program is suspended.

    -   It's now possible to customize the `steppingResumesAllThreads` in the launch configuration (default is `false`).

        -   If `true`, the debugger will resume all threads when the program is stepped.

    -   It's now possible to customize the `justMyCode` in the launch configuration (default is `false`).

        -   If `true`, the debugger will only stop on code from which is not in the standard library / site-packages.

    -   It's now possible to customize the `showReturnValue` in the launch configuration (default is `false`).

        -   If `true`, the debugger will show the return value of a function when stepping.

    -   It's now possible to customize the `breakOnSystemExitZero` in the launch configuration (default is `false`).

        -   If `true`, the debugger will break on `SystemExit` with a code of `0`.

    -   It's now possible to customize the `django` in the launch configuration (default is `false`).

        -   If `true`, the debugger will enable Django templates debugging.

    -   It's now possible to customize the `jinja` in the launch configuration (default is `false`).

        -   If `true`, the debugger will enable Jinja templates debugging.

    -   It's now possible to customize the `stopOnEntry` in the launch configuration (default is `false`).

        -   If `true`, the debugger will stop on entry.

    -   It's now possible to customize the `maxExceptionStackFrames` in the launch configuration (default is `0`).

        -   If `0`, the debugger will show all frames in the exception stack trace, otherwise it'll show only the first `maxExceptionStackFrames` frames.

    -   It's now possible to customize the `guiEventLoop` in the launch configuration (default is `matplotlib`).

        -   When set, the debugger will use the specified GUI event loop when a breakpoint is hit in the main thread to leave the windows interactive.
        -   Common values supported are: `matplotlib`, `wx`, `qt` , `qt4`, `qt5`, `qt6`, `gtk`, `tk`, `osx`, `glut`, `pyglet`, `gtk3`, `none`.
        -   It may also be the path to a custom function (such as `my_program.event_loop`).

    -   It's now possible to customize the `resolveSymlinks` in the launch configuration (default is `false`).

        -   If `true`, the debugger will resolve symlinks to the real file, even if it's a symlink, instead of showing the path that's actually being debugged.

    -   It's now possible to customize the `pathMappings` in the launch configuration (default is `[]`).

        -   This should be used when doing a remote debug session to translate the paths from that machine to the current local machine so that breakpoints paths are translated to the target machine correctly, as well as to show the proper files being hit locally.
        -   Example:

            ```json
            {
                "pathMappings": [{ "localRoot": "c:/Users/user/project", "remoteRoot": "/home/user/project" }]
            }
            ```

    -   It's now possible to customize the `autoReload` in the launch configuration (default is not having auto-reload enabled).

        -   To customize the `autoReload` settings, you can use the following settings:

            ```json
            {
                "autoReload": { "enable": true, "watchDirectories": ["/path/to/watch"] }
            }
            ```

        -   Note: The `watchDirectories` is optional and will be computed based on the `cwd` and `program` if the `watchDirectories` is not specified.
        -   Note: The auto reload will detect changes in the given directories and then, when a file change is detected, it'll do its best to discover the module and update the methods in place. This does come with some caveats and may not work in all cases (see: https://github.com/fabioz/PyDev.Debugger/blob/main/_pydevd_bundle/pydevd_reload.py for more details) -- for web frameworks (such as django, flask, etc) it's usually recommended to use the auto-reload which will kill the current process and start a new one.

    -   It's now possible to customize the inclusion/exclusion rules in `rules` in the launch configuration.

        -   This is an array of objects with the following properties:

            -   `include`: Whether to include or exclude the file from being traced by the debugger.
            -   `path`: The glob pattern which will be matched against the path to be included/excluded (example: `**/generated/*.py`).
            -   `module`: The module name to be included/excluded (example: `my.module.name`).
            -   Note: either `path` or `module` must be specified, both cannot be specified at the same time.

        -   Example:

            ```json
            {
                "rules": [
                    { "include": false, "path": "**/generated/*.py" },
                    { "include": false, "module": "my.module.name" }
                ]
            }
            ```

    -   Added `run-file-template` as being a valid `request` to customize run configurations using the command palette or code lenses (requires `PyDev for VSCode 0.15.0` or higher to be installed).

    -   Added `base-template` as being a valid `request` to customize all launch configurations whenever any launch is resolved.

    -   Added new command: `Re Launch last PyDev launch`

        -   This command will launch in regular mode the last PyDev launch (from code lens, Run current file, regular run, etc -- anything except test launches).

    -   Added new command: `Re Debug last PyDev launch`

        -   This command will debug the last PyDev launch (from code lens, Run current file, regular run, etc -- anything except test launches).

-   `0.0.6`:

    -   Uses `pydevd 3.2.3`
    -   Fixed issue due to the following breakage: `Python 3.13: dis.findlinestarts may now return line with None`.

-   `0.0.5`:

    -   Uses `pydevd 3.2.2`
    -   Support for `Python 3.13` (using `sys.monitoring` for faster debugging as Python `3.12`).
    -   pip-installs pydevd from pypi to take advantage of pre-compiled binaries without having to compile locally yourself!
        -   `python.pydev.debugger.pydevdCacheDirectory` may be used to customize the directory where it should be installed (if not specified it'll be installed inside the extension's directory).
        -   `python.pydev.debugger.pydevdPipInstall` may be used to customize whether it should actually be pip-installed or if it the version bundled with the extension should be used (which may not have accelerators).
    -   `python.pydev.debugger.pydevdPyFile` may be used to customize the path to the `pydevd.py` debugger file (may be used to force using a specific installation of the debugger).
        -   It can also be overridden in a specific launch configuration by setting the `pydevdPyDebuggerFile` setting in the launch configuration.

-   `0.0.4`:

    -   Fixed issue with variable replacing when no editor is active.
    -   When `console` is not set to a valid value, it'll fallback to `integratedTerminal`.
    -   Set `neverOpen` for internal console when launching with console set to `integratedTerminal`.
    -   Launch templates now have `integratedTerminal` as the default `console`.

-   `0.0.3`: Properly verifies whether `program` and `cwd` are strings and gives a better error message and bundles `pydevd 3.1.0`.
-   `0.0.2`: Properly validating `program` or `module` in launch config, completions working in debug console.
-   `0.0.1`: Initial integration supporting launch and attach modes (most features should be working already).
