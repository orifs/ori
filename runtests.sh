#!/usr/bin/env bash

# Run from the ori root directory
# Use file runtests_config.sh to selectively run tests
# TODO: nothing works if CWD has spaces in path

export REMOTEHOST=$2
export LOCALHOST=$3

export ORIG_DIR=`pwd`
export ORI_EXE=$ORIG_DIR/build/ori/ori
export ORI_HTTPD=$ORIG_DIR/build/ori_httpd/ori_httpd
export ORIFS_EXE=$ORIG_DIR/build/orifs/orifs
export ORIDBG_EXE=$ORIG_DIR/build/oridbg/oridbg
export ORISYNC_EXE=$ORIG_DIR/build/orisync/orisync
export ORI_TESTS=$ORIG_DIR/tests

export TEMP_DIR=$ORIG_DIR/tempdir
export SOURCE_FILES=$TEMP_DIR/files
export SOURCE_REPO=$TEMP_DIR/source_repo
export TEST_REPO2=$TEMP_DIR/test_repo2
export TEST_MERGEREPO=$TEMP_DIR/test_merge
export MTPOINT=$TEMP_DIR/mtpoint

export SOURCE_FS=oritest_source
export TEST_FS=oritest_fs
export TEST_FS2=oritest_fs2

export PYTHON="/usr/bin/env python"
export SCRIPTS=$ORIG_DIR/scripts
export TEST_RESULTS=$ORIG_DIR/ori_test_results.txt

# FreeBSD and Mac OS X use umount while Linux has fusermount -u
if [[ `uname` == "Linux" ]]; then
    export UMOUNT="fusermount -u"
else
    export UMOUNT="umount"
fi

# TODO: Fix tests that haven't been updated to new UI
HTTP_CLONE="no"
HTTP_PULL="no"
MERGE="no"
MOUNT_WRITE="no"
MOUNT_WRITE_PYTHON_MT="no"

if [ -f $ORIG_DIR/runtests_config.sh ]; then
    . $ORIG_DIR/runtests_config.sh
fi

# Check for tempdir
if [ -d $TEMP_DIR ]; then
    echo "Directory $TEMP_DIR already exists,"
    echo "please delete before running tests"
    exit
fi

if [ -d ~/.ori/$SOURCE_FS.ori ]; then
    echo "One or more test file systems have not been deleted!"
    exit
fi

mkdir $TEMP_DIR
mkdir $TEMP_DIR/$TEST_FS
mkdir $TEMP_DIR/$TEST_FS2

$ORIG_DIR/runtests_prep.sh &> $TEMP_DIR/runtests_prep.log

printf "Ori Test Suite\n----------\nRUNNING TESTS\n----------\n"

# Run tests
echo "runtests.sh test results" > $TEST_RESULTS
date >> $TEST_RESULTS
echo >> $TEST_RESULTS

for t in `find $ORI_TESTS -name \*.sh`; do
    # TODO: turn off tests
    TEST_NAME=`basename $t .sh`
    UPPERNAME=`echo $TEST_NAME | tr '[a-z]-' '[A-Z]_'`
    VARNAME=${UPPERNAME:3}
    if [ ${!VARNAME}"!" = "no!" ]; then
        echo "SKIPPED: $TEST_NAME" | tee -a $TEST_RESULTS
        continue
    fi

    echo Running $TEST_NAME
    cd $TEMP_DIR
    bash -ex $t &> $TEMP_DIR/$TEST_NAME.log
    # -e switch to quit on error

    if [ "$?" -ne "0" ] ; then
        echo "FAILED: $TEST_NAME" | tee -a $TEST_RESULTS
        exit 1
        # TODO: to keep directory state
    else
        echo "$TEST_NAME" >> $TEST_RESULTS
    fi
done

# Delete test repo, temp dir
if true; then
    cd $ORIG_DIR
    echo "Deleting directories"
    $ORI_EXE removefs $SOURCE_FS
    rm -rf $SOURCE_FILES
rm -rf $TEMP_DIR
fi
