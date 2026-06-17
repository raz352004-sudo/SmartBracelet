import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

import 'models/alert.dart';
import 'screens/history_screen.dart';
import 'screens/home_screen.dart';
import 'screens/settings_screen.dart';
import 'services/alert_history_service.dart';
import 'services/bracelet_ble_service.dart';
import 'services/notification_settings_service.dart';
import 'services/pikud_haoref_service.dart';

void main() {
  runApp(const SmartBraceletApp());
}

class SmartBraceletApp extends StatelessWidget {
  const SmartBraceletApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmartBracelet',
      locale: const Locale('he'),
      theme: ThemeData(colorSchemeSeed: Colors.indigo, useMaterial3: true),
      home: const RootScreen(),
    );
  }
}

class RootScreen extends StatefulWidget {
  const RootScreen({super.key});

  @override
  State<RootScreen> createState() => _RootScreenState();
}

class _RootScreenState extends State<RootScreen> {
  final bleService = BraceletBleService();
  final historyService = AlertHistoryService();
  final settingsService = NotificationSettingsService();
  late final PikudHaorefService pikudHaorefService;

  int _tabIndex = 0;
  bool _intensitySentForCurrentConnection = false;

  @override
  void initState() {
    super.initState();
    pikudHaorefService = PikudHaorefService(onAlarm: _onPikudHaorefAlarm);
    bleService.addListener(_onBleStateChange);
    settingsService.addListener(_onSettingsChange);
    settingsService.load();
    historyService.load();
    pikudHaorefService.start();
    _requestPermissions().then((_) => bleService.tryAutoReconnect());
  }

  // מסנכרן את סינון היישוב מההגדרות לשירות פיקוד העורף
  void _onSettingsChange() {
    pikudHaorefService.targetCity = settingsService.cityFilter;
  }

  // בכל חיבור חדש לצמיד, שולח את עוצמת הרטט השמורה בהגדרות
  void _onBleStateChange() {
    if (bleService.state == BraceletConnectionState.connected) {
      if (!_intensitySentForCurrentConnection) {
        _intensitySentForCurrentConnection = true;
        bleService.setVibrationIntensity(settingsService.vibrationIntensityPercent);
      }
    } else {
      _intensitySentForCurrentConnection = false;
    }
  }

  Future<void> _requestPermissions() async {
    await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
  }

  void _onPikudHaorefAlarm() {
    if (bleService.state == BraceletConnectionState.connected &&
        settingsService.isEnabled(AlertType.alarm)) {
      bleService.sendCode(AlertType.alarm.code);
      historyService.addEvent(AlertType.alarm);
    }
  }

  @override
  void dispose() {
    bleService.removeListener(_onBleStateChange);
    settingsService.removeListener(_onSettingsChange);
    bleService.dispose();
    historyService.dispose();
    settingsService.dispose();
    pikudHaorefService.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final screens = [
      HomeScreen(
        bleService: bleService,
        historyService: historyService,
        settingsService: settingsService,
      ),
      SettingsScreen(settingsService: settingsService, bleService: bleService),
      HistoryScreen(historyService: historyService),
    ];

    return Scaffold(
      body: IndexedStack(index: _tabIndex, children: screens),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _tabIndex,
        onDestinationSelected: (index) => setState(() => _tabIndex = index),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.home), label: 'בית'),
          NavigationDestination(icon: Icon(Icons.settings), label: 'הגדרות'),
          NavigationDestination(icon: Icon(Icons.history), label: 'היסטוריה'),
        ],
      ),
    );
  }
}
