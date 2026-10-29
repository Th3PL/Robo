// Prepara a coleta para o Edge Impulse: confere a qualidade e separa treino e teste POR RAJADA.
//
//   node dados/preparar.js dados/sessao1_detalhado.csv [dados/sessao2_detalhado.csv ...] [--equilibrar]
//
// Entrada: o "CSV detalhado" baixado da página do carrinho (rajada,tipo,obstaculo,vel,d0..d9,label).
// Saída:   dados/treino.csv e dados/teste.csv no formato do Edge Impulse (d0..d9,label).
//
// Por que separar por rajada: as janelas se sobrepõem em 80%. Num corte aleatório, quase a mesma
// janela cai no treino e no teste e a acurácia sai alta e mentirosa. Aqui a rajada inteira vai para
// um lado só: o teste mede situações que o modelo nunca viu, como vai acontecer no robô.

const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
const equilibrar = args.includes('--equilibrar');
const arquivos = args.filter(a => !a.startsWith('--'));
if (!arquivos.length) {
  console.error('uso: node dados/preparar.js <detalhado.csv> [outros.csv ...] [--equilibrar]');
  process.exit(1);
}

const CLASSES = ['livre', 'aproximando', 'bloqueado'];
const JANELA = 10;
const pct = (a, b) => (b ? (100 * a / b).toFixed(0) : '0') + '%';

// ---------------------------------------------------------------- leitura
const jan = [];          // { raj, tipo, obst, vel, x[10], label }
const erros = [];
arquivos.forEach((arq, f) => {
  const linhas = fs.readFileSync(arq, 'utf8').replace(/\r/g, '').trim().split('\n');
  const cab = linhas[0].split(',');
  const esperado = ['rajada', 'tipo', 'obstaculo', 'vel', ...Array.from({ length: JANELA }, (_, i) => 'd' + i), 'label'];
  if (cab.join(',') !== esperado.join(',')) {
    erros.push(`${arq}: cabeçalho inesperado. Use o "CSV detalhado" da página do carrinho.`);
    return;
  }
  for (let i = 1; i < linhas.length; i++) {
    const p = linhas[i].split(',');
    if (p.length !== esperado.length) { erros.push(`${arq} linha ${i + 1}: ${p.length} colunas`); continue; }
    const x = p.slice(4, 4 + JANELA).map(Number);
    const label = p[4 + JANELA];
    if (x.some(v => Number.isNaN(v) || v < 0 || v > 1)) { erros.push(`${arq} linha ${i + 1}: valor fora de 0..1`); continue; }
    if (!CLASSES.includes(label)) { erros.push(`${arq} linha ${i + 1}: rótulo "${label}"`); continue; }
    jan.push({ raj: `${f + 1}-${p[0]}`, tipo: p[1], obst: p[2], vel: +p[3], x, label });
  }
});
if (erros.length) { erros.slice(0, 10).forEach(e => console.log('ERRO  ' + e)); process.exit(1); }

// ---------------------------------------------------------------- forma de cada janela
const nivel = v => v.reduce((a, b) => a + b, 0) / v.length;
function incl(v) {             // inclinação por amostra (unidades normalizadas; 0,005 = 1 cm a cada 100 ms)
  const n = v.length, mx = (n - 1) / 2, my = nivel(v);
  let a = 0, b = 0;
  for (let i = 0; i < n; i++) { a += (i - mx) * (v[i] - my); b += (i - mx) ** 2; }
  return a / b;
}
jan.forEach(j => { j.nivel = nivel(j.x); j.incl = incl(j.x); });

// ---------------------------------------------------------------- rajadas
const rajadas = new Map();
for (const j of jan) {
  if (!rajadas.has(j.raj)) rajadas.set(j.raj, { id: j.raj, label: j.label, obst: j.obst, vel: j.vel, tipo: j.tipo, jan: [] });
  rajadas.get(j.raj).jan.push(j);
}
const faixaVel = v => (v < 170 ? 'lenta' : v < 225 ? 'media' : 'rapida');

console.log(`\n${arquivos.length} arquivo(s) · ${rajadas.size} rajadas · ${jan.length} janelas\n`);
console.log('classe        rajadas  janelas   obstáculos (rajadas)                       velocidades');
for (const c of CLASSES) {
  const rs = [...rajadas.values()].filter(r => r.label === c);
  const porObst = {}, porVel = {};
  rs.forEach(r => { porObst[r.obst] = (porObst[r.obst] || 0) + 1; porVel[faixaVel(r.vel)] = (porVel[faixaVel(r.vel)] || 0) + 1; });
  const nj = rs.reduce((s, r) => s + r.jan.length, 0);
  console.log(`${c.padEnd(13)} ${String(rs.length).padStart(7)}  ${String(nj).padStart(7)}   ` +
    Object.entries(porObst).map(([k, v]) => `${k} ${v}`).join(' · ').padEnd(42) + ' ' +
    Object.entries(porVel).map(([k, v]) => `${k} ${v}`).join(' · '));
}

// ---------------------------------------------------------------- separação por rajada
// Estratos = classe + obstáculo + faixa de velocidade. Em cada estrato, 1 rajada a cada 5 vai para
// o teste (a partir da terceira), então o teste cobre as mesmas variações que o treino.
const estratos = new Map();
for (const r of rajadas.values()) {
  const k = `${r.label}|${r.obst}|${faixaVel(r.vel)}`;
  if (!estratos.has(k)) estratos.set(k, []);
  estratos.get(k).push(r);
}
const teste = new Set();
for (const lista of estratos.values()) lista.forEach((r, i) => { if (i % 5 === 2) teste.add(r.id); });
for (const c of CLASSES) {           // garante pelo menos ~15% das rajadas de cada classe no teste
  const rs = [...rajadas.values()].filter(r => r.label === c);
  const falta = Math.ceil(rs.length * 0.15) - rs.filter(r => teste.has(r.id)).length;
  rs.filter(r => !teste.has(r.id)).filter((_, i) => i % 4 === 1).slice(0, Math.max(0, falta)).forEach(r => teste.add(r.id));
}
let tr = jan.filter(j => !teste.has(j.raj));
const te = jan.filter(j => teste.has(j.raj));

if (equilibrar) {                    // corta janelas das classes maiores, só no treino
  const menor = Math.min(...CLASSES.map(c => tr.filter(j => j.label === c).length));
  let seed = 7;
  const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  tr = CLASSES.flatMap(c => {
    const l = tr.filter(j => j.label === c);
    const manter = Math.min(l.length, Math.round(menor * 1.1));
    return l.map(j => [rnd(), j]).sort((a, b) => a[0] - b[0]).slice(0, manter).map(p => p[1]);
  });
}

// ---------------------------------------------------------------- estimativa honesta (1-NN)
const conf = CLASSES.map(() => CLASSES.map(() => 0));
for (const j of te) {
  let melhor = null, d = Infinity;
  for (const k of tr) {
    let s = 0;
    for (let i = 0; i < JANELA; i++) s += (j.x[i] - k.x[i]) ** 2;
    if (s < d) { d = s; melhor = k.label; }
  }
  conf[CLASSES.indexOf(j.label)][CLASSES.indexOf(melhor)]++;
}
const acc = conf.reduce((s, l, i) => s + l[i], 0) / Math.max(1, te.length);
const f1 = CLASSES.map((_, i) => {
  const tp = conf[i][i], fp = conf.reduce((s, l, k) => s + (k !== i ? l[i] : 0), 0), fn = conf[i].reduce((s, v, k) => s + (k !== i ? v : 0), 0);
  return tp ? (2 * tp) / (2 * tp + fp + fn) : 0;
});
console.log(`\nestimativa 1-NN (treino -> teste, rajadas separadas): ${(100 * acc).toFixed(1)}% em ${te.length} janelas`);
console.log('                real \\ previsto   ' + CLASSES.map(c => c.slice(0, 8).padStart(9)).join('') + '      F1');
CLASSES.forEach((c, i) => console.log('  ' + c.padEnd(28) + conf[i].map(v => String(v).padStart(9)).join('') + '    ' + f1[i].toFixed(2)));

// ---------------------------------------------------------------- checklist
const check = [];
const add = (ok, t, d) => check.push({ ok, t, d });
const de = c => jan.filter(j => j.label === c);
const nTr = CLASSES.map(c => tr.filter(j => j.label === c).length);
add(Math.min(...nTr) >= 400, 'volume: >= 400 janelas de treino por classe', nTr.join(' / '));
add(Math.max(...nTr) <= 1.5 * Math.min(...nTr), 'balanço: maior <= 1,5x menor', nTr.join(' / ') + (equilibrar ? ' (equilibrado)' : ''));
const ap = de('aproximando'), apDesce = ap.filter(j => j.incl < -0.005).length;
add(apDesce >= 0.8 * ap.length, "'aproximando': >= 80% das janelas descendo", `${apDesce}/${ap.length} (${pct(apDesce, ap.length)})`);
const bl = de('bloqueado'), blOk = bl.filter(j => Math.abs(j.incl) < 0.002 && j.nivel >= 0.04 && j.nivel <= 0.175).length;
add(blOk >= 0.85 * bl.length, "'bloqueado': >= 85% paradas entre 8 e 35 cm", `${blOk}/${bl.length} (${pct(blOk, bl.length)})`);
const li = de('livre'), liRisco = li.filter(j => j.nivel < 0.4 && j.incl < -0.002).length;
add(liRisco <= 0.05 * li.length, "'livre': <= 5% parecendo aproximação", `${liRisco}/${li.length} (${pct(liRisco, li.length)})`);
const obstPor = c => new Set([...rajadas.values()].filter(r => r.label === c && r.obst !== 'nenhum').map(r => r.obst)).size;
add(obstPor('aproximando') >= 3 && obstPor('bloqueado') >= 3, 'variedade: >= 3 obstáculos em aproximando e bloqueado',
  `aproximando ${obstPor('aproximando')} · bloqueado ${obstPor('bloqueado')}`);
const nTe = CLASSES.map(c => te.filter(j => j.label === c).length);
add(Math.min(...nTe) >= 80, 'teste: >= 80 janelas por classe', nTe.join(' / '));
add(acc >= 0.9 && Math.min(...f1) >= 0.85, 'estimativa: acerto >= 90% e F1 >= 0,85 em todas', `${(100 * acc).toFixed(1)}% · F1 mín ${Math.min(...f1).toFixed(2)}`);

console.log('\n--- checklist antes de subir no Edge Impulse ---');
check.forEach(c => console.log(`  ${c.ok ? 'OK   ' : 'FALHA'} ${c.t.padEnd(52)} ${c.d}`));

// ---------------------------------------------------------------- arquivos
const dir = path.dirname(arquivos[0]);
const cab = Array.from({ length: JANELA }, (_, i) => 'd' + i).join(',') + ',label';
const grava = (nome, l) => {
  fs.writeFileSync(path.join(dir, nome), [cab, ...l.map(j => j.x.map(v => v.toFixed(4)).join(',') + ',' + j.label)].join('\n') + '\n');
  console.log(`gerado: ${path.join(dir, nome)} (${l.length} janelas)`);
};
console.log('');
grava('treino.csv', tr);
grava('teste.csv', te);
const falhas = check.filter(c => !c.ok).length;
console.log(falhas
  ? `\n${falhas} critério(s) falhando: corrija na COLETA antes de treinar (ver docs/plano-coleta.md).`
  : '\nTudo ok: suba treino.csv como Training e teste.csv como Testing.');
