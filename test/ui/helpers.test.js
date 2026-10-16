// Unit tests for the pure helpers in data/www/app.js (no DOM, no dependencies).
// Run:  node test/ui/helpers.test.js        (also does a syntax check of app.js)
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const appJs = process.argv[2] || path.join(__dirname, '..', '..', 'data', 'www', 'app.js');
execFileSync(process.execPath, ['--check', appJs], { stdio: 'inherit' });
const src = fs.readFileSync(appJs, 'utf8');
function grab(name, kind) {
  // Take the declaration line; if its braces are unbalanced, extend to the next line that is exactly "}".
  const lines = src.split('\n');
  const start = lines.findIndex(l => l.startsWith(kind === 'const' ? `const ${name} = ` : `function ${name}(`));
  if (start < 0) throw new Error('cannot find ' + name);
  let out = [], depth = 0;
  for (let i = start; i < lines.length; i++) {
    out.push(lines[i]);
    depth += (lines[i].match(/{/g) || []).length - (lines[i].match(/}/g) || []).length;
    if (depth <= 0) break;
  }
  return out.join('\n') + '\n';
}
const code = ['hhmm', 'toMin', 'fmt', 'getPath', 'setPath', 'levelAt'].map(n => grab(n, 'fn')).join('\n') + grab('esc', 'const');
const H = {}; new Function('H', code + '\nObject.assign(H,{hhmm,toMin,fmt,getPath,setPath,levelAt,esc});')(H);

let fails = 0;
const eq = (a, b, msg) => { const ok = Math.abs(a - b) < 1e-6 || a === b; if (!ok) { fails++; console.log('FAIL', msg, '=>', a, 'expected', b); } };

// time helpers
eq(H.hhmm(0), '00:00', 'hhmm 0'); eq(H.hhmm(1439), '23:59', 'hhmm 1439'); eq(H.hhmm(1440), '00:00', 'hhmm wrap'); eq(H.hhmm(-60), '23:00', 'hhmm negative');
eq(H.toMin('06:30'), 390, 'toMin'); eq(H.toMin('24:00'), 0, 'toMin 24:00 wraps');
eq(H.fmt(12.345, 1), '12.3', 'fmt'); eq(H.fmt(null), '–', 'fmt null');

// path helpers
const o = {}; H.setPath(o, 'a.b.c', 5); eq(H.getPath(o, 'a.b.c'), 5, 'set/get path'); eq(H.getPath(o, 'x.y'), undefined, 'missing path');

// escaping
eq(H.esc('<b>"x"&\'y\''), '&lt;b&gt;&quot;x&quot;&amp;&#39;y&#39;', 'esc');

// curve interpolation: mirrors scheduleLevelAt() in src/Schedule.cpp
const ramp = [[360, 0], [405, 100], [1395, 100], [1439, 0]];   // default "Veg 18/6"
eq(H.levelAt(ramp, 360, false), 0, 'start of sunrise');
eq(H.levelAt(ramp, 382.5, false), 50, 'mid sunrise linear');
eq(H.levelAt(ramp, 382.5, true), 50, 'mid sunrise smooth (cosine symmetric)');
eq(H.levelAt(ramp, 371.25, true), 50 - 50 * Math.cos(Math.PI * 0.25), 'quarter sunrise smooth');
eq(H.levelAt(ramp, 720, false), 100, 'noon hold');
eq(H.levelAt(ramp, 1439, false), 0, 'last point');
eq(H.levelAt(ramp, 100, false), 0, 'night: wrap segment 1439 -> 360 both 0');
eq(H.levelAt([], 500, false), 0, 'empty');
eq(H.levelAt([[600, 42]], 100, true), 42, 'single point');

// midnight wrap with a rising wrap segment: 22:00 = 0 -> 02:00 = 100 (overnight ramp)
const night = [[120, 100], [600, 100], [660, 0], [1320, 0]];
eq(H.levelAt(night, 1380, false), 25, '23:00 on overnight ramp (1 of 4 h)');
eq(H.levelAt(night, 60, false), 75, '01:00 on overnight ramp (3 of 4 h)');
eq(H.levelAt(night, 1320, false), 0, 'exact last point');

// 18/6 that starts in the evening (points given unsorted, as the wizard builds them, then sorted)
const wiz = [[1200, 0], [1245, 100], [435, 100], [480, 0]].sort((a, b) => a[0] - b[0]);
eq(H.levelAt(wiz, 0, false), 100, 'midnight inside evening-start photoperiod');
eq(H.levelAt(wiz, 800, false), 0, 'afternoon dark');
eq(H.levelAt(wiz, 1222.5, false), 50, 'mid evening sunrise');

console.log(fails ? `${fails} test(s) FAILED` : 'all UI helper tests passed');
process.exit(fails ? 1 : 0);
