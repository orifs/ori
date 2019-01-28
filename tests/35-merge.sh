cd $TEMP_DIR
$ORI_EXE init $TEST_MERGEREPO
cd $TEST_MERGEREPO
NULLREV=`$ORI_EXE tip`

touch a
$ORI_EXE commit
REV=`$ORI_EXE tip`
$ORI_EXE checkout $NULLREV
$ORI_EXE branch test_branch
rm a
touch b
$ORI_EXE commit
$ORI_EXE merge $REV
$ORI_EXE commit

cd $TEMP_DIR
rm -rf $TEST_MERGEREPO
