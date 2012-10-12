var baseDir = "jstests_disk_directoryper";
var baseName = "directoryperdb"
port = allocatePorts( 1 )[ 0 ];
var directorySeparator = _isWindows() ? "\\" : "/";
dbpath = directorySeparator + "data" +
         directorySeparator + "db" +
         directorySeparator + baseDir + directorySeparator;

var m = startMongodTest(port, baseDir, false, {directoryperdb : "", nohttpinterface : "", bind_ip : "127.0.0.1"});
db = m.getDB( baseName );
db[ baseName ].save( {} );
assert.eq( 1, db[ baseName ].count() , "A : " + tojson( db[baseName].find().toArray() ) );

checkDir = function( dir ) {
    db.adminCommand( {fsync:1} );
    files = listFiles( dir );
    found = false;
    for( f in files ) {
        if ( new RegExp( baseName ).test( files[ f ].name ) ) {
            found = true;
            assert( files[ f ].isDirectory, "file not directory" );
        }
    }
    assert( found, "no directory" );

    files = listFiles( dir + baseName );
    for( f in files ) {
        if ( files[f].isDirectory )
            continue;
        var baseNameSlashBaseNameDot = baseName + directorySeparator + baseName + ".";
        assert(files[f].name.indexOf(baseNameSlashBaseNameDot) != -1, "B dir:" + dir + " f: " + f);
    }
}
checkDir( dbpath );

// file iterator
assert( m.getDBs().totalSize > 0, "bad size calc" );

// repair
db.runCommand( {repairDatabase:1, backupOriginalFiles:true} );
checkDir( dbpath );
files = listFiles( dbpath );
for( f in files ) {
    if (files[f].name.indexOf(dbpath + "backup_") == 0) {
        backupDir = files[ f ].name + directorySeparator;
    }
}
checkDir( backupDir );
assert.eq( 1, db[ baseName ].count() , "C" );

// tool test
stopMongod( port );

externalPath = directorySeparator + "data" +
               directorySeparator + "db" +
               directorySeparator + baseDir +
               "_external" + directorySeparator;

runMongoProgram( "mongodump", "--dbpath", dbpath, "--directoryperdb", "--out", externalPath );
resetDbpath( dbpath );
runMongoProgram( "mongorestore", "--dbpath", dbpath, "--directoryperdb", "--dir", externalPath );
m = startMongodTest(port, baseDir, true, {directoryperdb : "", nohttpinterface : "", bind_ip : "127.0.0.1"});
db = m.getDB( baseName );
checkDir( dbpath );
assert.eq( 1, db[ baseName ].count() , "C" );
assert( m.getDBs().totalSize > 0, "bad size calc" );

// drop db test
db.dropDatabase();
files = listFiles( dbpath );
files.forEach( function( f ) { assert( !new RegExp( baseName ).test( f.name ), "drop database - dir not cleared" ); } );

print("SUCCESS directoryperdb.js");
