import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/alert.dart';

/// סוגי ההתראות שאפשר להפעיל/לכבות מההגדרות
const configurableAlertTypes = [
  AlertType.alarm,
  AlertType.door,
  AlertType.intrusion,
];

class NotificationSettingsService extends ChangeNotifier {
  static const _prefsPrefix = 'notify_enabled_';
  static const _intensityPrefsKey = 'vibration_intensity_percent';
  static const _cityFilterPrefsKey = 'pikud_haoref_city_filter';
  static const defaultVibrationIntensity = 80;

  final Map<AlertType, bool> _enabled = {
    for (final type in configurableAlertTypes) type: true,
  };

  int vibrationIntensityPercent = defaultVibrationIntensity;

  /// שם היישוב לסינון התראות פיקוד העורף — null/ריק = כל הארץ
  String? cityFilter;

  Future<void> load() async {
    final prefs = await SharedPreferences.getInstance();
    for (final type in configurableAlertTypes) {
      _enabled[type] = prefs.getBool('$_prefsPrefix${type.name}') ?? true;
    }
    vibrationIntensityPercent =
        prefs.getInt(_intensityPrefsKey) ?? defaultVibrationIntensity;
    cityFilter = prefs.getString(_cityFilterPrefsKey);
    notifyListeners();
  }

  bool isEnabled(AlertType type) => _enabled[type] ?? true;

  Future<void> setEnabled(AlertType type, bool value) async {
    _enabled[type] = value;
    notifyListeners();
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool('$_prefsPrefix${type.name}', value);
  }

  Future<void> setVibrationIntensity(int percent) async {
    vibrationIntensityPercent = percent.clamp(0, 100);
    notifyListeners();
    final prefs = await SharedPreferences.getInstance();
    await prefs.setInt(_intensityPrefsKey, vibrationIntensityPercent);
  }

  Future<void> setCityFilter(String? city) async {
    final trimmed = city?.trim();
    cityFilter = (trimmed == null || trimmed.isEmpty) ? null : trimmed;
    notifyListeners();
    final prefs = await SharedPreferences.getInstance();
    if (cityFilter == null) {
      await prefs.remove(_cityFilterPrefsKey);
    } else {
      await prefs.setString(_cityFilterPrefsKey, cityFilter!);
    }
  }
}
