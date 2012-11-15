var t = db.jstests_ora;

// $where
t.drop();
for (var i = 0; i < 10; i += 1) {
    t.save({x: i, y: 10 - i});
}
assert.eq.automsg("1", "t.find({$or: [{$where: 'this.x === 2'}]}).count()");
assert.eq.automsg("2", "t.find({$or: [{$where: 'this.x === 2'}, {$where: 'this.y === 2'}]}).count()");
assert.eq.automsg("1", "t.find({$or: [{$where: 'this.x === 2'}, {$where: 'this.y === 8'}]}).count()");
assert.eq.automsg("10", "t.find({$or: [{$where: 'this.x === 2'}, {x: {$ne: 2}}]}).count()");
