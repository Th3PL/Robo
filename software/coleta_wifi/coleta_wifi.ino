/*
  Carrinho 2WD · ESP32-S3 — COLETA de dados para o Edge Impulse, sem cabo (Wi-Fi)

  O ESP32-S3 não tem Bluetooth clássico, então controle e download do CSV vão por Wi-Fi:
  a placa cria a rede "CARRINHO-COLETA" (senha carrinho123) e serve uma página em
  http://192.168.4.1 com todos os botões. O Monitor Serial (115200) aceita os mesmos
  comandos por letra (digite ? para a lista).

  O QUE SE GUARDA
    A distância CRUA de cada amostra (0 = sem eco), em rajadas. Cada rajada leva rótulo,
    obstáculo, velocidade e tipo. O pré-processamento (*) só é aplicado no download,
    rajada por rajada: dá para regerar o CSV sem coletar de novo.

  ROTEIROS AUTOMÁTICOS (o carrinho dirige e grava sozinho, então o rótulo é consistente)
    Aproximar  -> aproximando : anda reto gravando e para sozinho a 15 cm
    Empurrar   -> bloqueado   : anda até travar no batente, espera estabilizar e grava 3 s
    Livre      -> livre       : anda 3 s em espaço aberto; corta o fim se surgir obstáculo
    Rajada 3 s -> rótulo escolhido, com o carrinho no estado em que estiver

  NO COMPUTADOR
    dados/painel.js (atalho "Abrir painel de coleta.bat") controla o carrinho pelo navegador
    do notebook e guarda cada rajada no disco. Cada rajada tem um número único (uid) e fica
    na flash até o painel copiar: se o notebook cair da rede, ele busca o que faltou depois.

  Placa: ESP32S3 Dev Module · Flash Mode OPI 80MHz · Flash Size 32MB · PSRAM OPI PSRAM
*/

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_system.h>
#include <stdarg.h>

// ---------------------------------------------------------------- serial (USB nativa e UART)
#if ARDUINO_USB_CDC_ON_BOOT
  #define PORTA_USB Serial
  #define PORTA_UART Serial0
  #define DUAS_PORTAS 1
#elif ARDUINO_USB_MODE
  #include "HWCDC.h"
  HWCDC usbNativo;
  #define PORTA_USB usbNativo
  #define PORTA_UART Serial
  #define DUAS_PORTAS 1
#else
  #define PORTA_UART Serial
  #define DUAS_PORTAS 0
#endif

// ---------------------------------------------------------------- pinos
const int A_IA = 15, A_IB = 16;   // motor A, roda direita
const int B_IA = 4,  B_IB = 5;    // motor B, roda esquerda
const bool INVERTE_A = false, INVERTE_B = false;
const int TRIG = 1, ECHO = 2;     // HC-SR04 (Echo pelo divisor 1 kΩ / 1,5 kΩ)
const int LED_RGB = 38;           // LED RGB da placa ("RGB@IO38")

// ---------------------------------------------------------------- (*) pré-processamento
// IDÊNTICO no sketch de inferência. Se uma constante divergir, o modelo recebe na placa
// uma escala que nunca viu no treino.
const unsigned long PERIODO_MS = 100;     // (*) 10 Hz
const unsigned long TIMEOUT_US = 25000;   // (*) ~4 m
const int   DIST_MIN = 2, DIST_MAX = 200; // (*) faixa em cm
const int   PERTO_CM = 20;                // (*) sem eco logo após leitura < 20 cm = continua perto
const int   MEDIANA_N = 3;                // (*) remove picos isolados
const int   MEDIA_N = 5;                  // (*) média móvel
const float NORM = 200.0;                 // (*) normalização 0..1
const int   JANELA = 10;                  // (*) 10 amostras = 1 s
const int   STRIDE = 2;                   // passo entre janelas no CSV
const int   AQUECE = MEDIANA_N + MEDIA_N - 1;  // amostras até o filtro estabilizar

// ---------------------------------------------------------------- roteiros
const int PARADA_CM = 15;           // Aproximar para aqui
const int LIVRE_MIN_CM = 100;       // Livre corta a rajada se algo aparecer mais perto
const int TRAVA_MAX_CM = 35;        // Empurrar só grava se travou mais perto que isto
const int ZONA_CEGA_CM = 8;         // abaixo disto o HC-SR04 não mede direito
const int RAJADA_N = 30;            // 3 s
const int APROX_MAX_N = 80;         // 8 s
const int ESPERA_TRAVAR_N = 60;     // 6 s

const char *WIFI_NOME = "CARRINHO-COLETA";
const char *WIFI_SENHA = "carrinho123";

const int N_CLASSES = 3;
const char *CLASSE[N_CLASSES] = {"livre", "aproximando", "bloqueado"};
const int N_OBST = 6;
const char *OBST[N_OBST] = {"nenhum", "parede", "caixa", "cadeira", "canto", "outro"};
enum Tipo { T_MANUAL, T_APROX, T_EMPURRA, T_LIVRE, T_RAJADA };
const char *TIPO[] = {"manual", "aproximar", "empurrar", "livre", "rajada"};

// ---------------------------------------------------------------- memória
const int CAP = 15000;              // 25 min a 10 Hz
int16_t cru[CAP];                   // cm crus; 0 = sem eco
int nAmostras = 0;
// Cada rajada começa com até AQUECE leituras de ANTES do início ("pre"): elas só aquecem o
// filtro, como acontece na placa durante o uso, e não viram janela sozinhas.
// uid = ligação << 16 | número na ligação: único e sempre crescente, mesmo depois de reiniciar
struct Rajada { uint16_t inicio, n; uint8_t rotulo, obst, vel, tipo, pre; uint32_t uid; };
const int MAX_RAJ = 800;
Rajada raj[MAX_RAJ];
int nRaj = 0;
bool rajAberta = false;
int16_t anel[AQUECE];               // as últimas leituras cruas, para o "pre"
int nAnel = 0, iAnel = 0;
int gravadas(const Rajada &r) { return r.n - r.pre; }

// ---------------------------------------------------------------- transmissão ao computador
// Cada amostra e cada evento viram uma linha num anel de memória. O painel do computador
// (dados/painel.js) busca as linhas novas em /linhas para mostrar tudo ao vivo; as rajadas em si
// ele copia por /rajadas e /rajada. Linha: ligação,seq,tipo,... (ver painel.js)
const int N_LIN = 1000, LIN_TAM = 80;   // ~100 s de folga se o computador desconectar
char linhas[N_LIN][LIN_TAM];
uint32_t seqLin = 0;
uint16_t idRaj = 0;                     // número da rajada nesta ligação (só cresce)
uint32_t idGravado = 0;                 // uid da rajada em que a amostra atual foi gravada (0 = nenhuma)
uint32_t reinicios = 0;
uint32_t serie = 0;                     // sorteado uma vez por placa: separa uids se a flash for zerada
uint32_t uidBase = 0;                   // primeiro uid depois do último "Apagar tudo"
uint32_t proximoUid() { return (reinicios << 16) | (uint32_t)(idRaj + 1); }

void registrar(const char *fmt, ...) {
  seqLin++;
  char *l = linhas[seqLin % N_LIN];
  int k = snprintf(l, LIN_TAM, "%u,%u,", (unsigned)reinicios, (unsigned)seqLin);
  va_list ap; va_start(ap, fmt); vsnprintf(l + k, LIN_TAM - k, fmt, ap); va_end(ap);
}

// ---------------------------------------------------------------- cópia na flash
// Cada rajada fechada é anexada a /coleta.bin. Se a placa reiniciar (pilha, mau contato),
// tudo é recarregado no boot: a coleta não se perde mais. "Apagar tudo" apaga o arquivo.
const char *ARQ = "/coleta.bin";
bool flashOk = false;
String motivoReinicio;

// registro: n (2 bytes), rótulo, obstáculo, velocidade, tipo, pre, 0xA6, uid (4 bytes), n leituras
const int CAB_REG = 12;
void escreverRegistro(File &f, const Rajada &r) {
  uint8_t h[CAB_REG] = { (uint8_t)(r.n & 255), (uint8_t)(r.n >> 8), r.rotulo, r.obst, r.vel, r.tipo, r.pre, 0xA6,
                         (uint8_t)r.uid, (uint8_t)(r.uid >> 8), (uint8_t)(r.uid >> 16), (uint8_t)(r.uid >> 24) };
  f.write(h, CAB_REG);
  f.write((const uint8_t *)&cru[r.inicio], r.n * 2);
}
void salvarRajada(const Rajada &r) {
  if (!flashOk) return;
  File f = LittleFS.open(ARQ, "a");
  if (!f) return;
  escreverRegistro(f, r);
  f.close();
}
void regravarTudo() {
  if (!flashOk) return;
  File f = LittleFS.open(ARQ, "w");
  if (!f) return;
  for (int i = 0; i < nRaj; i++) escreverRegistro(f, raj[i]);
  f.close();
}
void carregar() {
  File f = LittleFS.open(ARQ, "r");
  if (!f) return;
  size_t valido = 0;
  while (f.available() >= CAB_REG) {
    uint8_t h[CAB_REG];
    f.read(h, CAB_REG);
    uint16_t n = h[0] | (h[1] << 8);
    if (h[7] != 0xA6 || n == 0 || nAmostras + n > CAP || nRaj >= MAX_RAJ || f.available() < n * 2) break;
    uint32_t uid = h[8] | (h[9] << 8) | ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    raj[nRaj] = { (uint16_t)nAmostras, n, h[2], h[3], h[4], h[5], h[6], uid };
    f.read((uint8_t *)&cru[nAmostras], n * 2);
    nAmostras += n;
    nRaj++;
    valido = f.position();
  }
  size_t tam = f.size();
  f.close();
  if (valido < tam) {                 // corta um registro pela metade (ou o formato antigo, sem uid)
    regravarTudo();
    if (!valido) avisar("cópia antiga na flash descartada (formato anterior ao painel)");
  }
}
String explicarReinicio(esp_reset_reason_t m) {
  switch (m) {
    case ESP_RST_POWERON:  return "energia cortada (chave, pilha ou mau contato)";
    case ESP_RST_BROWNOUT: return "queda de tensão (pilhas fracas ou pico dos motores)";
    case ESP_RST_PANIC:    return "erro no programa";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "programa travou (watchdog)";
    case ESP_RST_SW:       return "reinício pelo programa ou gravação";
    case ESP_RST_EXT:      return "botão RESET";
    default:               return "outro (código " + String((int)m) + ")";
  }
}

// ---------------------------------------------------------------- (*) o pipeline
// Copie esta struct sem mudar nada para o sketch de inferência.
struct Pipeline {
  int ultimoValido = -1;
  int med[MEDIANA_N]; int nMed = 0, iMed = 0;
  int mov[MEDIA_N];   int nMov = 0, iMov = 0;
  void reset() { ultimoValido = -1; nMed = iMed = nMov = iMov = 0; }
  // leitura crua em cm (0 = sem eco) -> valor normalizado 0..1
  float passo(int cm) {
    if (cm <= 0) cm = (ultimoValido > 0 && ultimoValido < PERTO_CM) ? ultimoValido : DIST_MAX;
    else { cm = constrain(cm, DIST_MIN, DIST_MAX); ultimoValido = cm; }
    med[iMed] = cm; iMed = (iMed + 1) % MEDIANA_N; if (nMed < MEDIANA_N) nMed++;
    int v[MEDIANA_N];
    for (int i = 0; i < nMed; i++) v[i] = med[i];
    for (int i = 1; i < nMed; i++) { int x = v[i], j = i - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
    mov[iMov] = v[nMed / 2]; iMov = (iMov + 1) % MEDIA_N; if (nMov < MEDIA_N) nMov++;
    long s = 0; for (int i = 0; i < nMov; i++) s += mov[i];
    return (s / (float)nMov) / NORM;
  }
};

// ---------------------------------------------------------------- estado
enum Modo { PARADO, CONTANDO, GRAVANDO, ROTEIRO };
Modo modo = PARADO;
Tipo tipoAtual = T_MANUAL;
int fase = 0, contaFase = 0;
unsigned long fimContagem = 0, proximo = 0, ultimoDriveWeb = 0;
bool driveWeb = false;
int rotulo = 0, obst = 1, vel = 200;
int ultimoCru = -1;
float ultimoFiltrado = -1;
float hist[5]; int nHist = 0;
Pipeline vivo;
String ultimaMsg = "pronto";
WebServer servidor(80);

void avisar(const char *fmt, ...) {
  char buf[160];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  ultimaMsg = buf;
  registrar("M,%s", buf);
  PORTA_UART.println(buf);
#if DUAS_PORTAS
  PORTA_USB.println(buf);
#endif
}

// ---------------------------------------------------------------- motores (com rampa)
int alvoEsq = 0, alvoDir = 0, atualEsq = 0, atualDir = 0;
void motor(int ia, int ib, int v, bool inv) {
  if (inv) v = -v;
  analogWrite(ia, v > 0 ? v : 0);
  analogWrite(ib, v < 0 ? -v : 0);
}
void andar(int e, int d) { alvoEsq = e; alvoDir = d; }
void pararMotores() { andar(0, 0); driveWeb = false; }
void rampa() {
  static unsigned long t = 0;
  if (millis() - t < 10) return;
  t = millis();
  // 10 por passo: de 0 a 200 em 0,2 s. Partida mais suave = menos pico de corrente nas pilhas
  atualEsq += constrain(alvoEsq - atualEsq, -10, 10);
  atualDir += constrain(alvoDir - atualDir, -10, 10);
  motor(A_IA, A_IB, atualDir, INVERTE_A);
  motor(B_IA, B_IB, atualEsq, INVERTE_B);
}

// ---------------------------------------------------------------- sensor
int lerCru() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseIn(ECHO, HIGH, TIMEOUT_US);
  if (us == 0) return 0;
  return constrain((int)(us / 58), 1, 500);
}

// ---------------------------------------------------------------- rajadas
int janelasDe(const Rajada &r) { int u = r.n - AQUECE - JANELA; return u < 0 ? 0 : u / STRIDE + 1; }

void fecharRajada(int cortarFinal = 0);

bool abrirRajada(int rot, Tipo t) {
  int pre = min(nAnel, AQUECE);
  if (nAmostras + pre >= CAP || nRaj >= MAX_RAJ) { avisar("memória cheia: baixe o CSV e apague"); return false; }
  uint32_t uid = proximoUid();
  idRaj++;
  raj[nRaj] = { (uint16_t)nAmostras, (uint16_t)pre, (uint8_t)rot, (uint8_t)obst, (uint8_t)vel, (uint8_t)t, (uint8_t)pre, uid };
  for (int i = 0; i < pre; i++) cru[nAmostras++] = anel[(iAnel - pre + i + AQUECE) % AQUECE];
  rajAberta = true;
  registrar("E,abre,%lu,%s,%s,%d,%s", (unsigned long)uid, CLASSE[rot], OBST[obst], vel, TIPO[t]);
  return true;
}
void gravar(int c) {
  if (!rajAberta) return;
  if (nAmostras >= CAP) { fecharRajada(); modo = PARADO; pararMotores(); avisar("memória cheia: baixe o CSV e apague"); return; }
  cru[nAmostras++] = c;
  raj[nRaj].n++;
  idGravado = raj[nRaj].uid;
}
void descartarRajada(const char *motivo) {
  if (rajAberta) {
    registrar("E,descarta,%lu,%s", (unsigned long)raj[nRaj].uid, motivo);
    nAmostras = raj[nRaj].inicio; rajAberta = false;
  }
  avisar("rajada descartada: %s", motivo);
}
void fecharRajada(int cortarFinal) {
  if (!rajAberta) return;
  Rajada &r = raj[nRaj];
  int corte = min(cortarFinal, (int)r.n);
  r.n -= corte; nAmostras -= corte;
  rajAberta = false;
  if (janelasDe(r) == 0) {
    registrar("E,descarta,%lu,curta", (unsigned long)r.uid);
    nAmostras = r.inicio; avisar("rajada curta demais, descartada"); return;
  }
  nRaj++;
  registrar("E,fecha,%lu,%d", (unsigned long)r.uid, gravadas(r));
  salvarRajada(r);
  avisar("rajada %d (%s, %s): %.1f s, %d janelas", nRaj, CLASSE[r.rotulo], OBST[r.obst], gravadas(r) / 10.0, janelasDe(r));
}
void desfazer() {
  if (rajAberta) { descartarRajada("desfeita"); modo = PARADO; pararMotores(); return; }
  if (!nRaj) { avisar("nada para desfazer"); return; }
  nRaj--; nAmostras = raj[nRaj].inicio;
  registrar("E,desfaz,%lu", (unsigned long)raj[nRaj].uid);
  regravarTudo();
  avisar("rajada %d apagada", nRaj + 1);
}

// ---------------------------------------------------------------- roteiros e gravação
void iniciar(Tipo t) {
  if (modo != PARADO) { avisar("ocupado: pare antes (p)"); return; }
  pararMotores();
  tipoAtual = t; modo = CONTANDO; fimContagem = millis() + 2000;
  const char *nome = t == T_APROX ? "Aproximar" : t == T_EMPURRA ? "Empurrar" : t == T_LIVRE ? "Livre" : t == T_RAJADA ? "Rajada 3 s" : "Gravação";
  avisar("%s em 2 s... saia da frente do sensor", nome);
}
void depoisDaContagem() {
  if (tipoAtual == T_MANUAL || tipoAtual == T_RAJADA) {
    if (!abrirRajada(rotulo, tipoAtual)) { modo = PARADO; return; }
    modo = GRAVANDO;
    avisar("GRAVANDO %s", CLASSE[rotulo]);
    return;
  }
  modo = ROTEIRO; fase = 0; contaFase = 0; nHist = 0;
  andar(vel, vel);
}
void fimDeRoteiro() { pararMotores(); modo = PARADO; }

void amostrar(int c, float f) {
  if (modo == GRAVANDO) {
    gravar(c);
    if (tipoAtual == T_RAJADA && rajAberta && gravadas(raj[nRaj]) >= RAJADA_N) { fecharRajada(); modo = PARADO; }
    return;
  }
  if (modo != ROTEIRO) return;
  contaFase++;
  if (tipoAtual == T_APROX) {
    if (fase == 0) {
      if (f <= PARADA_CM + 5) { fimDeRoteiro(); avisar("já está perto: afaste o carrinho do alvo"); return; }
      if (contaFase >= 3 && abrirRajada(1, T_APROX)) fase = 1;
      return;
    }
    gravar(c);
    if (f <= PARADA_CM || gravadas(raj[nRaj]) >= APROX_MAX_N) { fimDeRoteiro(); fecharRajada(); }
  } else if (tipoAtual == T_EMPURRA) {
    if (fase == 0) {
      hist[nHist % 5] = f; nHist++;
      float mx = -1, mn = 1e9;
      for (int i = 0; i < min(nHist, 5); i++) { mx = max(mx, hist[i]); mn = min(mn, hist[i]); }
      if (nHist >= 5 && mx - mn < 2 && f < TRAVA_MAX_CM && f >= ZONA_CEGA_CM) {
        if (abrirRajada(2, T_EMPURRA)) fase = 1;
      } else if (contaFase > ESPERA_TRAVAR_N) {
        fimDeRoteiro(); avisar("não travou: confira o batente (10 a 20 cm do obstáculo)");
      }
      return;
    }
    gravar(c);
    if (f < ZONA_CEGA_CM) { fimDeRoteiro(); descartarRajada("perto demais (zona cega): afaste o batente"); }
    else if (gravadas(raj[nRaj]) >= RAJADA_N) { fimDeRoteiro(); fecharRajada(); }
  } else if (tipoAtual == T_LIVRE) {
    if (fase == 0) {
      if (f < LIVRE_MIN_CM) { fimDeRoteiro(); avisar("obstáculo à frente: procure mais espaço"); return; }
      if (contaFase >= 3 && abrirRajada(0, T_LIVRE)) fase = 1;
      return;
    }
    gravar(c);
    if (f < LIVRE_MIN_CM) { fimDeRoteiro(); fecharRajada(10); }       // corta o último 1 s
    else if (gravadas(raj[nRaj]) >= RAJADA_N) { fimDeRoteiro(); fecharRajada(); }
  }
}

// ---------------------------------------------------------------- comandos
void ajuda() {
  avisar("--- comandos ---");
  avisar("0 1 2 rótulo (livre / aproximando / bloqueado)   o obstáculo seguinte");
  avisar("x Aproximar   y Empurrar   w Livre andando   r Rajada 3 s");
  avisar("g gravar (manual)   p parar tudo   u desfazer última rajada");
  avisar("F B L R dirigir   S parar motores   + - velocidade");
  avisar("n resumo   t distância   d CSV Edge Impulse   D CSV detalhado   z apagar tudo");
}
void resumo() {
  for (int k = 0; k < N_CLASSES; k++) {
    int nr = 0, nj = 0, na = 0;
    for (int i = 0; i < nRaj; i++) if (raj[i].rotulo == k) { nr++; nj += janelasDe(raj[i]); na += gravadas(raj[i]); }
    avisar("%-12s %3d rajadas  %4d janelas  %5.1f s", CLASSE[k], nr, nj, na / 10.0);
  }
  avisar("memória: %d de %d amostras", nAmostras, CAP);
}

void dirigir(char d, bool web) {
  // dirigir cancela os roteiros automáticos, mas não a gravação manual nem a Rajada 3 s
  bool roteiro = tipoAtual == T_APROX || tipoAtual == T_EMPURRA || tipoAtual == T_LIVRE;
  if (modo == ROTEIRO || (modo == CONTANDO && roteiro)) {
    if (modo == ROTEIRO) descartarRajada("roteiro interrompido pelo controle manual");
    modo = PARADO;
  }
  int v = vel, g = vel * 75 / 100;
  if (d == 'F') andar(v, v);
  else if (d == 'B') andar(-v, -v);
  else if (d == 'L') andar(-g, g);
  else if (d == 'R') andar(g, -g);
  else { pararMotores(); return; }
  driveWeb = web;
  ultimoDriveWeb = millis();
}

void enviarCsv(char formato, Print *serialOut);

void processar(char c, bool web) {
  switch (c) {
    case '0': case '1': case '2':
      if (modo == GRAVANDO) { fecharRajada(); modo = PARADO; }
      rotulo = c - '0'; avisar("rótulo: %s", CLASSE[rotulo]); break;
    case 'o': obst = (obst + 1) % N_OBST; avisar("obstáculo: %s", OBST[obst]); break;
    case 'x': iniciar(T_APROX); break;
    case 'y': iniciar(T_EMPURRA); break;
    case 'w': iniciar(T_LIVRE); break;
    case 'r': iniciar(T_RAJADA); break;
    case 'g': iniciar(T_MANUAL); break;
    case 'p':
      if (modo == ROTEIRO) descartarRajada("roteiro interrompido");
      else if (modo == GRAVANDO) fecharRajada();
      else avisar("parado");
      modo = PARADO; pararMotores(); break;
    case 'u': desfazer(); break;
    case 'F': case 'B': case 'L': case 'R': case 'S': dirigir(c, web); break;
    case '+': vel = min(255, vel + 10); avisar("velocidade %d", vel); break;
    case '-': vel = max(100, vel - 10); avisar("velocidade %d", vel); break;
    case 'n': resumo(); break;
    case 't': avisar("cru %d cm · filtrado %.0f cm", ultimoCru, ultimoFiltrado); break;
    case 'd': case 'D':
      if (web) avisar("use os botões de download da página");
      else enviarCsv(c == 'd' ? 'e' : 'd', (Print *)&PORTA_UART);
      break;
    case 'z': if (modo != PARADO) { avisar("pare antes de apagar"); break; }
              nAmostras = 0; nRaj = 0;
              uidBase = proximoUid();      // o painel não confunde isto com rajadas desfeitas
              { Preferences p; p.begin("coleta", false); p.putUInt("base", uidBase); p.end(); }
              registrar("E,apaga,%lu", (unsigned long)uidBase);
              if (flashOk) LittleFS.remove(ARQ);
              avisar("memória do carrinho apagada (memória e flash)"); break;
    case '?': ajuda(); break;
  }
}

// ---------------------------------------------------------------- CSV
// e = Edge Impulse (d0..d9,label) · d = detalhado (com rajada, tipo, obstáculo, velocidade)
// c = cru (uma linha por amostra) · a = formato A (1 feature, rótulo por limiar: a armadilha)
void enviarCsv(char formato, Print *serialOut) {
  if (modo != PARADO) { avisar("pare antes de baixar o CSV"); return; }
  String bloco;
  auto sai = [&](bool fim) {
    if (bloco.length() < 1500 && !fim) return;
    if (serialOut) serialOut->print(bloco); else servidor.sendContent(bloco);
    bloco = "";
  };
  if (!serialOut) {
    const char *nome = formato == 'e' ? "coleta_ei.csv" : formato == 'd' ? "coleta_detalhado.csv" : formato == 'c' ? "coleta_cru.csv" : "coleta_formatoA.csv";
    servidor.sendHeader("Content-Disposition", String("attachment; filename=") + nome);
    servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
    servidor.send(200, "text/csv", "");
  }
  if (formato == 'c') bloco = "rajada,tipo,obstaculo,vel,label,cm,aquece\n";
  else if (formato == 'a') bloco = "feature,label\n";
  else {
    if (formato == 'd') bloco = "rajada,tipo,obstaculo,vel,";
    for (int i = 0; i < JANELA; i++) bloco += "d" + String(i) + ",";
    bloco += "label\n";
  }
  int linhas = 0;
  for (int k = 0; k < nRaj; k++) {
    const Rajada &r = raj[k];
    String meta = String(k + 1) + "," + TIPO[r.tipo] + "," + OBST[r.obst] + "," + String(r.vel) + ",";
    if (formato == 'c') {
      for (int i = 0; i < r.n; i++) {
        bloco += meta + CLASSE[r.rotulo] + "," + String(cru[r.inicio + i]) + (i < r.pre ? ",1\n" : ",0\n");
        linhas++; sai(false);
      }
      continue;
    }
    // passa o pipeline amostra por amostra e solta cada janela assim que ela fica completa:
    // não há limite de tamanho de rajada
    Pipeline p;
    float ult[JANELA];
    for (int i = 0; i < r.n; i++) {
      float v = p.passo(cru[r.inicio + i]);
      ult[i % JANELA] = v;
      if (formato == 'a') {
        if (i < AQUECE) continue;
        float cm = v * NORM;
        bloco += String(v, 4) + "," + (cm > 60 ? "longe" : cm > 25 ? "medio" : "perto") + "\n";
        linhas++; sai(false);
        continue;
      }
      int s = i - (JANELA - 1);                 // início da janela que termina nesta amostra
      if (s < AQUECE || (s - AQUECE) % STRIDE) continue;
      if (formato == 'd') bloco += meta;
      for (int q = 0; q < JANELA; q++) bloco += String(ult[(s + q) % JANELA], 4) + ",";
      bloco += String(CLASSE[r.rotulo]) + "\n";
      linhas++; sai(false);
    }
  }
  sai(true);
  if (!serialOut) servidor.sendContent("");
  avisar("CSV enviado: %d linhas", linhas);
}

// uid,rótulo,obstáculo,velocidade,tipo,pre,n
String linhaRajada(const Rajada &r) {
  return String(r.uid) + "," + CLASSE[r.rotulo] + "," + OBST[r.obst] + "," + String(r.vel) + "," +
         TIPO[r.tipo] + "," + String(r.pre) + "," + String(r.n) + "\n";
}

// ---------------------------------------------------------------- página
const char PAGINA[] PROGMEM = R"PAG(<!doctype html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Coleta do carrinho</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;padding:12px;background:#f2f4f6;color:#17202a}
h1{font-size:20px;margin:4px 0 10px}section{background:#fff;border:1px solid #d5dbe1;border-radius:10px;padding:10px 12px;margin-bottom:10px}
h2{font-size:13px;margin:0 0 8px;color:#586572;text-transform:uppercase;letter-spacing:.05em}
.dist{font-size:44px;font-weight:600}.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
button,select,a.b{font:inherit;font-size:16px;padding:10px 12px;border-radius:8px;border:1px solid #9aa5b1;background:#fff;color:#17202a;text-decoration:none}
button.on{background:#0b6884;color:#fff;border-color:#0b6884}.stop{background:#c62828!important;color:#fff!important;border-color:#c62828!important;width:100%}
.pad{display:grid;grid-template-columns:repeat(3,68px);gap:6px;justify-content:center}.pad button{height:58px;touch-action:none}
table{width:100%;border-collapse:collapse;font-size:13px}td,th{padding:4px;border-bottom:1px solid #eee;text-align:right}td:first-child,th:first-child{text-align:left}
#msg{font-size:14px;color:#586572;min-height:1.4em;margin-top:4px}
</style></head><body>
<h1>Coleta do carrinho</h1>
<section><div class="row"><span class="dist" id="cm">--</span><span>cm</span><b id="eco" style="color:#c62828"></b><span id="modo" style="margin-left:auto"></span></div><div id="msg"></div>
<div id="boot" style="font-size:12px;color:#586572;margin-top:4px"></div></section>
<section><h2>Rótulo e cenário</h2>
<div class="row"><button data-r="0">livre</button><button data-r="1">aproximando</button><button data-r="2">bloqueado</button></div>
<div class="row" style="margin-top:8px"><select id="obst"></select><label>Velocidade <input type="range" id="vel" min="100" max="255" step="5"> <b id="velv"></b></label></div></section>
<section><h2>Roteiros automáticos</h2><div class="row">
<button data-c="x">Aproximar</button><button data-c="y">Empurrar</button><button data-c="w">Livre andando</button><button data-c="r">Rajada 3 s</button></div></section>
<section><h2>Dirigir (segure o botão)</h2><div class="pad"><span></span><button data-d="F">&#9650;</button><span></span>
<button data-d="L">&#9664;</button><button data-c="S">&#9632;</button><button data-d="R">&#9654;</button>
<span></span><button data-d="B">&#9660;</button><span></span></div>
<div class="row" style="margin-top:8px"><button data-c="g">Gravar</button><button data-c="p">Parar gravação</button></div></section>
<button class="stop" data-c="p">PARAR TUDO</button>
<section style="margin-top:10px"><h2>Dados</h2><table id="tab"></table>
<div class="row" style="margin-top:8px"><button data-c="u">Desfazer última rajada</button>
<a class="b" href="/csv?f=d">CSV detalhado</a><a class="b" href="/csv?f=e">CSV Edge Impulse</a><a class="b" href="/csv?f=c">Cru</a><a class="b" href="/csv?f=a">Formato A</a>
<button id="apagar">Apagar tudo</button></div></section>
<script>
const $=s=>document.querySelector(s);const OB=['nenhum','parede','caixa','cadeira','canto','outro'];
OB.forEach((o,i)=>{const e=document.createElement('option');e.value=i;e.textContent='Obstáculo: '+o;$('#obst').appendChild(e)});
function cmd(c){return fetch('/cmd?c='+encodeURIComponent(c)).catch(()=>{})}
document.querySelectorAll('[data-c]').forEach(b=>b.onclick=()=>cmd(b.dataset.c));
document.querySelectorAll('[data-r]').forEach(b=>b.onclick=()=>cmd(b.dataset.r));
document.querySelectorAll('[data-d]').forEach(b=>{let t=null;
 const ini=e=>{e.preventDefault();if(t)return;cmd(b.dataset.d);t=setInterval(()=>cmd(b.dataset.d),300)};
 const fim=e=>{if(!t)return;clearInterval(t);t=null;cmd('S')};
 b.addEventListener('pointerdown',ini);['pointerup','pointerleave','pointercancel'].forEach(n=>b.addEventListener(n,fim))});
$('#obst').onchange=e=>fetch('/cfg?obst='+e.target.value);
$('#vel').oninput=e=>{$('#velv').textContent=e.target.value};$('#vel').onchange=e=>fetch('/cfg?vel='+e.target.value);
$('#apagar').onclick=()=>{if(confirm('Apagar todas as rajadas gravadas?'))cmd('z')};
function upd(s){$('#cm').textContent=s.cm<0?'--':s.cm;$('#eco').textContent=s.cru===0?'sem eco':'';$('#modo').textContent=s.modo;$('#msg').textContent=s.msg;
 const m=Math.floor(s.up/60);$('#boot').textContent='ligado há '+m+' min '+(s.up%60)+' s · reinícios: '+s.boots+' · último: '+s.motivo+(s.flash?' · cópia na flash ativa':' · SEM cópia na flash');
 document.querySelectorAll('[data-r]').forEach(b=>b.classList.toggle('on',+b.dataset.r===s.rot));
 if(document.activeElement!==$('#obst'))$('#obst').value=s.obst;
 if(document.activeElement!==$('#vel')){$('#vel').value=s.vel;$('#velv').textContent=s.vel}
 let h='<tr><th>classe</th><th>rajadas</th><th>janelas</th>'+OB.slice(1).map(o=>'<th>'+o+'</th>').join('')+'</tr>';
 ['livre','aproximando','bloqueado'].forEach((c,i)=>{h+='<tr><td>'+c+'</td><td>'+s.raj[i]+'</td><td>'+s.jan[i]+'</td>'+s.mat[i].slice(1).map(v=>'<td>'+v+'</td>').join('')+'</tr>'});
 $('#tab').innerHTML=h+'<tr><td colspan="9">memória: '+s.uso+'% · meta: 600 janelas por classe</td></tr>'}
setInterval(()=>fetch('/estado').then(r=>r.json()).then(upd).catch(()=>{$('#modo').textContent='sem conexão'}),400);
</script></body></html>)PAG";

void rotas() {
  servidor.on("/", []() { servidor.send_P(200, "text/html; charset=utf-8", PAGINA); });
  servidor.on("/cmd", []() {
    String c = servidor.arg("c");
    if (c.length()) processar(c[0], true);
    servidor.send(200, "text/plain; charset=utf-8", ultimaMsg);
  });
  servidor.on("/cfg", []() {
    if (servidor.hasArg("obst")) obst = constrain(servidor.arg("obst").toInt(), 0, N_OBST - 1);
    if (servidor.hasArg("vel")) vel = constrain(servidor.arg("vel").toInt(), 100, 255);
    servidor.send(200, "text/plain", "ok");
  });
  servidor.on("/estado", []() {
    int nr[N_CLASSES] = {0}, nj[N_CLASSES] = {0}, mat[N_CLASSES][N_OBST] = {{0}};
    for (int i = 0; i < nRaj; i++) { nr[raj[i].rotulo]++; nj[raj[i].rotulo] += janelasDe(raj[i]); mat[raj[i].rotulo][raj[i].obst]++; }
    const char *m = modo == PARADO ? "parado" : modo == CONTANDO ? "contagem" : modo == GRAVANDO ? "GRAVANDO" : "roteiro";
    String j = "{\"cm\":" + String(ultimoFiltrado < 0 ? -1 : (int)lround(ultimoFiltrado)) + ",\"cru\":" + String(ultimoCru) +
               ",\"modo\":\"" + m + "\",\"rot\":" + rotulo +
               ",\"obst\":" + obst + ",\"vel\":" + vel + ",\"uso\":" + String(100L * nAmostras / CAP) + ",\"raj\":[";
    for (int k = 0; k < N_CLASSES; k++) j += String(nr[k]) + (k < N_CLASSES - 1 ? "," : "],\"jan\":[");
    for (int k = 0; k < N_CLASSES; k++) j += String(nj[k]) + (k < N_CLASSES - 1 ? "," : "],\"mat\":[");
    for (int k = 0; k < N_CLASSES; k++) {
      j += "[";
      for (int o = 0; o < N_OBST; o++) j += String(mat[k][o]) + (o < N_OBST - 1 ? "," : "]");
      j += k < N_CLASSES - 1 ? "," : "]";
    }
    String msg = ultimaMsg; msg.replace("\"", "'");
    j += ",\"msg\":\"" + msg + "\",\"up\":" + String(millis() / 1000) + ",\"boots\":" + String(reinicios) +
         ",\"motivo\":\"" + motivoReinicio + "\",\"flash\":" + (flashOk ? "true" : "false") + "}";
    servidor.send(200, "application/json", j);
  });
  // o computador busca as linhas novas: /linhas?desde=<última seq que ele já tem>
  servidor.on("/linhas", []() {
    uint32_t desde = (uint32_t)servidor.arg("desde").toInt();
    String out = "#" + String(reinicios) + "," + String(seqLin) + "\n";
    uint32_t ini = desde + 1;
    if (seqLin >= (uint32_t)N_LIN && ini + N_LIN <= seqLin) {
      out += "#perdido," + String(ini) + "," + String(seqLin - N_LIN) + "\n";
      ini = seqLin - N_LIN + 1;
    }
    for (uint32_t s = ini; s <= seqLin && s < ini + 400; s++) { out += linhas[s % N_LIN]; out += '\n'; }
    servidor.send(200, "text/plain; charset=utf-8", out);
  });
  // o painel confere o que já tem: "#serie,base,próximo uid" e uma linha por rajada guardada
  servidor.on("/rajadas", []() {
    servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
    servidor.send(200, "text/plain; charset=utf-8", "");
    String b = "#" + String(serie) + "," + String(uidBase) + "," + String(proximoUid()) + "\n";
    for (int k = 0; k < nRaj; k++) {
      b += linhaRajada(raj[k]);
      if (b.length() > 1500) { servidor.sendContent(b); b = ""; }
    }
    servidor.sendContent(b);
    servidor.sendContent("");
  });
  // e baixa as que faltam: /rajada?uid=N -> a mesma linha + as leituras cruas (com o "pre")
  servidor.on("/rajada", []() {
    uint32_t uid = strtoul(servidor.arg("uid").c_str(), nullptr, 10);
    for (int k = 0; k < nRaj; k++) {
      const Rajada &r = raj[k];
      if (r.uid != uid) continue;
      String b = linhaRajada(r);
      for (int i = 0; i < r.n; i++) { b += String(cru[r.inicio + i]); b += i < r.n - 1 ? ',' : '\n'; }
      servidor.send(200, "text/plain; charset=utf-8", b);
      return;
    }
    servidor.send(404, "text/plain; charset=utf-8", "rajada não encontrada");
  });
  servidor.on("/csv", []() {
    String f = servidor.arg("f");
    char k = f.length() ? f[0] : 'e';
    if (modo != PARADO) { servidor.send(409, "text/plain; charset=utf-8", "pare a gravação antes de baixar"); return; }
    enviarCsv(k, nullptr);
  });
}

// ---------------------------------------------------------------- LED de estado
void led() {
  static unsigned long t = 0;
  if (millis() - t < 100) return;
  t = millis();
  bool pisca = (millis() / 250) % 2;
  if (modo == CONTANDO)      rgbLedWrite(LED_RGB, pisca ? 40 : 0, pisca ? 30 : 0, 0);   // amarelo piscando
  else if (rajAberta)        rgbLedWrite(LED_RGB, 40, 0, 0);                            // vermelho: gravando
  else if (modo == ROTEIRO)  rgbLedWrite(LED_RGB, 0, 0, 40);                            // azul: andando
  else                       rgbLedWrite(LED_RGB, 0, pisca ? 12 : 4, 0);                // verde fraco: pronto
}

void setup() {
  for (int p : {A_IA, A_IB, B_IA, B_IB}) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(TRIG, OUTPUT); digitalWrite(TRIG, LOW); pinMode(ECHO, INPUT);
  PORTA_UART.begin(115200);
#if DUAS_PORTAS
  PORTA_USB.begin(115200);
  PORTA_USB.setTxTimeoutMs(0);
#endif
  // por que reiniciou + contador de reinícios (guardado na flash de configurações)
  motivoReinicio = explicarReinicio(esp_reset_reason());
  Preferences prefs;
  prefs.begin("coleta", false);
  reinicios = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", reinicios);
  serie = prefs.getUInt("serie", 0);
  if (!serie) { serie = esp_random() | 1; prefs.putUInt("serie", serie); }
  uidBase = prefs.getUInt("base", 0);
  prefs.end();

  // recupera as rajadas salvas antes de um reinício
  flashOk = LittleFS.begin(true);
  if (flashOk) carregar();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NOME, WIFI_SENHA, 1, 0, 2);
  WiFi.setTxPower(WIFI_POWER_5dBm);     // menos pico de corrente: o pack de pilhas agradece
  rotas();
  servidor.begin();
  delay(500);
  avisar("Wi-Fi \"%s\" (senha %s) -> http://%s", WIFI_NOME, WIFI_SENHA, WiFi.softAPIP().toString().c_str());
  ajuda();
  registrar("E,boot,%s", motivoReinicio.c_str());
  avisar("reinício %u: %s", reinicios, motivoReinicio.c_str());
  if (nRaj) avisar("recuperadas %d rajadas salvas na flash", nRaj);
  else if (!flashOk) avisar("flash indisponível: as rajadas ficam só na memória");
  proximo = millis();
}

void loop() {
  servidor.handleClient();
  while (PORTA_UART.available()) processar(PORTA_UART.read(), false);
#if DUAS_PORTAS
  while (PORTA_USB.available()) processar(PORTA_USB.read(), false);
#endif
  // segurança: botão de dirigir solto ou celular desconectado -> para
  if (driveWeb && millis() - ultimoDriveWeb > 700) { pararMotores(); }

  if (modo == CONTANDO && (long)(millis() - fimContagem) >= 0) depoisDaContagem();

  if ((long)(millis() - proximo) >= 0) {
    proximo += PERIODO_MS;                 // cadência fixa: não acumula atraso
    ultimoCru = lerCru();
    ultimoFiltrado = vivo.passo(ultimoCru) * NORM;
    anel[iAnel] = ultimoCru; iAnel = (iAnel + 1) % AQUECE; if (nAnel < AQUECE) nAnel++;
    idGravado = 0;
    amostrar(ultimoCru, ultimoFiltrado);
    // amostra: ms, cm cru (0 = sem eco), cm filtrado, rajada em que foi gravada, gravou (0/1)
    registrar("A,%lu,%d,%d,%lu,%d", millis(), ultimoCru, (int)lround(ultimoFiltrado), (unsigned long)idGravado, idGravado ? 1 : 0);
  }
  rampa();
  led();
}
