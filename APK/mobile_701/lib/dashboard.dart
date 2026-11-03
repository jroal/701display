import 'dart:async';

import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import 'add_fuel_dialog.dart';
import 'ble_client.dart';
import 'l10n/app_localizations.dart';
import 'range_overlay.dart';
import 'telemetry.dart';
import 'units.dart';

const _keepOnKey = 'keep_display_on';

// Palette and layout mirror UI_Layout_Init() in the ESP32 firmware (800x480).
const _bg = Color(0xFF101820);
const _text = Color(0xFFF4F7FA);
const _cyan = Color(0xFF65D6FF);
const _gold = Color(0xFFFFD166);
const _green = Color(0xFF2ECC71);
const _yellow = Color(0xFFF1C40F);
const _red = Color(0xFFE74C3C);

const double _w = 800, _h = 480;
const double _rpmMaxScale = 10000, _rpmPeak = 8000, _rpmRedline = 9000;

class Dashboard extends StatefulWidget {
  const Dashboard({super.key, required this.ble});
  final BleClient ble;

  @override
  State<Dashboard> createState() => _DashboardState();
}

class _DashboardState extends State<Dashboard> with WidgetsBindingObserver {
  final RangeOverlay _overlay = RangeOverlay();
  bool _overlayEnabled = false;
  bool _inBackground = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    widget.ble.addListener(_pushOverlayUpdate);
    _loadSettings();
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    widget.ble.removeListener(_pushOverlayUpdate);
    _setWakelock(false);
    unawaited(_overlay.hide());
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused) {
      _inBackground = true;
      unawaited(_syncOverlay());
    } else if (state == AppLifecycleState.resumed) {
      _inBackground = false;
      unawaited(_overlay.hide());
    }
  }

  // The overlay is only shown while the app is in the background.
  Future<void> _syncOverlay() async {
    if (!_overlayEnabled || !_inBackground) return;
    final l10n = AppLocalizations.of(context);
    if (!await RangeOverlay.hasPermission()) return;
    await _overlay.show(
      notificationTitle: l10n.overlayNotificationTitle,
      notificationContent: l10n.overlayNotificationContent,
    );
    _pushOverlayUpdate();
  }

  void _pushOverlayUpdate() {
    if (!_overlayEnabled || !_inBackground) return;
    final t = widget.ble.telemetry;
    final l10n = AppLocalizations.of(context);
    unawaited(
      _overlay.update(
        rangeText: t.mpg > 0
            ? (_units == UnitSystem.metric
                  ? l10n.rangeKmShort
                  : l10n.rangeMilesShort)(
                _units.milesToDisplay(t.rangeMiles).toStringAsFixed(0),
              )
            : (_units == UnitSystem.metric
                  ? l10n.rangeUnknownKmShort
                  : l10n.rangeUnknownShort),
        live: widget.ble.connected,
      ),
    );
  }

  Future<void> _setOverlayEnabled(bool v) async {
    if (v && !await RangeOverlay.ensurePermission()) {
      v = false;
    }
    if (!mounted) return;
    setState(() => _overlayEnabled = v);
    await RangeOverlay.saveEnabled(v);
    if (!v) await _overlay.hide();
  }

  bool _keepOn = false;
  UnitSystem _units = UnitSystem.imperial;

  Future<void> _setWakelock(bool on) async {
    try {
      await (on ? WakelockPlus.enable() : WakelockPlus.disable());
    } catch (_) {}
  }

  Future<void> _loadSettings() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final v = prefs.getBool(_keepOnKey) ?? false;
      final overlayOn =
          (prefs.getBool(rangeOverlayKey) ?? false) &&
          await RangeOverlay.hasPermission();
      if (!mounted) return;
      setState(() {
        _keepOn = v;
        _units = UnitSystem.fromName(prefs.getString(UnitSystem.prefsKey));
        _overlayEnabled = overlayOn;
      });
      _setWakelock(v);
    } catch (_) {}
  }

  Future<void> _setUnits(UnitSystem u) async {
    setState(() => _units = u);
    _pushOverlayUpdate();
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(UnitSystem.prefsKey, u.name);
    } catch (_) {}
  }

  Future<void> _setKeepOn(bool v) async {
    setState(() => _keepOn = v);
    _setWakelock(v);
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setBool(_keepOnKey, v);
    } catch (_) {}
  }

  List<Widget> _deviceTiles(AppLocalizations l10n) {
    final ble = widget.ble;
    final devices = ble.nearby.values.toList()
      ..sort((a, b) => a.name.compareTo(b.name));
    final selectedIsNearby = devices.any(
      (device) => device.selectionId == ble.selectedId,
    );
    Widget tile(String? id, String title, String? subtitle) => ListTile(
      dense: true,
      contentPadding: EdgeInsets.zero,
      title: Text(title),
      subtitle: subtitle == null ? null : Text(subtitle),
      trailing: ble.selectedId == id
          ? const Icon(Icons.check_circle, color: _green)
          : null,
      onTap: () => ble.selectDevice(id),
    );
    return [
      tile(null, l10n.anyDisplay, null),
      for (final device in devices)
        tile(
          device.selectionId,
          device.name,
          ble.connected && ble.connectedId == device.selectionId
              ? l10n.deviceDetailsConnected(device.address, device.rssi)
              : l10n.deviceDetails(device.address, device.rssi),
        ),
      if (ble.selectedId != null && !selectedIsNearby)
        tile(
          ble.selectedId,
          ble.selectedId!.startsWith('Husky701-')
              ? ble.selectedId!
              : l10n.savedDisplay,
          l10n.notInRange,
        ),
    ];
  }

  Future<void> _openAbout() async {
    final info = await PackageInfo.fromPlatform();
    if (!mounted) return;
    final l10n = AppLocalizations.of(context);
    await showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(l10n.aboutTitle),
        content: ListenableBuilder(
          listenable: widget.ble,
          builder: (_, _) {
            final d = widget.ble.displayInfo;
            final connected = widget.ble.connected;
            String row(String k, String? v) =>
                l10n.aboutRow(k, v ?? l10n.aboutUnknown);
            final lines = <String>[
              row(l10n.aboutAppVersion, '${info.version}+${info.buildNumber}'),
              row(
                l10n.aboutAppProtocol,
                '${Telemetry.minProtocolVersion}-${Telemetry.protocolVersion}',
              ),
              if (!connected)
                l10n.aboutConnectToSeeVersions
              else ...[
                row(
                  l10n.aboutDisplayFirmware,
                  d?.firmware ?? l10n.aboutOlderThanFirmware,
                ),
                row(l10n.aboutDisplayBuild, d?.build),
                row(
                  l10n.aboutDisplayProtocol,
                  (d?.protocol ?? widget.ble.protocolVersion)?.toString(),
                ),
              ],
            ];
            return Text(lines.join('\n'));
          },
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: Text(l10n.close),
          ),
        ],
      ),
    );
  }

  void _openSettings() {
    final l10n = AppLocalizations.of(context);
    showDialog<void>(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setDialog) => AlertDialog(
          title: Text(l10n.settingsTitle),
          content: SizedBox(
            width: 360,
            child: SingleChildScrollView(
              child: ListenableBuilder(
                listenable: widget.ble,
                builder: (_, _) => Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    CheckboxListTile(
                      contentPadding: EdgeInsets.zero,
                      controlAffinity: ListTileControlAffinity.leading,
                      title: Text(l10n.keepDisplayOn),
                      value: _keepOn,
                      onChanged: (v) {
                        _setKeepOn(v ?? false);
                        setDialog(() {});
                      },
                    ),
                    CheckboxListTile(
                      contentPadding: EdgeInsets.zero,
                      controlAffinity: ListTileControlAffinity.leading,
                      title: Text(l10n.showRangeOverlay),
                      subtitle: Text(l10n.showRangeOverlayHint),
                      value: _overlayEnabled,
                      onChanged: (v) async {
                        await _setOverlayEnabled(v ?? false);
                        setDialog(() {});
                      },
                    ),
                    const Divider(),
                    Text(l10n.unitsTitle),
                    const SizedBox(height: 8),
                    SegmentedButton<UnitSystem>(
                      showSelectedIcon: false,
                      segments: [
                        ButtonSegment(
                          value: UnitSystem.imperial,
                          label: Text(l10n.unitsImperial),
                        ),
                        ButtonSegment(
                          value: UnitSystem.metric,
                          label: Text(l10n.unitsMetric),
                        ),
                      ],
                      selected: {_units},
                      onSelectionChanged: (s) {
                        _setUnits(s.first);
                        setDialog(() {});
                      },
                    ),
                    const Divider(),
                    ListTile(
                      contentPadding: EdgeInsets.zero,
                      leading: const Icon(Icons.info_outline),
                      title: Text(l10n.aboutTitle),
                      onTap: _openAbout,
                    ),
                    const Divider(),
                    Text(l10n.displayToConnectTo),
                    ..._deviceTiles(l10n),
                  ],
                ),
              ),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx),
              child: Text(l10n.close),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: _bg,
      body: SafeArea(
        child: Stack(
          children: [
            Center(
              child: FittedBox(
                fit: BoxFit.contain,
                child: SizedBox(
                  width: _w,
                  height: _h,
                  child: ListenableBuilder(
                    listenable: widget.ble,
                    builder: (context, _) => _canvas(widget.ble),
                  ),
                ),
              ),
            ),
            Positioned(
              right: 0,
              top: 0,
              child: IconButton(
                tooltip: AppLocalizations.of(context).settingsTooltip,
                iconSize: 20,
                color: Colors.white54,
                icon: const Icon(Icons.settings),
                onPressed: _openSettings,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _label(
    String text,
    double size,
    Color color, {
    TextAlign align = TextAlign.center,
  }) {
    return Text(
      text,
      textAlign: align,
      maxLines: 1,
      overflow: TextOverflow.clip,
      style: TextStyle(
        fontSize: size,
        color: color,
        height: 1.0,
        decoration: TextDecoration.none,
        fontFeatures: const [FontFeature.tabularFigures()],
      ),
    );
  }

  // Positions a box by its center, like lv_obj_align(LV_ALIGN_CENTER, x, y).
  Widget _centered(double dx, double dy, double w, double h, Widget child) {
    return Positioned(
      left: _w / 2 + dx - w / 2,
      top: _h / 2 + dy - h / 2,
      width: w,
      height: h,
      child: Center(child: child),
    );
  }

  Widget _canvas(BleClient ble) {
    final l10n = AppLocalizations.of(context);
    final t = ble.telemetry;
    final live = ble.connected;
    final metric = _units == UnitSystem.metric;

    final rpmClamped = t.rpm > _rpmMaxScale ? _rpmMaxScale : t.rpm.toDouble();
    final fillWidth = (rpmClamped * 620 / _rpmMaxScale).clamp(1.0, 620.0);
    final rpmColor = t.rpm < _rpmPeak
        ? const Color(0xFF00DC00)
        : (t.rpm < _rpmRedline
              ? const Color(0xFFF0F000)
              : const Color(0xFFFF0000));

    final absText = t.absMode == 2
        ? l10n.absOff
        : (t.absMode == 1 ? l10n.absOffRoad : l10n.absOn);
    final String standText;
    final Color standColor;
    switch (t.sideStand) {
      case 1:
        standText = l10n.standDown;
        standColor = _red;
      case 2:
        standText = l10n.standUp;
        standColor = _green;
      default:
        standText = l10n.standUnknown;
        standColor = _cyan;
    }

    return Stack(
      children: [
        // Fuel gauge (top left)
        _centered(-220, -175, 300, 38, _fuelBar(t.fuelPercent, l10n)),
        _centered(-220, -135, 300, 30, _fuelMarkers(l10n)),
        _centered(
          240,
          -175,
          260,
          40,
          GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: () => showAddFuelDialog(context, ble, _units),
            child: _label(
              metric
                  ? l10n.fuelLiters(
                      _units.gallonsToDisplay(t.fuelGallons).toStringAsFixed(1),
                    )
                  : l10n.fuelGallons(t.fuelGallons.toStringAsFixed(2)),
              32,
              _text,
            ),
          ),
        ),

        // Reset button
        _centered(30, -175, 180, 44, _resetButton(live, l10n)),

        // Temp / gear
        _centered(
          -200,
          -65,
          400,
          56,
          _label(
            (metric ? l10n.tempCelsius : l10n.tempFahrenheit)(
              _units
                  .fahrenheitToDisplay(t.tempFahrenheit.toDouble())
                  .toStringAsFixed(0),
            ),
            48,
            _gold,
          ),
        ),
        _centered(
          200,
          -65,
          400,
          56,
          _label(
            t.gear == 0 ? l10n.gearNeutral : l10n.gearNumber(t.gear),
            48,
            _gold,
          ),
        ),

        // RPM bar
        _centered(
          0,
          25,
          620,
          34,
          Container(
            color: const Color.fromARGB(255, 35, 35, 35),
            alignment: Alignment.centerLeft,
            child: Container(width: fillWidth, height: 34, color: rpmColor),
          ),
        ),
        _centered(0, 65, 300, 30, _label(l10n.rpmValue(t.rpm), 24, _cyan)),

        // Economy / range
        _centered(
          -200,
          125,
          400,
          56,
          _label(
            metric
                ? (t.mpg > 0
                      ? l10n.economyMetric(
                          _units.economyToDisplay(t.mpg).toStringAsFixed(1),
                        )
                      : l10n.economyMetric('--'))
                : l10n.economy(t.mpg.toStringAsFixed(1)),
            48,
            _gold,
          ),
        ),
        _centered(
          200,
          125,
          400,
          56,
          _label(
            t.mpg > 0
                ? (metric ? l10n.rangeKmLabel : l10n.rangeMilesLabel)(
                    _units.milesToDisplay(t.rangeMiles).toStringAsFixed(0),
                  )
                : (metric ? l10n.rangeUnknownKmLabel : l10n.rangeUnknownLabel),
            48,
            _gold,
          ),
        ),

        // Status row
        Positioned(
          left: 20,
          bottom: 55,
          width: 240,
          child: _label(standText, 24, standColor, align: TextAlign.left),
        ),
        Positioned(
          left: 20,
          bottom: 15,
          width: 240,
          child: _label(absText, 24, _cyan, align: TextAlign.left),
        ),
        Positioned(
          left: _w / 2 - 50 - 90,
          bottom: 15,
          width: 180,
          child: _label(l10n.offRoadAbs, 24, _cyan),
        ),
        Positioned(
          left: _w / 2 + 100 - 35,
          bottom: 12,
          width: 70,
          height: 36,
          child: FittedBox(
            child: Switch(
              value: t.offroadAbsRequested,
              onChanged: live ? widget.ble.setOffroadAbs : null,
            ),
          ),
        ),
        Positioned(
          right: 20,
          bottom: 15,
          width: 80,
          child: _label(
            l10n.canLabel,
            24,
            live && t.canActive ? _green : _yellow,
            align: TextAlign.right,
          ),
        ),

        // BLE link state (not on the ESP32 screen; kept small in the corner)
        Positioned(
          left: 8,
          top: 6,
          child: _label(
            ble.status == BleStatus.scanning && ble.scanSeen > 0
                ? l10n.bleScanningWithCount(ble.scanSeen)
                : _statusText(l10n, ble.status),
            14,
            live ? _green : _yellow,
          ),
        ),
      ],
    );
  }

  Widget _fuelBar(int percent, AppLocalizations l10n) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF17354B),
        border: Border.all(color: _text, width: 3),
        borderRadius: BorderRadius.circular(4),
      ),
      child: Stack(
        alignment: Alignment.center,
        children: [
          Align(
            alignment: Alignment.centerLeft,
            child: FractionallySizedBox(
              widthFactor: percent.clamp(0, 100) / 100.0,
              heightFactor: 1,
              child: Container(
                decoration: BoxDecoration(
                  color: _cyan,
                  borderRadius: BorderRadius.circular(1),
                ),
              ),
            ),
          ),
          _label(l10n.fuelPercent(percent), 24, _cyan),
        ],
      ),
    );
  }

  Widget _fuelMarkers(AppLocalizations l10n) {
    final marks = [
      l10n.fuelMarkerEmpty,
      l10n.fuelMarkerQuarter,
      l10n.fuelMarkerHalf,
      l10n.fuelMarkerThreeQuarter,
      l10n.fuelMarkerFull,
    ];
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [for (final m in marks) _label(m, 24, _cyan)],
    );
  }

  Widget _resetButton(bool enabled, AppLocalizations l10n) {
    return Material(
      color: const Color(0xFF2196F3),
      borderRadius: BorderRadius.circular(4),
      child: InkWell(
        borderRadius: BorderRadius.circular(4),
        onTap: enabled ? widget.ble.resetFuel : null,
        child: Center(child: _label(l10n.reset, 24, _cyan)),
      ),
    );
  }

  static String _statusText(AppLocalizations l10n, BleStatus s) => switch (s) {
    BleStatus.idle => l10n.bleIdle,
    BleStatus.needPermission => l10n.blePermissionNeeded,
    BleStatus.bluetoothOff => l10n.bleBluetoothOff,
    BleStatus.scanning => l10n.bleScanning,
    BleStatus.connecting => l10n.bleConnecting,
    BleStatus.connected => l10n.bleConnected,
  };
}
