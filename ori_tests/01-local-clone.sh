cd $TEMP_DIR
$ORI_EXE clone $SOURCE_REPO $TEST_REPO2
cd $TEST_REPO2
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$TEST_REPO2"
$ORI_EXE verify
$ORI_EXE status
$ORI_EXE stats

bash -e $SCRIPTS/verify_refcounts.sh

cd $TEMP_DIR
rm -rf $TEST_REPO2
