cd $TEMP_DIR
mkdir -p $MTPOINT

$ORIFS_EXE --repo=$SOURCE_REPO $MTPOINT
sleep 1.5

echo "hello" > $MTPOINT/hello
echo "file 2" > $MTPOINT/file2.tst
mv $MTPOINT/file2.tst $MTPOINT/file3.tst
cp $MTPOINT/file10.tst $MTPOINT/file11.tst
echo "testing" > $MTPOINT/file3.tst
mv $MTPOINT/file11.tst $MTPOINT/file3.tst

#echo "commit" > $MTPOINT/.ori_control
cd $MTPOINT
$ORI_EXE commit
sleep 3

cd $SOURCE_REPO
# TODO: without this, deleted files won't be removed by checkout
rm -rf *
$ORI_EXE checkout

$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$MTPOINT"

$UMOUNT $MTPOINT

