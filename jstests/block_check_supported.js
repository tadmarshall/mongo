// Test that serverStatus() features dependent on the ProcessInfo::blockCheckSupported() routine
// work correctly.  These features are db.serverStatus({workingSet:1}).workingSet and
// db.serverStatus().indexCounters.
// Related to SERVER-9242.

var hostInfo = db.hostInfo();
var isXP = (hostInfo.os.name == 'Windows XP') ? true : false;

var blockDB = db.getSiblingDB('block_check_supported');
blockDB.dropDatabase();

var serverStatus = blockDB.serverStatus({workingSet:1});


