import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_701/telemetry.dart';

void main() {
  test('parses a firmware packet', () {
    // rpm 3200, temp 82 C, gear 3, CAN on, ABS off-road (1), stand up (2),
    // fuel 8.45 L (845), 65 %, 48.2 mpg (482)
    final packet = [
      1,
      3200 & 0xFF, 3200 >> 8,
      82, 0,
      3,
      0x01 | (1 << 1) | (2 << 3),
      845 & 0xFF, 845 >> 8,
      65,
      482 & 0xFF, 482 >> 8,
    ];
    final t = Telemetry.parse(packet)!;
    expect(t.rpm, 3200);
    expect(t.coolantTempC, 82);
    expect(t.gear, 3);
    expect(t.canActive, isTrue);
    expect(t.absMode, 1);
    expect(t.sideStand, 2);
    expect(t.fuelLiters, closeTo(8.45, 1e-9));
    expect(t.fuelPercent, 65);
    expect(t.mpg, closeTo(48.2, 1e-9));
    expect(t.tempFahrenheit, closeTo(179.6, 1e-6));
  });

  test('handles negative temperature and rejects bad packets', () {
    final t = Telemetry.parse([1, 0, 0, 0xF6, 0xFF, 0, 0, 0, 0, 0, 0, 0])!;
    expect(t.coolantTempC, -10);
    expect(Telemetry.parse([2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]), isNull);
    expect(Telemetry.parse([1, 0, 0]), isNull);
  });
}
