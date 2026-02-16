#!/usr/bin/env python3
# smart_plug/python_utils/pem_to_c.py
"""
Convert PEM certificate files to C strings for embedding.
Usage: python pem_to_c.py cert.pem output.c variable_name
"""

import sys
import os

def pem_to_c(input_file, output_file, var_name):
    """Convert PEM file to C string"""
    
    with open(input_file, 'r') as f:
        pem = f.read().strip()
    
    # Split into lines
    lines = pem.split('\n')
    
    with open(output_file, 'w') as f:
        f.write(f'// Automatically generated from {os.path.basename(input_file)}\n')
        f.write('// DO NOT EDIT DIRECTLY\n\n')
        f.write('#include <stddef.h>\n\n')
        f.write(f'const char {var_name}[] = \n')
        
        for line in lines:
            if line:
                f.write(f'    "{line}\\n"\n')
        
        f.write('    ;\n\n')
        f.write(f'const size_t {var_name}_len = sizeof({var_name}) - 1;\n')
    
    print(f"Converted {input_file} -> {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: pem_to_c.py input.pem output.c variable_name")
        sys.exit(1)
    
    pem_to_c(sys.argv[1], sys.argv[2], sys.argv[3])