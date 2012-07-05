import os
import os.path
import sys

dir1 = sys.argv[1]
dir2 = sys.argv[2]

def filenames(dname):
    rval = set()
    for root, dirs, files in os.walk(dname):
        relroot = root[len(dname):].lstrip('/')
        if relroot.startswith('.ori'):
            continue

        for fn in files:
            fullfn = os.path.join(relroot, fn)
            rval.add(fullfn)
    return rval

print("Checking directory trees")
set1 = filenames(dir1)
set2 = filenames(dir2)
diff = set1.symmetric_difference(set2)
if len(diff) > 0:
    print("Some files are not present in both directories:")
    print(diff)
    sys.exit(1)

# Check file contents
def readall(fname):
    with open(fname, 'rb') as f:
        return f.read()

print("Checking file contents")
for fn in set1:
    f1 = readall(os.path.join(dir1, fn))
    f2 = readall(os.path.join(dir2, fn))
    if f1 != f2:
        print("File contents not identical!")
        print(fn)
