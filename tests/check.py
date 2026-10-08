import json
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from backend import ROOT, build, lock_state

assert lock_state([{'solitaryBlockedBy': ['LOCK']}]) == 'locked'
assert lock_state([{'solitaryBlockedBy': []}]) == 'unlocked'
assert lock_state([{'solitaryBlockedBy': None, 'solitary': '123abc'}]) == 'unlocked'
assert lock_state([{'solitaryBlockedBy': None, 'solitary': '0'}]) == 'unknown'
assert lock_state([{'solitaryBlockedBy': ['WORKSPACE']}]) == 'unknown'
for invalid in ([], {}, None, [{}], [None]):
    assert lock_state(invalid) == 'unknown'
build()
rows = [json.loads(line) for line in subprocess.check_output([str(ROOT / 'falling-keys-reader'), '--self-test'], text=True).splitlines()]
assert [row['text'] for row in rows] == ['A', '"\\', 'ї']
with tempfile.TemporaryDirectory() as directory:
    output = str(Path(directory) / 'reader-test')
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'xkbcommon', 'libsystemd', 'libudev'], text=True).split()
    subprocess.run(['gcc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g', str(ROOT / 'tests/reader_test.c'), '-o', output, *flags], check=True)
    subprocess.run([output], check=True)
    subprocess.run(['gcc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g', str(ROOT / 'tests/seat_access_test.c'), '-o', output, *flags], check=True)
    subprocess.run([output], check=True)
    javascript = (ROOT / 'Layouts.js').read_text().replace('.pragma library', '')
    javascript += '''
const assert = require('node:assert/strict');
for (const width of [1, 100, 1920]) for (const layout of ['matrix','cascade']) {
  for (const code of [16,21,2,79]) for (const split of [false,true]) {
    for (let seq=0;seq<200;seq++) {
      const x=dropX(code,width,30,layout,seq,split,0.99999);
      assert(x>=0 && x<=width);
      if (split && code===16) assert(x<=width/2);
      if (split && code===21) assert(x>=width/2);
    }
  }
}
assert.equal(fallAlpha(0,100),1);
assert.equal(fallAlpha(80,100),1);
assert(Math.abs(fallAlpha(90,100)-0.5)<1e-10);
assert.equal(fallAlpha(100,100),0);
'''
    subprocess.run(['node', '-e', javascript], check=True)
print('PASS: JSON/UTF-8, live layout changes, duplicate presses, shared modifiers, unplug cleanup, lock states, split placement, fade endpoints')
