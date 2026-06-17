import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:smart_bracelet/services/pikud_haoref_service.dart';

void main() {
  group('PikudHaorefService JSON parsing', () {
    test('no-alert response (BOM + CRLF) is treated as empty, not an error', () {
      // הבייטים האמיתיים שחוזרים מ-oref.org.il כשאין אזעקה פעילה: EF BB BF 0D 0A
      final raw = utf8.decode([0xEF, 0xBB, 0xBF, 0x0D, 0x0A], allowMalformed: true);
      final body = raw.replaceFirst('﻿', '').trim();
      expect(body.isEmpty, isTrue);
    });

    test('real alert schema exposes id and data fields correctly', () {
      const sample = '''
      {"id": 1621242007417, "cat": "1", "title": "התרעות פיקוד העורף",
       "data": ["סעד", "אשדוד - יא,יב,טו,יז,מרינה"]}
      ''';
      final data = jsonDecode(sample) as Map<String, dynamic>;

      expect(data['id'].toString(), '1621242007417');
      expect(data['data'], isA<List>());
      expect((data['data'] as List).map((c) => c.toString()), contains('סעד'));
    });

    test('city filter matches a city contained in the alert data array', () {
      final service = PikudHaorefService(onAlarm: () {});
      service.targetCity = 'אשדוד';

      const sample = '''
      {"id": 1, "data": ["סעד", "אשדוד - יא,יב,טו,יז,מרינה"]}
      ''';
      final data = jsonDecode(sample) as Map<String, dynamic>;

      expect(service.matchesCityFilterForTest(data), isTrue);
    });

    test('city filter rejects an alert for a different city', () {
      final service = PikudHaorefService(onAlarm: () {});
      service.targetCity = 'תל אביב';

      const sample = '{"id": 1, "data": ["סעד"]}';
      final data = jsonDecode(sample) as Map<String, dynamic>;

      expect(service.matchesCityFilterForTest(data), isFalse);
    });
  });
}
