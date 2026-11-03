// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appName => 'Mobile 701';

  @override
  String get overlayNotificationTitle => '701 Display';

  @override
  String get overlayNotificationContent => 'Showing fuel range';

  @override
  String get settingsTitle => 'Settings';

  @override
  String get settingsTooltip => 'Settings';

  @override
  String get aboutTitle => 'About';

  @override
  String get close => 'Close';

  @override
  String get keepDisplayOn => 'Keep display ON while using';

  @override
  String get showRangeOverlay => 'Show fuel range over other apps';

  @override
  String get showRangeOverlayHint =>
      'Floating window while this app is in the background';

  @override
  String get displayToConnectTo => '701 display to connect to';

  @override
  String get anyDisplay => 'Any (first one found)';

  @override
  String get savedDisplay => 'Saved display';

  @override
  String get notInRange => 'Not in range';

  @override
  String deviceDetails(String arg1, int arg2) {
    return '$arg1  |  $arg2 dBm';
  }

  @override
  String deviceDetailsConnected(String arg1, int arg2) {
    return '$arg1  |  $arg2 dBm  |  connected';
  }

  @override
  String aboutRow(String arg1, String arg2) {
    return '$arg1: $arg2';
  }

  @override
  String get aboutAppVersion => 'App version';

  @override
  String get aboutAppProtocol => 'App BLE protocol';

  @override
  String get aboutDisplayFirmware => 'Display firmware';

  @override
  String get aboutDisplayBuild => 'Display build';

  @override
  String get aboutDisplayProtocol => 'Display BLE protocol';

  @override
  String get aboutUnknown => 'unknown';

  @override
  String get aboutOlderThanFirmware => 'older than 1.0.0';

  @override
  String get aboutConnectToSeeVersions =>
      'Connect to a display to see its versions.';

  @override
  String fuelGallons(String arg1) {
    return 'FUEL: $arg1 gal';
  }

  @override
  String tempFahrenheit(String arg1) {
    return 'TEMP: $arg1 F';
  }

  @override
  String get gearNeutral => 'GEAR: N';

  @override
  String gearNumber(int arg1) {
    return 'GEAR: $arg1';
  }

  @override
  String rpmValue(int arg1) {
    return '$arg1 RPM';
  }

  @override
  String economy(String arg1) {
    return 'ECON: $arg1';
  }

  @override
  String rangeMilesLabel(String arg1) {
    return 'RANGE: $arg1 mi';
  }

  @override
  String get rangeUnknownLabel => 'RANGE: -- mi';

  @override
  String rangeMilesShort(String arg1) {
    return '$arg1 mi';
  }

  @override
  String get rangeUnknownShort => '-- mi';

  @override
  String get rangeTitle => 'RANGE';

  @override
  String get absOff => 'ABS: OFF';

  @override
  String get absOffRoad => 'ABS: OFF-Road';

  @override
  String get absOn => 'ABS: ON';

  @override
  String get standDown => 'STAND: DOWN';

  @override
  String get standUp => 'STAND: UP';

  @override
  String get standUnknown => 'STAND: --';

  @override
  String get offRoadAbs => 'Off-road ABS';

  @override
  String get canLabel => 'CAN';

  @override
  String fuelPercent(int arg1) {
    return '$arg1%';
  }

  @override
  String get fuelMarkerEmpty => 'E';

  @override
  String get fuelMarkerQuarter => '1/4';

  @override
  String get fuelMarkerHalf => '1/2';

  @override
  String get fuelMarkerThreeQuarter => '3/4';

  @override
  String get fuelMarkerFull => 'F';

  @override
  String get reset => 'Reset';

  @override
  String get cancel => 'Cancel';

  @override
  String get addFuelTitle => 'Add fuel';

  @override
  String get addFuelPrompt => 'Fuel to add';

  @override
  String addFuelAmountLiters(String arg1) {
    return '$arg1 L';
  }

  @override
  String addFuelAmountGallons(String arg1) {
    return '$arg1 gal';
  }

  @override
  String addFuelNewTotal(String arg1) {
    return 'New total: $arg1';
  }

  @override
  String get addFuelConfirm => 'Add';

  @override
  String get addFuelTankFull => 'The tank is full.';

  @override
  String get addFuelNotConnected => 'Connect to a display to add fuel.';

  @override
  String get addFuelUnsupported =>
      'This display\'s firmware does not support adding fuel. Update it to version 1.1.0 or later.';

  @override
  String get unitsTitle => 'Units';

  @override
  String get unitsImperial => 'English (mi, gal, °F)';

  @override
  String get unitsMetric => 'Metric (km, L, °C)';

  @override
  String fuelLiters(String arg1) {
    return 'FUEL: $arg1 L';
  }

  @override
  String tempCelsius(String arg1) {
    return 'TEMP: $arg1 °C';
  }

  @override
  String economyMetric(String arg1) {
    return 'ECON: $arg1 L/100km';
  }

  @override
  String rangeKmLabel(String arg1) {
    return 'RANGE: $arg1 km';
  }

  @override
  String rangeKmShort(String arg1) {
    return '$arg1 km';
  }

  @override
  String get rangeUnknownKmLabel => 'RANGE: -- km';

  @override
  String get rangeUnknownKmShort => '-- km';

  @override
  String get bleIdle => 'BLE idle';

  @override
  String get blePermissionNeeded => 'BLE: permission needed';

  @override
  String get bleBluetoothOff => 'BLE: Bluetooth off';

  @override
  String get bleScanning => 'BLE: searching...';

  @override
  String get bleConnecting => 'BLE: connecting...';

  @override
  String get bleConnected => 'BLE: connected';

  @override
  String bleScanningWithCount(int arg1) {
    String _temp0 = intl.Intl.pluralLogic(
      arg1,
      locale: localeName,
      other: 'BLE: searching... ($arg1 devices seen)',
      one: 'BLE: searching... ($arg1 device seen)',
    );
    return '$_temp0';
  }
}
