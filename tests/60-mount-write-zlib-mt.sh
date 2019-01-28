PKG_NAME="zlib-1.2.11"
PKG_TARBALL="$PKG_NAME.tar.gz"
PKG_URL="http://zlib.net/$PKG_TARBALL"

# Download, extract, and compile source code
$ORI_EXE newfs $TEST_FS
$ORIFS_EXE $TEST_FS $TEST_FS

sleep 1

cd $TEST_FS

# Download & compare sources
wget "$PKG_URL"
tar xvf "$PKG_TARBALL"

# Compile
cd $PKG_NAME

./configure
make -j4

# Cleanup
cd $TEMP_DIR
$UMOUNT $TEST_FS

$ORI_EXE removefs $TEST_FS

