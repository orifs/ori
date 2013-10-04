cd $TEMP_DIR

# Mount first to use SSH over UDS socket
$ORIFS_EXE $SOURCE_FS

$ORI_EXE replicate localhost:$SOURCE_FS $TEST_FS

$ORIFS_EXE $TEST_FS

$PYTHON $SCRIPTS/compare.py "$SOURCE_FS" "$TEST_FS"

$UMOUNT $SOURCE_FS
$UMOUNT $TEST_FS

cd ~/.ori/$TEST_FS.ori
$ORIDBG_EXE verify
$ORIDBG_EXE stats

cd $TEMP_DIR
$ORI_EXE removefs $TEST_FS

