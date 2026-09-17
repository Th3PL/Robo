// VERSÃO INICIAL (histórico) - controle Bluetooth, base de exemplo do SMARS.
// Mantida no repositório apenas como registro da evolução. Problemas desta versão:
//  - mySerial.read() sem checar available(): devolve -1 a cada volta do loop
//  - velocidade fixa em 250, sem uso do slider do app
//  - 'J' tratado como parar (no app é ré-direita); sem diagonais
// Versão atual: software/smars_bluetooth/ e software/smars_completo/

#include <AFMotor.h>  //Adafruit Motor drive library might need to install.
#include <SoftwareSerial.h> //To declare new RX-TX pins
SoftwareSerial mySerial(9, 10);// RX, TX  // RX-pin on bluetooth goes to Servo_1 on motor driver and TX pin goes to servo_2
AF_DCMotor motor1(1); //Motor slot M1 on the board
AF_DCMotor motor2(2); //Motor slor M2 on the board
char bt='S';
void setup()
{
  Serial.begin(9600);  //start serial communication through USB serial.
  mySerial.begin(9600);// start serial communication using bluetooth.
  motor1.setSpeed(250);//Set the speed of motor 0 is lowest 255 is maximum.
  motor2.setSpeed(250); // For some motors lower value might not work so you have to do lots of trial and error to find the sweet spot.
}
void loop() {
bt=mySerial.read(); // Read in coming data from the phone app.
if(bt=='f'|| bt=='F')
{
 forward();
}
if(bt=='b'|| bt=='B')
{
 backward();
}
if(bt=='L'|| bt=='l')
{
 left();
}
if(bt=='R'|| bt=='r')
{
 right();
}
if(bt=='S')
{
 Stop();
}
if(bt=='j'||bt=='J')
{
 Stop();
}
}
void forward()
{
  Serial.print("fwd");
  motor1.run(FORWARD);
  motor2.run(FORWARD);
}

void backward()
{
  Serial.print("back");
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
}
void left()
{
  Serial.print("left");
  motor1.run(FORWARD);
  motor2.run(BACKWARD);
}
void right()
{
  Serial.print("right");
  motor1.run(BACKWARD);
  motor2.run(FORWARD);
}
void Stop()
{
  Serial.print(" All motor stopped");
  motor1.run(RELEASE);
  motor2.run(RELEASE);
}
