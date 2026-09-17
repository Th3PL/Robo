const fs=require('fs');
function readSTL(p){const buf=fs.readFileSync(p);const n=buf.readUInt32LE(80);let o=84;const tris=[];for(let i=0;i<n;i++){o+=12;const t=[];for(let v=0;v<3;v++){t.push([buf.readFloatLE(o),buf.readFloatLE(o+4),buf.readFloatLE(o+8)]);o+=12;}o+=2;tris.push(t);}return tris;}
function writeSTL(p,ts){const out=Buffer.alloc(84+ts.length*50);out.writeUInt32LE(ts.length,80);let o=84;for(const t of ts){const ax=t[1][0]-t[0][0],ay=t[1][1]-t[0][1],az=t[1][2]-t[0][2],bx=t[2][0]-t[0][0],by=t[2][1]-t[0][1],bz=t[2][2]-t[0][2];let nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;const nl=Math.hypot(nx,ny,nz)||1;out.writeFloatLE(nx/nl,o);out.writeFloatLE(ny/nl,o+4);out.writeFloatLE(nz/nl,o+8);o+=12;for(const v of t){out.writeFloatLE(v[0],o);out.writeFloatLE(v[1],o+4);out.writeFloatLE(v[2],o+8);o+=12;}out.writeUInt16LE(0,o);o+=2;}fs.writeFileSync(p,out);}
const [,, src, motorOut, livreOut] = process.argv;
const tris=readSTL(src);
for (const [name,sel,c] of [[motorOut,t=>t[0][0]>0&&t[0][1]<0,[19.46,-20.89]],[livreOut,t=>t[0][0]<0&&t[0][1]<0,[-19.99,-21.07]]]) {
  const w=tris.filter(sel).map(t=>t.map(v=>[Math.round((v[0]-c[0])*1e5)/1e5,Math.round((v[1]-c[1])*1e5)/1e5,v[2]]));
  // measure max radius on the tread (z between 3 and 18) from the origin
  let rmax=0; for(const t of w) for(const v of t) if(v[2]>3&&v[2]<18) rmax=Math.max(rmax,Math.hypot(v[0],v[1]));
  writeSTL(name,w); console.log(name, w.length,'tris, diâmetro externo', (2*rmax).toFixed(2),'mm');
}
