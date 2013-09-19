cd $TEMP_DIR

# Initialize source repository
$ORI_EXE newfs $SOURCE_FS
mkdir $SOURCE_FS
orifs $SOURCE_FS

cp -R $SOURCE_FILES/* $SOURCE_FS/

cd $SOURCE_FS
$ORI_EXE status
echo "Committing"
$ORI_EXE snapshot
#gdb $ORI_EXE -x $SCRIPTS/commit.gdb
cd ..

$UMOUNT $SOURCE_FS

cd ~/.ori/$SOURCE_FS.ori
$ORIDBG_EXE verify
#bash -e $SCRIPTS/verify_refcounts.sh

cd $TEMP_DIR

