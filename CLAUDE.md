# SmartBracelet

צמיד חכם לחירשים — אפליקציית Flutter (iOS+Android) שמתחברת דרך BLE לצמיד מבוסס ESP32,
ומתריעה ברטט+LED (ועתידית מסך OLED) על: אזעקת פיקוד העורף, פעמון דלת, גלאי פריצה.

## מבנה הפרויקט

- `app/` — אפליקציית Flutter (`smart_bracelet`). `lib/main.dart` הוא שורש האפליקציה.
- `firmware/esp32_ble/esp32_ble.ino` — קושחת ESP32 האמיתית (Arduino, עם BLE/OLED/Deep Sleep).
- `firmware/esp32_wokwi_sim/esp32_wokwi_sim.ino` — גרסה מקוצרת **בלי BLE** לבדיקה ב-Wokwi (פקודות
  דרך Serial במקום BLE). **בכוונה בתיקייה נפרדת** — Arduino IDE מקמפל את כל קבצי ה-.ino שבתיקייה
  כסקץ' אחד, אז שני הקבצים בתיקייה אחת גרמו לשגיאות "redefinition" (תוקן ב-2026-06-18).
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

**פינים (אומתו מול הסכמה הסופית ב-EasyEDA, 2026-06-17):**
- GPIO4 = רטט (PWM) | GPIO5 = LED (דרך נגד 220Ω) | GPIO15 = כפתור הפעלה / יציאה מ-Deep Sleep
- GPIO21/22 = SDA/SCL (OLED) | GPIO32 = שליטה ב-MOSFET (דרך מחלק R1=100K/R2=10K לשער Q1)
- **GPIO35 = ADC סוללה**, דרך מחלק נפרד R3(100K)+R5(100K) יחס 1:1 על קו ה-Vbat
  (אומת ידנית עם המשתמש: GPIO35 מחובר לנקודת החיתוך בין R3 ל-R5, לא לקו ה-Vbat הגולמי —
  קריטי, כי חיבור ישיר לסוללה (עד 4.2V) היה עלול לפגוע בפין ה-ADC המוגבל ל-3.3V)

**קושחה מעודכנת בהתאם** (`esp32_ble.ino`):
- LED בודד: ALARM=קבוע, DOOR=הבהוב יחיד, INTRUSION=הבהוב מהיר, TEST=הבהוב כפול
- Deep Sleep אחרי 2 דקות חוסר פעילות (אין חיבור BLE + אין דפוס פעיל); יציאה רק
  בלחיצה על GPIO15 (`esp_sleep_enable_ext0_wakeup`) — **לא ניתן "להתעורר" מחיבור BLE
  נכנס**, כי הרדיו כבוי לגמרי ב-Deep Sleep. זה מגביל בפועל: התעוררות תמיד דורשת כפתור.
- כיבוי אוטומטי: כשהמתח נמדד מתחת ל-3.2V, GPIO32→LOW (מנתק מתח מהמסך) ואז Deep Sleep קבוע.
- שום דבר מהקושחה (כולל הדפוסים, ה-Deep Sleep וה-MOSFET) **לא נבדק על חומרה אמיתית**.

**DSP לזיהוי צלילים דרך מיקרופון הטלפון — מחוץ לסקופ של הפרויקט הזה.** המשתמש החליט
לעשות את זה כפרויקט נפרד ללימוד עצמי, לא כחלק מ-SmartBracelet.

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

## עדכון 2026-06-18 — אימות BLE אמיתי על חומרה

- ESP32 פיזי נצרב בהצלחה עם `esp32_ble.ino` (Arduino IDE, board "ESP32 Dev Module").
- חיווט בפועל על ברדבורד: GND ו-3V3 משותפים. מנוע רטט = מודול 3 פינים (VCC/GND/SIG) מחובר
  ישירות ל-GPIO4 (לא נדרש טרנזיסטור — המודול כולל driver משלו). כפתור ל-GPIO15→GND בלי נגד
  (יש `INPUT_PULLUP` בקוד). **בלי OLED בשלב זה** (לא היה מסך פנוי) — מטופל graceful ע"י `oledAvailable`.
- חשוב: הקוד האמיתי **לא מאזין ל-Serial**, רק ל-BLE — אי אפשר לבדוק עם Serial Monitor.
- **האפליקציה האמיתית רצה בהצלחה על אייפון פיזי** (לא סימולטור — BLE לא עובד בסימולטור iOS)
  דרך `flutter run -d <device-id>`, אחרי תיקון code signing (ראה "תקלות שנפתרו" מתחת).
- בעיית קומפילציה ב-Arduino IDE: היה גם `wokwi_simulation.ino` בתוך `firmware/esp32_ble/` —
  הועבר לתיקייה נפרדת `firmware/esp32_wokwi_sim/` (Arduino מקמפל את כל קבצי .ino בתיקייה כסקץ' אחד).

**תקלות שנפתרו (iOS code signing, לזיכרון):**
1. אייפון לא מזוהה ב-Xcode → צריך "מצב מפתח" (Developer Mode) ב-הגדרות→פרטיות ואבטחה→מצב מפתח.
2. `flutter run` על מכשיר פיזי דרש Team ב-Signing & Capabilities (Xcode) — Personal Team מהאפליקה ID מספיק.
3. **תקלה מרכזית**: חלונית "codesign רוצה לגשת למפתח..." סירבה לקבל את סיסמת המק הנכונה, גם
   אחרי איפוס login keychain (העברת `~/Library/Keychains/login.keychain-db` לגיבוי + logout/login
   ליצירת אחד חדש). אומת ש-`security unlock-keychain` עבד עם הסיסמה דרך הטרמינל — כך שזו לא
   הייתה בעיית סיסמה אלא בעיה ידועה כש-codesign רץ משורת פקודה (לא מ-Xcode עצמו). **הפתרון**:
   `security set-key-partition-list -S apple-tool:,apple:,codesign: -s ~/Library/Keychains/login.keychain-db`
   (להריץ בטרמינל, מזין סיסמה כשמתבקש) — זה פתר את זה לחלוטין.
- עדיין לא נבדק על חומרה אמיתית (ESP32 פיזי) — זה השלב המעשי הבא.
