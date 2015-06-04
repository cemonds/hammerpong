/* UART Example, any character received on either the real
   serial port, or USB serial (or emulated serial to the
   Arduino Serial Monitor when using non-serial USB types)
   is printed as a message to both ports.

   This example code is in the public domain.
*/

// set this to the hardware serial port you wish to use
#define HWSERIAL Serial2
#define SERIAL_MODE_PIN 11
const int BUTTON_PIN = 17;

void setup() {
  Serial.begin(9600);
  HWSERIAL.begin(38400);
  pinMode(SERIAL_MODE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

byte value = 0;
void loop() {

  delay(10);
  digitalWrite(SERIAL_MODE_PIN, LOW);
  HWSERIAL.write(++value);
  digitalWrite(SERIAL_MODE_PIN, HIGH);
  if (digitalRead(BUTTON_PIN) == LOW) {
    value = 0; 
    Serial.println("reset");
  }
}

