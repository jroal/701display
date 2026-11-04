// Dev-only preview of the add-fuel dialog; no BLE or device needed.
// Run: flutter run -d chrome -t lib/dialog_preview.dart
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import 'add_fuel_dialog.dart';
import 'ble_client.dart';
import 'l10n/app_localizations.dart';
import 'telemetry.dart';
import 'units.dart';

class _FakeBle extends BleClient {
  _FakeBle() {
    status = BleStatus.connected;
    displayInfo = const DisplayInfo(protocol: 3);
  }

  @override
  Future<void> addFuel(double liters) async {
    debugPrint('addFuel($liters L)');
  }
}

void main() => runApp(const _Preview());

class _Preview extends StatefulWidget {
  const _Preview();

  @override
  State<_Preview> createState() => _PreviewState();
}

class _PreviewState extends State<_Preview> {
  final _ble = _FakeBle();
  var _units = UnitSystem.imperial;
  var _fuelL = 5.0;

  @override
  Widget build(BuildContext context) {
    _ble.telemetry = Telemetry(fuelLiters: _fuelL);
    return MaterialApp(
      theme: ThemeData.dark(useMaterial3: true),
      scrollBehavior: const MaterialScrollBehavior().copyWith(
        dragDevices: {...PointerDeviceKind.values},
      ),
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Builder(
        builder: (context) => Scaffold(
          body: Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                SegmentedButton<UnitSystem>(
                  segments: const [
                    ButtonSegment(
                      value: UnitSystem.imperial,
                      label: Text('Gallons'),
                    ),
                    ButtonSegment(
                      value: UnitSystem.metric,
                      label: Text('Liters'),
                    ),
                  ],
                  selected: {_units},
                  onSelectionChanged: (s) => setState(() => _units = s.first),
                ),
                const SizedBox(height: 16),
                Text('Current fuel: ${_fuelL.toStringAsFixed(1)} L'),
                SizedBox(
                  width: 300,
                  child: Slider(
                    value: _fuelL,
                    max: Telemetry.tankCapacityLiters,
                    onChanged: (v) => setState(() => _fuelL = v),
                  ),
                ),
                const SizedBox(height: 16),
                FilledButton(
                  onPressed: () => showAddFuelDialog(context, _ble, _units),
                  child: const Text('Open add-fuel dialog'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
