/// Decoded BLE telemetry packet. Layout is documented in Docs/BLE_Protocol.md
/// and mirrors ble_telemetry_pack() in the ESP32 firmware.
class Telemetry {
  static const int packetSize = 12;
  static const int protocolVersion = 1;

  // Must match TOTAL_FUEL_CAPACITY / unit conversions in the firmware UI.
  static const double litersToGallons = 0.264172;

  const Telemetry({
    this.rpm = 0,
    this.coolantTempC = 0,
    this.gear = 0,
    this.canActive = false,
    this.absMode = 0,
    this.sideStand = 0,
    this.fuelLiters = 0,
    this.fuelPercent = 0,
    this.mpg = 0,
  });

  final int rpm;
  final int coolantTempC;
  final int gear; // 0 = neutral
  final bool canActive;
  final int absMode; // 0 on, 1 off-road, 2 off
  final int sideStand; // 0 unknown, 1 down, 2 up
  final double fuelLiters;
  final int fuelPercent;
  final double mpg;

  double get fuelGallons => fuelLiters * litersToGallons;
  double get tempFahrenheit => coolantTempC * 9.0 / 5.0 + 32.0;
  double get rangeMiles => fuelGallons * mpg;

  /// Returns null when [data] is not a valid packet of the supported version.
  static Telemetry? parse(List<int> data) {
    if (data.length < packetSize || data[0] != protocolVersion) return null;
    int u16(int i) => data[i] | (data[i + 1] << 8);
    int s16(int i) {
      final v = u16(i);
      return v >= 0x8000 ? v - 0x10000 : v;
    }

    final flags = data[6];
    return Telemetry(
      rpm: u16(1),
      coolantTempC: s16(3),
      gear: data[5],
      canActive: (flags & 0x01) != 0,
      absMode: (flags >> 1) & 0x03,
      sideStand: (flags >> 3) & 0x03,
      fuelLiters: u16(7) / 100.0,
      fuelPercent: data[9].clamp(0, 100),
      mpg: u16(10) / 10.0,
    );
  }
}
