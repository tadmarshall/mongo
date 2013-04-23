var showHistory = function(value) {
    var config = db.getSiblingDB("config");
    var finalChunk = config.getCollection("chunks").findOne({"min.a":{$lte: value}, "max.a": {$gt: value}});
    print("split history for value " + value + ", which is supposed to be on " + finalChunk.shard);
    //print("finalChunk = " + tojson(finalChunk));
    var currentChunk = {};
    currentChunk.min = finalChunk.min;
    currentChunk.max = finalChunk.max;

    var history = [];
    while (tojson(currentChunk.min.a) != tojson({ "$minKey" : 1 }) ||
           tojson(currentChunk.max.a) != tojson({ "$maxKey" : 1 })) {

	// Find all migrations of this chunk, in reverse order
	//
	var mc = config.getCollection("changelog").find({ "what": /moveChunk/, "details.min": currentChunk.min, "details.max": currentChunk.max }).sort({time: -1});
	while (mc && mc.hasNext()) {
	    var move = mc.next();
            var row = {};
	    row.event = "move";
	    row.moveType = move.what;
	    row.time = move.time;
	    row.chunk = {};
	    row.chunk.min = {};
	    row.chunk.min.a = move.details.min.a;
	    row.chunk.max = {};
	    row.chunk.max.a = move.details.max.a;
	    row.from = move.details.from;
	    row.to = move.details.to;
            history.push(row);
	}

	// Find the split history of this chunk
	//
        var parent = config.getCollection("changelog").findOne({ "details.left.min": currentChunk.min, "details.left.max": currentChunk.max });
        if (parent) {
            var follow = 0;
        }
        else {
            parent = config.getCollection("changelog").findOne({ "details.right.min": currentChunk.min, "details.right.max": currentChunk.max });
            if (parent) {
                follow = 1;
            }
            else {
                doassert("FATAL logic error!  Cannot find parent of current split!");
            }
        }
        row = {};
        row.event = "split";
        row.which = follow;
        row.parent = parent;
        history.push(row);
        currentChunk.min = parent.details.before.min;
        currentChunk.max = parent.details.before.max;
    }
    var color1 = 0;
    var color2 = 31;
    for (var i = history.length - 1; i >= 0; --i) {
        var s = history[i];
	if (s.event == "move") {
	    var c0 = color1;
            var time = s.time.toISOString().substr(11,12);
	    var extraText = s.moveType;
	    if (s.moveType == "moveChunk.start") {
		extraText += " :: " + s.from + " => " + s.to;
	    }
            print(time + " migrate \x1B[" + c0 + "m[" + s.chunk.min.a + ", " + s.chunk.max.a +
                  ")\x1B[0m :: " + extraText);
	}
	else {
            var p = s.parent;
	    c0 = color1;
	    var c1 = (s.which == 0) ? color2 : 0;
	    var c2 = (s.which == 1) ? color2 : 0;
            time = p.time.toISOString().substr(11,12);
            print(time + "  split  \x1B[" + c0 + "m[" + p.details.before.min.a + ", " + p.details.before.max.a +
                  ")\x1B[0m => { \x1B[" + c1 + "m[" + p.details.left.min.a + ", " + p.details.left.max.a +
                  ")\x1B[0m, \x1B[" + c2 + "m[" + p.details.right.min.a + ", " + p.details.right.max.a + ")\x1B[0m }");
	    color1 = color2;
	    color2 = 31 + ((color2 - 31) + 1) % 6;
	}
    }
}

showHistory(17.900164937600493);
