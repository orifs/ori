cd $TEMP_DIR
cd $SOURCE_REPO
$ORI_HTTPD &
sleep 1

$ORI_EXE init $TEST_REPO2
cd $TEST_REPO2
$ORI_EXE pull http://localhost/$SOURCE_REPO
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$TEST_REPO2"
$ORI_EXE verify

kill %1

cd $TEMP_DIR
rm -rf $TEST_REPO2

