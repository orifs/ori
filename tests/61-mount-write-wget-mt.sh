PKG_NAME="wget-1.14"
PKG_TARBALL="$PKG_NAME.tar.gz"
PKG_URL="http://ftp.gnu.org/gnu/wget/$PKG_TARBALL"

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

./configure --without-ssl
make -j8

# Cleanup
cd $TEMP_DIR
$UMOUNT $TEST_FS

$ORI_EXE removefs $TEST_FS

