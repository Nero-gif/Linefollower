// Test motorů s omezením na max 6 V - TB6612FNG na Teensy 4.1
const int PWMA = 1, AIN2 = 2, AIN1 = 3, STBY = 4, BIN1 = 5, BIN2 = 6, PWMB = 7;
const int BATT_SENSE = 41;            // A17

const float R_TOP = 33000.0;          // R3
const float R_BOT = 10000.0;          // R4
const float ADC_VREF = 3.3;
const float CAL = 7.3 / 6.8;               // doladit podle multimetru - naměřená  / vypsaná

const float MOTOR_VMAX = 6.0;         // tvrdý strop napětí na motoru
const float TEST_V = 3.0;             // napětí při testu (klidně zvedni až na 6.0)
const float VBAT_MIN = 4.5;           // pod tím je vypínač vypnutý / jedeš jen z USB

const int PWM_BITS = 12;
const int PWM_MAX = (1 << PWM_BITS) - 1;

const int ADC_BITS = 12;

float readBattery() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(BATT_SENSE);
  float vadc = (sum / 16.0) / ((2^ADC_BITS) - 1) * ADC_VREF;      // 12bit ADC
  return vadc * (R_TOP + R_BOT) / R_BOT * CAL;
}

// volts: -6..+6 V na motoru (znaménko = směr)
void setMotor(int in1, int in2, int pwmPin, float volts, float vbat) {
  volts = constrain(volts, -MOTOR_VMAX, MOTOR_VMAX);
  float duty = fabsf(volts) / vbat;
  if (duty > 1.0) duty = 1.0;                         // baterie slabší než požadavek
  digitalWrite(in1, volts > 0);
  digitalWrite(in2, volts < 0);
  analogWrite(pwmPin, (int)(duty * PWM_MAX));
}

void stopAll() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
}

// Jede zadanou dobu a každých 10 ms přepočítá střídu podle aktuální baterie
// (při rozjezdu motorů baterie poklesne).
bool driveFor(float vA, float vB, uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    float vbat = readBattery();
    if (vbat < VBAT_MIN) { stopAll(); return false; }
    setMotor(AIN1, AIN2, PWMA, vA, vbat);
    setMotor(BIN1, BIN2, PWMB, vB, vbat);
    delay(10);
  }
  stopAll();
  delay(500);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  analogReadResolution(ADC_BITS));
  analogWriteResolution(PWM_BITS);
  analogWriteFrequency(PWMA, 20000);
  analogWriteFrequency(PWMB, 20000);

  stopAll();
  digitalWrite(STBY, HIGH);
  delay(2000);                                         // čas na položení robota

  float vbat = readBattery();
  Serial.printf("Vbat = %.2f V, max stridy pri %.1f V na motoru: %.0f %%\n",
                vbat, MOTOR_VMAX, min(1.0f, MOTOR_VMAX / vbat) * 100);
  if (vbat < VBAT_MIN) {
    Serial.println("Baterie nepripojena / vypinac vypnuty, motory nepojedou.");
    return;
  }

  driveFor( TEST_V, 0,       1000);   // A dopředu
  driveFor(-TEST_V, 0,       1000);   // A dozadu
  driveFor( 0,      TEST_V,  1000);   // B dopředu
  driveFor( 0,     -TEST_V,  1000);   // B dozadu
  driveFor( TEST_V, TEST_V,  1000);   // oba dopředu
  driveFor(-TEST_V,-TEST_V,  1000);   // oba dozadu

  digitalWrite(STBY, LOW);
}

void loop() {}