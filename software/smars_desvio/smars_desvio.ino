/*
  SMARS - desvio de obstáculos com HC-SR04 (sensor fixo, sem servo) - v3

  Ideia central: o carrinho NUNCA volta a andar pra frente "no escuro". Depois de um obstáculo
  ele gira e continua girando ATÉ O SENSOR VER CAMINHO LIVRE (várias leituras seguidas), com uma
  margem extra. Não depende de virar "exatamente 45°" - se girar devagar, só demora mais.

  Sequência ao encontrar obstáculo:
    1. para, dá uma ré curta
    2. gira pro lado preferido até ver caminho livre  -> achou? segue
    3. não achou: ré em arco pro outro lado (tira o nariz da parede) e gira pro outro lado até livre
    4. não achou: meia-volta e segue
  Batidas seguidas mantêm o MESMO lado (se virou pra direita e bateu de novo, a parede está à
  esquerda: virar mais pra direita). 3 desvios em 6 s = preso: ré em arco longa e troca de lado.

  Mecânica: girar no lugar obriga as rodas LIVRES a deslizar de lado. Elástico só nas rodas do
  motor; as livres têm que ficar lisas, senão o carrinho não gira.
*/

#include <AFMotor.h>

// ---------------- Motores ----------------
AF_DCMotor motorE(1);   // esquerdo em M1
AF_DCMotor motorD(2);   // direito em M2

const int VEL_MAX_E = 255;   // teto por motor (0-255): abaixe o mais forte se ele puxa pra um lado
const int VEL_MAX_D = 255;

// ---------------- Sensor HC-SR04 ----------------
const int PINO_TRIG = A0;
const int PINO_ECHO = A1;

// ---------------- Comportamento (ajuste aqui) ----------------
const int DIST_PARAR   = 20;    // cm: abaixo disso para e desvia
const int DIST_REDUZIR = 45;    // cm: daqui até PARAR vai desacelerando
const int DIST_LIVRE   = 40;    // cm: mínimo pra considerar a direção "livre" ao girar

const int VEL_CRUZEIRO = 220;   // velocidade normal
const int VEL_MINIMA   = 130;   // perto de obstáculo (abaixo disso o TT nem sai do lugar)
const int VEL_MANOBRA  = 255;   // manobras sempre com força total (precisa vencer o atrito lateral)

const int T_RE_MS           = 400;    // ré reta antes de girar
const int T_RE_ARCO_MS      = 700;    // ré em arco (quando o primeiro lado não abre)
const int T_GIRO_MIN_MS     = 250;    // gira pelo menos isso antes de aceitar "livre"
const int T_MARGEM_MS       = 200;    // continua girando esse tanto depois de ver livre (afasta da borda)
const unsigned long T_GIRO_MAX_MS = 2500;   // desiste de procurar pra esse lado depois disso
const int T_MEIA_VOLTA_MS   = 1200;   // último recurso (CALIBRE: tempo pra ~180°)
const int LEITURAS_LIVRES   = 3;      // leituras seguidas >= DIST_LIVRE pra aceitar
const int T_PAUSA_MS        = 120;

const unsigned long INTERVALO_PING_MS = 60;
const int MAX_DESVIOS_SEGUIDOS = 3;
const unsigned long JANELA_PRESO_MS = 6000;

// ---------------- Estado ----------------
int leituras[3] = {400, 400, 400};
byte idxLeitura = 0;
int distancia = 400;
unsigned long ultimoPing = 0, ultimoLog = 0;
bool ladoDireita = true;          // lado preferido pra virar (mantém entre batidas seguidas)
byte desviosRecentes = 0;
unsigned long inicioJanela = 0;

// ---------------- Motores ----------------
void velocidade(int e, int d) {
  motorE.setSpeed(map(constrain(e, 0, 255), 0, 255, 0, VEL_MAX_E));
  motorD.setSpeed(map(constrain(d, 0, 255), 0, 255, 0, VEL_MAX_D));
}
void frente(int v)   { velocidade(v, v); motorE.run(FORWARD);  motorD.run(FORWARD);  }
void re(int v)       { velocidade(v, v); motorE.run(BACKWARD); motorD.run(BACKWARD); }
void parar()         { motorE.run(RELEASE); motorD.run(RELEASE); }

// gira no lugar: direita = sentido horário visto de cima (esquerdo pra frente, direito pra trás)
void girar(bool direita, int v) {
  velocidade(v, v);
  motorE.run(direita ? FORWARD : BACKWARD);
  motorD.run(direita ? BACKWARD : FORWARD);
}
// ré em arco: dá ré girando o nariz pro lado pedido (a roda desse lado anda mais)
void reArco(bool narizDireita, int v) {
  if (narizDireita) velocidade(v / 3, v); else velocidade(v, v / 3);
  motorE.run(BACKWARD); motorD.run(BACKWARD);
}

// ---------------- Sensor ----------------
int lerCrua() {
  digitalWrite(PINO_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  unsigned long dur = pulseIn(PINO_ECHO, HIGH, 25000UL);   // timeout 25 ms ~ 4 m
  if (dur == 0) return 400;                                // sem eco = livre
  return constrain((int)(dur / 58), 2, 400);
}
int mediana3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { int t = a; a = b; b = t; }
  return b;
}
void atualizarDistancia() {              // um ping a cada 60 ms, mediana das 3 últimas
  if (millis() - ultimoPing < INTERVALO_PING_MS) return;
  ultimoPing = millis();
  leituras[idxLeitura] = lerCrua();
  idxLeitura = (idxLeitura + 1) % 3;
  distancia = mediana3(leituras[0], leituras[1], leituras[2]);
}
int medirParado() {                      // 3 leituras novas parado + zera o filtro
  parar();
  delay(T_PAUSA_MS);
  int a = lerCrua(); delay(40);
  int b = lerCrua(); delay(40);
  int c = lerCrua();
  int m = mediana3(a, b, c);
  leituras[0] = leituras[1] = leituras[2] = m;
  distancia = m;
  return m;
}

// ---------------- Manobras ----------------
// Gira pro lado pedido até o sensor ver caminho livre. true = achou.
bool girarAteLivre(bool direita, unsigned long maxMs) {
  Serial.print(F("  girando p/ ")); Serial.print(direita ? F("direita") : F("esquerda")); Serial.print(F("... "));
  girar(direita, VEL_MANOBRA);
  unsigned long t0 = millis();
  int livres = 0;
  while (millis() - t0 < maxMs) {
    delay(INTERVALO_PING_MS);
    int d = lerCrua();
    livres = (d >= DIST_LIVRE) ? livres + 1 : 0;
    if (livres >= LEITURAS_LIVRES && millis() - t0 >= (unsigned long)T_GIRO_MIN_MS) {
      delay(T_MARGEM_MS);              // um pouco além, pra não raspar na quina do obstáculo
      parar();
      Serial.print(F("livre em ")); Serial.print(millis() - t0); Serial.println(F(" ms"));
      return true;
    }
  }
  parar();
  Serial.println(F("nada livre"));
  return false;
}

bool registrarDesvio() {                 // true = 3 desvios em 6 s (preso)
  unsigned long agora = millis();
  if (agora - inicioJanela > JANELA_PRESO_MS) { inicioJanela = agora; desviosRecentes = 0; }
  desviosRecentes++;
  if (desviosRecentes >= MAX_DESVIOS_SEGUIDOS) { desviosRecentes = 0; inicioJanela = agora; return true; }
  return false;
}

void desviar() {
  bool preso = registrarDesvio();
  Serial.print(F("Obstaculo a ")); Serial.print(distancia); Serial.println(preso ? F(" cm - PRESO, fuga") : F(" cm"));
  parar();
  delay(T_PAUSA_MS);

  if (preso) {                           // preso: ré em arco longa pro lado oposto e troca o lado preferido
    ladoDireita = !ladoDireita;
    reArco(ladoDireita, VEL_MANOBRA);
    delay(2 * T_RE_ARCO_MS);
  } else {
    re(VEL_MANOBRA);
    delay(T_RE_MS);
  }

  // 1) lado preferido
  if (girarAteLivre(ladoDireita, T_GIRO_MAX_MS)) { medirParado(); return; }

  // 2) ré em arco pro outro lado (tira o nariz da parede) e tenta o outro lado
  reArco(!ladoDireita, VEL_MANOBRA);
  delay(T_RE_ARCO_MS);
  if (girarAteLivre(!ladoDireita, T_GIRO_MAX_MS)) { ladoDireita = !ladoDireita; medirParado(); return; }

  // 3) nada livre em lugar nenhum: meia-volta e vai
  Serial.println(F("  meia-volta"));
  girar(ladoDireita, VEL_MANOBRA);
  delay(T_MEIA_VOLTA_MS);
  medirParado();
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);
  Serial.println(F("SMARS - desvio de obstaculos v3"));
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_TRIG, LOW);
  parar();
  delay(500);
}

void loop() {
  atualizarDistancia();

  if (distancia <= DIST_PARAR) { desviar(); return; }

  int vel = VEL_CRUZEIRO;
  if (distancia < DIST_REDUZIR) vel = map(distancia, DIST_PARAR, DIST_REDUZIR, VEL_MINIMA, VEL_CRUZEIRO);
  frente(vel);

  if (millis() - ultimoLog >= 250) {
    ultimoLog = millis();
    Serial.print(distancia); Serial.print(F(" cm  vel=")); Serial.println(vel);
  }
}
