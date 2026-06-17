import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

/// פולינג על ה-API הציבורי (לא רשמי) של פיקוד העורף.
/// עובד רק כשמריצים אותו מ-IP ישראלי — השירות חוסם בקשות מחוץ לישראל.
class PikudHaorefService extends ChangeNotifier {
  static const _alertsUrl = 'https://www.oref.org.il/WarningMessages/alert/alerts.json';
  static const _pollInterval = Duration(seconds: 5);

  final VoidCallback onAlarm;

  PikudHaorefService({required this.onAlarm});

  Timer? _timer;
  String? _lastNotificationId;
  bool isPolling = false;
  String? lastError;

  /// שם היישוב שמסנן את ההתראות — null/ריק = להגיב לכל התרעה בארץ.
  /// ההשוואה היא "מכיל" (contains), כדי להתמודד עם שמות כמו "תל אביב - מרכז".
  String? targetCity;

  void start() {
    if (isPolling) return;
    isPolling = true;
    notifyListeners();
    unawaited(_poll());
    _timer = Timer.periodic(_pollInterval, (_) => _poll());
  }

  void stop() {
    _timer?.cancel();
    _timer = null;
    isPolling = false;
    notifyListeners();
  }

  Future<void> _poll() async {
    try {
      final response = await http.get(
        Uri.parse(_alertsUrl),
        headers: {
          'Referer': 'https://www.oref.org.il/',
          'X-Requested-With': 'XMLHttpRequest',
        },
      );

      final body = response.body.trim();
      if (lastError != null) {
        lastError = null;
        notifyListeners();
      }
      if (body.isEmpty || body == '{}') return; // אין אזעקה פעילה כרגע

      final data = jsonDecode(body) as Map<String, dynamic>;
      final id = data['notificationId']?.toString() ?? data['time']?.toString();
      if (id == null || id == _lastNotificationId) return;
      _lastNotificationId = id;

      if (_matchesCityFilter(data)) {
        onAlarm();
      }
    } catch (e) {
      lastError = e.toString();
      notifyListeners();
    }
  }

  /// מחלץ את רשימת היישובים מההתראה (שם השדה משתנה בין גרסאות ה-API הלא-רשמי)
  /// ובודק אם היישוב שנבחר בהגדרות נמצא בה. בלי סינון מוגדר — תמיד true.
  bool _matchesCityFilter(Map<String, dynamic> data) {
    final city = targetCity;
    if (city == null || city.isEmpty) return true;

    final rawCities = data['cities'] ?? data['data'];
    if (rawCities is! List) return true; // אין מידע על יישובים — לא חוסמים התרעה

    final cities = rawCities.map((c) => c.toString());
    return cities.any((c) => c.contains(city) || city.contains(c));
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}
