import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_en.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations)!;
  }

  static const LocalizationsDelegate<AppLocalizations> delegate =
      _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[Locale('en')];

  /// No description provided for @appName.
  ///
  /// In en, this message translates to:
  /// **'Mobile 701'**
  String get appName;

  /// No description provided for @overlayNotificationTitle.
  ///
  /// In en, this message translates to:
  /// **'701 Display'**
  String get overlayNotificationTitle;

  /// No description provided for @overlayNotificationContent.
  ///
  /// In en, this message translates to:
  /// **'Showing fuel range'**
  String get overlayNotificationContent;

  /// No description provided for @settingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get settingsTitle;

  /// No description provided for @settingsTooltip.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get settingsTooltip;

  /// No description provided for @aboutTitle.
  ///
  /// In en, this message translates to:
  /// **'About'**
  String get aboutTitle;

  /// No description provided for @close.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get close;

  /// No description provided for @keepDisplayOn.
  ///
  /// In en, this message translates to:
  /// **'Keep display ON while using'**
  String get keepDisplayOn;

  /// No description provided for @showRangeOverlay.
  ///
  /// In en, this message translates to:
  /// **'Show fuel range over other apps'**
  String get showRangeOverlay;

  /// No description provided for @showRangeOverlayHint.
  ///
  /// In en, this message translates to:
  /// **'Floating window while this app is in the background'**
  String get showRangeOverlayHint;

  /// No description provided for @displayToConnectTo.
  ///
  /// In en, this message translates to:
  /// **'701 display to connect to'**
  String get displayToConnectTo;

  /// No description provided for @anyDisplay.
  ///
  /// In en, this message translates to:
  /// **'Any (first one found)'**
  String get anyDisplay;

  /// No description provided for @savedDisplay.
  ///
  /// In en, this message translates to:
  /// **'Saved display'**
  String get savedDisplay;

  /// No description provided for @notInRange.
  ///
  /// In en, this message translates to:
  /// **'Not in range'**
  String get notInRange;

  /// No description provided for @deviceDetails.
  ///
  /// In en, this message translates to:
  /// **'{arg1}  |  {arg2} dBm'**
  String deviceDetails(String arg1, int arg2);

  /// No description provided for @deviceDetailsConnected.
  ///
  /// In en, this message translates to:
  /// **'{arg1}  |  {arg2} dBm  |  connected'**
  String deviceDetailsConnected(String arg1, int arg2);

  /// No description provided for @aboutRow.
  ///
  /// In en, this message translates to:
  /// **'{arg1}: {arg2}'**
  String aboutRow(String arg1, String arg2);

  /// No description provided for @aboutAppVersion.
  ///
  /// In en, this message translates to:
  /// **'App version'**
  String get aboutAppVersion;

  /// No description provided for @aboutAppProtocol.
  ///
  /// In en, this message translates to:
  /// **'App BLE protocol'**
  String get aboutAppProtocol;

  /// No description provided for @aboutDisplayFirmware.
  ///
  /// In en, this message translates to:
  /// **'Display firmware'**
  String get aboutDisplayFirmware;

  /// No description provided for @aboutDisplayBuild.
  ///
  /// In en, this message translates to:
  /// **'Display build'**
  String get aboutDisplayBuild;

  /// No description provided for @aboutDisplayProtocol.
  ///
  /// In en, this message translates to:
  /// **'Display BLE protocol'**
  String get aboutDisplayProtocol;

  /// No description provided for @aboutUnknown.
  ///
  /// In en, this message translates to:
  /// **'unknown'**
  String get aboutUnknown;

  /// No description provided for @aboutOlderThanFirmware.
  ///
  /// In en, this message translates to:
  /// **'older than 1.0.0'**
  String get aboutOlderThanFirmware;

  /// No description provided for @aboutConnectToSeeVersions.
  ///
  /// In en, this message translates to:
  /// **'Connect to a display to see its versions.'**
  String get aboutConnectToSeeVersions;

  /// No description provided for @fuelGallons.
  ///
  /// In en, this message translates to:
  /// **'FUEL: {arg1} gal'**
  String fuelGallons(String arg1);

  /// No description provided for @tempFahrenheit.
  ///
  /// In en, this message translates to:
  /// **'TEMP: {arg1} F'**
  String tempFahrenheit(String arg1);

  /// No description provided for @gearNeutral.
  ///
  /// In en, this message translates to:
  /// **'GEAR: N'**
  String get gearNeutral;

  /// No description provided for @gearNumber.
  ///
  /// In en, this message translates to:
  /// **'GEAR: {arg1}'**
  String gearNumber(int arg1);

  /// No description provided for @rpmValue.
  ///
  /// In en, this message translates to:
  /// **'{arg1} RPM'**
  String rpmValue(int arg1);

  /// No description provided for @economy.
  ///
  /// In en, this message translates to:
  /// **'ECON: {arg1}'**
  String economy(String arg1);

  /// No description provided for @rangeMilesLabel.
  ///
  /// In en, this message translates to:
  /// **'RANGE: {arg1} mi'**
  String rangeMilesLabel(String arg1);

  /// No description provided for @rangeUnknownLabel.
  ///
  /// In en, this message translates to:
  /// **'RANGE: -- mi'**
  String get rangeUnknownLabel;

  /// No description provided for @rangeMilesShort.
  ///
  /// In en, this message translates to:
  /// **'{arg1} mi'**
  String rangeMilesShort(String arg1);

  /// No description provided for @rangeUnknownShort.
  ///
  /// In en, this message translates to:
  /// **'-- mi'**
  String get rangeUnknownShort;

  /// No description provided for @rangeTitle.
  ///
  /// In en, this message translates to:
  /// **'RANGE'**
  String get rangeTitle;

  /// No description provided for @absOff.
  ///
  /// In en, this message translates to:
  /// **'ABS: OFF'**
  String get absOff;

  /// No description provided for @absOffRoad.
  ///
  /// In en, this message translates to:
  /// **'ABS: OFF-Road'**
  String get absOffRoad;

  /// No description provided for @absOn.
  ///
  /// In en, this message translates to:
  /// **'ABS: ON'**
  String get absOn;

  /// No description provided for @standDown.
  ///
  /// In en, this message translates to:
  /// **'STAND: DOWN'**
  String get standDown;

  /// No description provided for @standUp.
  ///
  /// In en, this message translates to:
  /// **'STAND: UP'**
  String get standUp;

  /// No description provided for @standUnknown.
  ///
  /// In en, this message translates to:
  /// **'STAND: --'**
  String get standUnknown;

  /// No description provided for @offRoadAbs.
  ///
  /// In en, this message translates to:
  /// **'Off-road ABS'**
  String get offRoadAbs;

  /// No description provided for @canLabel.
  ///
  /// In en, this message translates to:
  /// **'CAN'**
  String get canLabel;

  /// No description provided for @fuelPercent.
  ///
  /// In en, this message translates to:
  /// **'{arg1}%'**
  String fuelPercent(int arg1);

  /// No description provided for @fuelMarkerEmpty.
  ///
  /// In en, this message translates to:
  /// **'E'**
  String get fuelMarkerEmpty;

  /// No description provided for @fuelMarkerQuarter.
  ///
  /// In en, this message translates to:
  /// **'1/4'**
  String get fuelMarkerQuarter;

  /// No description provided for @fuelMarkerHalf.
  ///
  /// In en, this message translates to:
  /// **'1/2'**
  String get fuelMarkerHalf;

  /// No description provided for @fuelMarkerThreeQuarter.
  ///
  /// In en, this message translates to:
  /// **'3/4'**
  String get fuelMarkerThreeQuarter;

  /// No description provided for @fuelMarkerFull.
  ///
  /// In en, this message translates to:
  /// **'F'**
  String get fuelMarkerFull;

  /// No description provided for @reset.
  ///
  /// In en, this message translates to:
  /// **'Reset'**
  String get reset;

  /// No description provided for @cancel.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get cancel;

  /// No description provided for @addFuelTitle.
  ///
  /// In en, this message translates to:
  /// **'Add fuel'**
  String get addFuelTitle;

  /// No description provided for @addFuelPrompt.
  ///
  /// In en, this message translates to:
  /// **'Fuel to add'**
  String get addFuelPrompt;

  /// No description provided for @addFuelAmountLiters.
  ///
  /// In en, this message translates to:
  /// **'{arg1} L'**
  String addFuelAmountLiters(String arg1);

  /// No description provided for @addFuelAmountGallons.
  ///
  /// In en, this message translates to:
  /// **'{arg1} gal'**
  String addFuelAmountGallons(String arg1);

  /// No description provided for @addFuelNewTotal.
  ///
  /// In en, this message translates to:
  /// **'New total: {arg1}'**
  String addFuelNewTotal(String arg1);

  /// No description provided for @addFuelConfirm.
  ///
  /// In en, this message translates to:
  /// **'Add'**
  String get addFuelConfirm;

  /// No description provided for @addFuelTankFull.
  ///
  /// In en, this message translates to:
  /// **'The tank is full.'**
  String get addFuelTankFull;

  /// No description provided for @addFuelNotConnected.
  ///
  /// In en, this message translates to:
  /// **'Connect to a display to add fuel.'**
  String get addFuelNotConnected;

  /// No description provided for @addFuelUnsupported.
  ///
  /// In en, this message translates to:
  /// **'This display\'s firmware does not support adding fuel. Update it to version 1.1.0 or later.'**
  String get addFuelUnsupported;

  /// No description provided for @unitsTitle.
  ///
  /// In en, this message translates to:
  /// **'Units'**
  String get unitsTitle;

  /// No description provided for @unitsImperial.
  ///
  /// In en, this message translates to:
  /// **'English (mi, gal, °F)'**
  String get unitsImperial;

  /// No description provided for @unitsMetric.
  ///
  /// In en, this message translates to:
  /// **'Metric (km, L, °C)'**
  String get unitsMetric;

  /// No description provided for @fuelLiters.
  ///
  /// In en, this message translates to:
  /// **'FUEL: {arg1} L'**
  String fuelLiters(String arg1);

  /// No description provided for @tempCelsius.
  ///
  /// In en, this message translates to:
  /// **'TEMP: {arg1} °C'**
  String tempCelsius(String arg1);

  /// No description provided for @economyMetric.
  ///
  /// In en, this message translates to:
  /// **'ECON: {arg1} L/100km'**
  String economyMetric(String arg1);

  /// No description provided for @rangeKmLabel.
  ///
  /// In en, this message translates to:
  /// **'RANGE: {arg1} km'**
  String rangeKmLabel(String arg1);

  /// No description provided for @rangeKmShort.
  ///
  /// In en, this message translates to:
  /// **'{arg1} km'**
  String rangeKmShort(String arg1);

  /// No description provided for @rangeUnknownKmLabel.
  ///
  /// In en, this message translates to:
  /// **'RANGE: -- km'**
  String get rangeUnknownKmLabel;

  /// No description provided for @rangeUnknownKmShort.
  ///
  /// In en, this message translates to:
  /// **'-- km'**
  String get rangeUnknownKmShort;

  /// No description provided for @bleIdle.
  ///
  /// In en, this message translates to:
  /// **'BLE idle'**
  String get bleIdle;

  /// No description provided for @blePermissionNeeded.
  ///
  /// In en, this message translates to:
  /// **'BLE: permission needed'**
  String get blePermissionNeeded;

  /// No description provided for @bleBluetoothOff.
  ///
  /// In en, this message translates to:
  /// **'BLE: Bluetooth off'**
  String get bleBluetoothOff;

  /// No description provided for @bleScanning.
  ///
  /// In en, this message translates to:
  /// **'BLE: searching...'**
  String get bleScanning;

  /// No description provided for @bleConnecting.
  ///
  /// In en, this message translates to:
  /// **'BLE: connecting...'**
  String get bleConnecting;

  /// No description provided for @bleConnected.
  ///
  /// In en, this message translates to:
  /// **'BLE: connected'**
  String get bleConnected;

  /// No description provided for @bleScanningWithCount.
  ///
  /// In en, this message translates to:
  /// **'{arg1, plural, one{BLE: searching... ({arg1} device seen)} other{BLE: searching... ({arg1} devices seen)}}'**
  String bleScanningWithCount(int arg1);
}

class _AppLocalizationsDelegate
    extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['en'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return AppLocalizationsEn();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
