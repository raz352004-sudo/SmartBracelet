// SmartBracelet — ESP32 BLE
// מקבל פקודות BLE מהאפליקציה ומפעיל ישירות מנוע רטט + LED + מסך OLED על הצמיד
// (אין STM32 — ה-ESP32 שולט בפלטים בעצמו).
//
// ספריות נדרשות (Arduino Library Manager):
//   - "Adafruit SSD1306"
//   - "Adafruit GFX Library"
//   - "Adafruit BusIO" (תלות של שתי הספריות הקודמות)
//
// הערה: תמיכת ה-OLED ואימות ה-BLE bonding לא נבדקו על חומרה אמיתית —
// יש לאמת ולכוונן (כתובת I2C, פיני SDA/SCL) על הצמיד הפיזי.

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
#define CODE_ALARM      'A'   // אזעקת פיקוד העורף — רטט חזק רציף + LED אדום קבוע
#define CODE_DOOR       'D'   // פעמון דלת         — רטט קצר      + LED כחול קבוע
#define CODE_INTRUSION  'I'   // גלאי פריצה        — רטט רציף     + LED אדום מהבהב
#define CODE_TEST       'T'   // בדיקה              — רטט קצר      + LED ירוק קבוע

// ── פיני יציאה — התאם לחיווט בפועל על הצמיד ──
#define VIBRATION_PIN   25
#define LED_RED_PIN     26
#define LED_BLUE_PIN    27
#define LED_GREEN_PIN   14

// פין ADC למדידת מתח הסוללה דרך מחלק מתח — התאם לחיווט בפועל
#define BATTERY_ADC_PIN   34
#define BATTERY_MAX_MV    4200
#define BATTERY_MIN_MV    3300

// משכי זמן לכל דפוס (אפשר לכוונן)
#define DURATION_ALARM_MS      5000
#define DURATION_DOOR_MS        500
#define DURATION_INTRUSION_MS  8000
#define DURATION_TEST_MS        300
#define BLINK_INTERVAL_MS       200   // קצב הבהוב ה-LED באזעקת פריצה

// PWM למנוע הרטט (ESP32 Arduino core 3.x — API מבוסס פין, לא ערוץ)
#define VIBRATION_PWM_FREQ_HZ    5000
#define VIBRATION_PWM_RESOLUTION 8     // 0-255

// ── מסך OLED (SSD1306, I2C) — מחובר ל-SDA/SCL הסטנדרטיים של הבורד ──
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
Pattern        activePattern   = PATTERN_NONE;
unsigned long  patternStartMs  = 0;
unsigned long  lastBlinkMs     = 0;
bool           blinkOn         = false;

// ─────────────────────────────────────────
void allOutputsOff() {
  setVibrationOutput(false);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
}

void startPattern(char code) {
  allOutputsOff();
  patternStartMs = millis();
  lastBlinkMs = patternStartMs;
  blinkOn = true;

  switch (code) {
    case CODE_ALARM:
      activePattern = PATTERN_ALARM;
      setVibrationOutput(true);
      digitalWrite(LED_RED_PIN, HIGH);
      showEventScreen("ALARM!");
      break;
    case CODE_DOOR:
      activePattern = PATTERN_DOOR;
      setVibrationOutput(true);
      digitalWrite(LED_BLUE_PIN, HIGH);
      showEventScreen("DOOR");
      break;
    case CODE_INTRUSION:
      activePattern = PATTERN_INTRUSION;
      setVibrationOutput(true);
      digitalWrite(LED_RED_PIN, HIGH);
      showEventScreen("INTRUSION");
      break;
    case CODE_TEST:
      activePattern = PATTERN_TEST;
      setVibrationOutput(true);
      digitalWrite(LED_GREEN_PIN, HIGH);
      showEventScreen("TEST");
      break;
  }
}

// מטפל בדפוס הפעיל: הבהוב ל-INTRUSION, וכיבוי אוטומטי בסוף משך הזמן
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
    blinkOn = !blinkOn;
    digitalWrite(LED_RED_PIN, blinkOn ? HIGH : LOW);
    lastBlinkMs = millis();
  }
}

// ─────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    showIdleScreen();
  }
  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
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
uint8_t readBatteryPercent() {
  int raw = analogRead(BATTERY_ADC_PIN);
  // ESP32 ADC: 0-4095 = 0-3.3V. הכפלה ב-2 מתאימה למחלק מתח 1:1 — התאם לחיווט שלך
  uint32_t mv = (uint32_t)raw * 3300 / 4095 * 2;
  if (mv >= BATTERY_MAX_MV) return 100;
  if (mv <= BATTERY_MIN_MV) return 0;
  return (uint8_t)((mv - BATTERY_MIN_MV) * 100 / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

// ─────────────────────────────────────────
void setup() {
  ledcAttach(VIBRATION_PIN, VIBRATION_PWM_FREQ_HZ, VIBRATION_PWM_RESOLUTION);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  allOutputsOff();

  Wire.begin();  // SDA/SCL הסטנדרטיים של הבורד (בד"כ GPIO 21/22)
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
}

unsigned long lastBatteryUpdate = 0;

void loop() {
  updateActivePattern();

  if (deviceConnected && millis() - lastBatteryUpdate > 30000) {
    uint8_t percent = readBatteryPercent();
    lastBatteryPercent = percent;
    pBatteryChar->setValue(&percent, 1);
    pBatteryChar->notify();
    if (activePattern == PATTERN_NONE) showIdleScreen();
    lastBatteryUpdate = millis();
  }
}
