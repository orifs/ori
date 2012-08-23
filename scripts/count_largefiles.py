import os
import os.path
import sys


LF_LIMIT = 1024*1024

num_files = 0
num_largefiles = 0

for root, dirs, files in os.walk(sys.argv[1]):
    #print(root)
    for fn in files:
        fullpath = os.path.join(root, fn)
        num_files += 1
        if os.path.getsize(fullpath) >= LF_LIMIT:
            num_largefiles += 1

print("Large files: {}\nTotal files: {}\nLarge file ratio: {}".format(
    num_largefiles,
    num_files,
    float(num_largefiles) / num_files))
