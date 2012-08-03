# New file, new dir
cd $SOURCE_FILES
echo "New file" > $SOURCE_FILES/new
mkdir $SOURCE_FILES/new_dir

# Modify, delete
echo "Changed" > $SOURCE_FILES/a/a.txt
echo "Changed dir a" > $SOURCE_FILES/a/b.txt
rm $SOURCE_FILES/a/empty

# Replace file with dir
rm $SOURCE_FILES/b/b.txt
mkdir $SOURCE_FILES/b/b.txt
touch $SOURCE_FILES/b/b.txt/file

# Replace dir with file
rmdir $SOURCE_FILES/a/empty_dir
echo "New file" > $SOURCE_FILES/a/empty_dir

# Change LargeBlob
$PYTHON $SCRIPTS/changefile.py "$SOURCE_FILES/file10.tst"
echo "a" >> $SOURCE_FILES/file10.tst

# Copy changes to repo
rsync -rv --delete --exclude=".ori" $SOURCE_FILES/ $SOURCE_REPO/

# Status, commit
echo "Committing changes"
cd $SOURCE_REPO
$ORI_EXE status
$ORI_EXE commit
$ORI_EXE verify
$ORI_EXE status
$ORI_EXE stats

$PYTHON $SCRIPTS/compare.py "$SOURCE_FILES" "$SOURCE_REPO"

echo "Checking out again"
cd $SOURCE_REPO
rm -rf $SOURCE_REPO/*
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_FILES" "$SOURCE_REPO"

bash -e $SCRIPTS/verify_refcounts.sh
