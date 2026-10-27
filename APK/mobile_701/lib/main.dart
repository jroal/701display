import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'ble_client.dart';
import 'dashboard.dart';
import 'range_overlay.dart';

/// Entry point of the floating range overlay's Flutter engine. The plugin looks
/// it up by name in this library.
@pragma('vm:entry-point')
void overlayMain() {
  runApp(
    const MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Material(color: Colors.transparent, child: RangeOverlayView()),
    ),
  );
}

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations([
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  // Hide the status bar; keep the navigation bar visible while the app is in use.
  await SystemChrome.setEnabledSystemUIMode(
    SystemUiMode.manual,
    overlays: [SystemUiOverlay.bottom],
  );
  runApp(const Mobile701App());
}

class Mobile701App extends StatefulWidget {
  const Mobile701App({super.key, this.ble});

  /// Injectable for tests; defaults to a real BLE client that starts scanning.
  final BleClient? ble;

  @override
  State<Mobile701App> createState() => _Mobile701AppState();
}

class _Mobile701AppState extends State<Mobile701App> {
  late final BleClient _ble = widget.ble ?? (BleClient()..start());

  @override
  void dispose() {
    _ble.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Mobile 701',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true),
      home: Dashboard(ble: _ble),
    );
  }
}
