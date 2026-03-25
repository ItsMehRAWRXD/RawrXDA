# Organize Desktop Files into Project Folders

This helper keeps your Desktop tidy by ensuring every standalone file is wrapped inside its own folder named after the file. It's especially useful when you want each project to live inside a dedicated directory rather than scattered on the Desktop surface.

## Usage

```powershell
cd "C:\Users\HiH8e\OneDrive\Desktop\Powershield\tools"
./organize-desktop.ps1 -DryRun
```

Start with `-DryRun` to preview the folder creation/move operations. Once you are satisfied, rerun without `-DryRun`.

### Options

- `-SourcePath <path>` – point to a custom desktop if you manage multiple user profiles.
- `-Exclude <names>` – skip files you want to leave untouched (for example, shortcuts or scripts you still use in place).

## Safety notes

- The script skips existing directories to avoid moving entire projects into deeper nested folders.
- Always review the `-DryRun` output before performing a real pass; once files move, Windows will show them inside their new folders.
