enum UnitSystem {
  imperial,
  metric;

  static const prefsKey = 'unit_system';

  static UnitSystem fromName(String? name) => UnitSystem.values.firstWhere(
    (u) => u.name == name,
    orElse: () => imperial,
  );

  double gallonsToDisplay(double gallons) =>
      this == metric ? gallons * _litersPerGallon : gallons;

  double volumeToDisplay(double liters) =>
      this == metric ? liters : liters / _litersPerGallon;

  double volumeFromDisplay(double v) =>
      this == metric ? v : v * _litersPerGallon;

  double fahrenheitToDisplay(double f) => this == metric ? (f - 32) * 5 / 9 : f;

  double milesToDisplay(double miles) =>
      this == metric ? miles * _kmPerMile : miles;

  /// Miles per gallon to liters per 100 km; callers must pass mpg > 0.
  double economyToDisplay(double mpg) =>
      this == metric ? 100 * _litersPerGallon / (mpg * _kmPerMile) : mpg;
}

const _litersPerGallon = 3.785411784;
const _kmPerMile = 1.609344;
