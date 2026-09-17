// Minimal STL -> PNG renderer (orthographic, z-buffer, lambert shading). Node only, no deps.
const fs = require('fs');
const zlib = require('zlib');

function readSTL(p) {
  const buf = fs.readFileSync(p);
  const head = buf.slice(0, 500).toString('utf8');
  const tris = [];
  if (head.trim().toLowerCase().startsWith('solid') && head.includes('facet')) {
    const text = buf.toString('utf8');
    const re = /vertex\s+([-\d.eE+]+)\s+([-\d.eE+]+)\s+([-\d.eE+]+)/g;
    let m, t = [];
    while ((m = re.exec(text))) { t.push([+m[1], +m[2], +m[3]]); if (t.length === 3) { tris.push(t); t = []; } }
  } else {
    const n = buf.readUInt32LE(80); let o = 84;
    for (let i = 0; i < n; i++) { o += 12; const t = []; for (let v = 0; v < 3; v++) { t.push([buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)]); o += 12; } o += 2; tris.push(t); }
  }
  return tris;
}

function crc32(buf) {
  let c, crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) { c = (crc ^ buf[n]) & 0xff; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crc = (crc >>> 8) ^ c; }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const td = Buffer.concat([Buffer.from(type), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
function writePNG(path, w, h, rgb) {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  const png = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
  fs.writeFileSync(path, png);
}

// view: rotation matrix applied to (x,y,z) -> screen (u,v), depth w
function rotX(a) { const c = Math.cos(a), s = Math.sin(a); return [[1, 0, 0], [0, c, -s], [0, s, c]]; }
function rotY(a) { const c = Math.cos(a), s = Math.sin(a); return [[c, 0, s], [0, 1, 0], [-s, 0, c]]; }
function rotZ(a) { const c = Math.cos(a), s = Math.sin(a); return [[c, -s, 0], [s, c, 0], [0, 0, 1]]; }
function mul(A, B) { const R = [[0,0,0],[0,0,0],[0,0,0]]; for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) for (let k = 0; k < 3; k++) R[i][j] += A[i][k] * B[k][j]; return R; }
function apply(M, v) { return [M[0][0]*v[0]+M[0][1]*v[1]+M[0][2]*v[2], M[1][0]*v[0]+M[1][1]*v[1]+M[1][2]*v[2], M[2][0]*v[0]+M[2][1]*v[1]+M[2][2]*v[2]]; }

function render(tris, M, W, H, pad, tint) {
  // transform
  const T = tris.map(t => t.map(v => apply(M, v)));
  let minU = Infinity, maxU = -Infinity, minV = Infinity, maxV = -Infinity;
  for (const t of T) for (const v of t) { minU = Math.min(minU, v[0]); maxU = Math.max(maxU, v[0]); minV = Math.min(minV, v[1]); maxV = Math.max(maxV, v[1]); }
  const sc = Math.min((W - 2 * pad) / (maxU - minU), (H - 2 * pad) / (maxV - minV));
  const ox = (W - (maxU - minU) * sc) / 2, oy = (H - (maxV - minV) * sc) / 2;
  const img = Buffer.alloc(W * H * 3, 245);
  const zb = new Float32Array(W * H).fill(-Infinity);
  const light = [0.3, 0.5, 0.81]; const ll = Math.hypot(...light); light.forEach((v, i) => light[i] = v / ll);
  for (const t of T) {
    const p = t.map(v => [(v[0] - minU) * sc + ox, H - ((v[1] - minV) * sc + oy), v[2]]);
    // normal in view space
    const ax = t[1][0]-t[0][0], ay = t[1][1]-t[0][1], az = t[1][2]-t[0][2];
    const bx = t[2][0]-t[0][0], by = t[2][1]-t[0][1], bz = t[2][2]-t[0][2];
    let nx = ay*bz-az*by, ny = az*bx-ax*bz, nz = ax*by-ay*bx; const nl = Math.hypot(nx, ny, nz) || 1; nx/=nl; ny/=nl; nz/=nl;
    let lam = Math.abs(nx*light[0]+ny*light[1]+nz*light[2]);
    const shade = 0.25 + 0.75 * lam;
    const col = tint.map(c => Math.round(c * shade));
    const x0 = Math.max(0, Math.floor(Math.min(p[0][0], p[1][0], p[2][0]))), x1 = Math.min(W - 1, Math.ceil(Math.max(p[0][0], p[1][0], p[2][0])));
    const y0 = Math.max(0, Math.floor(Math.min(p[0][1], p[1][1], p[2][1]))), y1 = Math.min(H - 1, Math.ceil(Math.max(p[0][1], p[1][1], p[2][1])));
    const det = (p[1][0]-p[0][0])*(p[2][1]-p[0][1]) - (p[2][0]-p[0][0])*(p[1][1]-p[0][1]);
    if (Math.abs(det) < 1e-9) continue;
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
      const px = x + 0.5, py = y + 0.5;
      const l1 = ((p[1][0]-px)*(p[2][1]-py) - (p[2][0]-px)*(p[1][1]-py)) / det;
      const l2 = ((p[2][0]-px)*(p[0][1]-py) - (p[0][0]-px)*(p[2][1]-py)) / det;
      const l3 = 1 - l1 - l2;
      if (l1 < -1e-6 || l2 < -1e-6 || l3 < -1e-6) continue;
      const z = l1*p[0][2] + l2*p[1][2] + l3*p[2][2];
      const idx = y * W + x;
      if (z > zb[idx]) { zb[idx] = z; img[idx*3] = col[0]; img[idx*3+1] = col[1]; img[idx*3+2] = col[2]; }
    }
  }
  return img;
}

const [,, inPath, outPath, viewName] = process.argv;
const tris = readSTL(inPath);
const views = {
  top: rotZ(0),                                   // looking down -Z
  bottom: rotX(Math.PI),                          // looking up +Z
  front: rotX(-Math.PI / 2),                      // looking along +Y
  side: mul(rotX(-Math.PI / 2), rotZ(Math.PI/2)),
  iso: mul(rotX(-Math.PI / 3), rotZ(Math.PI / 4)),
  isobottom: mul(rotX(Math.PI * 2 / 3), rotZ(Math.PI / 4)),
};
const M = views[viewName] || views.iso;
const W = 900, H = 900;
const img = render(tris, M, W, H, 30, [70, 140, 220]);
writePNG(outPath, W, H, img);
console.log('wrote', outPath, 'tris', tris.length);
