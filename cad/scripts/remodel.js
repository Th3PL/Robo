// Replace the studded outer surface of each wheel with a smooth cylinder (optionally grooved / treaded),
// keeping the interior (motor socket / axle bore) untouched. Pure Node, no deps.
//
// Pipeline per wheel: weld -> cut zone triangles by planes (z rings, radial) -> repair T-junctions ->
// weld cut points -> radial compression onto the target profile -> drop collapsed triangles.
const fs = require('fs');

const [,, inPath, outPath, variant] = process.argv;
const GROOVE = variant === 'canal';      // one circumferential groove for a rubber band / o-ring
const TREAD = variant === 'ranhura';     // axial grooves across the tread

const R_FREE = 16.0, R_MOTOR = 16.5;   // Ø32 idler, Ø33 drive wheel: drive wheels must be the tallest so they never lose floor contact
const R_ZONE = 14.0;       // triangles with all verts at r >= this form the outer zone (body is 14.25-14.6)
const CHAMFER = 0.8;
const GROOVE_FLOOR = 3.0, GROOVE_D = 1.0;   // band groove: flat floor 3 mm wide, 1 mm deep; lower wall vertical (faces up),
                                            // upper wall a 45° ramp so the wheel prints lying flat with no support
const TREAD_N = 24, TREAD_W_DEG = 5, TREAD_DEPTH = 1.0, TREAD_INSET = 1.2, TREAD_EPS_DEG = 0.1;
const ANG_STEP = 4;        // degrees between radial split planes outside the tread band -> 90-gon
const EPS = 0.05;          // z gap between the two rings forming a groove wall
const K = 0.05, R_REF = 16.35;   // compression factor and original max radius
const WELD = 0.01;         // pre-weld: merge original vertices closer than this
const WELD2 = 0.02;        // post-weld: merge cut points closer than this (chains along a plane can be arbitrarily dense)
const ONEDGE = 3e-5;       // T-junction repair tolerance

function readSTL(p) {
  const buf = fs.readFileSync(p); const n = buf.readUInt32LE(80); let o = 84; const tris = [];
  for (let i = 0; i < n; i++) { o += 12; const t = []; for (let v = 0; v < 3; v++) { t.push([buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)]); o += 12; } o += 2; tris.push(t); }
  return tris;
}
function writeSTL(p, tris) {
  const out = Buffer.alloc(84 + tris.length * 50); out.writeUInt32LE(tris.length, 80); let o = 84;
  for (const t of tris) {
    const ax = t[1][0]-t[0][0], ay = t[1][1]-t[0][1], az = t[1][2]-t[0][2], bx = t[2][0]-t[0][0], by = t[2][1]-t[0][1], bz = t[2][2]-t[0][2];
    let nx = ay*bz-az*by, ny = az*bx-ax*bz, nz = ax*by-ay*bx; const nl = Math.hypot(nx, ny, nz) || 1;
    out.writeFloatLE(nx/nl, o); out.writeFloatLE(ny/nl, o+4); out.writeFloatLE(nz/nl, o+8); o += 12;
    for (const v of t) { out.writeFloatLE(v[0], o); out.writeFloatLE(v[1], o+4); out.writeFloatLE(v[2], o+8); o += 12; }
    out.writeUInt16LE(0, o); o += 2;
  }
  fs.writeFileSync(p, out);
}
const snap5 = v => v.map(x => Math.round(x * 1e5) / 1e5);
const key = v => v[0].toFixed(5) + ',' + v[1].toFixed(5) + ',' + v[2].toFixed(5);
const ekey = (a, b) => { const ka = key(a), kb = key(b); return ka < kb ? ka + '|' + kb : kb + '|' + ka; };
const dist = (a, b) => Math.hypot(a[0]-b[0], a[1]-b[1], a[2]-b[2]);

// proximity welding via union-find on a grid. keep: vertex keys that are never merged (originals)
function weld(tris, R, keep) {
  const pts = new Map(); for (const t of tris) for (const v of t) pts.set(key(v), v);
  const ids = [...pts.keys()]; const idx = new Map(ids.map((k, i) => [k, i]));
  const parent = ids.map((_, i) => i);
  const find = i => { while (parent[i] !== i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  const cellOf = v => [Math.floor(v[0] / R), Math.floor(v[1] / R), Math.floor(v[2] / R)];
  const grid = new Map();
  ids.forEach((k, i) => { const c = cellOf(pts.get(k)).join(','); (grid.get(c) || grid.set(c, []).get(c)).push(i); });
  ids.forEach((k, i) => {
    if (keep && keep.has(k)) return;
    const v = pts.get(k); const [cx, cy, cz] = cellOf(v);
    for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) for (let dz = -1; dz <= 1; dz++)
      for (const j of (grid.get((cx + dx) + ',' + (cy + dy) + ',' + (cz + dz)) || [])) {
        if (j <= i || (keep && keep.has(ids[j]))) continue;
        if (dist(v, pts.get(ids[j])) < R) { const a = find(i), b = find(j); if (a !== b) parent[b] = a; }
      }
  });
  const rep = i => pts.get(ids[find(i)]);
  const out = [];
  for (const t of tris) { const w = t.map(v => rep(idx.get(key(v)))); if (new Set(w.map(key)).size === 3) out.push(w); }
  return out;
}

// generic T-junction repair: insert any vertex lying on a triangle edge (fan split from the opposite vertex)
function repairTJunctions(tris, candidates, edgeOk) {   // edgeOk(a,b,tri): which edges may carry a hanging vertex   // candidates: only these vertex keys may hang on an edge (cut points)
  const pts = new Map(); for (const t of tris) for (const v of t) { const k = key(v); if (!candidates || candidates.has(k)) pts.set(k, v); }
  const C = 0.1; const grid = new Map();
  const cellOf = v => [Math.floor(v[0] / C), Math.floor(v[1] / C), Math.floor(v[2] / C)];
  for (const v of pts.values()) { const c = cellOf(v).join(','); (grid.get(c) || grid.set(c, []).get(c)).push(v); }
  const onSeg = (a, b, p) => {
    const abx = b[0]-a[0], aby = b[1]-a[1], abz = b[2]-a[2], L2 = abx*abx + aby*aby + abz*abz;
    const t = ((p[0]-a[0])*abx + (p[1]-a[1])*aby + (p[2]-a[2])*abz) / L2;
    if (t <= 1e-9 || t >= 1 - 1e-9) return null;
    const q = [a[0] + t*abx, a[1] + t*aby, a[2] + t*abz];
    return dist(p, q) < ONEDGE ? t : null;
  };
  const pointsOn = (a, b) => {
    const lo = cellOf([Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.min(a[2], b[2])]), hi = cellOf([Math.max(a[0], b[0]), Math.max(a[1], b[1]), Math.max(a[2], b[2])]);
    const ka = key(a), kb = key(b); const found = [];
    for (let x = lo[0] - 1; x <= hi[0] + 1; x++) for (let y = lo[1] - 1; y <= hi[1] + 1; y++) for (let z = lo[2] - 1; z <= hi[2] + 1; z++)
      for (const p of (grid.get(x + ',' + y + ',' + z) || [])) { const kp = key(p); if (kp === ka || kp === kb) continue; const t = onSeg(a, b, p); if (t !== null) found.push({ t, p }); }
    found.sort((u, w) => u.t - w.t); return found;
  };
  const out = []; let inserted = 0;
  const stack = tris.slice();
  while (stack.length) {
    const t = stack.pop(); let split = false;
    if (new Set(t.map(key)).size < 3) { out.push(t); continue; }
    for (let i = 0; i < 3 && !split; i++) {
      const a = t[i], b = t[(i + 1) % 3], c = t[(i + 2) % 3]; const kc = key(c);
      if (edgeOk && !edgeOk(a, b, t)) continue;
      const found = pointsOn(a, b); if (!found.length) continue;   // the apex itself may lie on the edge (collinear sliver): splitting yields degenerate pieces that are dropped later
      inserted += found.length; split = true;
      const chain = [a, ...found.map(s => s.p), b];
      for (let k = 0; k < chain.length - 1; k++) stack.push([chain[k], chain[k + 1], c]);
    }
    if (!split) out.push(t);
  }
  return { tris: out, inserted };
}

function processWheel(tris) {
  // ---- center & rim height
  const vm = new Map(); for (const t of tris) for (const v of t) vm.set(key(v), v);
  const verts = [...vm.values()];
  let mn = [Infinity, Infinity, Infinity], mx = [-Infinity, -Infinity, -Infinity];
  for (const v of verts) for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], v[k]); mx[k] = Math.max(mx[k], v[k]); }
  let cx = (mn[0] + mx[0]) / 2, cy = (mn[1] + mx[1]) / 2;
  for (let it = 0; it < 4; it++) {
    const rs = verts.map(v => Math.hypot(v[0] - cx, v[1] - cy)); const rmax = Math.max(...rs);
    let sx = 0, sy = 0, n = 0; verts.forEach((v, i) => { if (rs[i] > rmax - 0.3) { sx += v[0]; sy += v[1]; n++; } }); cx = sx / n; cy = sy / n;
  }
  const rOf = v => Math.hypot(v[0] - cx, v[1] - cy);
  const zMin = mn[2];
  let H = -Infinity; for (const v of verts) if (rOf(v) >= R_ZONE) H = Math.max(H, v[2]);
  const inZone = t => t.every(v => rOf(v) >= R_ZONE);
  // the motor wheel is the one with the small central D-hole (r < 3); the idler bore starts at r = 7.75
  const isMotor = verts.some(v => rOf(v) < 5);
  const R_NEW = isMotor ? R_MOTOR : R_FREE;

  // ---- target radius profile
  const zc = (zMin + H) / 2, z1 = zc - GROOVE_FLOOR / 2, z2 = zc + GROOVE_FLOOR / 2, z3 = z2 + GROOVE_D;
  const period = 360 / TREAD_N;
  const inGroove = a => { const phi = ((a * 180 / Math.PI) % period + period) % period; return phi < TREAD_W_DEG / 2 || phi > period - TREAD_W_DEG / 2; };
  const tz1 = zMin + TREAD_INSET, tz2 = H - TREAD_INSET;
  const Rz = (z, a) => {
    let r = R_NEW;
    if (z - zMin < CHAMFER) r = R_NEW - (CHAMFER - (z - zMin));
    if (H - z < CHAMFER) r = Math.min(r, R_NEW - (CHAMFER - (H - z)));
    if (GROOVE && z > z1 + EPS / 2 && z <= z2 + 1e-6) r = R_NEW - GROOVE_D;
    else if (GROOVE && z > z2 && z < z3) r = R_NEW - GROOVE_D + (z - z2);   // 45° ramp
    if (TREAD && z > tz1 + EPS / 2 && z < tz2 - EPS / 2 && inGroove(a)) r -= TREAD_DEPTH;
    return r;
  };

  // ---- plane sets
  const radial = a => { const r = a * Math.PI / 180, nx = -Math.sin(r), ny = Math.cos(r); return v => nx * (v[0] - cx) + ny * (v[1] - cy); };
  const zplanes = [zMin + CHAMFER, H - CHAMFER];
  if (GROOVE) zplanes.push(z1, z1 + EPS, z2, z3);
  if (TREAD) zplanes.push(tz1, tz1 + EPS, tz2 - EPS, tz2);
  const zPlaneFns = zplanes.map(zp => v => v[2] - zp);
  const coarse = []; for (let a = 0; a < 180; a += ANG_STEP) coarse.push(radial(a));
  const fine = [];
  if (TREAD) for (let k = 0; k * period < 180; k++) {
    const b = k * period, hw = TREAD_W_DEG / 2, land = period - TREAD_W_DEG;
    for (const e of [b - hw - TREAD_EPS_DEG, b - hw + TREAD_EPS_DEG, b + hw - TREAD_EPS_DEG, b + hw + TREAD_EPS_DEG, b, b + hw + land / 3, b + hw + 2 * land / 3]) fine.push(radial(((e % 180) + 180) % 180));
  }

  // ---- plane cutting (cut cache keyed by sub-edge + plane => identical points on shared edges)
  const cutCache = new Map(); let planeIdx = 0;
  const lerp = (a, b, s) => [a[0] + s * (b[0] - a[0]), a[1] + s * (b[1] - a[1]), a[2] + s * (b[2] - a[2])];
  const cut = (a, b, da, db) => { const ck = ekey(a, b) + '#' + planeIdx; if (cutCache.has(ck)) return cutCache.get(ck); const p = snap5((key(a) < key(b)) ? lerp(a, b, da / (da - db)) : lerp(b, a, db / (db - da))); cutCache.set(ck, p); return p; };
  function splitTri(t, f) {
    const d = t.map(f);
    const s = d.map(x => Math.abs(x) < 1e-4 ? 0 : Math.sign(x));
    if (!(s.includes(1) && s.includes(-1))) return [t];
    let i = -1;
    for (let k = 0; k < 3; k++) { const j = (k + 1) % 3, l = (k + 2) % 3; if (s[k] !== 0 && s[j] !== 0 && s[l] !== 0 && s[j] === s[l] && s[k] !== s[j]) i = k; }
    if (i === -1) { const k0 = s.indexOf(0); const j = (k0 + 1) % 3, l = (k0 + 2) % 3; const p = cut(t[j], t[l], d[j], d[l]); return [[t[k0], t[j], p], [t[k0], p, t[l]]]; }
    const j = (i + 1) % 3, l = (i + 2) % 3;
    const pa = cut(t[i], t[j], d[i], d[j]), pb = cut(t[l], t[i], d[l], d[i]);
    return [[t[i], pa, pb], [pa, t[j], t[l]], [pa, t[l], pb]];
  }
  const cutAll = (list, fns) => { for (const f of fns) { planeIdx++; const next = []; for (const t of list) next.push(...splitTri(t, f)); list = next; } return list; };

  let zone = [], rest = [];
  for (const t of tris) (inZone(t) ? zone : rest).push(t);
  const nZone = zone.length;
  zone = cutAll(zone, zPlaneFns);
  if (TREAD) {
    const inBand = t => t.every(v => v[2] > tz1 - 1e-6 && v[2] < tz2 + 1e-6);
    let band = [], outer = [];
    for (const t of zone) (inBand(t) ? band : outer).push(t);
    zone = [...cutAll(band, fine), ...cutAll(outer, coarse)];
  } else zone = cutAll(zone, coarse);

  // ---- stitch: repair T-junctions everywhere, then weld dense cut-point chains
  const origKeys = new Set(); for (const t of tris) for (const v of t) origKeys.add(key(v));
  const cutKeys = new Set(); for (const t of zone) for (const v of t) { const k = key(v); if (!origKeys.has(k)) cutKeys.add(k); }
  const zoneSet = new Set(zone);
  const ringZ = z => TREAD && (Math.abs(z - tz1) < 1e-6 || Math.abs(z - tz2) < 1e-6);
  const edgeOk = (a, b, t) => zoneSet.has(t) ? (ringZ(a[2]) && ringZ(b[2])) : (rOf(a) >= R_ZONE - 1e-6 && rOf(b) >= R_ZONE - 1e-6);
  const rep = repairTJunctions([...zone, ...rest], cutKeys, edgeOk);
  const w1 = weld(rep.tris, WELD2, origKeys);
  // welding can leave collinear slivers (a cut point merged onto a neighbour's edge); a second repair pass over edges
  // touching cut points inserts the point on the other side and collapses the sliver itself
  const isCut = v => !origKeys.has(key(v));
  const rep2 = repairTJunctions(w1, cutKeys, (a, b) => isCut(a) || isCut(b));
  const welded = rep2.tris;

  // ---- radial compression: r -> Rz(z,a) - K*(R_REF - r). Monotonic in r, so ordering (and manifoldness) is preserved;
  // the studs/octagon/ledges survive only as a ~0.1 mm skin under the new round tread.
  const proj = new Map();
  const P = v => { const k = key(v); if (proj.has(k)) return proj.get(k); const r0 = rOf(v); if (r0 < R_ZONE) return v; const a = Math.atan2(v[1] - cy, v[0] - cx), r = Rz(v[2], a) - K * (R_REF - r0); const p = snap5([cx + r * Math.cos(a), cy + r * Math.sin(a), v[2]]); proj.set(k, p); return p; };
  const result = [];
  for (const tv of welded) {
    const t = tv.map(P);
    if (new Set(t.map(key)).size < 3) continue;   // collapsed sliver
    result.push(t);                                // never drop non-degenerate triangles: it would open cracks next to kept slivers
  }
  // welding can collapse a thin 'tent' (two triangles sharing an edge, apexes on opposite sides but < WELD2 apart)
  // into a mirrored pair: zero-volume flap. Mirrored pairs cancel out, so drop both; exact duplicates keep one.
  const groups = new Map();
  result.forEach((t, i) => { const ks = t.map(key); const g = ks.slice().sort().join('|'); const o = (ks[0] < ks[1]) === (ks[1] < ks[2]) ? (ks[0] < ks[2] ? 1 : -1) : (ks[0] < ks[2] ? -1 : 1); (groups.get(g) || groups.set(g, []).get(g)).push({ i, o }); });
  const drop = new Set(); let flaps = 0;
  for (const g of groups.values()) { if (g.length < 2) continue; const pos = g.filter(x => x.o > 0), neg = g.filter(x => x.o < 0); if (pos.length && neg.length) { for (const x of g) drop.add(x.i); flaps++; } else for (const x of g.slice(1)) drop.add(x.i); }
  const cleaned = result.filter((_, i) => !drop.has(i));
  return { tris: cleaned, isMotor, cx, cy, H, zMin, nZone, nZoneOut: zone.length, inserted: rep.inserted + rep2.inserted, flaps };
}

// ---- split the file into wheels (2x2 grid) and process each
const raw = readSTL(inPath);
let mn = [Infinity, Infinity, Infinity], mx = [-Infinity, -Infinity, -Infinity];
for (const t of raw) for (const v of t) for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], v[k]); mx[k] = Math.max(mx[k], v[k]); }
// center the group on the origin up front: float32 STL precision is ~6e-5 at x~600 mm but ~4e-6 near 0
const gx = Math.round((mn[0] + mx[0]) / 2), gy = Math.round((mn[1] + mx[1]) / 2);
const all = weld(raw.map(t => t.map(v => snap5([v[0] - gx, v[1] - gy, v[2]]))), WELD);
console.log('welded:', raw.length, '->', all.length, 'triangles');
const clusters = [[], [], [], []];
for (const t of all) { const cx = (t[0][0] + t[1][0] + t[2][0]) / 3, cy = (t[0][1] + t[1][1] + t[2][1]) / 3; clusters[(cx < 0 ? 0 : 1) + (cy < 0 ? 0 : 2)].push(t); }
const out = [];
clusters.forEach((c, i) => {
  if (!c.length) return;
  const r = processWheel(c);
  console.log(`wheel ${i} (${r.isMotor ? 'motor Ø' + (2 * R_MOTOR) : 'livre Ø' + (2 * R_FREE)}): center (${r.cx.toFixed(2)}, ${r.cy.toFixed(2)}) rim z ${r.zMin.toFixed(2)}..${r.H.toFixed(2)}  tris ${c.length} -> ${r.tris.length}  (zone ${r.nZone} -> ${r.nZoneOut}, T-junctions fixed ${r.inserted}, flaps removed ${r.flaps})`);
  out.push(...r.tris);
});
writeSTL(outPath, out);
console.log('wrote', outPath, out.length, 'triangles', GROOVE ? '(with groove)' : TREAD ? '(treaded)' : '(smooth)');
