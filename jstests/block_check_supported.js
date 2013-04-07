// Test that serverStatus() features dependent on the ProcessInfo::blockCheckSupported() routine
// work correctly.  These features are db.serverStatus({workingSet:1}).workingSet and
// db.serverStatus().indexCounters.
// Related to SERVER-9242.

jsTest.log("TESTING workingSet and indexCounters portions of serverStatus");
var hostInfo = db.hostInfo();
var isXP = (hostInfo.os.name == 'Windows XP') ? true : false;

var blockDB = db.getSiblingDB('block_check_supported');
blockDB.dropDatabase();

var serverStatus = blockDB.serverStatus({workingSet:1});
if (!serverStatus) {
    doassert("Test FAILED: db.serverStatus({workingSet:1}) did not return a value");
}
if (!serverStatus.workingSet) {
    doassert("Test FAILED: db.serverStatus({workingSet:1}).workingSet was not returned");
}
if (!serverStatus.indexCounters) {
    doassert("Test FAILED: db.serverStatus().indexCounters was not returned");
}
var workingSet = serverStatus.workingSet;
var indexCounters_1 = serverStatus.indexCounters;

//isXP = true;
if (isXP) {
    var expectedResult = { info: 'not supported' };
    jsTest.log('Testing on Windows XP -- expecting ' + tojsononeline(expectedResult));
    assert.eq(workingSet,
              expectedResult,
              'Test FAILED: db.serverStatus({workingSet:1}).workingSet' +
              ' did not return the expected value: expected ' + tojsononeline(expectedResult) +
              ', got ' + tojsononeline(workingSet));
}




