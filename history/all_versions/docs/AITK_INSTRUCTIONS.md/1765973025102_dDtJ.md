# Instruction file integration

This project supports loading a custom instruction file that will be prepended to messages before every AI response.

- Environment variable: `AITK_INSTRUCTIONS_PATH`
  - If set, the file at this path will be loaded and watched for changes.
- Default locations (checked in order):
  - `$HOME/.aitk/instructions/tools.instructions.md`
  - `C:/Users/<USERNAME>/.aitk/instructions/tools.instructions.md`

Behavior:
- If the file exists it is read at initialization and its contents are prepended to every message sent to the model or fallback response generator.
- File changes are detected with a `QFileSystemWatcher` and the file is reloaded automatically.

Usage example:
- Place your custom instructions in `C:\Users\HiH8e\.aitk\instructions\tools.instructions.md` and restart the application, or set the `AITK_INSTRUCTIONS_PATH` environment variable to point to it.

Security & robustness:
- The loader logs errors when the file cannot be opened.
- The file is read as plain text and is not executed.
