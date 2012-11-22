//
// Tests whether sharded GLE fails sanely and correctly reports failures.  
//

jsTest.log( "Starting sharded cluster..." )

var st = new ShardingTest({ shards : 3,
                            mongos : 2,
                            verbose : 3,
                            other : { separateConfig : true } })

st.stopBalancer()

var mongos = st.s0
var admin = mongos.getDB( "admin" )
var config = mongos.getDB( "config" )
var coll = mongos.getCollection( jsTestName() + ".coll" )
var shards = config.shards.find().toArray()

jsTest.log( "Enabling sharding..." )

printjson( admin.runCommand({ enableSharding : "" + coll.getDB() }) )
printjson( admin.runCommand({ movePrimary : "" + coll.getDB(), to : shards[0]._id }) )
printjson( admin.runCommand({ shardCollection : "" + coll, key : { _id : 1 } }) )
printjson( admin.runCommand({ split : "" + coll, middle : { _id : 0 } }) )
printjson( admin.runCommand({ moveChunk : "" + coll, find : { _id : 0 }, to : shards[1]._id }) )

st.printShardingStatus()



jsTest.log( "Testing GLE 1...")

// insert to two diff shards
var doc = { _id : -1, hello : "world" }
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
doc = { _id : 1, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
jsTest.log("finished inserting " + tojson(doc));

jsTest.log( "GLE 1: " + tojson( coll.getDB().getLastErrorObj() ) )



jsTest.log( "Testing GLE 2 when writeback host goes down..." )

// insert to two diff shards
doc = { _id : -2, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
doc = { _id : 2, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
jsTest.log("finished inserting " + tojson(doc));

jsTest.log( "Stopping shard0 -- 001" )
MongoRunner.stopMongod( st.shard0 )

jsTest.log( "GLE 2: " + tojson( coll.getDB().getLastErrorObj() ) )

st.shard0 = MongoRunner.runMongod( st.shard0 )



jsTest.log( "Testing GLE 3 when main host goes down..." )

// insert to two diff shards
doc = { _id : -3, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
doc = { _id : 3, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
coll.insert(doc);
jsTest.log("finished inserting " + tojson(doc));

jsTest.log( "Stopping shard1 -- 002" )
MongoRunner.stopMongod( st.shard1 )

var gle3 = "not set in try block 3";
try{ 
    jsTest.log( "Calling GLE 3! " ) 
    gle3 = coll.getDB().getLastErrorObj()
    assert( false )
}
catch( e ){
    jsTest.log( "GLE 3: " + tojson( gle3 ) + " -- exception text = " + e )
    
    // Stupid string exceptions
    jsTest.log( "about to do assert test 3 on exception string" )
    assert( /could not get last error/.test( e + "") )
}

st.shard1 = MongoRunner.runMongod( st.shard1 )



jsTest.log( "Testing multi GLE 4 for multi-host writes..." )

coll.update({ hello : "world" }, { $set : { goodbye : "world" } }, false, true)

jsTest.log( "GLE 4: " + tojson( coll.getDB().getLastErrorObj() ) )

jsTest.log( "Testing multi GLE 5 when host goes down..." )

// insert to two diff shards
coll.update({ hello : "world" }, { $set : { goodbye : "world" } }, false, true)

jsTest.log( "Stopping shard0 -- 003" )
MongoRunner.stopMongod( st.shard0 )

var gle5 = "not set in try block 5";
try{ 
    jsTest.log( "Calling GLE 5! " ) 
    gle5 = coll.getDB().getLastErrorObj()
    assert( false )
}
catch( e ){
    jsTest.log( "GLE 5: " + tojson( gle3 ) + " -- exception text = " + e )
    
    // Stupid string exceptions
    jsTest.log( "about to do assert test 5 on exception string" )
    assert( /could not get last error/.test( e + "") )
}

st.shard0 = MongoRunner.runMongod( st.shard0 )



jsTest.log( "Testing stale version GLE 6 when host goes down..." )

var staleColl = st.s1.getCollection( coll + "" )
staleColl.findOne()

printjson( admin.runCommand({ moveChunk : "" + coll, find : { _id : 0 }, to : shards[2]._id }) )

jsTest.log( "Stopping shard2 -- 004" )
MongoRunner.stopMongod( st.shard2 )

jsTest.log( "Sending stale write..." )

staleColl.insert({ _id : 4, hello : "world" })
doc = { _id : 4, hello : "world" };
jsTest.log("about to insert " + tojson(doc));
staleColl.insert(doc);
jsTest.log("finished inserting " + tojson(doc));

jsTest.log( "about to test that gle is not null" )
assert.neq( null, staleColl.getDB().getLastError() )


jsTest.log( "Done!" )

st.stop()