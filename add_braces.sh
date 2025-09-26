#!/bin/bash

# Script to add braces around single statements using clang-tidy
# Uses readability-braces-around-statements check

cd "$(dirname "$0")"

echo "Adding braces around single statements with clang-tidy..."

# Find all .h and .C files in team/src and teams/ directories
find team/src teams -name "*.h" -o -name "*.C" | while read -r file; do
    echo "Processing: $file"
    clang-tidy --fix-errors "$file" -- -I team/src 2>/dev/null
done

echo "Brace addition complete!"
echo ""
echo "Running clang-format to clean up formatting..."
./format_code.sh

echo ""
echo "All formatting complete!"