// This test helper function returns a string containing the events surrounding the
// insertion of a document into a sharded collection, showing:
//   1) splits that affected the chunk in which the document should have been found;
//   2) migrates that affected the chunk in which the document should have been found;
//   3) the time of insertion of the document relative to splits and migrates.
//

/* Print the split, migrate and insertion history of a document
 *
 * @param config        config database
 * @param ns            namespace
 * @param label         display string to identify document
 * @param key           shard key
 * @param value         value of shard key to trace
 * @param insertTime    time that document was inserted
 */
var analyze_split_and_migrate_history = function(config, ns, label, key, value, insertTime) {
    var outputString = '';
    var out = function(string) {
        outputString += string + '\n';
    }
    print('Analyzing Split/Migrate/Insert history for document ' + tojson(label) +
          ' { ' + tojson(key) + ': ' + tojson(value) +
          ' } inserted at ' + tojson(insertTime));

    var findObj = {};
    findObj['ns'] = ns;
    findObj['min.' + key] = { $lte: value };
    findObj['max.' + key] = { $gt:  value };
    var finalChunk = config.getCollection('chunks').findOne(findObj);
    if (!finalChunk) {
        delete findObj['min.' + key];
        finalChunk = config.getCollection('chunks').findOne(findObj);
    }
    if (!finalChunk) {
        delete findObj['max.' + key];
        findObj['min.' + key] = { $lte: value };
        finalChunk = config.getCollection('chunks').findOne(findObj);
    }
    if (!finalChunk) {
        out('FATAL analysis logic error!  Cannot find final chunk containing document!');
        return outputString;
    }
    out('Split/Migrate/Insert history for document ' + tojson(label) +
        ' { ' + tojson(key) + ': ' + tojson(value) +
        ' }, which is supposed to be in chunk [' + finalChunk.min[key] +
        ', ' + finalChunk.max[key] + ') on ' + finalChunk.shard);
    var currentChunk = {};
    currentChunk.min = finalChunk.min;
    currentChunk.max = finalChunk.max;

    // Work backwards through split history until we reach [ Minkey, Maxkey )
    //
    var history = [];
    while (tojson(currentChunk.min[key]) != tojson({ '$minKey': 1 }) ||
           tojson(currentChunk.max[key]) != tojson({ '$maxKey': 1 })) {

        // Find all migrations of this chunk, in reverse order
        //
        var mc = config.getCollection('changelog').
                        find({ 'what': /moveChunk/,
                               'ns': ns,
                               'details.min': currentChunk.min,
                               'details.max': currentChunk.max }).
                        sort({time: -1});
        while (mc && mc.hasNext()) {
            var move = mc.next();
            var row = {};
            row.event = 'move';
            row.time = move.time;
            row.moveType = move.what;
            row.chunk = {};
            row.chunk.min = {};
            row.chunk.min[key] = move.details.min[key];
            row.chunk.max = {};
            row.chunk.max[key] = move.details.max[key];
            row.from = move.details.from;
            row.to = move.details.to;
            history.push(row);
        }

        // Find the split history of this chunk
        //
        var parent = config.getCollection('changelog').
                            findOne({ 'ns': ns,
                                      'details.left.min': currentChunk.min,
                                      'details.left.max': currentChunk.max });
        if (parent) {
            var follow = 0;
        }
        else {
            parent = config.getCollection('changelog').
                            findOne({ 'ns': ns,
                                      'details.right.min': currentChunk.min,
                                      'details.right.max': currentChunk.max });
            if (parent) {
                follow = 1;
            }
            else {
                out('FATAL analysis logic error!  Cannot find parent of current split!  (' +
                    tojson(currentChunk) + ')');
                return outputString;
            }
        }
        row = {};
        row.event = 'split';
        row.time = parent.time;
        row.which = follow;
        row.parent = parent;
        history.push(row);
        currentChunk.min = parent.details.before.min;
        currentChunk.max = parent.details.before.max;
    }

    // We've built up the history; now display it
    //
    var showedInsert = false;
    for (var i = history.length - 1; i >= 0; --i) {
        var s = history[i];
        if (!showedInsert && insertTime < s.time) {
            var time = insertTime.toISOString().substr(11,12);
            out(time + '  insert document ' + tojson(label) +
                ' { ' + tojson(key) + ': ' + tojson(value) + ' }');
            showedInsert = true;
        }
        time = s.time.toISOString().substr(11,12);
        if (s.event == 'move') {
            var extraText = s.moveType;
            if (s.moveType == 'moveChunk.start') {
                extraText += ' :: ' + s.from + ' => ' + s.to;
            }
            out(time + ' migrate [' +
                s.chunk.min[key] + ', ' + s.chunk.max[key] + ') :: ' + extraText);
        }
        else {
            var p = s.parent;
            out(time + '  split  [' +
                p.details.before.min[key] + ', ' + p.details.before.max[key] +
                ') => { [' +
                p.details.left.min[key] + ', ' + p.details.left.max[key] +
                '), [' +
                p.details.right.min[key] + ', ' + p.details.right.max[key] + ') }');
        }
    }
    if (!showedInsert) {
        time = insertTime.toISOString().substr(11,12);
        out(time + '  insert document ' + tojson(label) +
            ' { ' + tojson(key) + ': ' + tojson(value) + ' }');
    }
    print('Finished analyzing Split/Migrate/Insert history for document ' + tojson(label) +
          ' { ' + tojson(key) + ': ' + tojson(value) +
          ' } inserted at ' + tojson(insertTime) + '\n');
    return outputString;
}
