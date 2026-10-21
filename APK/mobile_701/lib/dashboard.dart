import 'dart:async';

import 'package:flutter/material.dart';

import 'ble_client.dart';

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

class _DashboardState extends State<Dashboard> {
  late final Timer _clock;
  DateTime _now = DateTime.now();

  @override
  void initState() {
    super.initState();
    _clock = Timer.periodic(const Duration(seconds: 1), (_) => setState(() => _now = DateTime.now()));
  }

  @override
  void dispose() {
    _clock.cancel();
    super.dispose();
  }

  static String _two(int v) => v.toString().padLeft(2, '0');

  String get _clockText =>
      '${_now.year}-${_two(_now.month)}-${_two(_now.day)}  ${_two(_now.hour)}:${_two(_now.minute)}:${_two(_now.second)}';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: _bg,
      body: SafeArea(
        child: Center(
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
      ),
    );
  }

  Widget _label(String text, double size, Color color,
      {TextAlign align = TextAlign.center}) {
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
    final t = ble.telemetry;
    final live = ble.connected;

    final rpmClamped = t.rpm > _rpmMaxScale ? _rpmMaxScale : t.rpm.toDouble();
    final fillWidth = (rpmClamped * 620 / _rpmMaxScale).clamp(1.0, 620.0);
    final rpmColor = t.rpm < _rpmPeak
        ? const Color(0xFF00DC00)
        : (t.rpm < _rpmRedline ? const Color(0xFFF0F000) : const Color(0xFFFF0000));

    final absText = t.absMode == 2 ? 'ABS: OFF' : (t.absMode == 1 ? 'ABS: OFF-Road' : 'ABS: ON');
    final String standText;
    final Color standColor;
    switch (t.sideStand) {
      case 1:
        standText = 'STAND: DOWN';
        standColor = _red;
      case 2:
        standText = 'STAND: UP';
        standColor = _green;
      default:
        standText = 'STAND: --';
        standColor = _cyan;
    }

    return Stack(
      children: [
        // Fuel gauge (top left)
        _centered(-220, -175, 300, 38, _fuelBar(t.fuelPercent)),
        _centered(-220, -135, 300, 30, _fuelMarkers()),
        _centered(240, -175, 260, 40,
            _label('FUEL: ${t.fuelGallons.toStringAsFixed(2)} gal', 32, _text)),

        // Reset button
        _centered(30, -175, 180, 44, _resetButton(live)),

        // Temp / gear
        _centered(-200, -65, 400, 56,
            _label('TEMP: ${t.tempFahrenheit.toStringAsFixed(0)} F', 48, _gold)),
        _centered(200, -65, 400, 56,
            _label(t.gear == 0 ? 'GEAR: N' : 'GEAR: ${t.gear}', 48, _gold)),

        // RPM bar
        _centered(0, 25, 620, 34, Container(
          color: const Color.fromARGB(255, 35, 35, 35),
          alignment: Alignment.centerLeft,
          child: Container(width: fillWidth, height: 34, color: rpmColor),
        )),
        _centered(0, 65, 300, 30, _label('${t.rpm} RPM', 24, _cyan)),

        // Economy / range
        _centered(-200, 125, 400, 56, _label('ECON: ${t.mpg.toStringAsFixed(1)}', 48, _gold)),
        _centered(200, 125, 400, 56,
            _label(t.mpg > 0 ? 'RANGE: ${t.rangeMiles.toStringAsFixed(0)} mi' : 'RANGE: -- mi', 48, _gold)),

        // Status row
        Positioned(
          left: 20, bottom: 55, width: 240,
          child: _label(standText, 24, standColor, align: TextAlign.left),
        ),
        Positioned(
          left: 20, bottom: 15, width: 240,
          child: _label(absText, 24, _cyan, align: TextAlign.left),
        ),
        Positioned(
          left: (_w - 320) / 2, bottom: 15, width: 320,
          child: _label(_clockText, 24, _cyan),
        ),
        Positioned(
          right: 20, bottom: 15, width: 80,
          child: _label('CAN', 24, live && t.canActive ? _green : _yellow, align: TextAlign.right),
        ),

        // BLE link state (not on the ESP32 screen; kept small in the corner)
        Positioned(
          right: 8, top: 6,
          child: _label(_statusText(ble.status), 14, live ? _green : _yellow),
        ),
      ],
    );
  }

  Widget _fuelBar(int percent) {
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
                decoration: BoxDecoration(color: _cyan, borderRadius: BorderRadius.circular(1)),
              ),
            ),
          ),
          _label('$percent%', 24, _cyan),
        ],
      ),
    );
  }

  Widget _fuelMarkers() {
    const marks = ['E', '1/4', '1/2', '3/4', 'F'];
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [for (final m in marks) _label(m, 24, _cyan)],
    );
  }

  Widget _resetButton(bool enabled) {
    return Material(
      color: const Color(0xFF2196F3),
      borderRadius: BorderRadius.circular(4),
      child: InkWell(
        borderRadius: BorderRadius.circular(4),
        onTap: enabled ? widget.ble.resetFuel : null,
        child: Center(child: _label('Reset', 24, _cyan)),
      ),
    );
  }

  static String _statusText(BleStatus s) => switch (s) {
        BleStatus.idle => 'BLE idle',
        BleStatus.needPermission => 'BLE: permission needed',
        BleStatus.bluetoothOff => 'BLE: Bluetooth off',
        BleStatus.scanning => 'BLE: searching...',
        BleStatus.connecting => 'BLE: connecting...',
        BleStatus.connected => 'BLE: connected',
      };
}
