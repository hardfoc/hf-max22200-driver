# Changelog

All notable changes to this driver are listed here. Versions follow
[Semantic Versioning](https://semver.org/). Entries marked **BREAKING** need source changes in
code that used the affected API; together they call for a major version bump at release.

## Unreleased

### Documentation

- The documentation site moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, drive modes,
  SPI protocol, architecture, API guide, configuration, porting, examples, troubleshooting), the Doxygen API
  reference under `/api/`, and every code sample compiled by the host build from `tests/docs/`.
- Corrected the hardware facts: the chip has no logic supply pin (VM 4.5 V to 36 V only), the outputs are eight
  single half-bridges `OUT0` to `OUT7`, SPI is mode 0 only, and there is no RESET pin; ENABLE, CMD and FAULT are
  the control pins.

### Breaking changes

- **BREAKING** (behaviour, no signature change): `SetHitCurrentMa()`, `SetHoldCurrentMa()`, their `...A()` and
  `...CurrentPercent()` variants, `SetHitDutyPercent()` and `SetHoldDutyPercent()` no longer change the
  channel's drive mode. A current setter on a VDR channel, or a duty setter on a CDR channel, now returns
  `INVALID_PARAMETER` and writes nothing. 1.0.0 silently flipped VDRnCDR and reinterpreted the other setpoint in
  the new unit (and the chip ignores a mode change while any channel is on or a TRIG pin is high). Select the
  mode with `ConfigureChannel()`, `ConfigureChannelCdr()` or `ConfigureChannelVdr()` before using the setters.

### Fixed

- VDR duty cycle is written as percent, as datasheet Table 3 defines HIT[6:0] / HOLD[6:0] in VDR: 40 % is now
  HIT[6:0] = 40 (1.0.0 wrote round(40 × 127 / 100) = 51, so the output ran at 51 %). Reading back returns the
  field as percent (100 and above read as 100 %). If you compensated with the 100 / 127 workaround, remove it.
- `Initialize()` waits the 2.5 ms wake-up time tWU after writing ACTIVE = 1 (datasheet Electrical
  Characteristics, "wake-up completed?" in Figure 6), so a channel switched on right after it drives its output.
  `Initialize()` now takes at least 3 ms.

### Added

- Host unit tests (`HF_MAX22200_BUILD_TESTS`) that run the driver against a model of the MAX22200 SPI protocol.

## 1.0.0

- First release: `MAX22200<SpiType>` with CDR and VDR channel configuration in real units, channel switching,
  full-bridge states, STATUS and FAULT access, selective fault clear, DPM configuration and a CRTP SPI interface.
