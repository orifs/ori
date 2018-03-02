cd $TEMP_DIR

# create source fs
$ORIFS_EXE $SOURCE_FS
cd $SOURCE_FS

# orisync initial configuration
orisync_init() {
cat <<End-of-message
#!/usr/bin/expect
set timeout 5
spawn $ORISYNC_EXE init
expect "Is this the first machine in the cluster (y/n)?"
send "y\r"
expect "Enter the cluster name:"
send "cluster\r"
expect "repositories."
exit
End-of-message
}

orisync_init | /usr/bin/expect - >> $TEMP_DIR/orisync_log.txt
if [ $? != 0 ]; then
  exit $?
fi

export CKEY=$(grep cluster-key ~/.ori/orisyncrc | awk '{print $2}')
echo $CKEY

# start orisync
$ORISYNC_EXE

# Add repo and add peer
$ORISYNC_EXE hostadd $REMOTEHOST
$ORISYNC_EXE add ~/.ori/$SOURCE_FS.ori/

# Try list and status
$ORISYNC_EXE list
$ORISYNC_EXE status


# setup orisync on remote
remote_setup() {
cat <<End-of-message
#!/usr/bin/expect
set timeout 20
spawn ssh $REMOTEHOST
expect " $"

send "orisync init\r"
expect "Is this the first machine in the cluster (y/n)?"
send "n\r"
expect "Enter the cluster name:"
send "cluster\r"
expect "Enter the cluster key:"
send "$CKEY\r"
expect "repositories."
send "orisync add ~/.ori/$SOURCE_FS.ori/\r"
sleep 1
expect " $"
send "orisync\r"
expect " $"

exit
End-of-message
}

remote_setup | /usr/bin/expect - >> $TEMP_DIR/orisync_log.txt
if [ $? != 0 ]; then
  exit $?
fi

rsh $REMOTEHOST ori replicate $LOCALHOST:$SOURCE_FS $SOURCE_FS
rsh $REMOTEHOST mkdir $SOURCE_FS
rsh $REMOTEHOST orifs $SOURCE_FS

rsh $REMOTEHOST orisync hostadd $LOCALHOST

rsh $REMOTEHOST orisync list
rsh $REMOTEHOST orisync status

rsh $REMOTEHOST ls $SOURCE_FS

# copy new files
mkdir abc123
cp -r $SOURCE_FILES ./abc123

sleep 5

# check remote
rsh $REMOTEHOST ls $SOURCE_FS/abc123
rsh $REMOTEHOST orisync list
rsh $REMOTEHOST orisync status
rsh $REMOTEHOST orisync exit
rsh $REMOTEHOST orisync remove ~/.ori/$SOURCE_FS.ori/
rsh $REMOTEHOST $UMOUNT $SOURCE_FS
rsh $REMOTEHOST ori removefs $SOURCE_FS
rsh $REMOTEHOST rmdir $SOURCE_FS

sleep 10

$ORISYNC_EXE list
$ORISYNC_EXE status

$ORISYNC_EXE exit


cd ~/.ori/$SOURCE_FS.ori/
$ORIDBG_EXE verify
$ORIDBG_EXE stats
cd $TEMP_DIR

$UMOUNT $SOURCE_FS
