/*
  SMARS - firmware completo: Bluetooth + sensor ultrassônico + modo autônomo (um único upload)

  Modos:
    MANUAL (padrão): comandos do app pelo Bluetooth. O sensor funciona como SEGURANÇA: se houver
                     obstáculo a menos de DIST_PARAR, os comandos de avanço (F/G/I) são bloqueados e o
                     carrinho para; ré e giros continuam liberados. Liberou o caminho, o avanço volta.
    AUTÔNOMO:        o carrinho anda sozinho desviando de obstáculos (mesma lógica do smars_desvio v3).

  Comandos Bluetooth (app "Arduino Bluetooth RC Car" ou terminal serial):
    F/B/L/R  frente / ré / esquerda / direita      S  parar (também sai do modo autônomo)
    G/I      frente-esquerda / frente-direita      H/J  ré-esquerda / ré-direita
    0-9, q   velocidade (slider do app)
    X        liga o modo autônomo                  x  desliga o modo autônomo

  Hardware: Arduino Uno + shield L293D (AFMotor) + 2x N20 em M1/M2 + HC-SR04 em A0/A1 +
            HC-05 em D9/D10 (pinos dos conectores de servo do shield) + bateria 9 V no shield.
*/

#include <AFMotor.h>
#include <SoftwareSerial.h>

// ---------------- Hardware ----------------
SoftwareSerial bt(9, 10);      // RX, TX
AF_DCMotor motorE(1);          // esquerdo em M1
AF_DCMotor motorD(2);          // direito em M2
const int PINO_TRIG = A0;
const int PINO_ECHO = A1;

const int VEL_MAX_E = 255;     // teto por motor: abaixe o mais forte se o carrinho puxa pra um lado
const int VEL_MAX_D = 255;

// ---------------- Parâmetros (ajuste aqui) ----------------
const int DIST_PARAR   = 20;   // cm: obstáculo -> bloqueia avanço (manual) / desvia (autônomo)
const int DIST_REDUZIR = 45;   // cm: autônomo desacelera daqui até DIST_PARAR
const int DIST_LIVRE   = 40;   // cm: mínimo pra considerar a direção livre ao girar

const int VEL_CRUZEIRO = 220;  // autônomo: velocidade normal
const int VEL_MINIMA   = 130;  // autônomo: perto de obstáculo
const int VEL_MANOBRA  = 255;  // manobras
const int VEL_CURVA_INTERNA = 90;   // manual: roda de dentro nas diagonais

const int T_RE_MS = 400, T_RE_ARCO_MS = 700, T_GIRO_MIN_MS = 250, T_MARGEM_MS = 200, T_MEIA_VOLTA_MS = 1200, T_PAUSA_MS = 120;
const unsigned long T_GIRO_MAX_MS = 2500;
const int LEITURAS_LIVRES = 3;
const unsigned long INTERVALO_PING_MS = 60;
const int MAX_DESVIOS_SEGUIDOS = 3;
const unsigned long JANELA_PRESO_MS = 6000;
const bool TELEMETRIA = false; // true: envia a distância pelo Bluetooth a cada 500 ms (use com app de terminal)

// ---------------- Estado ----------------
bool modoAuto = false;
char ultimoComando = 'S';
int vel = 255;
int leituras[3] = {400, 400, 400};
byte idxLeitura = 0;
int distancia = 400;
unsigned long ultimoPing = 0, ultimoLog = 0;
bool ladoDireita = true;
byte desviosRecentes = 0;
unsigned long inicioJanela = 0;

// ---------------- Motores ----------------
void aplicar(int e, int d, uint8_t dirE, uint8_t dirD) {
  motorE.setSpeed(map(constrain(e, 0, 255), 0, 255, 0, VEL_MAX_E));
  motorD.setSpeed(map(constrain(d, 0, 255), 0, 255, 0, VEL_MAX_D));
  motorE.run(dirE); motorD.run(dirD);
}
void frente(int v) { aplicar(v, v, FORWARD, FORWARD); }
void re(int v)     { aplicar(v, v, BACKWARD, BACKWARD); }
void parar()       { aplicar(0, 0, RELEASE, RELEASE); }
void girar(bool direita, int v) { aplicar(v, v, direita ? FORWARD : BACKWARD, direita ? BACKWARD : FORWARD); }
void reArco(bool narizDireita, int v) { if (narizDireita) aplicar(v / 3, v, BACKWARD, BACKWARD); else aplicar(v, v / 3, BACKWARD, BACKWARD); }

// ---------------- Sensor ----------------
int lerCrua() {
  digitalWrite(PINO_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  unsigned long dur = pulseIn(PINO_ECHO, HIGH, 25000UL);
  if (dur == 0) return 400;
  return constrain((int)(dur / 58), 2, 400);
}
int mediana3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { int t = a; a = b; b = t; }
  return b;
}
void atualizarDistancia() {
  if (millis() - ultimoPing < INTERVALO_PING_MS) return;
  ultimoPing = millis();
  leituras[idxLeitura] = lerCrua();
  idxLeitura = (idxLeitura + 1) % 3;
  distancia = mediana3(leituras[0], leituras[1], leituras[2]);
}
int medirParado() {
  parar(); delay(T_PAUSA_MS);
  int a = lerCrua(); delay(40);
  int b = lerCrua(); delay(40);
  int c = lerCrua();
  int m = mediana3(a, b, c);
  leituras[0] = leituras[1] = leituras[2] = m;
  distancia = m;
  return m;
}

// ---------------- Manobras do modo autônomo ----------------
bool girarAteLivre(bool direita, unsigned long maxMs) {
  girar(direita, VEL_MANOBRA);
  unsigned long t0 = millis(); int livres = 0;
  while (millis() - t0 < maxMs) {
    delay(INTERVALO_PING_MS);
    int d = lerCrua();
    livres = (d >= DIST_LIVRE) ? livres + 1 : 0;
    if (livres >= LEITURAS_LIVRES && millis() - t0 >= (unsigned long)T_GIRO_MIN_MS) { delay(T_MARGEM_MS); parar(); return true; }
  }
  parar(); return false;
}
bool registrarDesvio() {
  unsigned long agora = millis();
  if (agora - inicioJanela > JANELA_PRESO_MS) { inicioJanela = agora; desviosRecentes = 0; }
  desviosRecentes++;
  if (desviosRecentes >= MAX_DESVIOS_SEGUIDOS) { desviosRecentes = 0; inicioJanela = agora; return true; }
  return false;
}
void desviar() {
  bool preso = registrarDesvio();
  Serial.print(F("Obstaculo a ")); Serial.print(distancia); Serial.println(preso ? F(" cm - preso, fuga") : F(" cm"));
  parar(); delay(T_PAUSA_MS);
  if (preso) { ladoDireita = !ladoDireita; reArco(ladoDireita, VEL_MANOBRA); delay(2 * T_RE_ARCO_MS); }
  else       { re(VEL_MANOBRA); delay(T_RE_MS); }
  if (girarAteLivre(ladoDireita, T_GIRO_MAX_MS)) { medirParado(); return; }
  reArco(!ladoDireita, VEL_MANOBRA); delay(T_RE_ARCO_MS);
  if (girarAteLivre(!ladoDireita, T_GIRO_MAX_MS)) { ladoDireita = !ladoDireita; medirParado(); return; }
  girar(ladoDireita, VEL_MANOBRA); delay(T_MEIA_VOLTA_MS);
  medirParado();
}
void passoAutonomo() {
  if (distancia <= DIST_PARAR) { desviar(); return; }
  int v = VEL_CRUZEIRO;
  if (distancia < DIST_REDUZIR) v = map(distancia, DIST_PARAR, DIST_REDUZIR, VEL_MINIMA, VEL_CRUZEIRO);
  frente(v);
}

// ---------------- Modo manual ----------------
bool comandoAvanca(char c) { return c == 'F' || c == 'G' || c == 'I'; }

void executarManual() {
  char c = ultimoComando;
  if (comandoAvanca(c) && distancia <= DIST_PARAR) { parar(); return; }   // segurança: obstáculo veta o avanço
  int vi = vel * VEL_CURVA_INTERNA / 255;
  switch (c) {
    case 'F': frente(vel); break;
    case 'B': re(vel); break;
    case 'L': girar(false, vel); break;
    case 'R': girar(true, vel); break;
    case 'G': aplicar(vi,  vel, FORWARD,  FORWARD);  break;
    case 'I': aplicar(vel, vi,  FORWARD,  FORWARD);  break;
    case 'H': aplicar(vi,  vel, BACKWARD, BACKWARD); break;
    case 'J': aplicar(vel, vi,  BACKWARD, BACKWARD); break;
    default:  parar(); break;
  }
}

void tratarBluetooth() {
  while (bt.available()) {
    char c = bt.read();
    if (c >= '0' && c <= '9') { vel = map(c - '0', 0, 9, 25, 230); continue; }
    if (c == 'q' || c == 'Q') { vel = 255; continue; }
    if (c == 'X') { modoAuto = true;  Serial.println(F("modo AUTONOMO")); medirParado(); continue; }
    if (c == 'x') { modoAuto = false; ultimoComando = 'S'; parar(); Serial.println(F("modo MANUAL")); continue; }
    char u = toupper(c);
    if (u == 'S' || u == 'D') { modoAuto = false; ultimoComando = 'S'; parar(); continue; }
    if (u == 'F' || u == 'B' || u == 'L' || u == 'R' || u == 'G' || u == 'I' || u == 'H' || u == 'J') {
      modoAuto = false; ultimoComando = u;
      Serial.print(F("cmd ")); Serial.println(u);
    }
  }
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_TRIG, LOW);
  parar();
  Serial.println(F("SMARS completo: manual (Bluetooth) + seguranca + autonomo (X/x)"));
}

void loop() {
  tratarBluetooth();
  atualizarDistancia();

  if (modoAuto) passoAutonomo();
  else          executarManual();

  if (millis() - ultimoLog >= 500) {
    ultimoLog = millis();
    Serial.print(modoAuto ? F("[AUTO] ") : F("[MANUAL] ")); Serial.print(distancia); Serial.println(F(" cm"));
    if (TELEMETRIA) { bt.print(distancia); bt.println(F(" cm")); }
  }
}
