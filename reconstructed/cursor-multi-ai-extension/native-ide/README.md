# native-ide

This folder contains a small native IDE demo and build-helper utilities.

What I changed:

- Added `btnBuilder.java` — a minimal, self-contained Java 21 Swing GUI
    that can be run with `java --source 21 btnBuilder.java`. This replaces
    an empty file and provides a runnable entrypoint for the
    `BUILD_GUI.bat` script.
- Fixed `BUILD_GUI.bat` to remove Markdown fencing and make it a valid
    batch file that launches the Java single-file program.

Why:

- Per `MASTER_RULESET.md` we should provide production-ready, runnable
    examples and good documentation. These conservative edits add a
    minimal, safe runnable GUI and documentation without modifying other
    source code.

How to run:

1. Ensure you have Java 21 (or newer) installed and on PATH.
2. From this folder run `BUILD_GUI.bat` or the equivalent command:

        java --source 21 btnBuilder.java

Notes and compliance:

- No source behavior beyond a simulated build GUI was added; this keeps
    the change low-risk.
- If you want a native compiled launcher or integration with the real
    toolchain in `src/`, I can wire that next.
