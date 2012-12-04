jsTest.log("Setting up new ShardingTest");
s = new ShardingTest( "mrShardedOutput", 2, 1, 1, { chunksize : 1 } );

var config = s.getDB("config");
s.adminCommand( { enablesharding: "test" } );
s.adminCommand( { shardcollection: "test.foo", key: { "a": 1 } } );

db = s.getDB( "test" );
var aaa = "aaaaaaaaaaaaaaaa";
var str = aaa;
while (str.length < 1*1024) { str += aaa; }

s.printChunks();
s.printChangeLog();

function map2() { emit(this._id, {count: 1, y: this.y}); }
function reduce2(key, values) { return values[0]; }

var numdocs = 0;
var buildIs32bits = (db.serverBuildInfo().bits == 32);
var numbatch = buildIs32bits ? (30 * 1000) : (100 * 1000);
var nchunks = 0;

var numIterations = 2;
for (var it = 0; it < numIterations; ++it) {
    jsTest.log("Iteration " + it + ": saving new batch of " + numbatch + " documents");

    // add some more data for input so that chunks will get split further
    for (var i = 0; i < numbatch; ++i) {
        if (i % 1000 == 0) {
            print("\n========> Saved total of " + (numdocs + i) + " documents\n");
        }
        db.foo.save( {a: Math.random() * 1000, y: str, i: numdocs + i} );
    }
    print("\n========> Finished saving total of " + (numdocs + i) + " documents");

    var GLE = db.getLastError();
    assert.eq(null, GLE, "Setup FAILURE: db.getLastError() returned" + GLE);
    jsTest.log("No errors on insert batch.")
    numdocs += numbatch

    var savedCount = db.foo.find().itcount();
    var isBad = savedCount != numdocs;
    if (isBad) {
        jsTest.log("Setup PROBLEM: db.foo.find().itcount() = " + savedCount +
                   " after saving " + numdocs + " documents");

        // Stop balancer
        jsTest.log("Stopping balancer");
        s.stopBalancer();

        // Wait for writebacks
        jsTest.log("Waiting 10 seconds for writebacks");
        sleep( 10000 );

        jsTest.log("Checking for missing documents");
        for (var i = 0; i < numdocs; ++i) {
            if ( !db.foo.findOne({ i: i }, { i: 1 }) ) {
                print( "Could not find document " + i );
            }
            if ( i % 100 == 0 ) {
                print( "Checked " + i );
            }
        }
        print( "Checked " + i );
        s.printShardingStatus(true);
    }

    // Verify that wbl weirdness isn't causing this
    jsTest.log("Waiting for count to become correct");
    assert.soon( function() { var c = db.foo.find().itcount();
                              print( "Count is " + c );
                              return c == numdocs; },
                 "Count did not become correct after 30 seconds",
                 /* timeout */  30 * 1000,
                 /* interval */ 1000 );

    // Do the MapReduce step
    jsTest.log("Setup OK: count matches (" + numdocs + ") -- Starting MapReduce");
    res = db.foo.mapReduce(map2, reduce2, { out: { replace: "mrShardedOut", sharded: true }});
    var reduceOutputCount = res.counts.output;
    assert.eq( numdocs,
               reduceOutputCount,
               "MapReduce FAILED: res.counts.output = " + reduceOutputCount +
                    ", should be " + numdocs );
    jsTest.log("MapReduce results:");
    printjson(res);

    jsTest.log("Checking that all MapReduce output documents are in output collection");
    outColl = db["mrShardedOut"];
    var outCollCount = outColl.find().itcount();    // SERVER-3645 -can't use count()
    assert.eq( numdocs,
               outCollCount,
               "MapReduce FAILED: outColl.find().itcount() = " + outCollCount +
                    ", should be " + numdocs +
                    ": this may happen intermittently until resolution of SERVER-3627" );

    // make sure it's sharded and split
    var newnchunks = config.chunks.count({ns: db.mrShardedOut._fullName});
    print("Number of chunks: " + newnchunks);
    assert.gt( newnchunks, 1, "didnt split");

    // make sure num of chunks increases over time
    if (nchunks) {
        assert.gt( newnchunks, nchunks, "number of chunks did not increase between iterations");
    }
    nchunks = newnchunks;

    // check that chunks are well distributed
    jsTest.log("Checking chunk distribution");
    cur = config.chunks.find({ns: db.mrShardedOut._fullName});
    shardChunks = {};
    while (cur.hasNext()) {
        var chunk = cur.next();
        printjsononeline(chunk);
        var shardName = chunk.shard;
        if (shardChunks[shardName]) {
            shardChunks[shardName] += 1;
        }
        else {
            shardChunks[shardName] = 1;
        }
    }

    var count = 0;
    for (var prop in shardChunks) {
        print ("NUMBER OF CHUNKS FOR SHARD " + prop + ": " + shardChunks[prop]);
        if (!count)
            count = shardChunks[prop];
        assert.lt( Math.abs(count - shardChunks[prop]),
                   nchunks / 10,
                   "Chunks are not well balanced: " + count + " vs. " + shardChunks[prop] );
    }
}

jsTest.log("SUCCESS!  Stopping ShardingTest");
s.stop();
