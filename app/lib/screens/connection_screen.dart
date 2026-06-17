import 'package:flutter/material.dart';

import '../services/bracelet_ble_service.dart';

class ConnectionScreen extends StatefulWidget {
  final BraceletBleService bleService;

  const ConnectionScreen({super.key, required this.bleService});

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen>
    with SingleTickerProviderStateMixin {
  late final AnimationController _pulseController;

  @override
  void initState() {
    super.initState();
    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 1),
    )..repeat(reverse: true);
    widget.bleService.addListener(_onChange);
    widget.bleService.startScan();
  }

  @override
  void dispose() {
    widget.bleService.removeListener(_onChange);
    widget.bleService.stopScan();
    _pulseController.dispose();
    super.dispose();
  }

  void _onChange() => setState(() {});

  IconData _signalIcon(int rssi) {
    if (rssi >= -60) return Icons.signal_cellular_alt;
    if (rssi >= -80) return Icons.signal_cellular_alt_2_bar;
    return Icons.signal_cellular_alt_1_bar;
  }

  @override
  Widget build(BuildContext context) {
    final ble = widget.bleService;
    final isScanning = ble.state == BraceletConnectionState.scanning;

    return Scaffold(
      appBar: AppBar(title: const Text('חיבור לצמיד')),
      body: Column(
        children: [
          if (isScanning) const LinearProgressIndicator(minHeight: 3),
          Padding(
            padding: const EdgeInsets.fromLTRB(20, 20, 20, 8),
            child: Row(
              children: [
                ScaleTransition(
                  scale: isScanning
                      ? Tween(begin: 0.85, end: 1.15).animate(
                          CurvedAnimation(
                            parent: _pulseController,
                            curve: Curves.easeInOut,
                          ),
                        )
                      : const AlwaysStoppedAnimation(1),
                  child: CircleAvatar(
                    radius: 22,
                    backgroundColor: Theme.of(context).colorScheme.primaryContainer,
                    child: Icon(
                      Icons.bluetooth_searching,
                      color: Theme.of(context).colorScheme.primary,
                    ),
                  ),
                ),
                const SizedBox(width: 14),
                Expanded(
                  child: Text(
                    isScanning
                        ? 'מחפש צמידים בקרבת מקום...'
                        : ble.scanResults.isEmpty
                            ? 'לא נמצאו צמידים'
                            : 'נמצאו ${ble.scanResults.length} צמידים',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
              ],
            ),
          ),
          Expanded(
            child: ble.scanResults.isEmpty
                ? Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.watch, size: 56, color: Colors.grey.shade400),
                        const SizedBox(height: 12),
                        Text(
                          isScanning ? 'מחפש...' : 'ודא שהצמיד דלוק וקרוב למכשיר',
                          style: TextStyle(color: Colors.grey.shade600),
                        ),
                      ],
                    ),
                  )
                : ListView.separated(
                    padding: const EdgeInsets.symmetric(horizontal: 16),
                    itemCount: ble.scanResults.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 8),
                    itemBuilder: (context, index) {
                      final result = ble.scanResults[index];
                      final name = result.device.platformName.isNotEmpty
                          ? result.device.platformName
                          : 'מכשיר ללא שם';
                      return Card(
                        elevation: 0,
                        color: Theme.of(context).colorScheme.surfaceContainerHigh,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(14),
                        ),
                        child: ListTile(
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(14),
                          ),
                          leading: CircleAvatar(
                            backgroundColor: Theme.of(context).colorScheme.primary,
                            child: const Icon(Icons.watch, color: Colors.white),
                          ),
                          title: Text(name, style: const TextStyle(fontWeight: FontWeight.w600)),
                          subtitle: Text(result.device.remoteId.str),
                          trailing: Icon(
                            _signalIcon(result.rssi),
                            color: Theme.of(context).colorScheme.primary,
                          ),
                          onTap: () async {
                            try {
                              await ble.connect(result.device);
                              if (context.mounted) Navigator.of(context).pop();
                            } catch (e) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(content: Text('החיבור נכשל: $e')),
                                );
                              }
                            }
                          },
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: isScanning ? null : () => ble.startScan(),
        child: const Icon(Icons.refresh),
      ),
    );
  }
}
