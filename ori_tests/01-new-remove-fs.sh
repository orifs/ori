cd $TEMP_DIR

# newfs
$ORI_EXE newfs $TEST_FS

# mount
mkdir $TEST_FS
$ORIFS_EXE $TEST_FS
cd $TEST_FS
ls > /dev/null
cd ..
$UMOUNT $TEST_FS

cd ~/.ori/$TEST_FS.ori/
$ORIDBG_EXE verify
$ORIDBG_EXE stats
cd $TEMP_DIR

# remove
$ORI_EXE removefs $TEST_FS

