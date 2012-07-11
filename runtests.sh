#!/bin/bash -e
# -e switch to quit on error
# TODO: trap on error

ORIG_DIR=`pwd`
ORI_EXE=`pwd`/build/ori/ori
ORI_HTTPD=$ORIG_DIR/build/ori_httpd/ori_httpd
MOUNT_ORI_EXE=$ORIG_DIR/build/mount_ori/mount_ori
TEMP_DIR=`pwd`/tempdir
SOURCE_FILES=$TEMP_DIR/files
TEST_REPO=$TEMP_DIR/test_repo
TEST_REPO2=$TEMP_DIR/test_repo2
MTPOINT=$TEMP_DIR/mtpoint

PYTHON="/usr/bin/env python"
SCRIPTS=`pwd`/scripts

# TODO: new tests for this script

# Make directory of files
# Largest file: (2**16)KB == 64 MB
mkdir -p $SOURCE_FILES
for i in {1..12}; do
    SIZE=$((2**(i+1)))
    echo "Creating random file size ${SIZE}k"
    #dd if=/dev/urandom of="$SOURCE_FILES/file$i.tst" bs=2k count=$SIZE
    $PYTHON $SCRIPTS/randfile.py "$SOURCE_FILES/file$i.tst" $SIZE
done
mkdir -p $SOURCE_FILES/a
echo "Hello, world!" > $SOURCE_FILES/a/a.txt
touch $SOURCE_FILES/a/empty

# Repository
$ORI_EXE init $TEST_REPO
cd $TEST_REPO
cp -R $SOURCE_FILES/* $TEST_REPO/
$ORI_EXE status
echo "Committing"
$ORI_EXE commit
#gdb $ORI_EXE -x $SCRIPTS/commit.gdb
$ORI_EXE verify
rm -rf $TEST_REPO/*
$ORI_EXE status
echo "Checking out files again"
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_FILES" "$TEST_REPO"
$ORI_EXE verify

# Local cloning
if false; then
    cd $TEMP_DIR
    $ORI_EXE clone $TEST_REPO $TEST_REPO2
    cd $TEST_REPO2
    $ORI_EXE checkout
    $PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$TEST_REPO2"
    $ORI_EXE verify
    
    cd $TEMP_DIR
    rm -rf $TEST_REPO2
fi

# Local pulling
if false; then
    cd $ORIG_DIR
    $ORI_EXE init $TEST_REPO2
    cd $TEST_REPO2
    $ORI_EXE pull $TEST_REPO
    $ORI_EXE checkout
    $PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$TEST_REPO2"
    $ORI_EXE verify

    cd $ORIG_DIR
    rm -rf $TEST_REPO2
fi

# SSH pulling
if false; then
    cd $ORIG_DIR
    $ORI_EXE init $TEST_REPO2
    cd $TEST_REPO2
    $ORI_EXE pull localhost:$TEST_REPO
    #gdb $ORI_EXE -x $SCRIPTS/ssh-pull.gdb
    $ORI_EXE checkout
    $PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$TEST_REPO2"
    $ORI_EXE verify

    cd $ORIG_DIR
    rm -rf $TEST_REPO2
fi

# HTTP pulling
if false; then
    cd $TEMP_DIR
    cd $TEST_REPO
    $ORI_HTTPD &
    sleep 1

    $ORI_EXE init $TEST_REPO2
    cd $TEST_REPO2
    $ORI_EXE pull http://localhost/$TEST_REPO
    $ORI_EXE checkout
    $PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$TEST_REPO2"
    $ORI_EXE verify

    kill %1

    cd $TEMP_DIR
    rm -rf $TEST_REPO2
fi

# Test FUSE reading
if true; then
    cd $TEMP_DIR
    mkdir -p $MTPOINT

    $MOUNT_ORI_EXE -o repo=$TEST_REPO $MTPOINT
    sleep 1

    ls -lah $MTPOINT
    
    #$PYTHON $SCRIPTS/compare.py "$TEST_REPO" "$MTPOINT"

    umount $MTPOINT
fi

# Delete test repo, temp dir
cd $ORIG_DIR
echo "Deleting directories"
rm -rf $TEST_REPO
rm -rf $SOURCE_FILES
rm -rf $TEMP_DIR
