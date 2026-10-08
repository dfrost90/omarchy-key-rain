import QtQuick
import "Layouts.js" as Layouts

Canvas {
    id: root
    property string layout: "matrix"
    property bool splitMode: false
    property int scrambleDurationMs: 300
    onScrambleDurationMsChanged: {
        particles.forEach(function(p) {
            if (p.headTime < p.headDuration) p.headDuration = Math.min(scrambleDurationMs / 1000, fallEnd * 0.75 / p.speed);
        });
        requestPaint();
    }
    property int glyphSize: 22
    property real intensity: 1
    property real fallHeight: 1
    readonly property real fallEnd: Math.max(1, height * fallHeight)
    onFallEndChanged: requestPaint()
    onIntensityChanged: requestPaint()
    readonly property int trailSpacing: glyphSize + 3
    property color headColor: "#cbffd5"
    property color trailColor: "#36e568"
    onHeadColorChanged: requestPaint()
    onTrailColorChanged: requestPaint()
    property var particles: []
    readonly property bool hasParticles: particles.length > 0
    property int sequence: 0
    property double lastFrame: 0
    readonly property string glyphs: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ+-=<>:/?{}[]"
    renderStrategy: Canvas.Threaded
    function clear() { sequence = 0; particles = []; requestPaint(); }
    function spawn(text, code) {
        var cell = glyphSize + 8;
        var x = Layouts.dropX(code, width, cell, layout, sequence++, splitMode, Math.random());
        var next = particles.slice(-95);
        var trail = [];
        for (var i = 0; i < (layout === "matrix" ? 6 : 2); i++) {
            var interval = 0.08 + Math.random() * 0.12;
            trail.push({index: Math.floor(Math.random() * glyphs.length),
                        elapsed: Math.random() * interval, interval: interval});
        }
        var speed = 170 + Math.random() * 110;
        next.push({text: text, x: x, y: -glyphSize - 2,
                   speed: speed, trail: trail,
                   headTime: 0, headDuration: Math.min(scrambleDurationMs / 1000, fallEnd * 0.75 / speed),
                   headTick: 0, headIndex: Math.floor(Math.random() * glyphs.length)});
        particles = next;
    }
    function headText(p) {
        return p.headTime < p.headDuration ? glyphs.charAt(p.headIndex) : p.text;
    }
    function advance(dt) {
        particles = particles.filter(function(p) {
            p.y += p.speed * dt;
            // Count visible travel, so large glyphs get the same entry effect.
            if (p.y > 0 && p.headTime < p.headDuration) {
                p.headTime = Math.min(p.headDuration, p.headTime + dt);
                p.headTick += dt;
                if (p.headTick >= 0.05) {
                    p.headTick %= 0.05;
                    p.headIndex = (p.headIndex + 1 + Math.floor(Math.random() * (root.glyphs.length - 1))) % root.glyphs.length;
                }
            }
            for (var i = 0; i < p.trail.length; i++) {
                var tail = p.trail[i];
                tail.elapsed += dt;
                if (tail.elapsed >= tail.interval) {
                    tail.elapsed %= tail.interval;
                    tail.index = (tail.index + 1 + Math.floor(Math.random() * (root.glyphs.length - 1))) % root.glyphs.length;
                }
            }
            return p.y < root.fallEnd;
        });
    }
    Timer {
        interval: 33; repeat: true; running: root.hasParticles
        onRunningChanged: root.lastFrame = Date.now()
        onTriggered: {
            var now = Date.now();
            var dt = Math.min(0.1, (now - root.lastFrame) / 1000);
            root.lastFrame = now;
            root.advance(dt);
            root.requestPaint();
        }
    }
    onPaint: {
        var ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        ctx.font = glyphSize + "px monospace"; ctx.textAlign = "center";
        for (var i = 0; i < particles.length; i++) {
            var p = particles[i];
            var fade = Layouts.fallAlpha(p.y, fallEnd);
            var tails = p.trail.length;
            for (var j = tails; j >= 0; j--) {
                ctx.globalAlpha = intensity * fade * (j === 0 ? 0.9 : 0.42 * (1 - j / (tails + 1)));
                ctx.fillStyle = j === 0 ? headColor.toString() : trailColor.toString();
                ctx.fillText(j === 0 ? headText(p) : glyphs.charAt(p.trail[j - 1].index), p.x, p.y - j * trailSpacing);
            }
        }
        ctx.globalAlpha = 1;
    }
}
