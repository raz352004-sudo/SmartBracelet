import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

enum BraceletConnectionState { disconnected, scanning, connecting, connected }

/// מנהל את כל התקשורת ב-BLE עם הצמיד: סריקה, חיבור, שליחת קודי התראה,
/// והאזנה לרמת הסוללה. ה-UUIDs כאן חייבים להיות זהים לאלה שב-firmware/esp32_ble.
class BraceletBleService extends ChangeNotifier {
  static final Guid serviceUuid = Guid('50e8c294-2e41-48dd-b9a2-fbe82a5fad37');
  static final Guid commandCharUuid = Guid('84194079-fe08-4c20-a2b8-4224c7a07c80');
  static final Guid batteryCharUuid = Guid('e4e328fb-8875-49da-8609-4bbbf4ad6dfc');
  static final Guid intensityCharUuid = Guid('d64d880c-12ba-417c-9dfb-8d8683c3c772');

  static const _lastDeviceIdKey = 'last_bracelet_device_id';
  static const lowBatteryThreshold = 20;

  BraceletConnectionState state = BraceletConnectionState.disconnected;
  List<ScanResult> scanResults = [];
  BluetoothDevice? device;
  int? batteryLevel;

  bool get isLowBattery =>
      batteryLevel != null && batteryLevel! <= lowBatteryThreshold;

  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _intensityChar;
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;
  StreamSubscription<List<int>>? _batterySub;

  Future<void> startScan() async {
    scanResults = [];
    state = BraceletConnectionState.scanning;
    notifyListeners();

    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      scanResults = results
          .where((r) =>
              r.advertisementData.serviceUuids.contains(serviceUuid) ||
              r.device.platformName == 'SmartBracelet')
          .toList();
      notifyListeners();
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));
    if (state == BraceletConnectionState.scanning) {
      state = BraceletConnectionState.disconnected;
      notifyListeners();
    }
  }

  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
  }

  Future<void> connect(BluetoothDevice target) async {
    await stopScan();
    state = BraceletConnectionState.connecting;
    notifyListeners();

    device = target;
    await _connectionSub?.cancel();
    _connectionSub = target.connectionState.listen((connState) {
      if (connState == BluetoothConnectionState.disconnected) {
        state = BraceletConnectionState.disconnected;
        batteryLevel = null;
        notifyListeners();
      }
    });

    try {
      await target.connect(license: License.nonprofit);
      final services = await target.discoverServices();
      final service = services.firstWhere((s) => s.uuid == serviceUuid);

      _commandChar =
          service.characteristics.firstWhere((c) => c.uuid == commandCharUuid);
      _intensityChar =
          service.characteristics.firstWhere((c) => c.uuid == intensityCharUuid);
      final batteryChar =
          service.characteristics.firstWhere((c) => c.uuid == batteryCharUuid);

      await batteryChar.setNotifyValue(true);
      await _batterySub?.cancel();
      _batterySub = batteryChar.lastValueStream.listen((value) {
        if (value.isNotEmpty) {
          batteryLevel = value[0];
          notifyListeners();
        }
      });

      final initial = await batteryChar.read();
      if (initial.isNotEmpty) batteryLevel = initial[0];

      state = BraceletConnectionState.connected;
      notifyListeners();

      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_lastDeviceIdKey, target.remoteId.str);
    } catch (e) {
      state = BraceletConnectionState.disconnected;
      notifyListeners();
      rethrow;
    }
  }

  /// מנסה להתחבר אוטומטית לצמיד האחרון שהיה מחובר, בלי לדרוש סריקה ידנית.
  /// אם המכשיר לא בטווח או שההתחברות נכשלת — נכשל בשקט, המשתמש יוכל לסרוק ידנית.
  Future<void> tryAutoReconnect() async {
    final prefs = await SharedPreferences.getInstance();
    final lastId = prefs.getString(_lastDeviceIdKey);
    if (lastId == null) return;

    try {
      await connect(BluetoothDevice.fromId(lastId));
    } catch (_) {
      // לא נמצא / לא ניתן להתחבר אוטומטית כרגע
    }
  }

  Future<void> disconnect() async {
    await device?.disconnect();
    state = BraceletConnectionState.disconnected;
    batteryLevel = null;
    notifyListeners();
  }

  /// שולח קוד חד-ספרתי לצמיד: A=אזעקה, D=דלת, I=פריצה, T=בדיקה
  Future<void> sendCode(String code) async {
    final commandChar = _commandChar;
    if (commandChar == null) return;
    await commandChar.write(code.codeUnits, withoutResponse: false);
  }

  /// שולח לצמיד עוצמת רטט חדשה (0-100%)
  Future<void> setVibrationIntensity(int percent) async {
    final intensityChar = _intensityChar;
    if (intensityChar == null) return;
    final clamped = percent.clamp(0, 100);
    await intensityChar.write([clamped], withoutResponse: false);
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _connectionSub?.cancel();
    _batterySub?.cancel();
    super.dispose();
  }
}
