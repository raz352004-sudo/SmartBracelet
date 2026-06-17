import 'package:flutter/material.dart';

import '../models/alert.dart';
import '../services/alert_history_service.dart';
import '../services/bracelet_ble_service.dart';
import '../services/notification_settings_service.dart';
import 'connection_screen.dart';

class HomeScreen extends StatefulWidget {
  final BraceletBleService bleService;
  final AlertHistoryService historyService;
  final NotificationSettingsService settingsService;

  const HomeScreen({
    super.key,
    required this.bleService,
    required this.historyService,
    required this.settingsService,
  });

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    widget.bleService.addListener(_onChange);
  }

  @override
  void dispose() {
    widget.bleService.removeListener(_onChange);
    super.dispose();
  }

  void _onChange() => setState(() {});

  Future<void> _sendManualAlert(AlertType type) async {
    if (type != AlertType.test && !widget.settingsService.isEnabled(type)) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('${type.label} מנוטרל בהגדרות')),
      );
      return;
    }
    await widget.bleService.sendCode(type.code);
    await widget.historyService.addEvent(type);
  }

  @override
  Widget build(BuildContext context) {
    final ble = widget.bleService;
    final connected = ble.state == BraceletConnectionState.connected;

    return Scaffold(
      appBar: AppBar(title: const Text('SmartBracelet')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Card(
              child: ListTile(
                leading: Icon(
                  connected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                  color: connected ? Colors.green : Colors.red,
                ),
                title: Text(connected ? 'מחובר לצמיד' : 'לא מחובר'),
                subtitle: connected && ble.batteryLevel != null
                    ? Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(
                            ble.isLowBattery ? Icons.battery_alert : Icons.battery_full,
                            size: 16,
                            color: ble.isLowBattery ? Colors.orange : null,
                          ),
                          const SizedBox(width: 4),
                          Text(
                            'סוללה: ${ble.batteryLevel}%',
                            style: ble.isLowBattery
                                ? const TextStyle(color: Colors.orange, fontWeight: FontWeight.bold)
                                : null,
                          ),
                        ],
                      )
                    : null,
                trailing: TextButton(
                  onPressed: () async {
                    if (connected) {
                      await ble.disconnect();
                    } else {
                      await Navigator.of(context).push(
                        MaterialPageRoute(
                          builder: (_) => ConnectionScreen(bleService: ble),
                        ),
                      );
                    }
                  },
                  child: Text(connected ? 'התנתק' : 'חיבור'),
                ),
              ),
            ),
            if (connected && ble.isLowBattery)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Card(
                  color: Colors.orange.withValues(alpha: 0.12),
                  child: const Padding(
                    padding: EdgeInsets.all(12),
                    child: Row(
                      children: [
                        Icon(Icons.warning_amber_rounded, color: Colors.orange),
                        SizedBox(width: 10),
                        Expanded(child: Text('סוללת הצמיד חלשה — מומלץ לטעון בקרוב')),
                      ],
                    ),
                  ),
                ),
              ),
            const SizedBox(height: 24),
            const Text('התראות לבדיקה', style: TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            ElevatedButton.icon(
              onPressed: connected ? () => _sendManualAlert(AlertType.door) : null,
              icon: const Icon(Icons.sensor_door),
              label: const Text('פעמון דלת'),
            ),
            const SizedBox(height: 8),
            ElevatedButton.icon(
              onPressed: connected ? () => _sendManualAlert(AlertType.intrusion) : null,
              icon: const Icon(Icons.security),
              label: const Text('גלאי פריצה'),
            ),
            const SizedBox(height: 8),
            ElevatedButton.icon(
              onPressed: connected ? () => _sendManualAlert(AlertType.test) : null,
              icon: const Icon(Icons.notifications_active),
              label: const Text('בדיקה'),
            ),
          ],
        ),
      ),
    );
  }
}
