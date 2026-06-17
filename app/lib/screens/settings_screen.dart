import 'package:flutter/material.dart';

import '../models/alert.dart';
import '../services/bracelet_ble_service.dart';
import '../services/notification_settings_service.dart';

class SettingsScreen extends StatefulWidget {
  final NotificationSettingsService settingsService;
  final BraceletBleService bleService;

  const SettingsScreen({
    super.key,
    required this.settingsService,
    required this.bleService,
  });

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  @override
  Widget build(BuildContext context) {
    final settings = widget.settingsService;

    return Scaffold(
      appBar: AppBar(title: const Text('הגדרות')),
      body: ListView(
        children: [
          ...configurableAlertTypes.map((type) {
            return SwitchListTile(
              title: Text(type.label),
              value: settings.isEnabled(type),
              onChanged: (value) {
                setState(() => settings.setEnabled(type, value));
              },
            );
          }),
          const Divider(),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
            child: Text(
              'עוצמת רטט',
              style: Theme.of(context).textTheme.titleMedium,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: Row(
              children: [
                const Icon(Icons.vibration),
                Expanded(
                  child: Slider(
                    value: settings.vibrationIntensityPercent.toDouble(),
                    min: 0,
                    max: 100,
                    divisions: 20,
                    label: '${settings.vibrationIntensityPercent}%',
                    onChanged: (value) {
                      setState(() {
                        settings.setVibrationIntensity(value.round());
                      });
                    },
                    onChangeEnd: (value) {
                      widget.bleService.setVibrationIntensity(value.round());
                    },
                  ),
                ),
                SizedBox(
                  width: 48,
                  child: Text(
                    '${settings.vibrationIntensityPercent}%',
                    textAlign: TextAlign.center,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
