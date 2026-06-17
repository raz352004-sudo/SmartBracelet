// SmartBracelet — ESP32 BLE
// מקבל פקודות BLE מהאפליקציה ומפעיל ישירות מנוע רטט + LED + מסך OLED על הצמיד
// (אין STM32 — ה-ESP32 שולט בפלטים בעצמו).
//
// תואם לסכמה החשמלית ב-EasyEDA (2026-06-17): ESP32-WROOM-32, TP4056, LiPo 2000mAh,
// MOSFET AO3407 לכיבוי אוטומטי, OLED 0.96" I2C, LED אדום בודד, רטט PWM, כפתור הפעלה.
//
// ספריות נדרשות (Arduino Library Manager):
//   - "Adafruit SSD1306"
//   - "Adafruit GFX Library"
//   - "Adafruit BusIO" (תלות של שתי הספריות הקודמות)
//
// הערה: שום דבר כאן לא נבדק עדיין על חומרה אמיתית — יש לאמת ולכוונן בפועל.

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── BLE UUIDs (ייחודיים לפרויקט הזה) ──
#define SERVICE_UUID         "50e8c294-2e41-48dd-b9a2-fbe82a5fad37"
#define COMMAND_CHAR_UUID    "84194079-fe08-4c20-a2b8-4224c7a07c80"  // Write: קוד התראה מהאפליקציה
#define BATTERY_CHAR_UUID    "e4e328fb-8875-49da-8609-4bbbf4ad6dfc"  // Read/Notify: אחוז סוללה
#define INTENSITY_CHAR_UUID  "d64d880c-12ba-417c-9dfb-8d8683c3c772"  // Write: עוצמת רטט 0-100

// ── קודי התראה שמתקבלים מהאפליקציה ──
// LED בודד (לא RGB) — ההבחנה בין סוגי האירועים היא לפי קצב ההבהוב, לא צבע
#define CODE_ALARM      'A'   // אזעקת פיקוד העורף — רטט חזק רציף + LED קבוע
#define CODE_DOOR       'D'   // פעמון דלת         — רטט קצר      + הבהוב יחיד
#define CODE_INTRUSION  'I'   // גלאי פריצה        — רטט רציף     + הבהוב מהיר
#define CODE_TEST       'T'   // בדיקה              — רטט קצר      + הבהוב כפול

// ── פיני יציאה — לפי הסכמה החשמלית ב-EasyEDA ──
#define VIBRATION_PIN     4    // PWM
#define LED_PIN           5    // LED אדום בודד
#define POWER_BUTTON_PIN  15   // כפתור הפעלה / יציאה מ-Deep Sleep
#define MOSFET_PIN        32   // שליטה ב-AO3407: HIGH = מסך OLED מקבל מתח, LOW = כבוי
#define OLED_SDA_PIN       21
#define OLED_SCL_PIN       22

// פין ADC למדידת מתח הסוללה — מחלק מתח על קו הסוללה
#define BATTERY_ADC_PIN   34
#define BATTERY_MAX_MV    4200
#define BATTERY_MIN_MV    3300
#define BATTERY_CUTOFF_MV 3200  // מתחת לזה — כיבוי אוטומטי להגנה על הסוללה

// משכי זמן לכל דפוס (אפשר לכוונן)
#define DURATION_ALARM_MS      5000
#define DURATION_DOOR_MS        500
#define DURATION_INTRUSION_MS  8000
#define DURATION_TEST_MS        600
#define BLINK_INTERVAL_MS       150   // קצב הבהוב מהיר (גלאי פריצה)
#define DOUBLE_BLINK_GAP_MS     150   // המרווח בין שני ההבהובים בדפוס הבדיקה

// כמה זמן בלי חיבור BLE ובלי דפוס פעיל לפני שנכנסים ל-Deep Sleep
#define IDLE_SLEEP_TIMEOUT_MS  (2UL * 60UL * 1000UL)

// PWM למנוע הרטט (ESP32 Arduino core 3.x — API מבוסס פין, לא ערוץ)
#define VIBRATION_PWM_FREQ_HZ    5000
#define VIBRATION_PWM_RESOLUTION 8     // 0-255

// ── מסך OLED (SSD1306, I2C) ──
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1
#define OLED_I2C_ADDR 0x3C

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;
uint8_t lastBatteryPercent = 100;

BLEServer*          pServer        = nullptr;
BLECharacteristic*  pCommandChar   = nullptr;
BLECharacteristic*  pBatteryChar   = nullptr;
BLECharacteristic*  pIntensityChar = nullptr;
bool                deviceConnected = false;

// עוצמת רטט נוכחית, 0-100% — נשלטת מההגדרות באפליקציה
uint8_t vibrationIntensityPercent = 80;

// זמן הפעולה האחרונה (חיבור/אירוע) — לקביעה מתי להיכנס ל-Deep Sleep
unsigned long lastActivityMs = 0;

void setVibrationOutput(bool on) {
  uint8_t duty = on ? (uint8_t)((uint16_t)vibrationIntensityPercent * 255 / 100) : 0;
  ledcWrite(VIBRATION_PIN, duty);
}

// ── מסך OLED: מצב סרק (מצב חיבור + סוללה) ──
void showIdleScreen() {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SmartBracelet");
  display.println(deviceConnected ? "Connected" : "Waiting...");
  display.print("Battery: ");
  display.print(lastBatteryPercent);
  display.println("%");
  display.display();
}

// ── מסך OLED: אירוע פעיל (טקסט גדול) ──
void showEventScreen(const char* title) {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 24);
  display.println(title);
  display.display();
}

// ── מצב הדפוס הפעיל כרגע ──
enum Pattern { PATTERN_NONE, PATTERN_ALARM, PATTERN_DOOR, PATTERN_INTRUSION, PATTERN_TEST };
Pattern        activePattern    = PATTERN_NONE;
unsigned long  patternStartMs   = 0;
unsigned long  lastBlinkMs      = 0;
bool           ledOn            = false;
uint8_t        doubleBlinkCount = 0;  // לדפוס הבדיקה (שני הבהובים)

// ─────────────────────────────────────────
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
      break;
    case CODE_DOOR:
      activePattern = PATTERN_DOOR;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("DOOR");
      break;
    case CODE_INTRUSION:
      activePattern = PATTERN_INTRUSION;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("INTRUSION");
      break;
    case CODE_TEST:
      activePattern = PATTERN_TEST;
      setVibrationOutput(true);
      digitalWrite(LED_PIN, HIGH);
      showEventScreen("TEST");
      break;
  }
}

// מטפל בדפוס הפעיל: הבהוב מהיר לגלאי פריצה, הבהוב כפול לבדיקה, וכיבוי אוטומטי בסוף משך הזמן
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

// ─────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    lastActivityMs = millis();
    showIdleScreen();
  }
  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    lastActivityMs = millis();
    showIdleScreen();
    BLEDevice::startAdvertising();  // חזרה לפרסום כדי שהאפליקציה תוכל להתחבר מחדש
  }
};

// נקרא כשהאפליקציה כותבת קוד התראה (A/D/I/T) ל-characteristic
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue();
    if (value.length() == 0) return;

    char code = value[0];
    if (code == CODE_ALARM || code == CODE_DOOR ||
        code == CODE_INTRUSION || code == CODE_TEST) {
      lastActivityMs = millis();
      startPattern(code);
    }
  }
};

// נקרא כשהאפליקציה כותבת עוצמת רטט חדשה (0-100) ל-characteristic
class IntensityCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue();
    if (value.length() == 0) return;

    uint8_t percent = (uint8_t)value[0];
    if (percent > 100) percent = 100;
    vibrationIntensityPercent = percent;
  }
};

// ─────────────────────────────────────────
uint16_t readBatteryMv() {
  int raw = analogRead(BATTERY_ADC_PIN);
  // ESP32 ADC: 0-4095 = 0-3.3V. הכפלה ב-2 מתאימה למחלק מתח 1:1 — התאם לחיווט שלך
  return (uint16_t)((uint32_t)raw * 3300 / 4095 * 2);
}

uint8_t readBatteryPercent() {
  uint16_t mv = readBatteryMv();
  if (mv >= BATTERY_MAX_MV) return 100;
  if (mv <= BATTERY_MIN_MV) return 0;
  return (uint8_t)((mv - BATTERY_MIN_MV) * 100 / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

// כשהסוללה מתחת לסף הקריטי: מכבים את ה-MOSFET (מנתקים מתח מהמסך/היקפיים)
// ונכנסים ל-Deep Sleep קבוע — רק לחיצה על כפתור ההפעלה תוציא מכאן.
void shutDownForLowBattery() {
  allOutputsOff();
  if (oledAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Battery low");
    display.println("Shutting down...");
    display.display();
    delay(1500);
  }
  digitalWrite(MOSFET_PIN, LOW);
  enterDeepSleep();
}

// נכנסים ל-Deep Sleep. מקור ההתעוררות היחיד הוא כפתור ההפעלה (GPIO15) —
// ESP32 מכבה את רדיו ה-BLE לגמרי ב-Deep Sleep, כך שאי אפשר "להתעורר" מחיבור BLE
// נכנס; אפשר רק להישאר ערים כל עוד יש חיבור פעיל, ואז להירדם כשמתנתקים ואין פעילות.
void enterDeepSleep() {
  esp_sleep_enable_ext0_wakeup((gpio_num_t)POWER_BUTTON_PIN, 0);  // התעוררות ב-LOW
  esp_deep_sleep_start();
}

// ─────────────────────────────────────────
void setup() {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);  // מפעילים מתח להיקפיים (מסך וכו') בעלייה

  ledcAttach(VIBRATION_PIN, VIBRATION_PWM_FREQ_HZ, VIBRATION_PWM_RESOLUTION);
  pinMode(LED_PIN, OUTPUT);
  allOutputsOff();

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (oledAvailable) {
    lastBatteryPercent = readBatteryPercent();
    showIdleScreen();
  }

  BLEDevice::init("SmartBracelet");

  // bonding מסוג "Just Works" — ה-OS יזכור את הצמיד ולא יבקש סריקה מחדש כל פעם.
  // לא נדרש PIN מהמשתמש (ESP_IO_CAP_NONE), ולכן לא מספק הגנה מפני MITM —
  // מספיק לשימוש אישי, לא לסביבת ייצור עם דרישות אבטחה מחמירות.
  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  security->setCapability(ESP_IO_CAP_NONE);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* service = pServer->createService(SERVICE_UUID);

  pCommandChar = service->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE_ENC);
  pCommandChar->setCallbacks(new CommandCallbacks());

  pBatteryChar = service->createCharacteristic(
      BATTERY_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBatteryChar->addDescriptor(new BLE2902());

  pIntensityChar = service->createCharacteristic(
      INTENSITY_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE_ENC);
  pIntensityChar->setCallbacks(new IntensityCallbacks());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  lastActivityMs = millis();
}

unsigned long lastBatteryUpdate = 0;

void loop() {
  updateActivePattern();

  if (millis() - lastBatteryUpdate > 30000) {
    uint16_t mv = readBatteryMv();
    if (mv <= BATTERY_CUTOFF_MV) {
      shutDownForLowBattery();  // לא חוזר מכאן — נכנס ל-Deep Sleep
    }

    uint8_t percent = readBatteryPercent();
    lastBatteryPercent = percent;
    if (deviceConnected) {
      pBatteryChar->setValue(&percent, 1);
      pBatteryChar->notify();
    }
    if (activePattern == PATTERN_NONE) showIdleScreen();
    lastBatteryUpdate = millis();
  }

  // אם אין חיבור BLE ואין דפוס פעיל מספיק זמן — נכנסים ל-Deep Sleep לחיסכון בסוללה.
  // לחיצה על כפתור ההפעלה (GPIO15) תוציא מהשינה ותריץ את setup() מההתחלה.
  if (!deviceConnected && activePattern == PATTERN_NONE &&
      millis() - lastActivityMs > IDLE_SLEEP_TIMEOUT_MS) {
    enterDeepSleep();
  }
}
