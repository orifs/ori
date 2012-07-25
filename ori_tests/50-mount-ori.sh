cd $TEMP_DIR
mkdir -p $MTPOINT

$MOUNT_ORI_EXE -o repo=$SOURCE_REPO $MTPOINT
sleep 3

ls -lah $MTPOINT
$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$MTPOINT"

umount $MTPOINT

