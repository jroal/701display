import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'telemetry.dart';

enum BleStatus {
  idle,
  needPermission,
  bluetoothOff,
  scanning,
  connecting,
  connected,
}

class NearbyDevice {
  const NearbyDevice({
    required this.selectionId,
    required this.address,
    required this.name,
    required this.rssi,
    required this.lastSeen,
  });

  final String selectionId;
  final String address;
  final String name;
  final int rssi;
  final DateTime lastSeen;
}

class _ScannedDevice {
  const _ScannedDevice(this.device, this.selectionId);
  final BluetoothDevice device;
  final String selectionId;
}

/// BLE client for the ESP32 display. Scans for the 701 service, connects,
/// subscribes to telemetry notifications and reconnects automatically.
class BleClient extends ChangeNotifier {
  static final Guid serviceUuid = Guid('7a010000-701d-4a5e-9f3b-0701d15b1e00');
  static final Guid telemetryUuid = Guid(
    '7a010001-701d-4a5e-9f3b-0701d15b1e00',
  );
  static final Guid commandUuid = Guid('7a010002-701d-4a5e-9f3b-0701d15b1e00');
  static const int cmdResetFuel = 0x01;

  BleStatus status = BleStatus.idle;
  Telemetry telemetry = const Telemetry();

  bool _running = false;
  BluetoothDevice? _device;
  String? _activeSelectionId;
  BluetoothCharacteristic? _commandChar;
  StreamSubscription<List<int>>? _valueSub;

  bool get connected => status == BleStatus.connected;

  String? get connectedId => _activeSelectionId ?? _device?.remoteId.str;

  void _setStatus(BleStatus s) {
    status = s;
    notifyListeners();
  }

  Future<void> start() async {
    if (_running) return;
    _running = true;
    await _loadSelection();
    unawaited(_loop());
  }

  Future<void> _loop() async {
    while (_running) {
      try {
        if (!await _ensurePermissions()) {
          _setStatus(BleStatus.needPermission);
          await Future.delayed(const Duration(seconds: 3));
          continue;
        }
        // The adapter state is "unknown" until the first stream event, and stays
        // unknown/unauthorized until permissions are granted. Only report
        // "off" when the adapter explicitly says so.
        final state = await FlutterBluePlus.adapterState
            .firstWhere((s) => s != BluetoothAdapterState.unknown)
            .timeout(
              const Duration(seconds: 3),
              onTimeout: () => BluetoothAdapterState.unknown,
            );
        if (state == BluetoothAdapterState.off) {
          _setStatus(BleStatus.bluetoothOff);
          await Future.delayed(const Duration(seconds: 2));
          continue;
        }
        if (state == BluetoothAdapterState.unauthorized) {
          _setStatus(BleStatus.needPermission);
        }
        final device = await _scan();
        if (device == null) continue;
        await _connectAndRun(device);
      } catch (e) {
        debugPrint('BLE error: $e');
        await Future.delayed(const Duration(seconds: 2));
      }
    }
  }

  Future<bool> _ensurePermissions() async {
    // flutter_blue_plus prompts for the Bluetooth permissions on the first scan/connect.
    return true;
  }

  /// Short summary of the last scan, shown in the UI to diagnose discovery problems.
  String scanInfo = '';

  static const _deviceKey = 'ble_device_id';

  /// Advertised display name to connect to; null connects to the first one found.
  String? selectedId;

  /// 701 displays seen recently, keyed by their stable advertised name or address.
  final Map<String, NearbyDevice> nearby = {};

  Future<void> _loadSelection() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      selectedId = prefs.getString(_deviceKey);
      notifyListeners();
    } catch (_) {}
  }

  /// Chooses which display to connect to (null = any) and persists the choice.
  Future<void> selectDevice(String? id) async {
    if (id == selectedId) return;
    selectedId = id;
    notifyListeners();
    try {
      final prefs = await SharedPreferences.getInstance();
      if (id == null) {
        await prefs.remove(_deviceKey);
      } else {
        await prefs.setString(_deviceKey, id);
      }
    } catch (_) {}
    // Drop the current link if it is not the newly selected display; the loop reconnects.
    final current = _device;
    if (current != null &&
        id != null &&
        id != _activeSelectionId &&
        current.remoteId.str != id) {
      try {
        await current.disconnect();
      } catch (_) {}
    }
  }

  String _advertisedName(ScanResult r) {
    final name = r.advertisementData.advName.trim();
    return name.isNotEmpty ? name : r.device.platformName.trim();
  }

  bool _isTarget(ScanResult r) {
    final name = _advertisedName(r);
    return r.advertisementData.serviceUuids.contains(serviceUuid) ||
        name == 'Husky701' ||
        name.startsWith('Husky701-');
  }

  Future<_ScannedDevice?> _scan() async {
    _setStatus(BleStatus.scanning);
    final completer = Completer<_ScannedDevice?>();
    var seen = 0;
    final sub = FlutterBluePlus.onScanResults.listen((results) {
      seen = results.length;
      scanInfo = '$seen devices seen';
      final now = DateTime.now();
      for (final r in results) {
        if (!_isTarget(r)) continue;
        final address = r.device.remoteId.str;
        final name = _advertisedName(r);
        final selectionId = name.startsWith('Husky701-') ? name : address;
        nearby[selectionId] = NearbyDevice(
          selectionId: selectionId,
          address: address,
          name: name.isEmpty ? 'Husky701' : name,
          rssi: r.rssi,
          lastSeen: now,
        );
        if (selectionId != address) {
          nearby.removeWhere(
            (key, device) => key != selectionId && device.address == address,
          );
        }
        final wanted =
            selectedId == null ||
            selectedId == selectionId ||
            selectedId == address;
        if (selectedId == address && selectionId != address) {
          unawaited(selectDevice(selectionId));
        }
        if (wanted && !completer.isCompleted) {
          completer.complete(_ScannedDevice(r.device, selectionId));
        }
      }
      nearby.removeWhere((_, d) => now.difference(d.lastSeen).inSeconds > 30);
      notifyListeners();
    });
    // No service filter: some stacks drop 128-bit UUID filters; match by UUID or name instead.
    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));
    final device = await completer.future.timeout(
      const Duration(seconds: 1),
      onTimeout: () => null,
    );
    await sub.cancel();
    await FlutterBluePlus.stopScan();
    final cutoff = DateTime.now().subtract(const Duration(seconds: 30));
    nearby.removeWhere((_, d) => d.lastSeen.isBefore(cutoff));
    notifyListeners();
    return device;
  }

  Future<void> _connectAndRun(_ScannedDevice scanned) async {
    _setStatus(BleStatus.connecting);
    final device = scanned.device;
    _device = device;
    _activeSelectionId = scanned.selectionId;
    await device.connect(timeout: const Duration(seconds: 15));
    try {
      final services = await device.discoverServices();
      final service = services.firstWhere((s) => s.uuid == serviceUuid);
      final tele = service.characteristics.firstWhere(
        (c) => c.uuid == telemetryUuid,
      );
      _commandChar = service.characteristics.firstWhere(
        (c) => c.uuid == commandUuid,
      );

      await _valueSub?.cancel();
      _valueSub = tele.onValueReceived.listen(_onData);
      await tele.setNotifyValue(true);
      final initial = await tele.read();
      _onData(initial);
      _setStatus(BleStatus.connected);

      await device.connectionState.firstWhere(
        (s) => s == BluetoothConnectionState.disconnected,
      );
    } finally {
      await _valueSub?.cancel();
      _valueSub = null;
      _commandChar = null;
      _device = null;
      _activeSelectionId = null;
      try {
        await device.disconnect();
      } catch (_) {}
      if (_running) _setStatus(BleStatus.scanning);
    }
  }

  void _onData(List<int> data) {
    final t = Telemetry.parse(data);
    if (t == null) return;
    telemetry = t;
    notifyListeners();
  }

  /// Equivalent of pressing Reset on the ESP32 display (refills the fuel tank).
  Future<void> resetFuel() async {
    final c = _commandChar;
    if (c == null) return;
    await c.write([cmdResetFuel]);
  }

  @override
  void dispose() {
    _running = false;
    _device?.disconnect();
    super.dispose();
  }
}
