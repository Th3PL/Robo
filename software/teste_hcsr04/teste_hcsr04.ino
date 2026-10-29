// Teste do sensor ultrassônico HC-SR04: mostra a distância no Monitor Serial (115200)
// Funciona com o cabo na entrada UART ou na entrada USB da placa.
//
// Ligações: VCC no 5V, GND no G, Trig no GPIO1,
// Echo no GPIO2 passando pelo divisor (1 kΩ em série + 1,5 kΩ para o GND)

#include <stdarg.h>

// As duas entradas da placa: USB (nativa do ESP32-S3) e UART (pelo chip conversor)
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

const int TRIG = 1;
const int ECHO = 2;

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

// Mede a distância em cm; devolve -1 se o eco não voltar
float medirCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);  // pulso de 10 µs dispara a medição
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseIn(ECHO, HIGH, 25000);  // espera o eco por até 25 ms (~4 m)
  if (us == 0) return -1;
  return us * 0.0343f / 2;  // som: 0,0343 cm por µs, ida e volta
}

void setup() {
  pinMode(TRIG, OUTPUT);
  digitalWrite(TRIG, LOW);
  pinMode(ECHO, INPUT);
  PORTA_UART.begin(115200);
#if DUAS_PORTAS
  PORTA_USB.begin(115200);
  PORTA_USB.setTxTimeoutMs(0);  // sem computador ligado, não trava esperando
#endif
  delay(1500);
  dizer("Teste do HC-SR04: uma leitura a cada 200 ms\n");
}

void loop() {
  float cm = medirCm();
  if (cm < 0) {
    dizer("sem eco: nada à frente até ~4 m, ou fio solto\n");
  } else {
    int n = constrain((int)(cm / 5), 0, 40);  // uma barrinha a cada 5 cm
    char barra[41];
    memset(barra, '#', n);
    barra[n] = 0;
    dizer("%6.1f cm  %s\n", cm, barra);
  }
  delay(200);
}
