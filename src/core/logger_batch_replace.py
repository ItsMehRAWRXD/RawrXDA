#!/usr/bin/env python3
import re
import sys
from pathlib import Path

def fix_cout_in_file(filepath):
    """Replace std::cout with Logger in a single file"""
    content = Path(filepath).read_text()
    original = content
    
    # Add logger include if needed
    if 'logging/logger.h' not in content and 'std::cout' in content:
        # Find first include
        match = re.search(r'#include', content)
        if match:
            # Add after first include block
            include_block = re.search(r'((?:#include[^\n]*\n)+)', content)
            if include_block:
                insert_pos = include_block.end()
                basename = Path(filepath).stem
                logger_code = f'\n#include "logging/logger.h"\nstatic Logger s_logger("{basename}");\n'
                content = content[:insert_pos] + logger_code + content[insert_pos:]
    
    # Replace std::cout patterns
    content = re.sub(r'std::cout\s*<<\s*"([^"]+)"[^;]*;', 
                     r's_logger.info("\1");', content)
    content = re.sub(r'std::cout\s*<<', r's_logger.info(', content)
    content = re.sub(r'std::cerr\s*<<', r's_logger.error(', content)
    
    if content != original:
        Path(filepath).write_text(content)
        return True
    return False

if __name__ == "__main__":
    files = sys.argv[1:]
    fixed = 0
    for f in files:
        if fix_cout_in_file(f):
            print(f"Fixed: {f}")
            fixed += 1
    print(f"Total fixed: {fixed}/{len(files)}")
