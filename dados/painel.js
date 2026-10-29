// Painel de coleta no computador: controla o carrinho pelo navegador e guarda cada rajada no disco.
//
//   clique duas vezes em "Abrir painel de coleta.bat" (na pasta do projeto)
//   ou: node dados/painel.js [--sem-navegador]
//
// Abre http://localhost:8321. O notebook precisa estar na rede CARRINHO-COLETA; no Windows o painel
// reconecta sozinho se o Windows trocar de rede. Cada rajada que o carrinho fecha tem um número
// único e fica na flash dele: o painel copia as que ainda não tem. Se o notebook cair da rede,
// nada se perde: quando ele volta, o painel busca o que faltou.
//
// Arquivos em dados/coleta/:
//   rajadas.jsonl          tudo o que chegou, uma rajada por linha (remoções também). NÃO apague
//   coleta_detalhado.csv   entrada do preparar.js, sempre atualizado
//   coleta_cru.csv         leituras cruas, para reprocessar sem coletar de novo
//   treino.csv, teste.csv  gerados pelo botão "Conferir" (sobem no Edge Impulse)
//   fluxo_AAAA-MM-DD.log   tudo o que o carrinho mandou, para diagnóstico

const http = require('http');
const fs = require('fs');
const path = require('path');
const { execFile, exec } = require('child_process');

const [HOST, PORTA_CARRO] = (process.env.CARRINHO || '192.168.4.1').split(':');   // CARRINHO=ip:porta para testes
const PORTA = +(process.env.PAINEL_PORTA || 8321);
const REDE = 'CARRINHO-COLETA';
const PASTA = process.env.PAINEL_PASTA || path.join(__dirname, 'coleta');
const ARQ_RAJ = path.join(PASTA, 'rajadas.jsonl');
const ARQ_DET = path.join(PASTA, 'coleta_detalhado.csv');
const ARQ_CRU = path.join(PASTA, 'coleta_cru.csv');
const INICIO = Date.now();

// (*) IDÊNTICAS ao coleta_wifi.ino
const DIST_MIN = 2, DIST_MAX = 200, PERTO_CM = 20, MEDIANA_N = 3, MEDIA_N = 5, NORM = 200;
const JANELA = 10, STRIDE = 2, AQUECE = MEDIANA_N + MEDIA_N - 1;
const CLASSES = ['livre', 'aproximando', 'bloqueado'];
const META_JANELAS = 600;
// metas do docs/plano-aproximando-bloqueado.md
const OBST_PLANO = ['parede', 'caixa', 'cadeira', 'canto'];
const META = { empurrar: 6, parado: 9, aproximar: 18, livreAndando: 31, livreRajada: 22 };

const espera = ms => new Promise(ok => setTimeout(ok, ms));
const hora = () => new Date().toLocaleTimeString('pt-BR');

// ---------------------------------------------------------------- o pipeline (*), em float32 como na placa
class Pipeline {
  constructor() { this.ult = -1; this.med = []; this.mov = []; }
  passo(cm) {
    if (cm <= 0) cm = (this.ult > 0 && this.ult < PERTO_CM) ? this.ult : DIST_MAX;
    else { cm = Math.min(DIST_MAX, Math.max(DIST_MIN, cm)); this.ult = cm; }
    this.med.push(cm); if (this.med.length > MEDIANA_N) this.med.shift();
    const v = [...this.med].sort((a, b) => a - b);
    this.mov.push(v[Math.floor(v.length / 2)]); if (this.mov.length > MEDIA_N) this.mov.shift();
    const s = this.mov.reduce((a, b) => a + b, 0);
    return Math.fround(Math.fround(s / this.mov.length) / NORM);
  }
}

// janelas iguais às do CSV da página + a mesma conferência de forma do preparar.js
const nivel = v => v.reduce((a, b) => a + b, 0) / v.length;
function incl(v) {
  const n = v.length, mx = (n - 1) / 2, my = nivel(v);
  let a = 0, b = 0;
  for (let i = 0; i < n; i++) { a += (i - mx) * (v[i] - my); b += (i - mx) ** 2; }
  return a / b;
}
function analisar(r) {
  const p = new Pipeline(), ult = [], jan = [];
  r.cru.forEach((c, i) => {
    ult[i % JANELA] = p.passo(c);
    const s = i - (JANELA - 1);
    if (s < AQUECE || (s - AQUECE) % STRIDE) return;
    jan.push(Array.from({ length: JANELA }, (_, q) => ult[(s + q) % JANELA]));
  });
  const grav = r.cru.slice(r.pre);
  const semEco = grav.filter(c => c === 0).length / Math.max(1, grav.length);
  const fr = f => jan.filter(f).length / Math.max(1, jan.length);
  const pc = x => Math.round(100 * x) + '%';
  let nota = '';
  if (r.rotulo === 'aproximando') { const x = fr(j => incl(j) < -0.005); if (x < 0.8) nota = `só ${pc(x)} das janelas descendo`; }
  if (r.rotulo === 'bloqueado') { const x = fr(j => Math.abs(incl(j)) < 0.002 && nivel(j) >= 0.04 && nivel(j) <= 0.175); if (x < 0.85) nota = `só ${pc(x)} paradas entre 8 e 35 cm`; }
  if (r.rotulo === 'livre') { const x = fr(j => nivel(j) < 0.4 && incl(j) < -0.002); if (x > 0.05) nota = `${pc(x)} parecem aproximação`; }
  if (semEco === 1) nota = 'nenhum eco: o sensor está medindo?';
  else if (semEco >= 0.5 && r.rotulo !== 'livre') nota = `sem eco em ${pc(semEco)} das leituras`;
  return { jan, nota };
}

// ---------------------------------------------------------------- dados guardados no computador
const dados = new Map();     // chave "serie:uid" -> rajada
let versao = 1, erroCsv = '';

function aplicar(reg) {
  if (reg.ev === 'rajada') { reg.removida = false; reg.an = analisar(reg); dados.set(reg.chave, reg); }
  else if (dados.has(reg.chave)) {
    const r = dados.get(reg.chave);
    if (reg.ev === 'lixeira') { r.removida = true; r.motivo = reg.motivo; }
    if (reg.ev === 'restaura') { r.removida = false; r.fixa = true; }
  }
}
function guardar(reg) {
  fs.appendFileSync(ARQ_RAJ, JSON.stringify(reg) + '\n');   // antes de aplicar(), que acrescenta campos calculados
  aplicar(reg);
  versao++;
  agendarCsv();
}
function carregar() {
  if (!fs.existsSync(ARQ_RAJ)) return;
  for (const l of fs.readFileSync(ARQ_RAJ, 'utf8').split('\n')) {
    if (!l.trim()) continue;
    try { aplicar(JSON.parse(l)); } catch { /* linha cortada por queda de energia do notebook */ }
  }
}
const ativas = () => [...dados.values()].filter(r => !r.removida);
function lixeira(chave, motivo) {
  const r = dados.get(chave);
  if (!r || r.removida) return;
  guardar({ ev: 'lixeira', chave, motivo, quando: new Date().toISOString() });
  nota(`rajada movida para a lixeira (${motivo})`);
}
function restaurar(chave) {
  const r = dados.get(chave);
  if (!r || !r.removida) return;
  guardar({ ev: 'restaura', chave, quando: new Date().toISOString() });
}

let tCsv = null;
function agendarCsv() { clearTimeout(tCsv); tCsv = setTimeout(gerarCsvs, 800); }
function gerarCsvs() {
  const cabJ = Array.from({ length: JANELA }, (_, i) => 'd' + i).join(',');
  const det = ['rajada,tipo,obstaculo,vel,' + cabJ + ',label'];
  const cru = ['rajada,tipo,obstaculo,vel,label,cm,aquece'];
  for (const r of ativas()) {
    const meta = `${r.chave.replace(':', '-')},${r.tipo},${r.obst},${r.vel},`;
    r.cru.forEach((c, i) => cru.push(meta + `${r.rotulo},${c},${i < r.pre ? 1 : 0}`));
    for (const j of r.an.jan) det.push(meta + j.map(v => v.toFixed(4)).join(',') + ',' + r.rotulo);
  }
  try {
    fs.writeFileSync(ARQ_DET, det.join('\n') + '\n');
    fs.writeFileSync(ARQ_CRU, cru.join('\n') + '\n');
    if (erroCsv) { erroCsv = ''; versao++; }
  } catch (e) {
    erroCsv = `não consegui gravar ${path.basename(e.path || ARQ_DET)}: feche o arquivo no Excel (${e.code})`;
    versao++;
  }
}
function conferir() {
  return new Promise(ok => execFile(process.execPath, [path.join(__dirname, 'preparar.js'), ARQ_DET], { windowsHide: true },
    (e, out, err) => ok((out || '') + (err || ''))));
}

// ---------------------------------------------------------------- ligação com o carrinho
const carro = { conectado: false, ultimoContato: 0, boot: null, desde: 0, estado: null };
const vivo = [];             // últimas amostras, para o gráfico
let nAmostra = 0, semEcoN = 0;
const msgs = [];
let nMsg = 0;
let lista = null;            // última resposta de /rajadas: { serie, base, prox, total, faltam }

function pedir(caminho, ms = 2000) {
  return new Promise((ok, falha) => {
    const req = http.get({ host: HOST, port: PORTA_CARRO || 80, path: caminho, timeout: ms, headers: { connection: 'close' } }, res => {
      let corpo = '';
      res.setEncoding('utf8');
      res.on('data', x => { corpo += x; });
      res.on('end', () => ok({ status: res.statusCode, corpo }));
      res.on('error', falha);
    });
    req.on('timeout', () => req.destroy(new Error('tempo esgotado')));
    req.on('error', falha);
  });
}
function mensagem(txt, de) {
  msgs.push({ n: ++nMsg, hora: hora(), txt, de });
  if (msgs.length > 60) msgs.shift();
}
function nota(txt) { mensagem(txt, 'painel'); console.log(`[${hora()}] ${txt}`); }
function contato() {
  carro.ultimoContato = Date.now();
  if (!carro.conectado) { carro.conectado = true; nota('conectado ao carrinho'); agendarSync(); }
}
function falhou() {
  if (carro.conectado && Date.now() - carro.ultimoContato > 3000) { carro.conectado = false; nota('sem conexão com o carrinho'); }
}

let fluxoDia = '', fluxo = null;
function gravarFluxo(linhas) {
  const d = new Date(), z = n => String(n).padStart(2, '0');
  const dia = `${d.getFullYear()}-${z(d.getMonth() + 1)}-${z(d.getDate())}`;
  if (dia !== fluxoDia) { if (fluxo) fluxo.end(); fluxoDia = dia; fluxo = fs.createWriteStream(path.join(PASTA, `fluxo_${dia}.log`), { flags: 'a' }); }
  fluxo.write(linhas.join('\n') + '\n');
}

function tratarLinhas(corpo) {
  const linhas = corpo.split('\n').filter(Boolean);
  if (!linhas.length || linhas[0][0] !== '#') return;
  contato();
  const [boot, seq] = linhas[0].slice(1).split(',');
  if (carro.boot === null) {             // primeira conversa: só os últimos 15 s, para o gráfico
    carro.boot = boot; carro.desde = Math.max(0, +seq - 150);
    return;
  }
  if (boot !== carro.boot) {
    carro.boot = boot; carro.desde = 0;
    nota(`o carrinho reiniciou (ligação ${boot}): as rajadas guardadas voltam da flash`);
    return;
  }
  const novas = linhas.slice(1);
  if (novas.length) gravarFluxo(novas);
  for (const l of novas) {
    if (l.startsWith('#perdido')) continue;   // só o gráfico perde: as rajadas vêm por /rajadas
    const p = l.split(',');
    carro.desde = Math.max(carro.desde, +p[1]);
    if (p[2] === 'A') {
      const cru = +p[4];
      semEcoN = cru === 0 ? semEcoN + 1 : 0;
      vivo.push({ n: ++nAmostra, cru, cm: +p[5], g: +p[7] });
      if (vivo.length > 300) vivo.shift();
    } else if (p[2] === 'M') mensagem(p.slice(3).join(','), 'carro');
    else if (p[2] === 'E' && ['fecha', 'desfaz', 'apaga', 'boot'].includes(p[3])) {
      if (p[3] === 'fecha' && carro.estado) carro.estado.modo = 'parado';   // fechou = parou: copia já
      agendarSync();
    }
  }
}
async function lacoLinhas() {
  for (;;) {
    try { tratarLinhas((await pedir('/linhas?desde=' + carro.desde)).corpo); await espera(250); }
    catch { falhou(); await espera(1000); }
  }
}
async function lacoEstado() {
  for (;;) {
    try { carro.estado = JSON.parse((await pedir('/estado', 1500)).corpo); contato(); }
    catch { falhou(); }
    await espera(700);
  }
}

// copia do carrinho as rajadas que o computador ainda não tem
let syncAtual = null, tSync = null;
function agendarSync(ms = 300) { clearTimeout(tSync); tSync = setTimeout(sincronizar, ms); }
// devolve true se terminou com o computador em dia com a lista do carrinho
function sincronizar() {
  if (!syncAtual) syncAtual = sincronizarAgora().finally(() => { syncAtual = null; });
  return syncAtual;
}
async function sincronizarAgora() {
  if (!carro.conectado) return false;
  if (carro.estado && carro.estado.modo !== 'parado') { agendarSync(1000); return false; }   // não atrapalha a gravação
  try {
    const { status, corpo } = await pedir('/rajadas', 5000);
    if (status !== 200 || corpo[0] !== '#') return false;
    const linhas = corpo.split('\n').filter(Boolean);
    const [serie, base, prox] = linhas[0].slice(1).split(',');
    const doCarro = linhas.slice(1).map(l => l.split(','));
    for (const p of doCarro) {
      const chave = serie + ':' + p[0];
      if (dados.has(chave)) continue;
      const r = await pedir('/rajada?uid=' + p[0], 3000);
      if (r.status !== 200) continue;
      const [cab, leit = ''] = r.corpo.trim().split('\n');
      const q = cab.split(','), cru = leit.split(',').map(Number);
      if (cru.length !== +q[6] || cru.some(Number.isNaN)) continue;   // chegou pela metade: tenta de novo depois
      const reg = { ev: 'rajada', chave, serie, uid: +q[0], rotulo: q[1], obst: q[2], vel: +q[3], tipo: q[4], pre: +q[5], cru, quando: new Date().toISOString() };
      guardar(reg);
      const num = [...dados.keys()].indexOf(chave) + 1;
      nota(`rajada #${num} copiada: ${reg.rotulo}, ${reg.obst}, ${reg.an.jan.length} janelas${reg.an.nota ? ' (atenção: ' + reg.an.nota + ')' : ''}`);
    }
    // Desfazer no carrinho tira daqui também. As de antes do último "Apagar" do carrinho ficam:
    // o computador é o arquivo, o carrinho é só o depósito temporário.
    const uids = new Set(doCarro.map(p => +p[0]));
    if (carro.estado && carro.estado.flash) {
      for (const r of dados.values()) {
        if (r.serie === serie && !r.removida && !r.fixa && r.uid >= +base && r.uid < +prox && !uids.has(r.uid)) lixeira(r.chave, 'desfeita no carrinho');
      }
    }
    const novo = { serie, base: +base, prox: +prox, total: doCarro.length, faltam: doCarro.filter(p => !dados.has(serie + ':' + p[0])).length };
    if (JSON.stringify(novo) !== JSON.stringify(lista)) { lista = novo; versao++; }
    return novo.faltam === 0;
  } catch { return false; /* tenta de novo no próximo ciclo */ }
}

// ---------------------------------------------------------------- Wi-Fi do Windows
const wifi = { ssid: null, auto: true, ultimaTentativa: 0, erro: '' };
function conectarWifi() {
  wifi.ultimaTentativa = Date.now();
  execFile('netsh', ['wlan', 'connect', 'name=' + REDE], { windowsHide: true }, e => {
    wifi.erro = e ? `o Windows não conectou na ${REDE}. O carrinho está ligado? Se estiver, conecte uma vez pela barra de tarefas para o Windows aprender a rede` : '';
    if (!e) nota(`pedi ao Windows para voltar à rede ${REDE}`);
  });
}
function lerWifi() {
  if (process.platform !== 'win32') return;
  execFile('netsh', ['wlan', 'show', 'interfaces'], { windowsHide: true }, (e, out) => {
    const m = /^\s*SSID\s*:\s*(.+?)\s*$/m.exec(out || '');
    wifi.ssid = m ? m[1] : null;
    const semCarro = !carro.conectado && Date.now() - carro.ultimoContato > 6000;
    if (wifi.auto && semCarro && wifi.ssid !== REDE && Date.now() - wifi.ultimaTentativa > 15000) conectarWifi();
  });
}

// ---------------------------------------------------------------- o que o navegador recebe
function dadosPainel() {
  const todas = [...dados.values()], at = ativas();
  const num = new Map(todas.map((r, i) => [r.chave, i + 1]));
  const conta = f => at.filter(f).length;
  const manual = t => t === 'rajada' || t === 'manual';
  const resumo = r => ({
    chave: r.chave, num: num.get(r.chave), rotulo: r.rotulo, obst: r.obst, vel: r.vel, tipo: r.tipo,
    janelas: r.an.jan.length, seg: (r.cru.length - r.pre) / 10, nota: r.an.nota,
    quando: r.quando, spark: r.cru.slice(r.pre), motivo: r.motivo,
  });
  return {
    totais: CLASSES.map(c => ({ classe: c, rajadas: conta(r => r.rotulo === c), janelas: at.filter(r => r.rotulo === c).reduce((s, r) => s + r.an.jan.length, 0), meta: META_JANELAS })),
    progresso: {
      meta: META,
      obst: OBST_PLANO.map(o => ({
        obst: o,
        empurrar: conta(r => r.rotulo === 'bloqueado' && r.tipo === 'empurrar' && r.obst === o),
        parado: conta(r => r.rotulo === 'bloqueado' && manual(r.tipo) && r.obst === o),
        aproximar: conta(r => r.rotulo === 'aproximando' && r.tipo === 'aproximar' && r.obst === o),
        aproxVel: at.filter(r => r.rotulo === 'aproximando' && r.tipo === 'aproximar' && r.obst === o).map(r => r.vel),
      })),
      livreAndando: conta(r => r.rotulo === 'livre' && r.tipo === 'livre'),
      livreRajada: conta(r => r.rotulo === 'livre' && manual(r.tipo)),
    },
    rajadas: at.slice(-40).reverse().map(resumo),
    lixeira: todas.filter(r => r.removida).reverse().map(resumo),
    noComputador: at.length, carro: lista,
    arquivos: { pasta: PASTA, detalhado: ARQ_DET, cru: ARQ_CRU }, erroCsv,
  };
}
function estadoPainel(a, m, v) {
  return {
    inicio: INICIO, conectado: carro.conectado,
    semContato: carro.ultimoContato ? Math.round((Date.now() - carro.ultimoContato) / 1000) : null,
    wifi: { ...wifi, rede: REDE, windows: process.platform === 'win32' },
    carro: carro.estado, semEco: Math.floor(semEcoN / 10),
    amostras: vivo.filter(s => s.n > a).slice(-150), nAmostra,
    msgs: msgs.filter(x => x.n > m), nMsg,
    versao, dados: v === versao ? null : dadosPainel(),
  };
}

// ---------------------------------------------------------------- servidor local (só neste computador)
const srv = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://localhost');
  const json = (o, st = 200) => { res.writeHead(st, { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' }); res.end(JSON.stringify(o)); };
  const q = k => u.searchParams.get(k);
  try {
    if (u.pathname === '/') {
      res.writeHead(200, { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store' });
      return res.end(fs.readFileSync(path.join(__dirname, 'painel.html')));
    }
    if (u.pathname === '/api/estado') return json(estadoPainel(+q('a') || 0, +q('m') || 0, +(q('v') ?? -1)));
    // comandos só pelo próprio painel: o cabeçalho impede outro site de mandar o carrinho andar
    if (req.method !== 'POST' || req.headers['x-painel'] !== '1') { res.writeHead(404); return res.end(); }
    if (u.pathname === '/api/cmd') {
      const c = q('c') || '';
      if (c === 'z') {             // só apaga o carrinho com uma conferência fresca de que tudo foi copiado
        await sincronizar();
        if (!(await sincronizar())) return json({ ok: false, msg: 'ainda há rajadas só no carrinho (ou ele está gravando): espere o painel copiar' });
      }
      const r = await pedir('/cmd?c=' + encodeURIComponent(c), 1500);
      return json({ ok: true, msg: r.corpo });
    }
    if (u.pathname === '/api/cfg') {
      const p = ['obst', 'vel'].filter(k => u.searchParams.has(k)).map(k => k + '=' + encodeURIComponent(q(k)));
      await pedir('/cfg?' + p.join('&'), 1500);
      return json({ ok: true });
    }
    if (u.pathname === '/api/lixeira') { lixeira(q('chave'), 'removida no painel'); return json({ ok: true }); }
    if (u.pathname === '/api/restaurar') { restaurar(q('chave')); return json({ ok: true }); }
    if (u.pathname === '/api/conferir') { gerarCsvs(); return json({ ok: true, saida: await conferir() }); }
    if (u.pathname === '/api/wifi') {
      if (u.searchParams.has('auto')) wifi.auto = q('auto') === '1';
      if (u.searchParams.has('conectar')) conectarWifi();
      return json({ ok: true });
    }
    res.writeHead(404); res.end();
  } catch (e) { json({ ok: false, msg: 'o carrinho não respondeu (' + e.message + ')' }, 502); }
});

function abrirNavegador() {
  const url = `http://localhost:${PORTA}`;
  if (process.platform === 'win32') exec(`start "" "${url}"`);
  else exec(`${process.platform === 'darwin' ? 'open' : 'xdg-open'} ${url}`);
}

fs.mkdirSync(PASTA, { recursive: true });
carregar();
gerarCsvs();
srv.on('error', e => {
  if (e.code !== 'EADDRINUSE') throw e;
  console.log('O painel já está aberto em outra janela. Abrindo o navegador...');
  abrirNavegador();
  setTimeout(() => process.exit(0), 500);
});
srv.listen(PORTA, '127.0.0.1', () => {
  console.log(`Painel de coleta: http://localhost:${PORTA}`);
  console.log(`Dados em ${PASTA} (${ativas().length} rajadas guardadas)`);
  console.log('Deixe esta janela aberta durante a coleta. Fechar a janela = parar o painel.\n');
  if (!process.argv.includes('--sem-navegador')) abrirNavegador();
  lacoLinhas();
  lacoEstado();
  setInterval(sincronizar, 5000);
  lerWifi();
  setInterval(lerWifi, 5000);
});
process.on('SIGINT', () => { gerarCsvs(); process.exit(0); });
if (process.env.PAINEL_SEGUNDOS) setTimeout(() => { gerarCsvs(); process.exit(0); }, 1000 * +process.env.PAINEL_SEGUNDOS);   // para testes
