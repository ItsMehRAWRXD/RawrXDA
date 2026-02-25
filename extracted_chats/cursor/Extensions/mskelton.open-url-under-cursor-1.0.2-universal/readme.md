# Open URL

A VS Code extension that provides a command to open URLs under the cursor in
your default browser.

## Features

This extension provides a single command:

- **Open URL Under Cursor**: Detects if the cursor is positioned within a URL
  and opens it in your default browser.

## Usage

1. Position your cursor within a URL in your editor
2. Run the command using one of these methods:
   - Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (Mac) to open the
     command palette, then search for "Open URL Under Cursor"
   - Assign a keyboard shortcut to the `open-url.openUrlUnderCursor` command

## Installation

You can install this extension through the VS Code Marketplace or by building
from source:

### Building from Source

1. Clone the repository
2. Run `npm install` to install dependencies
3. Run `npm run compile` to build the extension
4. Copy the extension files to your VS Code extensions folder or use the
   "Developer: Install Extension from Location..." command
