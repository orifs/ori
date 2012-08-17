PKG_NAME="zlib-1.2.7"
PKG_TARBALL="$PKG_NAME.tar.gz"
PKG_URL="http://zlib.net/$PKG_TARBALL"

TEMP_REPO="$TEMP_DIR/temp_repo"
EXTRACT_DIR="$TEMP_DIR/extract_dir"

# Download, extract, and compile source code
cd $TEMP_DIR
mkdir -p $MTPOINT
mkdir -p $EXTRACT_DIR

$ORI_EXE init $TEMP_REPO

$MOUNT_ORI_EXE -o repo=$TEMP_REPO $MTPOINT
sleep 1.5

cd $MTPOINT
# Download & compare sources
wget "$PKG_URL"
tar xvf "$PKG_TARBALL"

cp "$PKG_TARBALL" $EXTRACT_DIR/
cd $EXTRACT_DIR
tar xvf "$PKG_TARBALL"

$PYTHON $SCRIPTS/compare.py "$EXTRACT_DIR" "$MTPOINT"

# Compile
cd $MTPOINT
cd $PKG_NAME

./configure
make -j10


# Cleanup
cd $TEMP_DIR
umount $MTPOINT

rm -rf $TEMP_REPO
rm -rf $EXTRACT_DIR
