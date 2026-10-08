.pragma library

// Linux evdev positions, independent of the currently selected language.
function keyboardSide(code) {
    if ([16, 17, 18, 19, 20, 30, 31, 32, 33, 34, 44, 45, 46, 47, 48].indexOf(code) >= 0) return -1;
    if ([21, 22, 23, 24, 25, 35, 36, 37, 38, 49, 50].indexOf(code) >= 0) return 1;
    return 0;
}

function dropX(code, width, cell, layout, sequence, split, random) {
    var side = split ? keyboardSide(code) : 0;
    var span = side === 0 ? width : width / 2;
    var start = side === 1 ? width / 2 : 0;
    var columns = Math.max(1, Math.floor(span / cell));
    var column = layout === "cascade" ? sequence % columns : Math.floor(random * columns);
    return start + Math.min(span / 2, cell / 2) + column * cell;
}

// Smoothly fade the entire stream over the last fifth of its fall distance.
function fallAlpha(y, end) {
    var progress = Math.max(0, Math.min(1, (y / Math.max(1, end) - 0.8) / 0.2));
    return 1 - progress * progress * (3 - 2 * progress);
}
