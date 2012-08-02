# Check refcounts
echo "Checking refcounts"
REFCOUNTS_1=`$ORI_EXE refcount | tee ../refcount1.txt`
$ORI_EXE rebuildrefs
REFCOUNTS_2=`$ORI_EXE refcount | tee ../refcount2.txt`
test "$REFCOUNTS_1" = "$REFCOUNTS_2"
