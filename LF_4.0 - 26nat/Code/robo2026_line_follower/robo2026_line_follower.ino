/*
  ROBO 2026 - sledovač čáry s PID regulací
  Deska: Teensy 4.1 (Teensyduino), driver TB6612FNG, 16kanálový RC reflexní senzor (QTR-16RC)

  Průběh:
   1) zapnutí -> kalibrace (vozítko se krátce natáčí vlevo/vpravo nad čárou) a uložení do EEPROM
   2) čekání START_DELAY_MS (postav vozítko před čáru START/CÍL)
   3) jízda: PID, 1. přejetí START/CÍL = start, 2. přejetí = cíl -> plynulé zastavení za čárou

  Režimy (MODE):
   MODE_RUN          normální jízda
   MODE_SENSOR_TEST  výpis senzorů na USB sériovou linku (kontrola pořadí 0..15)
   MODE_MOTOR_TEST   test motorů A a B (kontrola směru a strany)
*/

#include <Arduino.h>
#include <EEPROM.h>

#define MODE_RUN         0
#define MODE_SENSOR_TEST 1
#define MODE_MOTOR_TEST  2
#define MODE MODE_RUN

#define CALIBRATE_ON_BOOT 1   // 1 = kalibrace při startu a uložení, 0 = načíst uloženou kalibraci z EEPROM

// ===================== PINY (ze schématu - ZKONTROLUJ!) =====================
// TB6612FNG
const uint8_t PIN_PWMA = 1, PIN_AIN2 = 2, PIN_AIN1 = 3;
const uint8_t PIN_STBY = 4;
const uint8_t PIN_BIN1 = 5, PIN_BIN2 = 6, PIN_PWMB = 7;

// Strana motorů a směr otáčení
#define MOTOR_A_IS_LEFT 0     // 1: motor A (M1) je levý, 0: pravý
#define INVERT_A 1            // 1: otočí směr motoru A
#define INVERT_B 0            // 1: otočí směr motoru B

// Čárový senzor QTR-16RC: index 0 = RC01 ... 15 = RC16 (piny z test_sensor.ino)
// Pokud je RC01 vpravo, otoč pořadí pole (nebo ověř v MODE_SENSOR_TEST)
const uint8_t N = 16;
uint8_t QTR_PINS[N] = {29, 28, 30, 27, 31, 26, 32, 25,
                       33, 40, 34, 39, 35, 38, 36, 37};   // RC01..RC16
const uint16_t QTR_TIMEOUT_US = 3000;

// Měření baterie (dělič 33k / 10k na pinu 41)
const uint8_t PIN_VBAT = 41;
const float VBAT_DIV = (33.0f + 10.0f) / 10.0f;
const float VBAT_NOMINAL = 6.0f;      // napětí, pro které ladíš rychlosti

// ===================== PARAMETRY JÍZDY =====================
float Kp = 480.0f;            // [rychlost / jednotku senzoru]
float Ki = 0.0f;
float Kd = 2.5f;
float BASE_SPEED = 900.0f;   // 0..1000
float SPEED_DROP = 300.0f;   // o kolik se zpomalí při max. odchylce
const float MIN_SPEED = 220.0f;
const float ACCEL = 1200.0f; // zrychlení [jednotek/s] (nízké kvůli vodě)
const float DECEL = 2500.0f;
const float D_FILTER = 0.25f;
const float U_MAX = 700.0f;
const float I_MAX = 200.0f;

const float THR_LINE = 0.45f;           // práh detekce čáry na normalizované hodnotě
const int   START_LINE_MIN_W = 13;      // šířka souvislého úseku [senzory] = čára START/CÍL
const int   START_LINE_END_W = 8;
const uint32_t MIN_EVENT_GAP_MS = 1000; // min. doba mezi startem a cílem

const uint32_t GAP_HOLD_MS = 350;       // přerušovaná čára: držení posledního řízení
const float    GAP_SPEED = 350.0f;
const uint32_t LOST_STOP_MS = 2500;     // ztráta čáry delší než toto -> stop
const float    SEARCH_SPEED = 250.0f;

const uint32_t STOP_RAMP_MS = 900;      // plynulé zastavení po cíli
const uint32_t START_DELAY_MS = 4000;
const int      CAL_SPEED = 360;
const uint32_t CAL_TIME_MS = 4300;
const uint32_t CAL_SWITCH_MS = 600;

// ===================== GLOBÁLNÍ STAV =====================
uint16_t calMin[N], calMax[N];
float vScale = 1.0f;

enum State { ST_RUN, ST_STOPPING, ST_DONE };
State state = ST_RUN;

// ===================== MOTORY =====================
static void driveMotor(uint8_t in1, uint8_t in2, uint8_t pwm, int speed, bool inv) {
  if (inv) speed = -speed;
  speed = constrain(speed, -1000, 1000);
  if (speed > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (speed < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else                { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); }
  analogWrite(pwm, abs(speed));   // 10bit PWM
}

// left/right v rozsahu -1000..1000 (před kompenzací baterie)
static void setMotors(float left, float right) {
  int l = (int)constrain(left * vScale, -1000.0f, 1000.0f);
  int r = (int)constrain(right * vScale, -1000.0f, 1000.0f);
#if MOTOR_A_IS_LEFT
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, l, INVERT_A);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, r, INVERT_B);
#else
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, r, INVERT_A);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, l, INVERT_B);
#endif
}

static void brakeMotors() {
  digitalWrite(PIN_AIN1, HIGH); digitalWrite(PIN_AIN2, HIGH); analogWrite(PIN_PWMA, 1023);
  digitalWrite(PIN_BIN1, HIGH); digitalWrite(PIN_BIN2, HIGH); analogWrite(PIN_PWMB, 1023);
}

// ===================== BATERIE =====================
static float readBatteryV() {
  analogReadResolution(12);
  analogReadAveraging(8);
  float adc = analogRead(PIN_VBAT);
  return adc * 3.3f / 4095.0f * VBAT_DIV;
}

static void updateBatteryScale() {
  float v = readBatteryV();
  if (v < 3.0f) { vScale = 1.0f; return; }   // měření nedává smysl -> bez kompenzace
  vScale = constrain(VBAT_NOMINAL / v, 0.7f, 1.4f);
}

// ===================== SENZORY =====================
static void readRaw(uint16_t *raw) {
  for (uint8_t i = 0; i < N; i++) {
    pinMode(QTR_PINS[i], OUTPUT);
    digitalWrite(QTR_PINS[i], HIGH);
    raw[i] = QTR_TIMEOUT_US;
  }
  delayMicroseconds(15);
  for (uint8_t i = 0; i < N; i++) pinMode(QTR_PINS[i], INPUT);

  uint32_t t0 = micros(), t;
  uint8_t remaining = N;
  while (remaining && (t = micros() - t0) < QTR_TIMEOUT_US) {
    for (uint8_t i = 0; i < N; i++) {
      if (raw[i] == QTR_TIMEOUT_US && !digitalRead(QTR_PINS[i])) {
        raw[i] = t;
        remaining--;
      }
    }
  }
}

// čára = 1.0, bílá = 0.0
static void readNorm(float *v) {
  uint16_t raw[N];
  readRaw(raw);
  for (uint8_t i = 0; i < N; i++) {
    float range = (float)calMax[i] - (float)calMin[i];
    if (range < 50.0f) v[i] = 0.0f;
    else v[i] = constrain(((float)raw[i] - (float)calMin[i]) / range, 0.0f, 1.0f);
  }
}

static void saveCal() {
  EEPROM.put(0, (uint32_t)0xC0FFEE26);
  EEPROM.put(4, calMin);
  EEPROM.put(4 + sizeof(calMin), calMax);
}

static bool loadCal() {
  uint32_t magic;
  EEPROM.get(0, magic);
  if (magic != 0xC0FFEE26) return false;
  EEPROM.get(4, calMin);
  EEPROM.get(4 + sizeof(calMin), calMax);
  return true;
}

static void calibrate() {
  for (uint8_t i = 0; i < N; i++) { calMin[i] = 65535; calMax[i] = 0; }
  uint32_t t0 = millis();
  uint16_t raw[N];
  while (millis() - t0 < CAL_TIME_MS) {
    bool other = (((millis() - t0) + CAL_SWITCH_MS / 2) / CAL_SWITCH_MS) & 1;
    int d = other ? -1 : 1;
    setMotors(d * CAL_SPEED, -d * CAL_SPEED);
    readRaw(raw);
    for (uint8_t i = 0; i < N; i++) {
      if (raw[i] < calMin[i]) calMin[i] = raw[i];
      if (raw[i] > calMax[i]) calMax[i] = raw[i];
    }
  }
  brakeMotors();
  saveCal();
}

// ===================== DETEKCE ČÁRY =====================
struct Blob { int s, e; float c; };
static Blob gBlobs[8];

// Pozor: typ Blob se nesmí objevit v podpisu funkce, Arduino IDE by pro něj
// vygenerovalo prototyp dřív, než je struktura deklarována.
static int findBlobs(const float *v, int maxn, int &widest) {
  int n = 0; widest = 0;
  int i = 0;
  while (i < N) {
    if (v[i] >= THR_LINE) {
      int s = i;
      while (i < N && v[i] >= THR_LINE) i++;
      int e = i - 1;
      float sw = 0, swx = 0;
      for (int k = max(0, s - 1); k <= min((int)N - 1, e + 1); k++) { sw += v[k]; swx += v[k] * k; }
      if (n < maxn && sw > 0) { gBlobs[n].s = s; gBlobs[n].e = e; gBlobs[n].c = swx / sw; n++; }
      if (e - s + 1 > widest) widest = e - s + 1;
    } else i++;
  }
  return n;
}

// ===================== SÉRIOVÉ LADĚNÍ (jen před startem) =====================
static void handleSerialTuning() {
  while (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    int sp = s.indexOf(' ');
    if (sp < 0) continue;
    String k = s.substring(0, sp);
    float val = s.substring(sp + 1).toFloat();
    if (k == "kp") Kp = val;
    else if (k == "ki") Ki = val;
    else if (k == "kd") Kd = val;
    else if (k == "speed") BASE_SPEED = val;
    else if (k == "drop") SPEED_DROP = val;
    Serial.printf("Kp=%.2f Ki=%.3f Kd=%.2f speed=%.0f drop=%.0f\n", Kp, Ki, Kd, BASE_SPEED, SPEED_DROP);
  }
}

// ===================== JÍZDA =====================
static uint32_t tPrev = 0;
static float vCur = 0, prevErr = 0, dFilt = 0, iSum = 0, lastU = 0, lastC = 7.5f;
static float lostMs = 0;
static bool inLine = false;
static int lineEvents = 0;
static uint32_t lastEventMs = 0, stopStartMs = 0;
static float stopV = 0;
static uint32_t batMs = 0;

static void runStep() {
  uint32_t now = micros();
  float dt = (now - tPrev) * 1e-6f;
  tPrev = now;
  if (dt <= 0.0f || dt > 0.05f) dt = 0.003f;
  uint32_t ms = millis();

  if (ms - batMs > 250) { batMs = ms; updateBatteryScale(); }

  if (state == ST_STOPPING) {
    uint32_t t = ms - stopStartMs;
    if (t >= STOP_RAMP_MS) { brakeMotors(); state = ST_DONE; return; }
    float v = stopV * (1.0f - (float)t / STOP_RAMP_MS);
    setMotors(v, v);
    return;
  }
  if (state == ST_DONE) { brakeMotors(); return; }

  float v[N];
  readNorm(v);
  Blob *blobs = gBlobs;
  int widest;
  int nb = findBlobs(v, 8, widest);

  // --- čára START/CÍL ---
  bool onLine = widest >= START_LINE_MIN_W;
  if (onLine && !inLine) {
    inLine = true;
    if (lineEvents == 0 || ms - lastEventMs > MIN_EVENT_GAP_MS) {
      lineEvents++;
      lastEventMs = ms;
      if (lineEvents >= 2) {            // cíl
        state = ST_STOPPING;
        stopStartMs = ms;
        stopV = vCur;
        return;
      }
    }
  } else if (inLine && widest < START_LINE_END_W) {
    inLine = false;
  }

  float u = 0, vTarget = BASE_SPEED;
  bool searching = false;
  float leftCmd, rightCmd;

  if (onLine) {
    // široká příčná čára -> rovně
    u = 0; prevErr = 0; lostMs = 0;
    vTarget = GAP_SPEED;
  } else if (nb > 0) {
    // vyber úsek nejblíž poslední poloze (křižovatka -> rovně)
    int best = 0;
    for (int i = 1; i < nb; i++)
      if (fabsf(blobs[i].c - lastC) < fabsf(blobs[best].c - lastC)) best = i;
    float c = blobs[best].c;
    lastC = c;
    lostMs = 0;

    float err = c - 7.5f;               // + = čára vpravo
    iSum = constrain(iSum + Ki * err * dt, -I_MAX, I_MAX);
    float dRaw = (err - prevErr) / dt;
    dFilt += D_FILTER * (dRaw - dFilt);
    prevErr = err;

    u = Kp * err + iSum + Kd * dFilt;
    u = constrain(u, -U_MAX, U_MAX);
    lastU = u;

    float ratio = min(fabsf(err) / 7.5f, 1.0f);
    vTarget = max(BASE_SPEED - SPEED_DROP * ratio, MIN_SPEED);
  } else {
    // čára ztracena
    lostMs += dt * 1000.0f;
    if (lostMs < GAP_HOLD_MS) {          // přerušovaná čára
      u = lastU * 0.6f;
      vTarget = GAP_SPEED;
    } else if (lostMs < LOST_STOP_MS) {  // hledání čáry
      searching = true;
    } else {
      brakeMotors();
      state = ST_DONE;
      return;
    }
  }

  if (searching) {
    float dir = (lastC >= 7.5f) ? 1.0f : -1.0f;
    vCur = 0;
    leftCmd = dir * SEARCH_SPEED;
    rightCmd = -dir * SEARCH_SPEED;
  } else {
    // rampa rychlosti
    if (vTarget > vCur) vCur = min(vTarget, vCur + ACCEL * dt);
    else                vCur = max(vTarget, vCur - DECEL * dt);
    leftCmd  = vCur + u;
    rightCmd = vCur - u;
  }
  leftCmd  = constrain(leftCmd, -300.0f, 1000.0f);
  rightCmd = constrain(rightCmd, -300.0f, 1000.0f);
  setMotors(leftCmd, rightCmd);
}

// ===================== TESTY =====================
static void sensorTest() {
  float v[N];
  readNorm(v);
  for (uint8_t i = 0; i < N; i++) Serial.print(v[i] > THR_LINE ? '#' : '.');
  Serial.print("  ");
  for (uint8_t i = 0; i < N; i++) { Serial.print(v[i], 1); Serial.print(' '); }
  Serial.printf(" | Vbat=%.2f V\n", readBatteryV());
  delay(100);
}

static void motorTest() {
  Serial.println("Motor A dopredu"); driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, 400, INVERT_A); delay(1000);
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, 0, false); delay(500);
  Serial.println("Motor B dopredu"); driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, 400, INVERT_B); delay(1000);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, 0, false); delay(2000);
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);

  uint8_t outs[] = {PIN_PWMA, PIN_AIN1, PIN_AIN2, PIN_STBY, PIN_BIN1, PIN_BIN2, PIN_PWMB};
  for (uint8_t p : outs) pinMode(p, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);
  analogWriteResolution(10);
  analogWriteFrequency(PIN_PWMA, 20000);
  analogWriteFrequency(PIN_PWMB, 20000);
  brakeMotors();

  updateBatteryScale();

#if MODE == MODE_RUN
  delay(1500);
#if CALIBRATE_ON_BOOT
  calibrate();
#else
  if (!loadCal()) calibrate();
#endif
  // čekání na postavení vozítka před čáru START/CÍL
  uint32_t t0 = millis();
  while (millis() - t0 < START_DELAY_MS) handleSerialTuning();
  tPrev = micros();
  batMs = millis();
#elif MODE == MODE_SENSOR_TEST
  if (!loadCal()) calibrate();
#endif
}

void loop() {
#if MODE == MODE_RUN
  runStep();
#elif MODE == MODE_SENSOR_TEST
  sensorTest();
#else
  motorTest();
#endif
}