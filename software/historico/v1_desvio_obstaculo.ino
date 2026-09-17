// VERSÃO INICIAL (histórico) - desvio de obstáculo, base de exemplo do SMARS.
// Mantida no repositório apenas como registro da evolução. Problemas desta versão:
//  - pulseIn sem timeout: sem eco devolve 0 -> "0 <= 7" -> carrinho dava ré sem obstáculo
//  - int echoTime estoura acima de 32 ms
//  - reage só a 7 cm, em velocidade máxima: encostava antes de reagir
//  - sempre virava para o mesmo lado por 500 ms, às cegas: ficava preso em cantos
// Versões seguintes: software/smars_desvio/ (v2 e v3) e software/smars_completo/

#include <AFMotor.h>

AF_DCMotor MotorL(1);  // Motor for drive Left on M1
AF_DCMotor MotorR(2);  // Motor for drive Right on M2

//ultrasonic setup:
 const int trigPin = A0; // trig pin connected to Arduino's pin A0
 const int echoPin = A1; // echo pin connected to Arduino's pin A1

// defines variables
long duration;
int distanceCm=0;

void setup() {
  Serial.begin(115200);      // set up Serial library at 115200 bps
  Serial.println("SMARS Obstacle Avoidance Mod");
  pinMode(trigPin, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);   // Sets the echoPin as an Input

  // Set the speed to start, from 0 (off) to 255 (max speed)
  // sometimes the motors don't have the same speed, so use these values tomake your SMARS move straight
  MotorL.setSpeed(255);
  MotorR.setSpeed(255);
  // turn on motor
  MotorL.run(RELEASE);
  MotorR.run(RELEASE);
}

// main program loop
void loop() {
 distanceCm=getDistance(); // variable to store the distance measured by the sensor

 Serial.println(distanceCm); // print the distance that was measured

  //if the distance is less than 7cm, SMARS will go backward for 1 second, and turn right for 1 second
  if(distanceCm <= 7){
    MotorL.run(BACKWARD);
    MotorR.run(BACKWARD);
    delay(500);
    MotorL.run(FORWARD);
    MotorR.run(BACKWARD);
    delay(500);
  }
  else {
    MotorL.run(FORWARD); //otherwise it will continue forward
    MotorR.run(FORWARD);
  }
}

//RETURNS THE DISTANCE MEASURED BY THE HC-SR04 DISTANCE SENSOR
int getDistance() {
  int echoTime;                   //variable to store the time it takes for a ping to bounce off an object
  int calcualtedDistance;         //variable to store the distance calculated from the echo time

  //send out an ultrasonic pulse that's 10ms long
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  echoTime = pulseIn(echoPin, HIGH);      //use the pulsein command to see how long it takes for the
                                          //pulse to bounce back to the sensor

  calcualtedDistance = echoTime / 58.26;  //calculate the distance of the object that reflected the pulse (half the bounce time multiplied by the speed of sound)
  return calcualtedDistance;              //send back the distance that was calculated
}
