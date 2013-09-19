cd $TEMP_DIR

$ORI_EXE newfs $TEST_FS
$ORI_EXE replicate $TEST_FS $TEST_FS2

cd ~/.ori/$TEST_FS2.ori
$ORIDBG_EXE verify
$ORIDBG_EXE stats

cd $TEMP_DIR
$ORI_EXE removefs $TEST_FS
$ORI_EXE removefs $TEST_FS2

