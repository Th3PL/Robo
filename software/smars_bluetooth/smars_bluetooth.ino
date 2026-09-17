/*
  SMARS - controle por Bluetooth (app "Arduino Bluetooth RC Car" ou similar)

  Comandos recebidos pelo Bluetooth:
    F/B/L/R  frente / ré / esquerda / direita       S  parar
    G/I      frente-esquerda / frente-direita       H/J  ré-esquerda / ré-direita
    0-9, q   velocidade (slider do app): 0 = 10%, 9 = 90%, q = 100%

  Força do motor: setSpeed(255) já é o máximo em software. A força real depende da tensão
  que chega ao motor (o shield L293D perde ~1,5 V). Alimente o shield com 7,4 V (2x18650) ou 6xAA.
*/

#include <AFMotor.h>
#include <SoftwareSerial.h>

SoftwareSerial bt(9, 10);   // RX, TX  (RX do módulo no pino Servo_1, TX no Servo_2)
AF_DCMotor motorE(1);       // esquerdo em M1
AF_DCMotor motorD(2);       // direito em M2

const int VEL_MAX_E = 255;  // abaixe o motor mais forte se o carrinho puxa pra um lado
const int VEL_MAX_D = 255;
const int VEL_CURVA_INTERNA = 90;   // velocidade da roda de dentro nas diagonais (G/I/H/J)

int vel = 255;              // velocidade atual (0-255), muda com o slider do app

void aplicar(int e, int d, uint8_t dirE, uint8_t dirD) {
  motorE.setSpeed(map(constrain(e, 0, 255), 0, 255, 0, VEL_MAX_E));
  motorD.setSpeed(map(constrain(d, 0, 255), 0, 255, 0, VEL_MAX_D));
  motorE.run(dirE);
  motorD.run(dirD);
}

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  aplicar(0, 0, RELEASE, RELEASE);
  Serial.println(F("SMARS Bluetooth pronto"));
}

void loop() {
  if (!bt.available()) return;          // sem isso, read() devolve -1 a cada volta do loop
  char c = bt.read();

  // slider de velocidade do app: '0'..'9' e 'q'
  if (c >= '0' && c <= '9') { vel = map(c - '0', 0, 9, 25, 230); return; }
  if (c == 'q' || c == 'Q') { vel = 255; return; }

  int vi = vel * VEL_CURVA_INTERNA / 255;   // roda de dentro nas diagonais, proporcional
  switch (c) {
    case 'F': case 'f': Serial.println(F("frente"));   aplicar(vel, vel, FORWARD,  FORWARD);  break;
    case 'B': case 'b': Serial.println(F("re"));       aplicar(vel, vel, BACKWARD, BACKWARD); break;
    case 'L': case 'l': Serial.println(F("esquerda")); aplicar(vel, vel, BACKWARD, FORWARD);  break;
    case 'R': case 'r': Serial.println(F("direita"));  aplicar(vel, vel, FORWARD,  BACKWARD); break;
    case 'G': case 'g': aplicar(vi,  vel, FORWARD,  FORWARD);  break;   // frente-esquerda
    case 'I': case 'i': aplicar(vel, vi,  FORWARD,  FORWARD);  break;   // frente-direita
    case 'H': case 'h': aplicar(vi,  vel, BACKWARD, BACKWARD); break;   // ré-esquerda
    case 'J': case 'j': aplicar(vel, vi,  BACKWARD, BACKWARD); break;   // ré-direita
    case 'S': case 's': case 'D': case 'd':
      Serial.println(F("parar")); aplicar(0, 0, RELEASE, RELEASE); break;
  }
}
