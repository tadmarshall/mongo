var t = db.jstests_ora;                                                                                // #1
t.drop();                                                                                              // #2
for (var i = 0; i < 10; i += 1) { t.save({x: i, y: 10 - i}); }                                         // #3
assert.eq.automsg("1", "t.find({$or: [{$where: 'this.x === 2'}]}).count()");                           // #4
assert.eq.automsg("2", "t.find({$or: [{$where: 'this.x === 2'}, {$where: 'this.y === 2'}]}).count()"); // #5
assert.eq.automsg("1", "t.find({$or: [{$where: 'this.x === 2'}, {$where: 'this.y === 8'}]}).count()"); // #6
assert.eq.automsg("10", "t.find({$or: [{$where: 'this.x === 2'}, {x: {$ne: 2}}]}).count()");           // #7
