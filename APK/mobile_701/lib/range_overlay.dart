import 'dart:async';

import 'package:android_intent_plus/android_intent.dart';
import 'package:android_intent_plus/flag.dart';
import 'package:flutter/material.dart';
import 'package:flutter_overlay_window/flutter_overlay_window.dart';
import 'package:shared_preferences/shared_preferences.dart';

const rangeOverlayKey = 'range_overlay_enabled';
const _appPackage = 'com.jroal.mobile_701_display';

// Logical sizes; the plugin takes raw pixels on first show, so they are scaled
// by the device pixel ratio.
const _overlayWidthDp = 136.0;
const _overlayHeightDp = 66.0;

/// Shows the remaining fuel range in a floating window while the app is in the
/// background. The overlay runs in its own Flutter engine ([overlayMain]), so the
/// BLE data is pushed to it from the main app.
class RangeOverlay {
  String _lastPayload = '';

  static Future<bool> loadEnabled() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      return prefs.getBool(rangeOverlayKey) ?? false;
    } catch (_) {
      return false;
    }
  }

  static Future<void> saveEnabled(bool v) async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setBool(rangeOverlayKey, v);
    } catch (_) {}
  }

  /// Asks for the "display over other apps" permission if needed.
  /// Returns whether the overlay may be shown.
  static Future<bool> ensurePermission() async {
    try {
      if (await FlutterOverlayWindow.isPermissionGranted()) return true;
      return await FlutterOverlayWindow.requestPermission() ?? false;
    } catch (_) {
      return false;
    }
  }

  static Future<bool> hasPermission() async {
    try {
      return await FlutterOverlayWindow.isPermissionGranted();
    } catch (_) {
      return false;
    }
  }

  Future<void> show() async {
    try {
      if (await FlutterOverlayWindow.isActive()) return;
      _lastPayload = '';
      final ratio = WidgetsBinding
          .instance
          .platformDispatcher
          .views
          .first
          .devicePixelRatio;
      await FlutterOverlayWindow.showOverlay(
        width: (_overlayWidthDp * ratio).round(),
        height: (_overlayHeightDp * ratio).round(),
        alignment: OverlayAlignment.topRight,
        enableDrag: true,
        flag: OverlayFlag.defaultFlag,
        visibility: NotificationVisibility.visibilitySecret,
        overlayTitle: '701 Display',
        overlayContent: 'Showing fuel range',
      );
    } catch (e) {
      debugPrint('Overlay show failed: $e');
    }
  }

  Future<void> hide() async {
    try {
      if (await FlutterOverlayWindow.isActive()) {
        await FlutterOverlayWindow.closeOverlay();
      }
    } catch (_) {}
    _lastPayload = '';
  }

  /// Pushes new values to the overlay; repeated identical values are skipped.
  Future<void> update({required String rangeText, required bool live}) async {
    final payload = '$live|$rangeText';
    if (payload == _lastPayload) return;
    try {
      if (!await FlutterOverlayWindow.isActive()) return;
      await FlutterOverlayWindow.shareData({'range': rangeText, 'live': live});
      _lastPayload = payload;
    } catch (_) {}
  }
}

class RangeOverlayView extends StatefulWidget {
  const RangeOverlayView({super.key});

  @override
  State<RangeOverlayView> createState() => _RangeOverlayViewState();
}

class _RangeOverlayViewState extends State<RangeOverlayView> {
  StreamSubscription<dynamic>? _sub;
  String _range = '-- mi';
  bool _live = false;

  @override
  void initState() {
    super.initState();
    _sub = FlutterOverlayWindow.overlayListener.listen((data) {
      if (data is Map && mounted) {
        setState(() {
          _range = data['range'] as String? ?? _range;
          _live = data['live'] as bool? ?? false;
        });
      }
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final color = _live ? const Color(0xFFFFD166) : Colors.white38;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: _openApp,
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0xE6101820),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: const Color(0xFF65D6FF), width: 1.5),
        ),
        alignment: Alignment.center,
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
        child: FittedBox(
          fit: BoxFit.scaleDown,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Text(
                'RANGE',
                style: TextStyle(
                  color: Color(0xFF65D6FF),
                  fontSize: 13,
                  height: 1.0,
                  decoration: TextDecoration.none,
                ),
              ),
              Text(
                _range,
                style: TextStyle(
                  color: color,
                  fontSize: 34,
                  fontWeight: FontWeight.bold,
                  height: 1.1,
                  decoration: TextDecoration.none,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _openApp() async {
    try {
      await const AndroidIntent(
        action: 'android.intent.action.MAIN',
        category: 'android.intent.category.LAUNCHER',
        package: _appPackage,
        componentName: '$_appPackage.MainActivity',
        flags: [Flag.FLAG_ACTIVITY_NEW_TASK, Flag.FLAG_ACTIVITY_SINGLE_TOP],
      ).launch();
    } catch (e) {
      debugPrint('Opening app from overlay failed: $e');
    }
  }
}
