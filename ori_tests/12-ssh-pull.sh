cd $ORIG_DIR
$ORI_EXE init $TEST_REPO2
cd $TEST_REPO2
$ORI_EXE pull localhost:$SOURCE_REPO
#gdb $ORI_EXE -x $SCRIPTS/ssh-pull.gdb
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$TEST_REPO2"
$ORI_EXE verify

cd $ORIG_DIR
rm -rf $TEST_REPO2
