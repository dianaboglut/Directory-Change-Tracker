#!/bin/bash


# Check if the file exists
if [ ! -f "$1" ]; then
    echo "File does not exist"
    exit 1
fi

# Check for non-ASCII characters
#if grep -qP "[^\x00-\x7F]" "$file"; then
#    echo "$file" # Non Ascii Char Found
#    exit 1
#fi

file="$1"

# Check for keywords
if grep -qE 'corrupted|dangerous|risk|attack|malware|malicious' "$1"; then
    #echo "File contains suspicious keywords"
    echo "$file"
    exit 3
fi

# Check for file size (number of lines, words, and characters)
line_count=$(wc -l < "$1")
word_count=$(wc -w < "$1")
char_count=$(wc -m < "$1")

# Check the conditions for line count, word count and char count
if [ "$line_count" -lt 3 ] && [ "$word_count" -gt 1000 ] && [ "$char_count" -gt 2000 ]; then
    echo "$file"
    exit 1
fi

# If none of the checks failed, exit with success
echo "SAFE" # > "/dev/null"
exit 0
