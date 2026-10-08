import QtQuick
import QtTest
import ".." as Effect
TestCase {
    name: "EntryScramble"
    Effect.Rain { id: rain; width: 800; height: 600 }
    function init() { rain.clear(); rain.fallHeight = 1; rain.scrambleDurationMs = 300; }
    function test_settles() {
        rain.spawn("Enter", 28);
        var p = rain.particles[0];
        p.y = 10;
        verify(rain.headText(p) !== "Enter");
        rain.advance(0.1);
        rain.advance(0.19);
        verify(p.headTime < 0.3);
        rain.advance(0.02);
        compare(rain.headText(p), "Enter");
        rain.advance(0.1);
        compare(rain.headText(p), "Enter");
    }
    function test_duration() {
        rain.scrambleDurationMs = 1000;
        rain.spawn("Space", 57);
        var p = rain.particles[0]; p.y = 10;
        rain.advance(0.5);
        verify(rain.headText(p) !== "Space");
        rain.advance(0.5);
        compare(rain.headText(p), "Space");
    }
    function test_short_fall() {
        rain.fallHeight = 0.1;
        rain.scrambleDurationMs = 1000;
        rain.spawn("A", 30);
        var p = rain.particles[0];
        verify(p.headDuration <= rain.fallEnd * 0.75 / p.speed);
        p.y = 1;
        rain.advance(p.headDuration);
        compare(rain.headText(p), "A");
        verify(p.y < rain.fallEnd);
    }
    function test_toggle() {
        rain.spawn("ї", 16);
        var p = rain.particles[0];
        rain.scrambleDurationMs = 0;
        compare(rain.headText(p), "ї");
        rain.scrambleDurationMs = 300;
        compare(rain.headText(p), "ї");
        rain.scrambleDurationMs = 0;
        rain.spawn("A", 30);
        compare(rain.headText(rain.particles[1]), "A");
    }
}
