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
  פולינג כל 5 שניות, דורש headers (`Referer`, `X-Requested-With`), **חסום ל-IP ישראלי בלבד**.
- flutter_blue_plus גרסה 2.3.8: `BluetoothDevice.connect()` דורש פרמטר `license:` (למשל `License.nonprofit`).
- BLE bonding ("Just Works", `ESP_IO_CAP_NONE`) ותמיכת OLED בקושחה **לא נבדקו על חומרה אמיתית** —
  המשתמש מתכנן לחבר ESP32 פיזי ולבדוק (פינים: רטט=25, LED אדום=26/כחול=27/ירוק=14, סוללה ADC=34).

## סביבת הפיתוח (macOS)

הותקנו על המחשב המקורי: Flutter SDK (brew cask), Android cmdline-tools + platform 34/36 +
build-tools, OpenJDK 21 (`flutter config --jdk-dir=...`), Xcode (המשתמש התקין בעצמו מה-App Store),
CocoaPods, gh CLI (מחובר לחשבון `raz352004-sudo`). אם זו סביבה חדשה — כל זה צריך התקנה.

## סטטוס (2026-06-17)

- Build מאומת בהצלחה גם ל-Android (APK debug) וגם ל-iOS (סימולטור).
- נוסף: auto-reconnect לצמיד אחרון, אזהרת סוללה חלשה, סליידר עוצמת רטט (PWM בקושחה),
  סינון התראות פיקוד העורף לפי יישוב (`NotificationSettingsService.cityFilter`).
- דף ההצגה פומבי וחי, כולל גרסה רספונסיבית למובייל.
- עדיין לא נבדק על חומרה אמיתית (ESP32 פיזי) — זה השלב המעשי הבא.
