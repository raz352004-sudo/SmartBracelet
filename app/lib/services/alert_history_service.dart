import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/alert.dart';

class AlertHistoryService extends ChangeNotifier {
  static const _prefsKey = 'alert_history';
  static const _maxEntries = 100;

  List<AlertEvent> events = [];

  Future<void> load() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList(_prefsKey) ?? [];
    events = raw
        .map((e) => AlertEvent.fromJson(jsonDecode(e) as Map<String, dynamic>))
        .toList();
    notifyListeners();
  }

  Future<void> addEvent(AlertType type) async {
    final event = AlertEvent(type: type, timestamp: DateTime.now());
    events.insert(0, event);
    if (events.length > _maxEntries) {
      events = events.sublist(0, _maxEntries);
    }
    notifyListeners();

    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(
      _prefsKey,
      events.map((e) => jsonEncode(e.toJson())).toList(),
    );
  }
}
