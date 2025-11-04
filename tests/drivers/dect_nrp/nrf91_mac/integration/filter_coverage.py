#!/usr/bin/env python3
"""
Filter lcov coverage.info file to only include DECT NR+ stack files.
"""

import sys
import os

def filter_coverage(input_file, output_file, target_dirs):
    """Filter coverage file to only include files from target directories."""
    current_file = None
    include_file = False
    output_lines = []
    files_included = 0
    files_excluded = 0

    try:
        with open(input_file, 'r') as f:
            for line in f:
                if line.startswith('SF:'):
                    current_file = line[3:].strip()
                    # Check if file path matches any target directory
                    include_file = any(current_file.startswith(d) for d in target_dirs)
                    if include_file:
                        output_lines.append(line)
                        files_included += 1
                    else:
                        files_excluded += 1
                elif include_file:
                    output_lines.append(line)
                elif line.startswith('end_of_record'):
                    if include_file:
                        output_lines.append(line)
                    include_file = False
                    current_file = None

        with open(output_file, 'w') as f:
            f.writelines(output_lines)

        print(f"Filtered coverage: {files_included} files included, {files_excluded} files excluded")
        return files_included > 0

    except Exception as e:
        print(f"Error filtering coverage: {e}", file=sys.stderr)
        return False

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: filter_coverage.py <input.info> <output.info> <dir1> [dir2] ...", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    target_dirs = sys.argv[3:]

    # Convert to absolute paths
    target_dirs = [os.path.abspath(d) for d in target_dirs]

    success = filter_coverage(input_file, output_file, target_dirs)
    sys.exit(0 if success else 1)

