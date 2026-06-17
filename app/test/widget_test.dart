import 'package:flutter_test/flutter_test.dart';

import 'package:smart_bracelet/main.dart';

void main() {
  testWidgets('App builds without throwing', (WidgetTester tester) async {
    await tester.pumpWidget(const SmartBraceletApp());
    expect(find.text('SmartBracelet'), findsOneWidget);
  });
}
