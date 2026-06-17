import 'package:flutter/material.dart';

import '../models/alert.dart';
import '../services/alert_history_service.dart';

class HistoryScreen extends StatefulWidget {
  final AlertHistoryService historyService;

  const HistoryScreen({super.key, required this.historyService});

  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen> {
  @override
  void initState() {
    super.initState();
    widget.historyService.addListener(_onChange);
  }

  @override
  void dispose() {
    widget.historyService.removeListener(_onChange);
    super.dispose();
  }

  void _onChange() => setState(() {});

  String _formatTimestamp(DateTime time) {
    String two(int n) => n.toString().padLeft(2, '0');
    return '${two(time.day)}/${two(time.month)}/${time.year} '
        '${two(time.hour)}:${two(time.minute)}:${two(time.second)}';
  }

  @override
  Widget build(BuildContext context) {
    final events = widget.historyService.events;

    return Scaffold(
      appBar: AppBar(title: const Text('היסטוריית התראות')),
      body: events.isEmpty
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.history, size: 56, color: Colors.grey.shade400),
                  const SizedBox(height: 12),
                  Text('אין התראות עדיין', style: TextStyle(color: Colors.grey.shade600)),
                ],
              ),
            )
          : ListView.separated(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              itemCount: events.length,
              separatorBuilder: (_, _) => const SizedBox(height: 8),
              itemBuilder: (context, index) {
                final event = events[index];
                return Card(
                  elevation: 0,
                  color: Theme.of(context).colorScheme.surfaceContainerHigh,
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
                  child: ListTile(
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
                    leading: CircleAvatar(
                      backgroundColor: event.type.color.withValues(alpha: 0.15),
                      child: Icon(event.type.icon, color: event.type.color),
                    ),
                    title: Text(event.type.label, style: const TextStyle(fontWeight: FontWeight.w600)),
                    subtitle: Text(_formatTimestamp(event.timestamp)),
                  ),
                );
              },
            ),
    );
  }
}
