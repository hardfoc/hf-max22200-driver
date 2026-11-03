# Architecture

How the driver is put together: three layers, what each owns, and how one call reaches the chip.

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagrams/driver-layers.dark.svg">
  <img src="diagrams/driver-layers.svg"
    alt="Layer diagram: the application calls the hardware-agnostic MAX22200 driver, which uses the CRTP SPI
    interface, which calls the platform port you write, which drives the SPI bus and the ENABLE, CMD and
    FAULT pins of the MAX22200.">
</picture>
<!-- /figure -->

## The three layers

| Layer | Where | You change it when |
| --- | --- | --- |
| **Hardware-agnostic core**: `MAX22200<SpiType>` | [`inc/max22200.hpp`](../inc/max22200.hpp), [`src/max22200.ipp`](../src/max22200.ipp) | Never, for a new board |
| **Communication interface**: `SpiInterface<Derived>` | [`inc/max22200_spi_interface.hpp`](../inc/max22200_spi_interface.hpp) | Never |
| **Platform port**: your class | Your project | Once per MCU or board ([Porting](porting.md)) |

Everything above the port is the same on ESP32, STM32, a Linux host or the unit-test model. That is how the
[host tests](testing.md) run the real driver code without hardware.

## How one call reaches the chip

```mermaid
sequenceDiagram
    participant A as Application
    participant D as MAX22200 core
    participant P as Your port
    participant C as MAX22200 chip
    A->>D: ConfigureChannel(0, cfg)
    D->>D: check channel, IFS, SRC vs fCHOP
    D->>D: mA to HIT/HOLD (x 127 / IFS), ms to HIT_T (x fCHOP / 40)
    D->>P: GpioSet(CMD, ACTIVE)
    D->>P: Transfer({0x82}, 1 byte)
    P->>C: CSB low, command byte, CSB high
    C-->>P: STATUS[7:0]
    D->>P: GpioSet(CMD, INACTIVE)
    D->>P: Transfer(4 bytes, LSB first)
    P->>C: CSB low, CFG_CH0, CSB high
    D-->>A: DriverStatus::OK
```

## Why CRTP

The interface is a class template parameter, not a virtual base class. The compiler sees your port's methods
directly and inlines them; the driver has no vtable, allocates nothing and needs no RTTI. The cost is that the
driver type names the port: `MAX22200<MySpi>`. Code that has to work with several port types takes the driver as
a template parameter too.

## State the driver keeps

The driver holds a reference to your port, never a copy, and a few values in the object:

| State | Why |
| --- | --- |
| `BoardConfig` | IFS and optional current and duty limits for every unit conversion |
| Cached STATUS | ONCH for the 8-bit channel writes, FREQM for HIT-time and duty conversions |
| Last fault byte | STATUS[7:0] from the most recent command byte (`GetLastFaultByte()`) |
| `DriverStatistics` | Transfer and failure counters |

The STATUS cache is refreshed by `ReadStatus()`, `WriteStatus()` and `Initialize()`. If something else writes
STATUS (a raw `WriteRegister32()`, another task), call `ReadStatus()` before the next `EnableChannel()`: the 8-bit
ONCH write uses the cached mask for the channels you did not name.

## Errors without exceptions

Every call that touches the chip returns `max22200::DriverStatus`:

| Value | Meaning |
| --- | --- |
| `OK` | Done |
| `INVALID_PARAMETER` | Channel above 7, pair above 3, IFS of 0, SRC at 50 kHz or more, a value out of range; nothing is written |
| `COMMUNICATION_ERROR` | Your `Transfer()` or `GpioRead()` returned `false`, or `Initialize()` saw COMER three times |
| `INITIALIZATION_ERROR` | Your `Initialize()` or `Configure()` returned `false`, or `GpioSet(ENABLE, ACTIVE)` did during `Driver::Initialize()` (nothing is sent) |
| `GPIO_ERROR` | Your `GpioSet()` returned `false` for CMD (the frame is not sent, or the access fails when CMD did not go back low) or for ENABLE in `EnableDevice()`, `DisableDevice()` and `Deinitialize()` (the device may still be enabled) |

`HARDWARE_FAULT` and `TIMEOUT` exist in the enum but no call returns them. Chip faults (overcurrent, open load) are
not errors of the call: read them with `ReadStatus()` and `ReadFaultRegister()`.

## Source map

| Path | Contents |
| --- | --- |
| `inc/max22200.hpp` | `MAX22200<SpiType>`: every public call |
| `src/max22200.ipp` | Template implementation, included by the header |
| `inc/max22200_spi_interface.hpp` | `SpiInterface<Derived>`, `CtrlPin`, `GpioSignal` |
| `inc/max22200_types.hpp` | `ChannelConfig`, `StatusConfig`, `FaultStatus`, `DpmConfig`, `BoardConfig`, enums, conversions |
| `inc/max22200_registers.hpp` | Register banks, command byte, bit fields |
| `tests/` | Host unit tests and the compiled documentation snippets |
| `examples/esp32/` | ESP-IDF applications |
