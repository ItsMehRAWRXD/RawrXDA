#!/usr/bin/env python3
"""
Remove ALL emojis from toolchain files to prevent ROE malformaties
"""

import os
import re

def remove_emojis_from_file(file_path):
    """Remove all emojis from a file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Remove all emoji characters
        emoji_pattern = re.compile(
            "["
            "\U0001F600-\U0001F64F"  # emoticons
            "\U0001F300-\U0001F5FF"  # symbols & pictographs
            "\U0001F680-\U0001F6FF"  # transport & map symbols
            "\U0001F1E0-\U0001F1FF"  # flags (iOS)
            "\U00002600-\U000026FF"  # miscellaneous symbols
            "\U00002700-\U000027BF"  # dingbats
            "\U0001F900-\U0001F9FF"  # supplemental symbols
            "\U0001FA70-\U0001FAFF"  # symbols and pictographs extended-a
            "\U0001F018-\U0001F0FF"  # playing cards
            "\U0001F200-\U0001F2FF"  # enclosed characters
            "\U0001F300-\U0001F5FF"  # miscellaneous symbols and pictographs
            "\U0001F600-\U0001F64F"  # emoticons
            "\U0001F680-\U0001F6FF"  # transport and map symbols
            "\U0001F700-\U0001F77F"  # alchemical symbols
            "\U0001F780-\U0001F7FF"  # geometric shapes extended
            "\U0001F800-\U0001F8FF"  # supplemental arrows-c
            "\U0001F900-\U0001F9FF"  # supplemental symbols and pictographs
            "\U0001FA00-\U0001FA6F"  # chess symbols
            "\U0001FA70-\U0001FAFF"  # symbols and pictographs extended-a
            "\U0001FB00-\U0001FBFF"  # symbols for legacy computing
            "\U0001FC00-\U0001FCFF"  # symbols for legacy computing
            "\U0001FD00-\U0001FDFF"  # symbols for legacy computing
            "\U0001FE00-\U0001FEFF"  # variation selectors
            "\U0001FF00-\U0001FFFF"  # symbols for legacy computing
            "]+"
        )
        
        # Remove emojis
        cleaned_content = emoji_pattern.sub('', content)
        
        # Also remove common emoji symbols that might be missed
        common_emojis = [
            '🔧', '🚀', '📁', '📦', '✅', '❌', '🎉', '🎭', '🛡️', '🔐', '🥷', '📝', 
            '⚙️', '🔗', '📋', '🏢', '🔄', '📊', '🎯', '🔍', '📚', '📖', '⚡', '🔩',
            '🌐', '🎨', '🗃️', '☕', '📄', '🗂️', '🏗️', '👁️', '📚', '⚠️', '🔄'
        ]
        
        for emoji in common_emojis:
            cleaned_content = cleaned_content.replace(emoji, '')
        
        # Write back to file
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(cleaned_content)
        
        print(f"Cleaned emojis from: {file_path}")
        return True
        
    except Exception as e:
        print(f"Error cleaning {file_path}: {e}")
        return False

def main():
    """Remove emojis from all toolchain files"""
    print("Removing ALL emojis from toolchain files...")
    
    # List of toolchain files to clean
    toolchain_files = [
        'eon_to_machine_code_compiler.eon',
        'custom_linker.eon', 
        'custom_assembler.eon',
        'self_hosting_bootstrap_compiler.eon',
        'compiler_pattern_generator.py',
        'antivirus_friendly_compiler.py',
        'test_complete_toolchain.bat',
        'proper_exe_compiler.py',
        'eon-compiler-gui.py'
    ]
    
    cleaned_count = 0
    
    for filename in toolchain_files:
        if os.path.exists(filename):
            if remove_emojis_from_file(filename):
                cleaned_count += 1
        else:
            print(f"File not found: {filename}")
    
    print(f"\nCleaned {cleaned_count} files")
    print("All emojis removed from toolchain files!")
    print("ROE malformaties should be eliminated!")

if __name__ == "__main__":
    main()
