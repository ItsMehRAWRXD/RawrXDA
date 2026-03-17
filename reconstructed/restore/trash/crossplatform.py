#!/usr/bin/env python3
"""
restore_trash_crossplatform.py

Cross-platform script to restore deleted files from the system trash/recycle bin on Windows, Linux, and macOS.
- Windows: Restores from $Recycle.Bin
- Linux: Restores from ~/.local/share/Trash/files
- macOS: Restores from ~/.Trash

Usage:
  python restore_trash_crossplatform.py --restore filename.txt --restore-dir /path/to/restore
"""
import os
import sys
import shutil
import argparse
import platform
from pathlib import Path

def get_trash_dirs():
    system = platform.system()
    home = str(Path.home())
    if system == 'Windows':
        # Find all $Recycle.Bin subfolders
        drives = [d + ':' for d in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' if os.path.exists(d + ':\\')]
        bins = []
        for drive in drives:
            recycle = os.path.join(drive + '\\', '$Recycle.Bin')
            if os.path.exists(recycle):
                for userdir in os.listdir(recycle):
                    userpath = os.path.join(recycle, userdir)
                    if os.path.isdir(userpath):
                        bins.append(userpath)
        return bins
    elif system == 'Linux':
        return [os.path.join(home, '.local/share/Trash/files')]
    elif system == 'Darwin':
        return [os.path.join(home, '.Trash')]
    else:
        print(f"Unsupported OS: {system}")
        sys.exit(1)

def find_file_in_trash(filename, trash_dirs):
    for tdir in trash_dirs:
        for root, dirs, files in os.walk(tdir):
            if filename in files:
                return os.path.join(root, filename)
    return None

def restore_file(trash_path, restore_dir):
    if not os.path.exists(restore_dir):
        os.makedirs(restore_dir)
    dest = os.path.join(restore_dir, os.path.basename(trash_path))
    if os.path.exists(dest):
        base, ext = os.path.splitext(dest)
        i = 1
        while os.path.exists(f"{base}_restored_{i}{ext}"):
            i += 1
        dest = f"{base}_restored_{i}{ext}"
    shutil.move(trash_path, dest)
    print(f"Restored: {trash_path} -> {dest}")

def main():
    parser = argparse.ArgumentParser(description="Restore deleted files from system trash/recycle bin (cross-platform)")
    parser.add_argument('--restore', required=True, help='Filename to restore (exact name)')
    parser.add_argument('--restore-dir', required=True, help='Directory to restore the file to')
    args = parser.parse_args()

    trash_dirs = get_trash_dirs()
    trash_path = find_file_in_trash(args.restore, trash_dirs)
    if not trash_path:
        print(f"File '{args.restore}' not found in trash.")
        sys.exit(1)
    restore_file(trash_path, args.restore_dir)

if __name__ == '__main__':
    main()
