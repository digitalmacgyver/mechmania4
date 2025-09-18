#!/usr/bin/env python3
"""
Script to modernize C++ headers in MechMania IV codebase
Converts old-style headers (iostream.h) to modern C++ headers (iostream)
"""

import os
import re

# Mapping of old headers to new headers
HEADER_MAP = {
    'iostream.h': 'iostream',
    'fstream.h': 'fstream',
    'iomanip.h': 'iomanip',
    'string.h': 'cstring',
    'stdio.h': 'cstdio',
    'stdlib.h': 'cstdlib',
    'math.h': 'cmath',
    'time.h': 'ctime',
    'assert.h': 'cassert',
    'limits.h': 'climits',
    'float.h': 'cfloat',
}

def fix_headers_in_file(filepath):
    """Fix headers in a single file"""

    # Skip non-source files
    if not (filepath.endswith('.C') or filepath.endswith('.h') or
            filepath.endswith('.cpp') or filepath.endswith('.cc')):
        return False

    with open(filepath, 'r') as f:
        content = f.read()

    original_content = content
    modified = False

    # Replace old-style headers
    for old_header, new_header in HEADER_MAP.items():
        pattern = r'#include\s*<' + re.escape(old_header) + r'>'
        replacement = '#include <' + new_header + '>'
        content, count = re.subn(pattern, replacement, content)
        if count > 0:
            modified = True
            print(f"  Replaced {old_header} with {new_header} ({count} occurrences)")

    # Add using namespace std if iostream is included and not already present
    if '#include <iostream>' in content and 'using namespace std;' not in content:
        # Find the last include statement
        include_lines = []
        lines = content.split('\n')
        last_include_idx = -1

        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i

        if last_include_idx >= 0:
            # Insert using namespace std after the last include
            lines.insert(last_include_idx + 1, '')
            lines.insert(last_include_idx + 2, 'using namespace std;')
            content = '\n'.join(lines)
            modified = True
            print(f"  Added 'using namespace std;'")

    # Fix BOOL to bool (common in old code)
    if 'BOOL' in content:
        # First check if BOOL is typedef'd
        if 'typedef' not in content or 'BOOL' not in content:
            content = re.sub(r'\bBOOL\b', 'bool', content)
            modified = True
            print(f"  Replaced BOOL with bool")

    # Fix TRUE/FALSE to true/false
    content = re.sub(r'\bTRUE\b', 'true', content)
    content = re.sub(r'\bFALSE\b', 'false', content)

    if content != original_content:
        with open(filepath, 'w') as f:
            f.write(content)
        return True

    return False

def process_directory(directory):
    """Process all C++ files in directory recursively"""

    total_modified = 0

    for root, dirs, files in os.walk(directory):
        # Skip .git directory
        if '.git' in dirs:
            dirs.remove('.git')

        for filename in files:
            filepath = os.path.join(root, filename)

            # Skip binary files and temporary files
            if filename.startswith('.') or filename.endswith('.o'):
                continue

            print(f"Processing {filepath}...")
            if fix_headers_in_file(filepath):
                total_modified += 1

    return total_modified

if __name__ == '__main__':
    src_dir = 'team/src'

    print(f"Modernizing C++ headers in {src_dir}...")
    modified_count = process_directory(src_dir)

    print(f"\nModified {modified_count} files")
    print("Header modernization complete!")