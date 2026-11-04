import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_701/add_fuel_dialog.dart';
import 'package:mobile_701/ble_client.dart';
import 'package:mobile_701/l10n/app_localizations.dart';
import 'package:mobile_701/units.dart';

void main() {
  testWidgets('add-fuel dialog fits a compact landscape screen', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(640, 320);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final ble = BleClient()
      ..status = BleStatus.connected
      ..displayInfo = const DisplayInfo(protocol: 3);

    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Builder(
          builder: (context) => Scaffold(
            body: Center(
              child: TextButton(
                onPressed: () =>
                    showAddFuelDialog(context, ble, UnitSystem.imperial),
                child: const Text('Add fuel'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('Add fuel'));
    await tester.pumpAndSettle();

    expect(find.text('Fuel to add'), findsOneWidget);
    final picker = tester.widget<CupertinoPicker>(find.byType(CupertinoPicker));
    expect(picker.scrollController!.selectedItem, 9);
    expect(tester.takeException(), isNull);
  });
}
