#!/bin/bash

# Run from the ori root directory
# Use file runtests_config.sh to selectively run tests
# TODO: nothing works if CWD has spaces in path

export ORIG_DIR=`pwd`
export ORI_EXE=$ORIG_DIR/build/ori/ori
export ORI_HTTPD=$ORIG_DIR/build/ori_httpd/ori_httpd
export MOUNT_ORI_EXE=$ORIG_DIR/build/mount_ori/mount_ori
export ORI_TESTS=$ORIG_DIR/ori_tests

export TEMP_DIR=$ORIG_DIR/tempdir
export SOURCE_FILES=$TEMP_DIR/files
export SOURCE_REPO=$TEMP_DIR/source_repo
export TEST_REPO2=$TEMP_DIR/test_repo2
export MTPOINT=$TEMP_DIR/mtpoint

export PYTHON="/usr/bin/env python"
export SCRIPTS=$ORIG_DIR/scripts
export TEST_RESULTS=$ORIG_DIR/ori_test_results.txt

. $ORIG_DIR/runtests_config.sh

# Make directory of files
# Largest file: (2**16)KB == 64 MB
mkdir -p $SOURCE_FILES
for i in {1..13}; do
    SIZE=$((2**(i+1)))
    echo "Creating random file size ${SIZE}k"
    #dd if=/dev/urandom of="$SOURCE_FILES/file$i.tst" bs=2k count=$SIZE
    $PYTHON $SCRIPTS/randfile.py "$SOURCE_FILES/file$i.tst" $SIZE
done
mkdir -p $SOURCE_FILES/a
echo "Hello, world!" > $SOURCE_FILES/a/a.txt
touch $SOURCE_FILES/a/empty

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

# Run tests
echo "runtests.sh test results" > $TEST_RESULTS
date >> $TEST_RESULTS
echo >> $TEST_RESULTS

for t in `find $ORI_TESTS -name '*.sh' | sort`; do
    # TODO: turn off tests
    TEST_NAME=`basename $t .sh`
    UPPERNAME=`echo $TEST_NAME | tr '[a-z]-' '[A-Z]_'`
    VARNAME=${UPPERNAME:3}
    if [ ${!VARNAME} = "no" ]; then
        echo "SKIPPED: $TEST_NAME" | tee -a $TEST_RESULTS
        continue
    fi

    echo Running $TEST_NAME
    cd $TEMP_DIR
    bash -e $t
    # -e switch to quit on error

    if [ "$?" -ne "0" ] ; then
        echo "FAILED: $TEST_NAME" | tee -a $TEST_RESULTS
    else
        echo "$TEST_NAME" >> $TEST_RESULTS
    fi
done

# Delete test repo, temp dir
cd $ORIG_DIR
echo "Deleting directories"
rm -rf $SOURCE_REPO
rm -rf $SOURCE_FILES
rm -rf $TEMP_DIR
