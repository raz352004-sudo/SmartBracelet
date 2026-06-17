# SmartBracelet

צמיד חכם לחירשים — אפליקציית Flutter (iOS+Android) שמתחברת דרך BLE לצמיד מבוסס ESP32,
ומתריעה ברטט+LED (ועתידית מסך OLED) על: אזעקת פיקוד העורף, פעמון דלת, גלאי פריצה.

## מבנה הפרויקט

- `app/` — אפליקציית Flutter (`smart_bracelet`). `lib/main.dart` הוא שורש האפליקציה.
- `firmware/esp32_ble/esp32_ble.ino` — קושחת ESP32 (Arduino).
- `presentation/index.html` — דף הצגה/פרופוזל לפרויקט, חי ב-GitHub Pages:
  https://raz352004-sudo.github.io/SmartBracelet/
- `.github/workflows/pages.yml` — דיפלוי אוטומטי של `presentation/` ל-GitHub Pages בכל push ל-master.

## החלטות חשובות

- **אין STM32.** ה-ESP32 שולט בעצמו במנוע הרטט וב-LED ישירות (GPIO), לא מעביר UART לבקר נוסף.
  קוד הייחוס הישן (MQTT+STM32, ב-`~/Downloads/esp8266_mqtt.ino`) הוחלף לחלוטין.
- קודי BLE: `A`=אזעקה, `D`=דלת, `I`=פריצה, `T`=בדיקה. ה-UUIDs מוגדרים פעמיים —
  בקושחה (`esp32_ble.ino`) ובאפליקציה (`lib/services/bracelet_ble_service.dart`) — **חייבים להישאר זהים**.
- פיקוד העורף: אין API רשמי. משתמשים ב-`https://www.oref.org.il/WarningMessages/alert/alerts.json`,
  פולינג כל 5 שניות, דורש headers (`Referer`, `X-Requested-With`). **אומת ב-2026-06-17**: ה-endpoint
  הזה למעשה *לא* היה חסום מהמכונה הזו (כן קיבלנו 200). מבנה התשובה האמיתי (מאומת מול כמה מימושים
  פתוחים בקוד פתוח, ולא רק ניחוש): `{"id": ..., "cat": ..., "title": ..., "data": [...]}` — שדה
  היישובים הוא `data` (לא `cities`), ושדה ה-ID הוא `id` (לא `notificationId`). כשאין אזעקה פעילה
  השרת מחזיר רק BOM+CRLF (`EF BB BF 0D 0A`), לא JSON ריק — הקוד מטפל בזה במפורש. ה-History endpoint
  (`.../History/AlertsHistory.json`) חסום (403) מהמכונה הזו, גם אם הראשי לא.
- flutter_blue_plus גרסה 2.3.8: `BluetoothDevice.connect()` דורש פרמטר `license:` (למשל `License.nonprofit`).
- BLE bonding ("Just Works", `ESP_IO_CAP_NONE`) ותמיכת OLED בקושחה **לא נבדקו על חומרה אמיתית**.

## חומרה (סכמה חשמלית ב-EasyEDA, 2026-06-17)

ESP32-WROOM-32 + TP4056 (טעינת USB-C) + LiPo 3.7V 2000mAh + MOSFET AO3407 (כיבוי אוטומטי,
עם נגדי 100K+10K לשליטה בשער) + OLED 0.96" I2C + LED אדום בודד (לא RGB — ההבחנה בין סוגי
האירועים היא לפי קצב הבהוב, לא צבע) + כפתור הפעלה + כפתור Reset (EN).

**פינים (עודכן מהפלייסהולדרים הקודמים):**
- GPIO4 = רטט (PWM) | GPIO5 = LED | GPIO15 = כפתור הפעלה / יציאה מ-Deep Sleep
- GPIO21/22 = SDA/SCL (OLED) | GPIO32 = שליטה ב-MOSFET | GPIO34 = ADC סוללה

**קושחה מעודכנת בהתאם** (`esp32_ble.ino`):
- LED בודד: ALARM=קבוע, DOOR=הבהוב יחיד, INTRUSION=הבהוב מהיר, TEST=הבהוב כפול
- Deep Sleep אחרי 2 דקות חוסר פעילות (אין חיבור BLE + אין דפוס פעיל); יציאה רק
  בלחיצה על GPIO15 (`esp_sleep_enable_ext0_wakeup`) — **לא ניתן "להתעורר" מחיבור BLE
  נכנס**, כי הרדיו כבוי לגמרי ב-Deep Sleep. זה מגביל בפועל: התעוררות תמיד דורשת כפתור.
- כיבוי אוטומטי: כשהמתח נמדד מתחת ל-3.2V, GPIO32→LOW (מנתק מתח מהמסך) ואז Deep Sleep קבוע.
- שום דבר מהקושחה (כולל הדפוסים, ה-Deep Sleep וה-MOSFET) **לא נבדק על חומרה אמיתית**.

**רעיון לאפליקציה שעלה (לא ממומש)**: DSP לזיהוי צלילים דרך מיקרופון הטלפון — עדיין לא תוכנן.

## סביבת הפיתוח (macOS)

הותקנו על המחשב המקורי: Flutter SDK (brew cask), Android cmdline-tools + platform 34/36 +
build-tools, OpenJDK 21 (`flutter config --jdk-dir=...`), Xcode (המשתמש התקין בעצמו מה-App Store),
CocoaPods, gh CLI (מחובר לחשבון `raz352004-sudo`). אם זו סביבה חדשה — כל זה צריך התקנה.

## שלבים הבאים (לפי תכנון EasyEDA)

PCB Layout ב-EasyEDA → הזמנת PCB מ-JLCPCB → הזמנת רכיבים מאליאקספרס (גודל 0805) →
הרכבה והלחמה → אימות הקושחה (BLE, Deep Sleep, MOSFET) על החומרה הפיזית.

## סטטוס (2026-06-17)

- Build מאומת בהצלחה גם ל-Android (APK debug) וגם ל-iOS (סימולטור).
- נוסף: auto-reconnect לצמיד אחרון, אזהרת סוללה חלשה, סליידר עוצמת רטט (PWM בקושחה),
  סינון התראות פיקוד העורף לפי יישוב (`NotificationSettingsService.cityFilter`).
- דף ההצגה פומבי וחי, כולל גרסה רספונסיבית למובייל.
- הסכמה החשמלית (EasyEDA) הושלמה — הקושחה עודכנה לפינים והעיצוב האמיתיים (ראו "חומרה" מעלה).
- עדיין לא נבדק על חומרה אמיתית (ESP32 פיזי) — זה השלב המעשי הבא.
