#!/bin/bash -e
# -e switch to quit on error
# TODO: trap on error

ORIG_DIR=`pwd`
ORI_EXE=`pwd`/build/ori/ori
TEMP_DIR=`pwd`/test_temp
TEST_REPO=`pwd`/test_repo

PYTHON="/usr/bin/env python"
SCRIPTS=`pwd`/scripts

# TODO: new tests for this script

# Make directory of files
# Largest file: (2**16)KB == 64 MB
mkdir -p $TEMP_DIR
for i in {1..9}; do
    SIZE=$((2**(i+1)))
    echo "Creating random file size ${SIZE}k"
    #dd if=/dev/urandom of="$TEMP_DIR/file$i.tst" bs=2k count=$SIZE
    $PYTHON $SCRIPTS/randfile.py "$TEMP_DIR/file$i.tst" $SIZE
done
mkdir -p $TEMP_DIR/a
echo "Hello, world!" > $TEMP_DIR/a/a.txt

# Repository
$ORI_EXE init $TEST_REPO
cd $TEST_REPO
cp -R $TEMP_DIR/* $TEST_REPO/
$ORI_EXE status
echo "Committing"
$ORI_EXE commit
#gdb $ORI_EXE -x $SCRIPTS/commit.gdb
$ORI_EXE verify
rm -rf $TEST_REPO/*
$ORI_EXE status
echo "Checking out files again"
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$TEMP_DIR" "$TEST_REPO"
$ORI_EXE verify

# SSH pulling
if true; then
    cd $ORIG_DIR
    TEST_REPO2=`pwd`/test_repo2
    $ORI_EXE init $TEST_REPO2
    cd $TEST_REPO2
    $ORI_EXE pull localhost:$TEST_REPO
    #gdb $ORI_EXE -x $SCRIPTS/ssh-pull.gdb
    $ORI_EXE checkout
    $PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$TEST_REPO2"
    cd $ORIG_DIR
    rm -rf $TEST_REPO2
fi

# Delete test repo, temp dir
cd $ORIG_DIR
echo "Deleting directories"
rm -rf $TEST_REPO
rm -rf $TEMP_DIR
