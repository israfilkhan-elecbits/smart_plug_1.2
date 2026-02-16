#!/usr/bin/env python3
# smart_plug/scripts/html_to_c.p

import sys
import os
import re

def html_to_c(input_file, output_file):
    """Convert HTML file to C string with proper escaping"""
    
    # Open with UTF-8 encoding
    with open(input_file, 'r', encoding='utf-8') as f:
        html = f.read()
    
    # Escape double quotes and backslashes
    html = html.replace('\\', '\\\\')
    html = html.replace('"', '\\"')
    
    # Split into lines for readability
    lines = html.split('\n')
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('// Automatically generated from {}\n'.format(os.path.basename(input_file)))
        f.write('// DO NOT EDIT DIRECTLY\n\n')
        f.write('#include <stddef.h>\n\n')
        f.write('const char index_html[] = {\n')
        
        # Write as string literal with line breaks
        for i, line in enumerate(lines):
            if i > 0:
                f.write('    "\\n"\n')
            if line:
                f.write('    "{}"\n'.format(line))
        
        f.write('};\n\n')
        f.write('const size_t index_html_len = sizeof(index_html) - 1;\n')
    
    print(f"Converted {input_file} -> {output_file}")

def pem_to_c(input_file, output_file, var_name):
    """Convert PEM certificate to C string"""
    
    # Open with UTF-8 encoding
    with open(input_file, 'r', encoding='utf-8') as f:
        pem = f.read()
    
    # Escape double quotes and backslashes
    pem = pem.replace('\\', '\\\\')
    pem = pem.replace('"', '\\"')
    
    # Split into lines
    lines = pem.split('\n')
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('// Automatically generated from {}\n'.format(os.path.basename(input_file)))
        f.write('// DO NOT EDIT DIRECTLY\n\n')
        f.write('#include <stddef.h>\n\n')
        f.write('const char {}[] = {{\n'.format(var_name))
        
        # Write as string literal with line breaks
        for line in lines:
            if line:
                f.write('    "{}"\n'.format(line))
            f.write('    "\\n"\n')
        
        f.write('};\n\n')
        f.write('const size_t {}_len = sizeof({}) - 1;\n'.format(var_name, var_name))
    
    print(f"Converted {input_file} -> {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage:")
        print("  html_to_c.py input.html output.c")
        print("  pem_to_c.py input.pem output.c var_name")
        sys.exit(1)
    
    if len(sys.argv) == 4:
        pem_to_c(sys.argv[1], sys.argv[2], sys.argv[3])
    else:
        html_to_c(sys.argv[1], sys.argv[2])