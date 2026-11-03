import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import 'ble_client.dart';
import 'l10n/app_localizations.dart';
import 'telemetry.dart';
import 'units.dart';

/// Minimum display firmware protocol that understands the add-fuel command.
const _minAddFuelProtocol = 3;

/// Picker step: 0.1 L in metric, 0.05 gal in English units.
double _stepIn(UnitSystem units) => units == UnitSystem.metric ? 0.1 : 0.05;

/// Lets the user pick an amount of fuel to add, limited to the tank's free
/// space, and sends it to the display when confirmed.
Future<void> showAddFuelDialog(
  BuildContext context,
  BleClient ble,
  UnitSystem units,
) async {
  final l10n = AppLocalizations.of(context);
  final metric = units == UnitSystem.metric;
  final currentL = ble.telemetry.fuelLiters;
  final roomL = (Telemetry.tankCapacityLiters - currentL).clamp(
    0.0,
    Telemetry.tankCapacityLiters,
  );
  final step = _stepIn(units);
  // Small epsilon keeps values like 0.3 / 0.1 from rounding down a step.
  final maxSteps = (units.volumeToDisplay(roomL) / step + 1e-6).floor();
  final protocol = ble.displayInfo?.protocol ?? ble.protocolVersion ?? 0;

  String? blocked;
  if (!ble.connected) {
    blocked = l10n.addFuelNotConnected;
  } else if (protocol < _minAddFuelProtocol) {
    blocked = l10n.addFuelUnsupported;
  } else if (maxSteps < 1) {
    blocked = l10n.addFuelTankFull;
  }

  String amount(double v) => metric
      ? l10n.addFuelAmountLiters(v.toStringAsFixed(1))
      : l10n.addFuelAmountGallons(v.toStringAsFixed(2));

  var selected = 1; // number of steps to add
  final controller = FixedExtentScrollController();
  final addedL = await showDialog<double>(
    context: context,
    builder: (ctx) => StatefulBuilder(
      builder: (ctx, setDialog) {
        final newTotal = units.volumeToDisplay(currentL) + selected * step;
        return AlertDialog(
          scrollable: true,
          title: Text(l10n.addFuelTitle),
          content: blocked != null
              ? Text(blocked)
              : SizedBox(
                  width: 280,
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(l10n.addFuelPrompt),
                      SizedBox(
                        height: 140,
                        child: CupertinoPicker(
                          scrollController: controller,
                          itemExtent: 40,
                          onSelectedItemChanged: (i) =>
                              setDialog(() => selected = i + 1),
                          children: [
                            for (var i = 1; i <= maxSteps; i++)
                              Center(
                                child: Text(
                                  amount(i * step),
                                  style: const TextStyle(
                                    fontSize: 22,
                                    color: Colors.white,
                                  ),
                                ),
                              ),
                          ],
                        ),
                      ),
                      Text(l10n.addFuelNewTotal(amount(newTotal))),
                    ],
                  ),
                ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx),
              child: Text(blocked != null ? l10n.close : l10n.cancel),
            ),
            if (blocked == null)
              TextButton(
                onPressed: () => Navigator.pop(
                  ctx,
                  units.volumeFromDisplay(selected * step),
                ),
                child: Text(l10n.addFuelConfirm),
              ),
          ],
        );
      },
    ),
  );
  controller.dispose();
  if (addedL != null) {
    // Never ask for more than the free space; the display clamps as well.
    await ble.addFuel(addedL > roomL ? roomL : addedL);
  }
}
