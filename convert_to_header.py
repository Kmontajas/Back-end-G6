# convert_to_header.py
import sys

input_file = "trash_classifier_model.tflite"
output_file = "trash_classifier_model_data.h"
var_name = "g_model_data"

with open(input_file, "rb") as f:
    data = f.read()

with open(output_file, "w") as f:
    f.write(f"const unsigned char {var_name}[] = {{\n")
    for i, b in enumerate(data):
        if i % 12 == 0:
            f.write("  ")
        f.write(f"0x{b:02x}, ")
        if i % 12 == 11:
            f.write("\n")
    f.write("\n};\n")
    f.write(f"const int {var_name}_len = {len(data)};\n")

print(f"Created {output_file} with {len(data)} bytes.")
