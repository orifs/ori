import sys
import random

filename = sys.argv[1]
size_kb = int(sys.argv[2])

with open(filename, 'wb') as f:
    curr_size = 0
    while curr_size < size_kb:
        f.write(hex(random.getrandbits(8192)))
        curr_size += 2
