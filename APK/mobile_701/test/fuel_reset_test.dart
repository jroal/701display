import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_701/ble_client.dart';
import 'package:mobile_701/dashboard.dart';
import 'package:mobile_701/l10n/app_localizations.dart';
import 'package:mobile_701/telemetry.dart';
import 'package:shared_preferences/shared_preferences.dart';

class FakeBleClient extends BleClient {
  final resetCompleter = Completer<void>();
  int resetCalls = 0;

  @override
  Future<void> resetFuel() {
    resetCalls++;
    return resetCompleter.future;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('mobile_701/fuel_resets');
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  setUp(() {
    SharedPreferences.setMockInitialValues({});
  });

  tearDown(() {
    messenger.setMockMethodCallHandler(channel, null);
  });

  Future<FakeBleClient> showDashboard(WidgetTester tester) async {
    final ble = FakeBleClient()
      ..status = BleStatus.connected
      ..telemetry = const Telemetry(mpg: 48.5, fuelLiters: 3.0);
    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Dashboard(ble: ble),
      ),
    );
    await tester.pump();
    return ble;
  }

  testWidgets('records tap-time telemetry once after a successful reset', (
    tester,
  ) async {
    final calls = <MethodCall>[];
    messenger.setMockMethodCallHandler(channel, (call) async {
      calls.add(call);
      return null;
    });
    final ble = await showDashboard(tester);
    final before = DateTime.now().toUtc();
    await tester.tap(find.text('Reset'));
    await tester.pump();
    ble.telemetry = const Telemetry(mpg: 0, fuelLiters: 13);
    await tester.tap(find.text('Reset'));
    expect(ble.resetCalls, 1);
    expect(calls, isEmpty);
    ble.resetCompleter.complete();
    await tester.pumpAndSettle();

    expect(calls, hasLength(1));
    expect(calls.single.method, 'append');
    final row = (calls.single.arguments as Map)['row'] as String;
    final fields = row.trim().split(',');
    expect(fields, hasLength(3));
    final timestamp = DateTime.parse(fields[0]);
    expect(timestamp.isUtc, isTrue);
    expect(timestamp.isBefore(before), isFalse);
    expect(timestamp.isAfter(DateTime.now().toUtc()), isFalse);
    expect(fields[1], '48.5');
    expect(fields[2], '0.7925');
  });

  testWidgets('does not save a record when the reset fails', (tester) async {
    final calls = <MethodCall>[];
    messenger.setMockMethodCallHandler(channel, (call) async {
      calls.add(call);
      return null;
    });
    final ble = await showDashboard(tester);
    await tester.tap(find.text('Reset'));
    ble.resetCompleter.completeError(StateError('disconnected'));
    await tester.pumpAndSettle();
    expect(calls, isEmpty);
    expect(
      find.text(
        'Fuel reset failed. Check the Bluetooth connection and try again.',
      ),
      findsOneWidget,
    );
  });

  testWidgets('warns if the reset succeeds but saving fails', (tester) async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      throw PlatformException(code: 'storage_error');
    });
    final ble = await showDashboard(tester);
    await tester.tap(find.text('Reset'));
    ble.resetCompleter.complete();
    await tester.pumpAndSettle();
    expect(
      find.text(
        'Fuel was reset, but the record could not be saved to Downloads/FuelResets.csv.',
      ),
      findsOneWidget,
    );
  });
}
