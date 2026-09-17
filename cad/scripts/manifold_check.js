// Edge-manifold check: every undirected edge must be used by exactly 2 triangles with opposite direction.
const fs = require('fs');
const p = process.argv[2];
const buf = fs.readFileSync(p); const n = buf.readUInt32LE(80); let o = 84;
const PREC = +(process.env.PREC || 4); const key = v => v[0].toFixed(PREC) + "," + v[1].toFixed(PREC) + "," + v[2].toFixed(PREC);
const dir = new Map(); let degenerate = 0;
let mn = [Infinity, Infinity, Infinity], mx = [-Infinity, -Infinity, -Infinity];
for (let i = 0; i < n; i++) {
  o += 12; const t = [];
  for (let v = 0; v < 3; v++) { t.push([buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)]); o += 12; }
  o += 2;
  for (const v of t) for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], v[k]); mx[k] = Math.max(mx[k], v[k]); }
  const ks = t.map(key);
  if (ks[0] === ks[1] || ks[1] === ks[2] || ks[0] === ks[2]) { degenerate++; continue; }
  for (let e = 0; e < 3; e++) { const k = ks[e] + '>' + ks[(e + 1) % 3]; dir.set(k, (dir.get(k) || 0) + 1); }
}
let open = 0, nonmanifold = 0, sameDir = 0, ok = 0;
const seen = new Set(); const samples = [];
const centers = process.env.ORIGIN ? [[0,0]] : process.env.CENTERED ? [[-19.99,-21.07],[19.46,-20.89],[-20.10,21.79],[19.34,21.97]] : [[619.01, 95.93], [658.46, 96.11], [618.90, 138.79], [658.34, 138.97]];
const describe = (a, b) => {
  const pa = a.split(',').map(Number), pb = b.split(',').map(Number);
  const m = [(pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2, (pa[2] + pb[2]) / 2];
  let best = null, bd = Infinity; for (const c of centers) { const d = Math.hypot(m[0] - c[0], m[1] - c[1]); if (d < bd) { bd = d; best = c; } }
  return `r=${bd.toFixed(2)} z=${m[2].toFixed(2)} ang=${(Math.atan2(m[1] - best[1], m[0] - best[0]) * 180 / Math.PI).toFixed(1)} len=${Math.hypot(pa[0]-pb[0], pa[1]-pb[1], pa[2]-pb[2]).toFixed(4)}`;
};
for (const [k, c] of dir) {
  const [a, b] = k.split('>'); const rk = b + '>' + a; const u = a < b ? a + '|' + b : b + '|' + a;
  if (seen.has(u)) continue; seen.add(u);
  const rc = dir.get(rk) || 0;
  if (c === 1 && rc === 1) ok++;
  else if (c + rc === 1) { open++; if (samples.length < 12) samples.push('open ' + describe(a, b)); }
  else if (rc === 0 || c === 0) { sameDir++; if (samples.length < 12) samples.push('sameDir ' + describe(a, b)); }
  else { nonmanifold++; if (samples.length < 12) samples.push(`nonmanifold(${c}+${rc}) ` + describe(a, b)); }
}
for (const s of samples) console.log('   ', s);
console.log(`${p}: tris=${n} degenerate=${degenerate} edges ok=${ok} open=${open} sameDirOnly=${sameDir} nonmanifold(>2)=${nonmanifold}`);
console.log('  bbox', mn.map(v => v.toFixed(2)).join(','), '->', mx.map(v => v.toFixed(2)).join(','), 'dims', mn.map((v, i) => (mx[i] - v).toFixed(2)).join(' x '));
