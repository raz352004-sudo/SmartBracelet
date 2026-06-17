import 'package:flutter/material.dart';

enum AlertType { alarm, door, intrusion, test }

extension AlertTypeInfo on AlertType {
  /// קוד חד-ספרתי שנשלח לצמיד דרך BLE
  String get code {
    switch (this) {
      case AlertType.alarm:
        return 'A';
      case AlertType.door:
        return 'D';
      case AlertType.intrusion:
        return 'I';
      case AlertType.test:
        return 'T';
    }
  }

  String get label {
    switch (this) {
      case AlertType.alarm:
        return 'אזעקת פיקוד העורף';
      case AlertType.door:
        return 'פעמון דלת';
      case AlertType.intrusion:
        return 'גלאי פריצה';
      case AlertType.test:
        return 'בדיקה';
    }
  }

  IconData get icon {
    switch (this) {
      case AlertType.alarm:
        return Icons.warning_amber_rounded;
      case AlertType.door:
        return Icons.sensor_door;
      case AlertType.intrusion:
        return Icons.security;
      case AlertType.test:
        return Icons.notifications_active;
    }
  }

  Color get color {
    switch (this) {
      case AlertType.alarm:
        return Colors.red;
      case AlertType.door:
        return Colors.blue;
      case AlertType.intrusion:
        return Colors.orange;
      case AlertType.test:
        return Colors.green;
    }
  }
}

class AlertEvent {
  final AlertType type;
  final DateTime timestamp;

  AlertEvent({required this.type, required this.timestamp});

  Map<String, dynamic> toJson() => {
        'type': type.name,
        'timestamp': timestamp.toIso8601String(),
      };

  factory AlertEvent.fromJson(Map<String, dynamic> json) => AlertEvent(
        type: AlertType.values.byName(json['type'] as String),
        timestamp: DateTime.parse(json['timestamp'] as String),
      );
}
