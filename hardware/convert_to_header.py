# tflite_to_header.py
import sys
bname = "trash_classifier_model_int8_32.tflite"
out = "model_data.h"
with open(bname,"rb") as f:
    data = f.read()
arr = ", ".join("0x{:02x}".format(c) for c in data)
with open(out,"w") as f:
    f.write("const unsigned char g_model_data[] = {")
    # write in lines
    for i in range(0, len(data), 12):
        f.write("\n  " + ", ".join("0x{:02x}".format(c) for c in data[i:i+12]) + ",")
    f.write("\n};\n")
    f.write("const unsigned int g_model_data_len = {};\n".format(len(data)))
print("Wrote", out)
