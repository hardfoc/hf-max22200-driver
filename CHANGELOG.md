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
- Documented that VDR duty-cycle setpoints are written scaled by 127 / 100 while the chip reads them as percent.

### Added

- Host unit tests (`HF_MAX22200_BUILD_TESTS`) that run the driver against a model of the MAX22200 SPI protocol.

## 1.0.0

- First release: `MAX22200<SpiType>` with CDR and VDR channel configuration in real units, channel switching,
  full-bridge states, STATUS and FAULT access, selective fault clear, DPM configuration and a CRTP SPI interface.
