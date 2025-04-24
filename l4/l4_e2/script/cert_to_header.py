import sys

cer_file_name = sys.argv[1]
cer_file = "../" + cer_file_name
header_file="../src/certificate.h"

key = open(cer_file, "r")
c_file = open(header_file,"w")
for line in key:
    line = line.replace("\n","")
    c_line = "\"" + line + "\\n\" \\\n"
    c_file.write(c_line)
print('Certificate converted to C header file in:',header_file)
key.close()
c_file.close()