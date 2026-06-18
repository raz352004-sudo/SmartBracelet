// SmartBracelet — גרסת סימולציה ל-Wokwi (wokwi.com)
// אין BLE בקוד הזה (Wokwi לא מדמה רדיו BLE) — במקום זאת שולחים פקודה
// דרך ה-Serial Monitor: שלח A / D / I / T ולחץ Enter, ותראה את הדפוס מופעל.
// זה בודק רק את לוגיקת הדפוסים (LED + רטט + OLED) — לא את ה-BLE/Deep Sleep/MOSFET האמיתיים.
//
// חיווט ב-Wokwi (תואם לפינים האמיתיים בסכמה):
//   LED (עם נגד 220 אוהם בטור) -> GPIO5 -> GND
//   Pushbutton -> GPIO15 -> GND
//   SSD1306 OLED (I2C) -> SDA=GPIO21, SCL=GPIO22
//   (מנוע רטט: ב-Wokwi אפשר לדמות עם LED נוסף או "Vibration Motor" אם קיים ברכיבים)
//
// ספריות נדרשות: "Adafruit SSD1306", "Adafruit GFX Library", "Adafruit BusIO"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define CODE_ALARM      'A'
#define CODE_DOOR       'D'
#define CODE_INTRUSION  'I'
#define CODE_TEST       'T'

#define VIBRATION_PIN     4
#define LED_PIN           5
#define OLED_SDA_PIN       21
#define OLED_SCL_PIN       22

#define DURATION_ALARM_MS      5000
#define DURATION_DOOR_MS        500
#define DURATION_INTRUSION_MS  8000
#define DURATION_TEST_MS        600
#define BLINK_INTERVAL_MS       150
#define DOUBLE_BLINK_GAP_MS     150

#define VIBRATION_PWM_FREQ_HZ    5000
#define VIBRATION_PWM_RESOLUTION 8

#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;
uint8_t vibrationIntensityPercent = 80;

void setVibrationOutput(bool on) {
  uint8_t duty = on ? (uint8_t)((uint16_t)vibrationIntensityPercent * 255 / 100) : 0;
  ledcWrite(VIBRATION_PIN, duty);
}

void showIdleScreen() {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SmartBracelet");
  display.println("Simulation mode");
  display.println("Send A/D/I/T...");
  display.display();
}

void showEventScreen(const char* title) {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 24);
  display.println(title);
  display.display();
}

enum Pattern { PATTERN_NONE, PATTERN_ALARM, PATTERN_DOOR, PATTERN_INTRUSION, PATTERN_TEST };
Pattern        activePattern    = PATTERN_NONE;
unsigned long  patternStartMs   = 0;
unsigned long  lastBlinkMs      = 0;
bool           ledOn            = false;
uint8_t        doubleBlinkCount = 0;

void allOutputsOff() {
  setVibrationOutput(false);
  digitalWrite(LED_PIN, LOW);
}

void startPattern(char code) {
  allOutputsOff();
  patternStartMs = millis();
  lastBlinkMs = patternStartMs;
  ledOn = true;
  doubleBlinkCount = 0;

  switch (code) {
    case CODE_ALARM:
      activePattern = PATTERN_ALARM;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("ALARM!");
      Serial.println("-> ALARM pattern");
      break;
    case CODE_DOOR:
      activePattern = PATTERN_DOOR;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("DOOR");
      Serial.println("-> DOOR pattern");
      break;
    case CODE_INTRUSION:
      activePattern = PATTERN_INTRUSION;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("INTRUSION");
      Serial.println("-> INTRUSION pattern");
      break;
    case CODE_TEST:
      activePattern = PATTERN_TEST;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("TEST");
      Serial.println("-> TEST pattern");
      break;
  }
}

void updateActivePattern() {
  if (activePattern == PATTERN_NONE) return;

  unsigned long elapsed = millis() - patternStartMs;
  unsigned long duration;
  switch (activePattern) {
    case PATTERN_ALARM:     duration = DURATION_ALARM_MS;     break;
    case PATTERN_DOOR:      duration = DURATION_DOOR_MS;      break;
    case PATTERN_INTRUSION: duration = DURATION_INTRUSION_MS; break;
    case PATTERN_TEST:      duration = DURATION_TEST_MS;      break;
    default:                duration = 0;                    break;
  }

  if (elapsed >= duration) {
    allOutputsOff();
    activePattern = PATTERN_NONE;
    showIdleScreen();
    return;
  }

  if (activePattern == PATTERN_INTRUSION && millis() - lastBlinkMs >= BLINK_INTERVAL_MS) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    lastBlinkMs = millis();
  } else if (activePattern == PATTERN_TEST &&
             doubleBlinkCount < 2 &&
             millis() - lastBlinkMs >= DOUBLE_BLINK_GAP_MS) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    lastBlinkMs = millis();
    if (!ledOn) doubleBlinkCount++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("SmartBracelet simulation — send A / D / I / T + Enter");

  ledcAttach(VIBRATION_PIN, VIBRATION_PWM_FREQ_HZ, VIBRATION_PWM_RESOLUTION);
  pinMode(LED_PIN, OUTPUT);
  allOutputsOff();

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  showIdleScreen();
}

void loop() {
  updateActivePattern();

  if (Serial.available() > 0) {
    char code = Serial.read();
    if (code == CODE_ALARM || code == CODE_DOOR ||
        code == CODE_INTRUSION || code == CODE_TEST) {
      startPattern(code);
    }
  }
}
