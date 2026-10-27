// Výpis QTR-16RC každých 10 s
const uint8_t QTR_PINS[16] = {36, 37, 35, 38, 34, 39, 33, 40,
                              32, 25, 31, 26, 30, 27, 29, 28};  // RC01..RC16
const uint16_t TIMEOUT_US = 2500;
const uint32_t PRINT_MS = 10000;

// vals[i] = doba vybíjení v µs (TIMEOUT_US = nevybilo se = černá)
void readQTR(uint16_t *vals) {
  uint16_t done = 0;

  for (int i = 0; i < 16; i++) {
    pinMode(QTR_PINS[i], OUTPUT);
    digitalWrite(QTR_PINS[i], HIGH);   // nabít kondenzátor
    vals[i] = TIMEOUT_US;
  }
  delayMicroseconds(20);

  for (int i = 0; i < 16; i++) pinMode(QTR_PINS[i], INPUT);

  uint32_t t0 = micros();
  uint32_t dt;
  while ((dt = micros() - t0) < TIMEOUT_US && done != 0xFFFF) {
    for (int i = 0; i < 16; i++) {
      if (!(done & (1 << i)) && !digitalRead(QTR_PINS[i])) {
        vals[i] = dt;
        done |= (1 << i);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  uint16_t v[16];
  readQTR(v);

  for (int i = 0; i < 16; i++) Serial.printf("%4d ", i + 1);
  Serial.println();
  for (int i = 0; i < 16; i++) Serial.printf("%4u ", v[i]);
  Serial.println("\n");

  delay(PRINT_MS);
}