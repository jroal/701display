import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'telemetry.dart';

enum BleStatus { idle, needPermission, bluetoothOff, scanning, connecting, connected }

/// BLE client for the ESP32 display. Scans for the 701 service, connects,
/// subscribes to telemetry notifications and reconnects automatically.
class BleClient extends ChangeNotifier {
  static final Guid serviceUuid = Guid('7a010000-701d-4a5e-9f3b-0701d15b1e00');
  static final Guid telemetryUuid = Guid('7a010001-701d-4a5e-9f3b-0701d15b1e00');
  static final Guid commandUuid = Guid('7a010002-701d-4a5e-9f3b-0701d15b1e00');
  static const int cmdResetFuel = 0x01;

  BleStatus status = BleStatus.idle;
  Telemetry telemetry = const Telemetry();

  bool _running = false;
  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandChar;
  StreamSubscription<List<int>>? _valueSub;

  bool get connected => status == BleStatus.connected;

  void _setStatus(BleStatus s) {
    status = s;
    notifyListeners();
  }

  Future<void> start() async {
    if (_running) return;
    _running = true;
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
        if (FlutterBluePlus.adapterStateNow != BluetoothAdapterState.on) {
          _setStatus(BleStatus.bluetoothOff);
          await Future.delayed(const Duration(seconds: 2));
          continue;
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

  Future<BluetoothDevice?> _scan() async {
    _setStatus(BleStatus.scanning);
    final completer = Completer<BluetoothDevice?>();
    final sub = FlutterBluePlus.onScanResults.listen((results) {
      if (results.isNotEmpty && !completer.isCompleted) {
        completer.complete(results.last.device);
      }
    });
    await FlutterBluePlus.startScan(
      withServices: [serviceUuid],
      timeout: const Duration(seconds: 10),
    );
    final device = await completer.future.timeout(
      const Duration(seconds: 11),
      onTimeout: () => null,
    );
    await sub.cancel();
    await FlutterBluePlus.stopScan();
    return device;
  }

  Future<void> _connectAndRun(BluetoothDevice device) async {
    _setStatus(BleStatus.connecting);
    _device = device;
    await device.connect(timeout: const Duration(seconds: 15));
    try {
      final services = await device.discoverServices();
      final service = services.firstWhere((s) => s.uuid == serviceUuid);
      final tele = service.characteristics.firstWhere((c) => c.uuid == telemetryUuid);
      _commandChar = service.characteristics.firstWhere((c) => c.uuid == commandUuid);

      await _valueSub?.cancel();
      _valueSub = tele.onValueReceived.listen(_onData);
      await tele.setNotifyValue(true);
      final initial = await tele.read();
      _onData(initial);
      _setStatus(BleStatus.connected);

      await device.connectionState.firstWhere((s) => s == BluetoothConnectionState.disconnected);
    } finally {
      await _valueSub?.cancel();
      _valueSub = null;
      _commandChar = null;
      _device = null;
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
