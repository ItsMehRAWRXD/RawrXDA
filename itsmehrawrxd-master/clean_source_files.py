#!/usr/bin/env python3
"""
Remove all emojis and unicode symbols from source files
This fixes the ROE (malformed UTF-8) issues
"""
import os
import re
from pathlib import Path

def remove_emojis(text):
    """Remove all emojis and unicode symbols"""
    # Remove emojis and symbols
    emoji_pattern = re.compile("["
        "\U0001F600-\U0001F64F"  # emoticons
        "\U0001F300-\U0001F5FF"  # symbols & pictographs
        "\U0001F680-\U0001F6FF"  # transport & map symbols
        "\U0001F1E0-\U0001F1FF"  # flags (iOS)
        "\U00002500-\U00002BEF"  # chinese char
        "\U00002702-\U000027B0"
        "\U00002702-\U000027B0"
        "\U000024C2-\U0001F251"
        "\U0001f926-\U0001f937"
        "\U00010000-\U0010ffff"
        "\u2640-\u2642" 
        "\u2600-\u2B55"
        "\u200d"
        "\u23cf"
        "\u23e9"
        "\u231a"
        "\ufe0f"  # dingbats
        "\u3030"
        "]+", flags=re.UNICODE)
    
    return emoji_pattern.sub('', text)

def clean_file(file_path):
    """Clean a single file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_size = len(content)
        cleaned_content = remove_emojis(content)
        new_size = len(cleaned_content)
        
        if original_size != new_size:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(cleaned_content)
            print(f"CLEANED: {file_path} ({original_size - new_size} chars removed)")
            return True
        else:
            print(f"OK: {file_path} (no emojis found)")
            return False
    except Exception as e:
        print(f"ERROR: {file_path} - {e}")
        return False

def main():
    print("=== EMOJI CLEANUP TOOL ===")
    print("Removing all emojis to fix ROE (malformed UTF-8) issues")
    print()
    
    # File extensions to clean
    extensions = ['.eon', '.py', '.js', '.bat', '.sh', '.md', '.txt', '.asm']
    
    cleaned_count = 0
    total_count = 0
    
    # Walk through all files
    for root, dirs, files in os.walk('.'):
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                file_path = os.path.join(root, file)
                total_count += 1
                if clean_file(file_path):
                    cleaned_count += 1
    
    print()
    print(f"=== CLEANUP COMPLETE ===")
    print(f"Files processed: {total_count}")
    print(f"Files cleaned: {cleaned_count}")
    print(f"Files unchanged: {total_count - cleaned_count}")

if __name__ == "__main__":
    main()
