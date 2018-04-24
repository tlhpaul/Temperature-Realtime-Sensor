/*  Arduino DC Motor Control - PWM | H-Bridge | L298N  -  Example 01
    by Dejan Nedelkovski, www.HowToMechatronics.com
*/
#define enA 9
#define in1 6
#define in2 7
#define button 4

int rotDirection = 0;
int pressed = false;
void setup() {
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);

  // Set initial rotation direction
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
}
void loop() {
  int pwmOutput = 255;
  analogWrite(enA, pwmOutput); // Send PWM signal to L298N Enable pin

    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    rotDirection = 0;
    delay(20);
  }

