import os
import sys
import os.path
import random

CHANGE_SIZE = 30

size = os.path.getsize(sys.argv[1])
changes = [0]
for i in range(3):
    changes.append(random.randint(0,size-1-CHANGE_SIZE))
changes.sort()

newbuf = b''

with open(sys.argv[1], 'rb') as f:
    for i,c in enumerate(changes):
        if c == 0: continue
        rsize = max(0, c-changes[i-1])
        newbuf += f.read(rsize)
        for j in range(random.randint(CHANGE_SIZE-5, CHANGE_SIZE+5)):
            newbuf += chr(random.randint(60,80))
        changes[i] += CHANGE_SIZE
        f.seek(CHANGE_SIZE, os.SEEK_CUR)
    rsize = max(0, size - changes[-1])
    newbuf += f.read(rsize)

with open(sys.argv[1], 'wb') as f:
    f.write(newbuf)
