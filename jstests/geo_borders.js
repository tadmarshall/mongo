t = db.borders;
print(" 1: dropping db.borders");
t.drop();

epsilon = 0.0001;

// For these tests, *required* that step ends exactly on max
min = -1;
max = 1;
step = 1;
numItems = 0;

print(" 2: inserting data into db.borders");
for ( var x = min; x <= max; x += step ) {
    for ( var y = min; y <= max; y += step ) {
        t.insert({ loc: { x: x, y: y} });
        numItems++;
    }
}

overallMin = -1;
overallMax = 1;

center = (overallMax + overallMin) / 2
center = [center, center]
radius = overallMax
realRadius = Math.sqrt(Math.pow(overallMax, 2) + Math.pow(overallMax, 2));

offCenter = [center[0] + radius, center[1] + radius]
onBounds = [offCenter[0] + epsilon, offCenter[1] + epsilon]
offBounds = [onBounds[0] + epsilon, onBounds[1] + epsilon]
onBoundsNeg = [-onBounds[0], -onBounds[1]]

// Print values we will be using for tests
print("Values used for tests:");
print("  overallMin =  " + overallMin);
print("  overallMax =  " + overallMax);
print("  epsilon =     " + epsilon);
print("  numItems =    " + numItems);
print("  center =      " + center);
print("  radius =      " + radius);
print("  realRadius =  " + realRadius);
print("  offCenter =   " + offCenter);
print("  onBounds =    " + onBounds);
print("  offBounds =   " + offBounds);
print("  onBoundsNeg = " + onBoundsNeg);

// Create a point index slightly smaller than the points we have
var indexMax = overallMax - epsilon / 2;
var indexMin = overallMin + epsilon / 2;
var indexRange = { max: indexMax, min: indexMin };
print(" 3: Create a point index slightly smaller than the points we have: " +
      tojson(indexRange) + " (error expected)");
t.ensureIndex({ loc: "2d" }, indexRange);
var err = db.getLastError();
print("    getLastError() returned: " + err);
assert(err);

// Create a point index only slightly bigger than the points we have
indexMax = overallMax + epsilon;
indexMin = overallMin - epsilon;
indexRange = { max: indexMax, min: indexMin };
print(" 4: Create a point index only slightly bigger than the points we have: " +
      tojson(indexRange) + " (no error expected)");
t.ensureIndex({ loc: "2d" }, indexRange);
err = db.getLastError();
print("    getLastError() returned: " + err);
assert.isnull(err);

// ************
// Box Tests
// ************

// If the bounds are bigger than the box itself, just clip at the borders
print(" 5: If the bounds are bigger than the box itself, just clip at the borders");
assert.eq(numItems,
          t.find( { loc: { $within: { $box: [
                    [overallMin - 2 * epsilon, overallMin - 2 * epsilon],
                    [overallMax + 2 * epsilon, overallMax + 2 * epsilon] ] } } } ).count() );

// Test clipping on one edge
print(" 6: Test clipping on one edge");
assert.eq(numItems - 3,
          t.find( { loc: { $within: { $box: [
                    [overallMin, overallMin],
                    [overallMax + 2 * epsilon, overallMax - epsilon] ] } } } ).count() );

// Check this works also for bounds where only a single dimension is off-bounds
print(" 7: Test clipping on two edges");
assert.eq(numItems - 5,
          t.find( { loc : { $within : { $box : [
                    [overallMin, overallMin],
                    [overallMax - epsilon, overallMax - epsilon] ] } } } ).count() );

// Make sure we can get at least close to the bounds of the index
print(" 8: Make sure we can get at least close to the bounds of the index");
assert.eq(numItems,
          t.find( { loc : { $within : { $box : [
                    [ overallMin - epsilon / 2, overallMin - epsilon / 2 ],
                    [ overallMax + epsilon / 2, overallMax + epsilon / 2 ] ] } } } ).count() );

// Make sure we can get at least close to the bounds of the index (reverse box corners)
print(" 9: Make sure we can get at least close to the bounds of the index (reverse box corners)");
assert.eq(numItems,
          t.find( { loc : { $within : { $box : [
                    [ overallMax + epsilon / 2, overallMax + epsilon / 2 ],
                    [ overallMin - epsilon / 2, overallMin - epsilon / 2 ] ] } } } ).count() );

// Check that swapping min/max has good behavior
print("10: Check that swapping min/max has good behavior");
assert.eq(numItems,
          t.find( { loc : { $within : { $box : [
                    [ overallMax + epsilon / 2, overallMax + epsilon / 2 ],
                    [ overallMin - epsilon / 2, overallMin - epsilon / 2 ] ] } } } ).count() );

print("11: Check that swapping min/max has good behavior (rotate corners)");
assert.eq(numItems,
          t.find( { loc : { $within : { $box : [
                    [ overallMax + epsilon / 2, overallMin - epsilon / 2 ],
                    [ overallMin - epsilon / 2, overallMax + epsilon / 2 ] ] } } } ).count() );

// **************
// Circle tests
// **************

// Make sure we can get all points when radius is exactly at full bounds
print("12: Make sure we can get all points when radius is should include them all");
var result = t.find({ loc: { $within: { $center: [center, realRadius + epsilon]}} }).count();
assert.eq(numItems, result);

// Make sure we can get all points when radius is exactly at full bounds
print("13: Make sure we can get expected points when radius is exactly at full bounds");
result = t.find({ loc: { $within: { $center: [center, radius + epsilon]}} }).count();
assert.eq(numItems - 4, result);

// Make sure we can get points when radius is over full bounds
print("14: Make sure we can get points when radius is over full bounds");
assert.lt(0,
          t.find( { loc : { $within : { $center : [ center, radius + 2 * epsilon ] } } } ).count() );

// Make sure we can get points when radius is over full bounds, off-centered
print("15: Make sure we can get points when radius is over full bounds, off-centered");
assert.lt(0,
          t.find( { loc : { $within : { $center : [ offCenter, radius + 2 * epsilon ] } } } ).count() );

// Make sure we get correct corner point when center is in bounds
// (x bounds wrap, so could get other corner)
print("16: Make sure we get correct corner point when center is in bounds");
cornerPt = t.findOne( { loc : { $within : { $center : [ offCenter, step / 2 ] } } } );
assert.eq(cornerPt.loc.x, overallMax);
assert.eq(cornerPt.loc.y, overallMax);

// Make sure we get correct corner point when center is on bounds
// NOTE: Only valid points on MIN bounds
print("17: Make sure we get correct corner point when center is on bounds");
cornerPt = t.findOne({ loc: { $within: { $center: [onBoundsNeg, Math.sqrt(2 * epsilon * epsilon) + (step / 2)]}} });
assert.eq( cornerPt.loc.y, overallMin )

// Make sure we can't get corner point when center is over bounds
print("18: Make sure we can't get corner point when center is over bounds (exception expected)");
var gotException = false;
try {
    t.findOne({ loc: { $within: { $center: [offBounds, Math.sqrt(8 * epsilon * epsilon) + (step / 2)]}} });
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
assert(gotException, "Failed to get expected exception");

// Make sure we can't get corner point when center is on max bounds
print("19: Make sure we can't get corner point when center is on max bounds (exception expected)");
gotException = false;
try {
    t.findOne({ loc: { $within: { $center: [onBounds, Math.sqrt(8 * epsilon * epsilon) + (step / 2)]}} });
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
//assert(gotException, "Failed to get expected exception");

// ***********
// Near tests
// ***********

// Make sure we can get all nearby points to point in range
print("20: Make sure we can get all nearby points to point in range");
assert.eq( overallMax, t.find( { loc : { $near : offCenter } } ).next().loc.y );

// Make sure we can get all nearby points to point on boundary
print("21: Make sure we can get all nearby points to point on boundary");
assert.eq( overallMin, t.find( { loc : { $near : onBoundsNeg } } ).next().loc.y );

// Make sure we can't get all nearby points to point over boundary
print("22: Make sure we can't get all nearby points to point over boundary (exception expected)");
gotException = false;
try {
    t.findOne({ loc: { $near: offBounds} });
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
assert(gotException, "Failed to get expected exception");

// Make sure we can't get all nearby points to point on max boundary
print("23: Make sure we can't get all nearby points to point on max boundary (exception expected)");
gotException = false;
try {
    t.findOne({ loc: { $near: onBoundsNeg} });
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
//assert(gotException, "Failed to get expected exception");

// Make sure we can get all nearby points within one step (4 points in top
// corner)
print("24: Make sure we can get all nearby points within one step (4 points in top corner)");
assert.eq( 4, t.find( { loc : { $near : offCenter, $maxDistance : step * 1.9 } } ).count() );

// **************
// Command Tests
// **************
// Make sure we can get all nearby points to point in range
print("25: Make sure we can get all nearby points to point in range");
assert.eq(overallMax,
          db.runCommand( { geoNear : "borders", near : offCenter } ).results[0].obj.loc.y );

// Make sure we can get all nearby points to point on boundary
print("26: Make sure we can get all nearby points to point on boundary");
assert.eq(overallMin,
          db.runCommand( { geoNear : "borders", near : onBoundsNeg } ).results[0].obj.loc.y );

// Make sure we can't get all nearby points to point over boundary
print("27: Make sure we can't get all nearby points to point over boundary (exception expected)");
gotException = false;
try {
    db.runCommand({ geoNear: "borders", near: offBounds }).results.length;
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
assert(gotException, "Failed to get expected exception");

// Make sure we can't get all nearby points to point on max boundary
print("28: Make sure we can't get all nearby points to point on max boundary (exception expected)");
gotException = false;
try {
    db.runCommand({ geoNear: "borders", near: onBounds }).results.length;
} catch (e) {
    err = db.getLastError();
    print("    Got expected exception: " + err);
    gotException = true;
}
//assert(gotException, "Failed to get expected exception");

// Make sure we can get all nearby points within one step (4 points in top
// corner)
print("29: Make sure we can get all nearby points within one step (4 points in top corner)");
assert.eq(4,
          db.runCommand({ geoNear: "borders",
                          near: offCenter,
                          maxDistance : step * 1.5 } ).results.length );

print("30: geo_borders.js test completed successfully");
