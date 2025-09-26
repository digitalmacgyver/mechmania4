#!/bin/bash

# Script to format all .h and .C files with clang-format
# Using Google C++ style

cd "$(dirname "$0")"

echo "Formatting C++ files with clang-format (Google style)..."

# Find all .h and .C files in team/src and teams/ directories
find team/src teams -name "*.h" -o -name "*.C" | while read -r file; do
    echo "Formatting: $file"
    clang-format -i -style=file "$file"
done

echo "Code formatting complete!"
echo ""
echo "Formatted files:"
find team/src teams -name "*.h" -o -name "*.C" | sort