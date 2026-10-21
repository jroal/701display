import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'ble_client.dart';
import 'dashboard.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations([
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
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
