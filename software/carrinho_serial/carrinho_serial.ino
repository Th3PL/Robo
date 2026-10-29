// Carrinho com ESP32-S3 + ponte H L9110S, controlado pelo Monitor Serial (115200)
// Funciona com o cabo na entrada UART ou na entrada USB da placa.
//
// Digite um comando e aperte Enter:
//   f  frente          t  trás (ré)
//   e  gira p/ esquerda d  gira p/ direita
//   fe curva frente-esquerda   fd curva frente-direita
//   te ré-esquerda             td ré-direita
//   p  para
//   v 150   muda a velocidade (0 a 255)
//   ?  mostra esta lista
// Um número depois do movimento é o tempo em ms: "f 1000" anda 1 s e para sozinho.

#include <stdarg.h>

// As duas entradas da placa: USB (nativa do ESP32-S3) e UART (pelo chip conversor).
// Quais objetos são quais depende da opção "USB CDC On Boot" da IDE; isto cobre os dois casos.
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

const int A_IA = 15;  // motor A, roda direita
const int A_IB = 16;
const int B_IA = 4;   // motor B, roda esquerda
const int B_IB = 5;

// Roda girando ao contrário? Troque false por true
const bool INVERTE_A = false;
const bool INVERTE_B = false;

struct Movimento { const char *nome; const char *descricao; int esq; int dir; };  // esq e dir em % da velocidade
const Movimento MOVIMENTOS[] = {
  {"f",  "frente",                100,  100},
  {"t",  "trás",                 -100, -100},
  {"e",  "gira para a esquerda",  -75,   75},
  {"d",  "gira para a direita",    75,  -75},
  {"fe", "curva frente-esquerda",  45,  100},
  {"fd", "curva frente-direita",  100,   45},
  {"te", "ré para a esquerda",    -45, -100},
  {"td", "ré para a direita",    -100,  -45},
};

int vel = 200;                 // velocidade, de 0 a 255
int fatorEsq = 0, fatorDir = 0;
int alvoEsq = 0, alvoDir = 0, atualEsq = 0, atualDir = 0;
unsigned long paraEm = 0;      // 0 = sem tempo marcado
String linha;
unsigned long ultimoCaractere = 0;

// Escreve a mensagem nas duas entradas
void dizer(const char *fmt, ...) {
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  PORTA_UART.print(buf);
#if DUAS_PORTAS
  PORTA_USB.print(buf);
#endif
}

void motor(int ia, int ib, int v, bool inverte) {
  if (inverte) v = -v;
  analogWrite(ia, v > 0 ? v : 0);
  analogWrite(ib, v < 0 ? -v : 0);
}

void aplicar(int esq, int dir) {
  fatorEsq = esq;
  fatorDir = dir;
  alvoEsq = vel * esq / 100;
  alvoDir = vel * dir / 100;
}

void parar() {
  aplicar(0, 0);
  paraEm = 0;
  dizer("-> parado\n");
}

void ajuda() {
  dizer("\nComandos (digite e aperte Enter):\n");
  for (const Movimento &m : MOVIMENTOS) dizer("  %-3s %s\n", m.nome, m.descricao);
  dizer("  p   para\n");
  dizer("  v   velocidade, ex.: v 150 (0 a 255)\n");
  dizer("  ?   esta lista\n");
  dizer("Tempo opcional em ms depois do movimento, ex.: f 1000\n");
  dizer("Velocidade atual: %d\n\n", vel);
}

void executar(String s) {
  s.trim();
  s.toLowerCase();
  if (s.length() == 0) return;
  if (s == "?") { ajuda(); return; }

  unsigned int i = 0;
  while (i < s.length() && isalpha(s[i])) i++;
  String cmd = s.substring(0, i);
  String resto = s.substring(i);
  resto.trim();
  long numero = resto.length() ? resto.toInt() : -1;

  if (cmd == "p") { parar(); return; }
  if (cmd == "v") {
    if (numero >= 0) vel = constrain(numero, 0, 255);
    aplicar(fatorEsq, fatorDir);
    dizer("-> velocidade %d\n", vel);
    return;
  }
  for (const Movimento &m : MOVIMENTOS) {
    if (cmd == m.nome) {
      aplicar(m.esq, m.dir);
      paraEm = numero > 0 ? millis() + numero : 0;
      if (numero > 0) dizer("-> %s por %ld ms (velocidade %d)\n", m.descricao, numero, vel);
      else dizer("-> %s (velocidade %d), digite p para parar\n", m.descricao, vel);
      return;
    }
  }
  dizer("Comando \"%s\" não reconhecido. Digite ? para ver a lista.\n", s.c_str());
}

// Junta os caracteres até o Enter (funciona com qualquer opção de fim de linha)
void receber(char c) {
  if (c == '\n' || c == '\r') { executar(linha); linha = ""; }
  else { linha += c; ultimoCaractere = millis(); }
}

void setup() {
  int pinos[] = {A_IA, A_IB, B_IA, B_IB};
  for (int p : pinos) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);  // tudo parado ao ligar
  }
  PORTA_UART.begin(115200);
#if DUAS_PORTAS
  PORTA_USB.begin(115200);
  PORTA_USB.setTxTimeoutMs(0);  // sem computador ligado, não trava esperando
#endif
  delay(1500);  // tempo para o Monitor Serial reconectar depois do reset
  dizer("Carrinho pronto.\n");
  ajuda();
}

void loop() {
  while (PORTA_UART.available()) receber(PORTA_UART.read());
#if DUAS_PORTAS
  while (PORTA_USB.available()) receber(PORTA_USB.read());
#endif
  if (linha.length() && millis() - ultimoCaractere > 150) { executar(linha); linha = ""; }

  if (paraEm && (long)(millis() - paraEm) >= 0) parar();

  // Rampa: a cada 10 ms aproxima a velocidade real da desejada (evita tranco)
  static unsigned long t = 0;
  if (millis() - t >= 10) {
    t = millis();
    atualEsq += constrain(alvoEsq - atualEsq, -20, 20);
    atualDir += constrain(alvoDir - atualDir, -20, 20);
    motor(A_IA, A_IB, atualDir, INVERTE_A);
    motor(B_IA, B_IB, atualEsq, INVERTE_B);
  }
}
