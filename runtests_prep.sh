#!/bin/bash

# Make directory of files
# Largest file: (2**16)KB == 64 MB
mkdir -p $SOURCE_FILES
for i in {6..11}; do
    SIZE=$((2**(i+1)))
    echo "Creating random file size ${SIZE}k"
    #dd if=/dev/urandom of="$SOURCE_FILES/file$i.tst" bs=2k count=$SIZE
    $PYTHON $SCRIPTS/randfile.py "$SOURCE_FILES/file$i.tst" $SIZE
done
mkdir -p $SOURCE_FILES/a
echo "Hello, world!" > $SOURCE_FILES/a/a.txt
touch $SOURCE_FILES/a/empty
mkdir $SOURCE_FILES/a/empty_dir
mkdir $SOURCE_FILES/b
echo "Foo" > $SOURCE_FILES/b/b.txt

# Add Linux kernel?
if true; then
    #curl "http://www.kernel.org/pub/linux/kernel/v3.0/testing/linux-3.6-rc5.tar.bz2" | tar -C "$SOURCE_FILES" -xvf -
    tar -C "$SOURCE_FILES" -xvf ori-perf/data/linux-tip.tgz
fi

cat > $TEMP_DIR/.gdbinit <<EOM
file $ORIG_DIR/build/ori/ori
set args -d -o repo=source_repo,daemon_timeout=5 mtpoint
EOM

# Initial tests/initialize source repo
for script in repo_init.sh commit_again.sh; do
    bash -ex $SCRIPTS/$script
    if [ "$?" -ne "0" ] ; then
        echo "Script $script failed!"
        exit 1
    fi
done

