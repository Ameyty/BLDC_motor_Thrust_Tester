#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TMP117.h>
#include <HX711.h>

// ================= OLED =================
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ================= SENSORS =================
Adafruit_TMP117 tempSensor;
HX711 lc1, lc2, lc3;

// ================= PINS =================
#define BTN_A 15
#define BTN_B 22
#define POT_PIN 4
#define RPM_PIN 21
#define ESC_PIN 20

// ================= ESC =================
uint32_t pwmMin = 3276;
uint32_t pwmMax = 6553;

// ================= STATE =================
enum State {
  MENU,
  RADIO,
  ESC_WAIT,
  ESC_RUN,
  ESC_DONE,
  LOADCELL_CAL
};

State state = MENU;
int menuIndex = 0;

// ================= RPM =================
volatile int pulses = 0;
float rpm = 0;
unsigned long lastRPM = 0;

// ================= FLAGS =================
bool tempOK = false;

// ================= ISR =================
void IRAM_ATTR rpmISR() {
  pulses++;
}

// ================= FUNCTIONS =================
void setESC(float percent) {
  percent = constrain(percent, 0, 100);
  uint32_t duty = pwmMin + (percent / 100.0) * (pwmMax - pwmMin);
  ledcWrite(ESC_PIN, duty);
}

void updateRPM() {
  unsigned long now = millis();
  float dt = (now - lastRPM) / 1000.0;

  if (dt > 0) rpm = (pulses / dt) * 60.0;

  pulses = 0;
  lastRPM = now;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  delay(1000);

  Wire.begin(6, 7);
  Wire.setClock(100000);
  delay(200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 20);
  display.print("Initializing...");
  display.display();
  delay(500);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.print("Temp sensor...");
  display.display();

  tempOK = tempSensor.begin();
  delay(300);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.print("Load cells...");
  display.display();

  lc1.begin(3, 2);
  lc2.begin(11, 10);
  lc3.begin(19, 18);

  delay(300);

  lc1.tare();
  lc2.tare();
  lc3.tare();

  delay(300);

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(RPM_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(RPM_PIN), rpmISR, RISING);

  delay(200);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.print("ESC setup...");
  display.display();

  ledcAttach(ESC_PIN, 50, 16);

  setESC(0);
  delay(500);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.print("READY");
  display.display();

  delay(1000);
}

// ================= LOOP =================
void loop() {

  // ===== MENU =====
  if (state == MENU) {

    display.clearDisplay();

    const char* items[3] = {
      "Radiomaster Pocket",
      "Calibrate ESC",
      "Calibrate Load Cells"
    };

    for (int i = 0; i < 3; i++) {
      display.setCursor(0, 18 + i * 12);
      if (i == menuIndex) display.print(">");
      display.setCursor(10, 18 + i * 12);
      display.print(items[i]);
    }

    display.display();

    if (digitalRead(BTN_A) == LOW) {
      menuIndex = (menuIndex + 1) % 3;
      delay(200);
    }

    if (digitalRead(BTN_B) == LOW) {
      if (menuIndex == 0) state = RADIO;
      if (menuIndex == 1) state = ESC_WAIT;
      if (menuIndex == 2) state = LOADCELL_CAL;
      delay(200);
    }
  }

  // ===== RADIO MODE =====
  else if (state == RADIO) {

    float thrust = lc2.get_units(2);
    float torque = abs(lc1.get_units(2) - lc3.get_units(2)) * 0.0098;

    float temp = 0;
    if (tempOK) {
      sensors_event_t e;
      tempSensor.getEvent(&e);
      temp = e.temperature;
    }

    updateRPM();

    display.clearDisplay();

    display.setCursor(0,18);
    display.print("Thrust:");
    display.print(thrust);

    display.setCursor(0,30);
    display.print("RPM:");
    display.print(rpm);

    display.setCursor(0,42);
    display.print("Temp:");
    display.print(temp);

    display.setCursor(0,54);
    display.print("Torque:");
    display.print(torque);

    display.display();

    if (digitalRead(BTN_B) == LOW) {
      state = MENU;
      delay(200);
    }

    delay(200);
  }

  // ===== ESC WAIT =====
  else if (state == ESC_WAIT) {

    display.clearDisplay();

    display.setCursor(0,18);
    display.print("Calibrating ESC");

    display.setCursor(0,30);
    display.print("Connect battery");

    display.setCursor(0,42);
    display.print("Press A");

    display.display();

    if (digitalRead(BTN_A) == LOW) {
      state = ESC_RUN;
      delay(300);
    }
  }

  // ===== ESC CALIBRATION =====
  else if (state == ESC_RUN) {

    display.clearDisplay();
    display.setCursor(0,30);
    display.print("Calibrating...");
    display.display();

    delay(1000);

    setESC(100);
    delay(3000);

    setESC(0);
    delay(4000);

    state = ESC_DONE;
  }

  // ===== ESC DONE =====
  else if (state == ESC_DONE) {

    int pot = analogRead(POT_PIN);
    float throttle = map(pot, 0, 4095, 0, 100);

    setESC(throttle);

    float thrust = lc2.get_units(2);
    float torque = abs(lc1.get_units(2) - lc3.get_units(2)) * 0.0098;

    float temp = 0;
    if (tempOK) {
      sensors_event_t e;
      tempSensor.getEvent(&e);
      temp = e.temperature;
    }

    updateRPM();

    display.clearDisplay();

    display.setCursor(0,18);
    display.print("Throttle:");
    display.print(throttle);

    display.setCursor(0,30);
    display.print("Thrust:");
    display.print(thrust);

    display.setCursor(0,42);
    display.print("Torque:");
    display.print(torque);

    display.setCursor(0,54);
    display.print("Temp:");
    display.print(temp);

    display.display();

    if (digitalRead(BTN_B) == LOW) {
      setESC(0);
      state = MENU;
      delay(200);
    }

    delay(200);
  }

  // ===== LOAD CELL CAL =====
  else if (state == LOADCELL_CAL) {

    display.clearDisplay();
    display.setCursor(10,30);
    display.print("Work in progress");
    display.display();

    if (digitalRead(BTN_B) == LOW) {
      state = MENU;
      delay(200);
    }
  }
}