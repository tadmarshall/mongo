// This test writes documents to a 'foo' collection in two passes.  Checks are made that all
// inserted records make it into the 'foo' collection.  If the count of documents in the 'foo'
// collection does not match the number inserted, the test tries to find each document by its
// insertion number ('i') through mongos.  If not found through mongos, the shards are queried
// directly and the results are printed.  It should be possible to figure out what the history
// of the document was by examining the output of printShardingStatus (at the end of the document
// check) and comparing it with the migrations logged earlier.

// This test is adapted from jstests/sharding/mrShardedOutput.js, mostly by removing the MapReduce.

// Load helper function
//
load('jstests/libs/analyze_split_and_migrate_history.js');

jsTest.log("Setting up new ShardingTest('sharded_saves')");
var st = new ShardingTest({ name: 'sharded_saves', 
                            shards: 2, 
                            mongos: 1,
                            other: { 
                                separateConfig: true,
                                mongosOptions: { verbose: 3, chunkSize: 1 },
                                configOptions: { verbose: 3 },
                                shardOptions: { verbose: 3, master: '', oplogSize: 480 }
                            } });
var config = st.getDB('config');
st.adminCommand( { enablesharding: 'test' } );
st.adminCommand( { shardcollection: 'test.foo', key: { 'a': 1 } } );

var testDB = st.getDB( 'test' );
var aaa = 'aaaaaaaaaaaaaaaa';
var str = aaa;
while (str.length < 1*1024) { str += aaa; }
testDB.foo.ensureIndex( { i: 1 } );

st.printChunks();
st.printChangeLog();

var numDocs = 0;
var buildIs32bits = (testDB.serverBuildInfo().bits == 32);
var numBatch = buildIs32bits ? (30 * 1000) : (100 * 1000);
var numChunks = 0;
var docLookup = [];
var foundText = [ 'not found on either shard',
                  'found on shard0000',
                  'found on shard0001',
                  'found on both shard0000 and shard0001' ];

var numIterations = 2;
for (var iter = 0; iter < numIterations; ++iter) {
    jsTest.log('Iteration ' + iter + ': saving new batch of ' + numBatch + ' documents');

    // Add some more data for input so that chunks will get split further
    for (var i = 0; i < numBatch; ++i) {
        if (i % 1000 == 0) {
            print('\n========> Saved total of ' + (numDocs + i) + ' documents\n');
        }
        var randomKey = Math.random() * 1000;
        var timeNow = new ISODate();
        var lookupRecord = { v: randomKey, t: timeNow };
        docLookup.push(lookupRecord);
        testDB.foo.save( {a: randomKey, y: str, i: numDocs + i} );
    }
    print('\n========> Finished saving total of ' + (numDocs + i) + ' documents');

    var GLE = testDB.getLastError();
    assert.eq(null, GLE, 'Setup FAILURE: testDB.getLastError() returned' + GLE);
    jsTest.log('No errors on insert batch.');
    numDocs += numBatch;

    var savedCount = testDB.foo.find().itcount();
    if (savedCount != numDocs) {
        jsTest.log('Setup FAILURE: testDB.foo.find().itcount() = ' + savedCount +
                   ' after saving ' + numDocs + ' documents -- (will assert after diagnostics)');

        // Stop balancer
        jsTest.log('Stopping balancer');
        st.stopBalancer();

        // Wait for writebacks
        jsTest.log('Waiting 10 seconds for writebacks');
        sleep(10000);

        jsTest.log('Checking for missing documents');
        var missingDocuments = [];
        for (i = 0; i < numDocs; ++i) {
            if ( !testDB.foo.findOne({ i: i }, { i: 1 }) ) {
                missingDocuments.push(i);
                var found0 = st.shard0.getDB('test').foo.findOne({ i: i }, { i: 1 }) ? 1 : 0;
                var found1 = st.shard1.getDB('test').foo.findOne({ i: i }, { i: 1 }) ? 2 : 0;
                lookupRecord = docLookup[i];
                print('\n*** ====> mongos could not find document ' + i +
                      ' with shard key ' + tojson(lookupRecord.v) +
                      ' inserted at ' + tojson(lookupRecord.t) +
                      ': ' + foundText[found0 + found1] + '\n');
            }
            if (i % 1000 == 0) {
                print( '\n========> Checked ' + i + '\n');
            }
        }
        print('\n========> Finished checking ' + i + '\n');
        printShardingStatus(config, true);

        // Display the history of each missing document
        //
        print('=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
        var bufferedOutput = [];
        for (i = 0; i < missingDocuments.length; ++i) {
            var j = missingDocuments[i];
            lookupRecord = docLookup[j];
            bufferedOutput.push(analyze_split_and_migrate_history(st.config0.getDB('config'),
                                                                  'test.foo',
                                                                  '' + j,
                                                                  'a',
                                                                  lookupRecord.v,
                                                                  lookupRecord.t));
        }
        for (i = 0; i < missingDocuments.length; ++i) {
            print(bufferedOutput[i]);
        }
        print('=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');

        // Verify that WriteBackListener weirdness isn't causing this
        jsTest.log('Waiting for count to become correct');
        assert.soon(function() { var c = testDB.foo.find().itcount();
                                 print( 'Count is ' + c );
                                 return c == numDocs; },
                    'Setup FAILURE: Count did not become correct after 30 seconds',
                    /* timeout */  30 * 1000,
                    /* interval */ 1000);

        doassert('Setup FAILURE: getLastError was null for insert, but inserted count was wrong');
    }

    // This bit of code was here in mrShardedOutput.js because of MapReduce, but leaving it in
    // place even without MapReduce seems prudent, since we are trying to test everything that
    // mrShardedOutput.js was testing except MR.
    var shards = config.shards.find().toArray();
    var hasChunkOnShard = function(coll, shard) {
        return config.chunks.findOne({ ns : coll + '', shard : shard }) != null;
    }

    // Wait for at least one balance round.
    assert.soon(function(){
        return hasChunkOnShard('test.foo', shards[0]._id) && 
               hasChunkOnShard('test.foo', shards[1]._id);
    }, 'Test FAILURE: No migration performed in two minutes', 2 * 60 * 1000);

    // Now we can proceed, since we have chunks on both shards.
    jsTest.log('Setup OK: count matches (' + numDocs + ')');
    printShardingStatus(config, true);
    cur = config.chunks.find({ns: 'test.foo'});
    shardChunks = {};
    while (cur.hasNext()) {
        var chunk = cur.next();
        var shardName = chunk.shard;
        if (shardChunks[shardName]) {
            shardChunks[shardName] += 1;
        }
        else {
            shardChunks[shardName] = 1;
        }
    }
    for (var prop in shardChunks) {
        print('Number of chunks for shard ' + prop + ': ' + shardChunks[prop]);
    }
}

jsTest.log('SUCCESS!  Stopping ShardingTest "' + st._testName + '"');
st.stop();
