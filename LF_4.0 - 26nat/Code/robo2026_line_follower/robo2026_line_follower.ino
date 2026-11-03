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
   MODE_DRY_RUN      celá logika bez motorů + výpis stavu (neseš vozítko rukou po dráze)
*/

#include <Arduino.h>
#include <EEPROM.h>

#define MODE_RUN         0
#define MODE_SENSOR_TEST 1
#define MODE_MOTOR_TEST  2
#define MODE_DRY_RUN     3   // celá logika jízdy BEZ motorů + výpis stavu (neseš vozítko rukou po dráze)
#define MODE MODE_RUN

#define FAST_READ 1           // 1 = čtení senzorů skončí dřív (stačí poznat černou) -> rychlejší smyčka
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
float Kd = 5.0f;          // Td = Kd/Kp ~ 30 ms
float BASE_SPEED = 1200.0f;   // 0..1000
float SPEED_DROP = 450.0f;   // o kolik se zpomalí při max. odchylce
const float MIN_SPEED = 280.0f;
const float ACCEL = 1200.0f; // zrychlení [jednotek/s] (nízké kvůli vodě)
const float DECEL = 12000.0f;   // brzdit smí skoro okamžitě, zrychlovat opatrně (voda)
const float D_FILTER = 0.25f;
float U_REF = 350.0f;        // stálé řízení |uAvg| při kterém je plná redukce rychlosti (zatáčka)
const float U_TAU = 0.08f;   // [s] filtr střední hodnoty řízení
const float REV_RUN = 400.0f;// kolik smí vnitřní kolo couvat při ostré zatáčce

// --- pravoúhlé zatáčky ---
const int   CORNER_MIN_W = 7;           // čára dotýkající se okraje lišty a široká aspoň tolik senzorů = zatáčka
const float CORNER_CONFIRM_MS = 8.0f;  // jak dlouho musí podmínka platit (odliší šikmou čáru START/CÍL)
float CORNER_SPEED = 450.0f;            // rychlost při přiblížení k zatáčce
const float CORNER_TURN_V = 200.0f;     // dopředná složka při otáčení
float CORNER_TURN_U = 450.0f;           // rozdíl rychlostí kol při otáčení (větší = ostřejší)
const float CORNER_EXIT_C = 2.5f;       // otáčení končí, když je čára do ±2,5 senzoru od středu
const float CORNER_LOST_C = 5.5f;       // čára zmizela a naposledy byla dál než 4 senzory od středu
const uint32_t CORNER_MIN_MS = 60;
const uint32_t CORNER_MAX_MS = 1500;
const int   CORNER_JUMP = 4;            // náhlý nárůst šířky čáry o tolik senzorů = pravoúhlá zatáčka (vodorovná čára se objeví naráz)
const int   CORNER_MEM_W = 5;           // čára u kraje lišty širší než tolik senzorů se pamatuje pro případ, že zatáčku přejedu
const uint32_t CORNER_MEM_MS = 250;     // jak dlouho po ní platí "čára zmizela = zatáčka"
uint32_t CORNER_FWD_MS = 40;            // před otáčením popojeď rovně (osa kol se dostane k zatáčce); při vysoké rychlosti se zkracuje
// --- zóna zpomalení mezi zatáčkami: od dokončení zatáčky č. ZONE_AFTER do dokončení zatáčky č. ZONE_UNTIL ---
float ZONE_SPEED = 1000.0f;              // max. rychlost v zóně
int   ZONE_AFTER = 2;                   // zpomal po dokončení 2. zatáčky
int   ZONE_UNTIL = 3;                   // znovu zrychli po dokončení 3. zatáčky (ZONE_AFTER >= ZONE_UNTIL = zóna vypnutá)
uint32_t CORNER_BRAKE_MS = 40;          // brzdění zkratem motorů před otáčením při plné rychlosti (úměrně rychlosti před zatáčkou)

// --- křižovatka / široká čára / bílá tečka uprostřed ---
const int TRACK_MAX_W = 6;    // souvislá oblast širší než tolik senzorů = křižovatka (ne obyčejná čára)
const int CLUSTER_GAP = 4;    // bloky oddělené mezerou do tolika senzorů (bílá tečka) se berou jako jedna oblast
const float CROSS_REACQ = 4.0f;       // po křižovatce znovu převezmi čáru, pokud je do ±4 senzorů od odhadu
const uint32_t CROSS_MAX_MS = 500;    // max. doba "slepé" jízdy přes křižovatku
const float CROSS_SPEED = 1000.0f;     // rychlost při průjezdu křižovatkou
const float U_HOLD_TAU = 0.15f;       // [s] pomalý filtr řízení pro držení směru (křižovatka, přerušovaná čára)
const float GAP_U_GAIN = 1.0f;        // přerušovaná čára: kolik pomalého řízení se drží (1 = celé)
float CROSS_U_GAIN = 0.4f;            // křižovatka: kolik pomalého řízení se drží (0 = čistě rovně)
const int TRACK_WIN   = 3;    // v široké oblasti se sleduje jen okolí ±3 senzory od poslední polohy čáry
const float U_MAX = 700.0f;
const float I_MAX = 200.0f;

const float THR_LINE = 0.45f;           // práh detekce čáry na normalizované hodnotě
const int   START_LINE_MIN_W = 13;      // šířka souvislého úseku [senzory] = čára START/CÍL
const int   START_LINE_END_W = 8;
const uint32_t MIN_EVENT_GAP_MS = 700; // min. doba mezi startem a cílem

//prerusovana
const uint32_t GAP_HOLD_MS = 350;       // přerušovaná čára: držení posledního řízení
const float    GAP_SPEED = 1000.0f;
const uint32_t LOST_STOP_MS = 2500;     // ztráta čáry delší než toto -> stop
const float    SEARCH_SPEED = 250.0f;

const uint32_t STOP_RAMP_MS = 900;      // plynulé zastavení po cíli
const uint32_t START_DELAY_MS = 4000;
const int      CAL_SPEED = 360;
const uint32_t CAL_TIME_MS = 4300;
const uint32_t CAL_SWITCH_MS = 600;

// ===================== GLOBÁLNÍ STAV =====================
uint16_t calMin[N], calMax[N], cutUs[N];
const float CUT_FRAC = 0.7f;   // čtení se utne při 70 % rozsahu bílá..černá; neodpojený senzor = 1.0
float vScale = 1.0f;

enum State { ST_RUN, ST_STOPPING, ST_DONE };
State state = ST_RUN;

// ===================== MOTORY =====================
static void driveMotor(uint8_t in1, uint8_t in2, uint8_t pwm, int speed, bool inv) {
#if MODE == MODE_DRY_RUN
  return;
#endif
  if (inv) speed = -speed;
  speed = constrain(speed, -1000, 1000);
  if (speed > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (speed < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else                { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); }
  analogWrite(pwm, abs(speed));   // 10bit PWM
}

// Když jedno kolo narazí na limit, posune se i druhé: zachová se rozdíl rychlostí (= točivost)
static void limitWheels(float &l, float &r, float lo, float hi) {
  float over = max(l, r) - hi;
  if (over > 0) { l -= over; r -= over; }
  float under = lo - min(l, r);
  if (under > 0) { l += under; r += under; }
}

// left/right v rozsahu -1000..1000 (před kompenzací baterie)
static void setMotors(float left, float right) {
  float lf = left * vScale, rf = right * vScale;
  limitWheels(lf, rf, -1000.0f, 1000.0f);
  int l = (int)constrain(lf, -1000.0f, 1000.0f);
  int r = (int)constrain(rf, -1000.0f, 1000.0f);
#if MOTOR_A_IS_LEFT
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, l, INVERT_A);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, r, INVERT_B);
#else
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, r, INVERT_A);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, l, INVERT_B);
#endif
}

static void brakeMotors() {
#if MODE == MODE_DRY_RUN
  return;
#endif
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
// lim = nejdelší čas čekání pro každý senzor (nullptr = QTR_TIMEOUT_US pro všechny)
static void readRawLim(uint16_t *raw, const uint16_t *lim) {
  uint16_t maxLim = 0;
  for (uint8_t i = 0; i < N; i++) {
    pinMode(QTR_PINS[i], OUTPUT);
    digitalWrite(QTR_PINS[i], HIGH);
    raw[i] = lim ? lim[i] : QTR_TIMEOUT_US;
    if (raw[i] > maxLim) maxLim = raw[i];
  }
  delayMicroseconds(15);
  for (uint8_t i = 0; i < N; i++) pinMode(QTR_PINS[i], INPUT);

  uint32_t t0 = micros(), t;
  while ((t = micros() - t0) < maxLim) {
    uint8_t pending = 0;
    for (uint8_t i = 0; i < N; i++) {
      uint16_t li = lim ? lim[i] : QTR_TIMEOUT_US;
      if (raw[i] == li && t < li) {
        pending++;
        if (!digitalRead(QTR_PINS[i])) raw[i] = t;
      }
    }
    if (!pending) break;
  }
}

static void readRaw(uint16_t *raw) { readRawLim(raw, nullptr); }

static void computeCut() {
  for (uint8_t i = 0; i < N; i++) {
    float range = (float)calMax[i] - (float)calMin[i];
    if (range < 50.0f) cutUs[i] = QTR_TIMEOUT_US;
    else cutUs[i] = (uint16_t)constrain((float)calMin[i] + CUT_FRAC * range, 1.0f, (float)QTR_TIMEOUT_US);
  }
}

// čára = 1.0, bílá = 0.0
static void readNorm(float *v) {
  uint16_t raw[N];
#if FAST_READ
  readRawLim(raw, cutUs);
#else
  readRaw(raw);
#endif
  for (uint8_t i = 0; i < N; i++) {
    float lo = calMin[i];
#if FAST_READ
    float hi = cutUs[i];
#else
    float hi = calMax[i];
#endif
    float range = hi - lo;
    if (range < 30.0f) v[i] = 0.0f;
    else v[i] = constrain(((float)raw[i] - lo) / range, 0.0f, 1.0f);
  }
}

static void saveCal() {
  EEPROM.put(0, (uint32_t)0xC0FFEE26);
  EEPROM.put(4, calMin);
  EEPROM.put(4 + sizeof(calMin), calMax);
  computeCut();
}

static bool loadCal() {
  uint32_t magic;
  EEPROM.get(0, magic);
  if (magic != 0xC0FFEE26) return false;
  EEPROM.get(4, calMin);
  EEPROM.get(4 + sizeof(calMin), calMax);
  computeCut();
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

// Jen pro MODE_DRY_RUN bez uložené kalibrace: posouvej lištu rukou přes čáru
static void handCal(uint32_t ms) {
  for (uint8_t i = 0; i < N; i++) { calMin[i] = 65535; calMax[i] = 0; }
  Serial.println("Kalibrace rukou: posouvej lištu přes čáru (i krajními senzory) ...");
  uint32_t t0 = millis();
  uint16_t raw[N];
  while (millis() - t0 < ms) {
    readRaw(raw);
    for (uint8_t i = 0; i < N; i++) {
      if (raw[i] < calMin[i]) calMin[i] = raw[i];
      if (raw[i] > calMax[i]) calMax[i] = raw[i];
    }
  }
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

// těžiště čáry z úseku senzorů s..e (včetně sousedů)
static float centroidRange(const float *v, int s, int e) {
  float sw = 0, swx = 0;
  for (int k = max(0, s - 1); k <= min((int)N - 1, e + 1); k++) { sw += v[k]; swx += v[k] * k; }
  return sw > 0 ? swx / sw : 7.5f;
}

// těžiště jen v okolí ±half od očekávané polohy (ignoruje křížící čáru mimo okno)
static bool windowCentroid(const float *v, float center, int half, float &out) {
  int cc = (int)roundf(center);
  float sw = 0, swx = 0;
  for (int k = max(0, cc - half); k <= min((int)N - 1, cc + half); k++) {
    if (v[k] < 0.2f) continue;
    sw += v[k]; swx += v[k] * k;
  }
  if (sw < 0.5f) return false;
  out = swx / sw;
  return true;
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
    else if (k == "uref") U_REF = val;
    else if (k == "cross") CROSS_U_GAIN = val;
    else if (k == "cspeed") CORNER_SPEED = val;
    else if (k == "cturn") CORNER_TURN_U = val;
    else if (k == "cfwd") CORNER_FWD_MS = (uint32_t)val;
    else if (k == "cbrake") CORNER_BRAKE_MS = (uint32_t)val;
    else if (k == "zone") ZONE_SPEED = val;
    else if (k == "zafter") ZONE_AFTER = (int)val;
    else if (k == "zuntil") ZONE_UNTIL = (int)val;
    Serial.printf("Kp=%.2f Ki=%.3f Kd=%.2f speed=%.0f drop=%.0f uref=%.0f\n", Kp, Ki, Kd, BASE_SPEED, SPEED_DROP, U_REF);
  }
}

// ===================== JÍZDA =====================
static uint32_t tPrev = 0;
static float uAvg = 0, vCur = 0, prevErr = 0, dFilt = 0, iSum = 0, lastU = 0, lastC = 7.5f;
static float lostMs = 0;
static bool inLine = false;
static int lineEvents = 0;
static uint32_t lastEventMs = 0, stopStartMs = 0;
static float stopV = 0;
static float gV[N];
static float uHold = 0, gU = 0;
static bool crossMode = false;
static uint32_t crossStartMs = 0;
static float cPred = 7.5f;
static bool cornerMode = false;
static int cornerDir = 0, cornerCandDir = 0;
static float cornerCand = 0;
static uint32_t cornerStartMs = 0;
static int prevWidest = 4, gW = 0, edgeDir = 0, edgeDirLast = 0;
static uint32_t edgeSeenMs = 0;
static float cVEntry = 0;
static int cornerCount = 0;   // počet dokončených pravoúhlých zatáček od zapnutí
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
  memcpy(gV, v, sizeof(v));
  Blob *blobs = gBlobs;
  int widest;
  int nb = findBlobs(v, 8, widest);
  int pw = prevWidest;
  prevWidest = widest;
  gW = widest;

  // --- čára START/CÍL ---
  bool onLine = widest >= START_LINE_MIN_W && v[0] >= THR_LINE && v[N-1] >= THR_LINE;   // čára přes celou lištu
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

  // --- pravoúhlá zatáčka: vzor "čára se dotýká kraje lišty, druhý kraj je bílý" ---
  bool lw = v[0] < 0.25f && v[1] < 0.25f;
  bool rw = v[N-1] < 0.25f && v[N-2] < 0.25f;
  int act = 0, first = -1, last = -1;
  for (int k = 0; k < N; k++)
    if (v[k] >= THR_LINE) { act++; if (first < 0) first = k; last = k; }
  bool noHoles = act > 0 && (last - first + 1) == act;   // díra uvnitř = křižovatka, ne zatáčka
  bool patR = nb > 0 && noHoles && !onLine && v[N-1] >= THR_LINE && lw && widest >= CORNER_MEM_W;
  bool patL = nb > 0 && noHoles && !onLine && v[0] >= THR_LINE && rw && widest >= CORNER_MEM_W;
  if (!cornerMode) {          // pamatuj si poslední takový vzor (i v "křižovatce"), kdyby se zatáčka přejela
    if (patR) { edgeDir = 1; edgeDirLast = 1; edgeSeenMs = ms; }
    else if (patL) { edgeDir = -1; edgeDirLast = -1; edgeSeenMs = ms; }
  }

  bool lostEntry = false;
  if (!cornerMode && !onLine) {
    int dirC = 0;
    bool wide = widest >= CORNER_MIN_W;
    bool abrupt = (widest - pw) >= CORNER_JUMP && lineEvents > 0;   // vodorovná čára se objevila naráz
    if (!crossMode && patR && wide) dirC = 1;
    else if (!crossMode && patL && wide) dirC = -1;
    else if (nb == 0) {
      if (edgeDir != 0 && ms - edgeSeenMs < CORNER_MEM_MS) { dirC = edgeDir; lostEntry = true; }   // zatáčku jsem přejel
      else if (fabsf(lastC - 7.5f) > CORNER_LOST_C) { dirC = (lastC > 7.5f) ? 1 : -1; lostEntry = true; }
    }
    if (dirC != 0) {
      if (dirC == cornerCandDir) cornerCand += dt * 1000.0f;
      else { cornerCandDir = dirC; cornerCand = dt * 1000.0f; }
      if (lostEntry || abrupt || cornerCand >= CORNER_CONFIRM_MS) {
        cornerMode = true; crossMode = false; cornerDir = dirC; cornerStartMs = ms; cVEntry = vCur;
        cornerCand = 0; cornerCandDir = 0; edgeDir = 0;
      }
    } else { cornerCand = 0; cornerCandDir = 0; }
  }

  // --- pravoúhlá zatáčka: 1) popojeď rovně, 2) otáčej, dokud není čára zase uprostřed ---
  if (cornerMode) {
    uint32_t t = ms - cornerStartMs;
    // čím rychleji se do zatáčky přijelo, tím déle se brzdí a tím méně se popojíždí
    float sp = constrain(cVEntry / 1000.0f, 0.0f, 1.0f);
    uint32_t tBrake = (uint32_t)(CORNER_BRAKE_MS * sp);
    uint32_t tFwd = tBrake + (uint32_t)(CORNER_FWD_MS * (1.0f - sp));
    bool centered = false;
    if (t > tFwd && !onLine && widest < CORNER_MIN_W)
      for (int i = 0; i < nb; i++)
        if (fabsf(blobs[i].c - 7.5f) < CORNER_EXIT_C) centered = true;
    if (onLine || (centered && t > tFwd + CORNER_MIN_MS) || t > CORNER_MAX_MS) {
      cornerMode = false;
      if (!onLine) cornerCount++;       // onLine = poplach (šikmá čára START/CÍL), nepočítá se
      prevErr = 0; dFilt = 0; iSum = 0; uAvg = 0; lostMs = 0;
      lastC = 7.5f;
      vCur = CORNER_TURN_V;
    } else {
      if (t < tBrake) { brakeMotors(); vCur = CORNER_SPEED; return; }   // brzdění zkratem
      float cl, cr;
      if (t < tFwd) { cl = cr = CORNER_SPEED; vCur = CORNER_SPEED; }
      else {
        cl = CORNER_TURN_V + cornerDir * CORNER_TURN_U;
        cr = CORNER_TURN_V - cornerDir * CORNER_TURN_U;
        vCur = CORNER_TURN_V;
      }
      limitWheels(cl, cr, -REV_RUN, 1000.0f / vScale);
      setMotors(cl, cr);
      return;
    }
  }

  float u = 0, vTarget = BASE_SPEED;
  bool searching = false;
  float leftCmd, rightCmd;

  if (onLine) {
    // široká příčná čára -> rovně
    u = 0; prevErr = 0; lostMs = 0; crossMode = false;
    vTarget = GAP_SPEED;
  } else if (nb > 0) {
    // vyber úsek nejblíž poslední poloze (křižovatka -> rovně)
    int best = 0;
    for (int i = 1; i < nb; i++)
      if (fabsf(blobs[i].c - lastC) < fabsf(blobs[best].c - lastC)) best = i;
    // spoj bloky oddělené malou mezerou (bílá tečka uprostřed křižovatky)
    int cs = blobs[best].s, ce = blobs[best].e;
    bool grew = true;
    while (grew) {
      grew = false;
      for (int i = 0; i < nb; i++) {
        if (blobs[i].e < cs && cs - blobs[i].e - 1 <= CLUSTER_GAP) { cs = blobs[i].s; grew = true; }
        else if (blobs[i].s > ce && blobs[i].s - ce - 1 <= CLUSTER_GAP) { ce = blobs[i].e; grew = true; }
      }
    }
    int cw = ce - cs + 1;
    float c;
    bool crossHold = false;
    if (!crossMode && cw > TRACK_MAX_W) {              // vstup do křižovatky
      crossMode = true; crossStartMs = ms; cPred = lastC;
    }
    if (crossMode) {
      // Průjezd křižovatkou: měření se ignoruje, drží se poslední střední řízení (uAvg),
      // tj. vozítko jede dál stejným směrem a nepřeskočí na křížící čáru.
      float meas = centroidRange(v, cs, ce);
      bool narrow = cw <= TRACK_MAX_W;
      if (narrow && fabsf(meas - cPred) <= CROSS_REACQ) { crossMode = false; c = meas; }
      else if (ms - crossStartMs > CROSS_MAX_MS) {
        crossMode = false;
        if (!windowCentroid(v, cPred, TRACK_WIN, c)) c = cPred;
      } else { crossHold = true; c = cPred; }
      if (!crossMode) { prevErr = c - 7.5f; dFilt = 0; iSum = 0; }
    } else {
      c = centroidRange(v, cs, ce);                    // běžná čára
    }
    if (!crossHold) lastC = c;
    lostMs = 0;

    if (crossHold) {
      u = uHold * CROSS_U_GAIN;
      vTarget = min(BASE_SPEED, CROSS_SPEED);
    } else {
    float err = c - 7.5f;               // + = čára vpravo
    iSum = constrain(iSum + Ki * err * dt, -I_MAX, I_MAX);
    float dRaw = (err - prevErr) / dt;
    dFilt += D_FILTER * (dRaw - dFilt);
    prevErr = err;

    u = Kp * err + iSum + Kd * dFilt;
    u = constrain(u, -U_MAX, U_MAX);
    lastU = u;

    // Znaménková střední hodnota řízení = stálá zatáčka (v zatáčce je err malé, ale u velké);
    // kmitání na rovince se vyruší. Zpomalí se dřív, než se robot dostane daleko z čáry.
    uAvg += (dt / (dt + U_TAU)) * (u - uAvg);
    uHold += (dt / (dt + U_HOLD_TAU)) * (u - uHold);
    float ratio = max(fabsf(err) / 7.5f, fabsf(uAvg) / U_REF);
    ratio = min(ratio, 1.0f);
    vTarget = max(BASE_SPEED - SPEED_DROP * ratio, MIN_SPEED);
    }
  } else {
    // čára ztracena
    lostMs += dt * 1000.0f;
    if (lostMs < GAP_HOLD_MS) {          // přerušovaná čára
      u = uHold * GAP_U_GAIN;
      vTarget = GAP_SPEED;
    } else if (lostMs < LOST_STOP_MS) {  // hledání čáry
      searching = true;
    } else {
      brakeMotors();
      state = ST_DONE;
      return;
    }
  }

  gU = u;
  if (crossMode) vTarget = min(vTarget, CROSS_SPEED);
  if (cornerCount >= ZONE_AFTER && cornerCount < ZONE_UNTIL) vTarget = min(vTarget, ZONE_SPEED);   // zóna zpomalení
  if (cornerCand > 0) vTarget = min(vTarget, CORNER_SPEED);   // blíží se zatáčka -> zpomal

  if (searching) {
    // směr hledání: strana poslední zatáčky u kraje lišty, jinak strana poslední polohy čáry
    float dir = (edgeDirLast != 0 && ms - edgeSeenMs < 1000) ? (float)edgeDirLast
                                                              : ((lastC >= 7.5f) ? 1.0f : -1.0f);
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
  limitWheels(leftCmd, rightCmd, -REV_RUN, 1000.0f / vScale);
  setMotors(leftCmd, rightCmd);
}

// ===================== TESTY =====================
static void sensorTest() {
  uint32_t tUs = micros();
  float v[N];
  readNorm(v);
  tUs = micros() - tUs;
  for (uint8_t i = 0; i < N; i++) Serial.print(v[i] > THR_LINE ? '#' : '.');
  Serial.print("  ");
  for (uint8_t i = 0; i < N; i++) { Serial.print(v[i], 1); Serial.print(' '); }
  Serial.printf(" | Vbat=%.2f V | čtení %lu us\n", readBatteryV(), (unsigned long)tUs);
  delay(100);
}

static void dryPrint() {
  static uint32_t last = 0;
  if (millis() - last < 40) return;
  last = millis();
  for (uint8_t i = 0; i < N; i++) Serial.print(gV[i] >= THR_LINE ? '#' : '.');
  Serial.printf(" cc=%d c=%.1f w=%d e=%d u=%.0f uAvg=%.0f uHold=%.0f v=%.0f lost=%.0f ev=%d %s%s\n",
                cornerCount, lastC, gW, edgeDir, gU, uAvg, uHold, vCur, lostMs, lineEvents,
                crossMode ? "[KRIZOVATKA] " : "", cornerMode ? "[ZATACKA] " : "");
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
#elif MODE == MODE_DRY_RUN
  if (!loadCal()) handCal(6000);
  tPrev = micros();
  batMs = millis();
#endif
}

void loop() {
#if MODE == MODE_RUN
  runStep();
#elif MODE == MODE_SENSOR_TEST
  sensorTest();
#elif MODE == MODE_DRY_RUN
  runStep();
  dryPrint();
#else
  motorTest();
#endif
}
