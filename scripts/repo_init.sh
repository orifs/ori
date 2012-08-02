# Initialize source repository
$ORI_EXE init $SOURCE_REPO
cd $SOURCE_REPO
cp -R $SOURCE_FILES/* $SOURCE_REPO/
$ORI_EXE status
echo "Committing"
$ORI_EXE commit
#gdb $ORI_EXE -x $SCRIPTS/commit.gdb
$ORI_EXE verify
rm -rf $SOURCE_REPO/*
$ORI_EXE stats
echo "Checking out files again"
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_FILES" "$SOURCE_REPO"
$ORI_EXE verify

bash -e $SCRIPTS/verify_refcounts.sh
