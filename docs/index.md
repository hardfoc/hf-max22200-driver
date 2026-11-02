---
hide:
  - navigation
  - toc
---

<div class="hf-hero" markdown>
<span class="hf-kicker">HardFOC driver</span>

# MAX22200 Driver

<p>A C++20 driver for the Analog Devices MAX22200 octal solenoid and motor driver. It brings the chip up,
configures each half-bridge for current or voltage drive with HIT and HOLD levels, switches channels, runs bridges
and reads faults, on any MCU with SPI.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/hardfoc/hf-max22200-driver){ .md-button }
</div>

The driver has no platform code. You give it a small class that moves bytes over SPI and drives the ENABLE and
CMD pins; it does the rest: the two-phase command and data protocol, the power-up sequence, register encoding in
milliamps, percent and milliseconds, and fault decoding.

<div class="hf-chip" markdown>
<div class="hf-chip__head">
<span class="hf-chip__kicker">Octal serial-controlled solenoid and motor driver</span>
<span class="hf-chip__name">MAX22200</span>
<span class="hf-chip__vendor">Analog Devices (Maxim Integrated)</span>
</div>

Outputs
:   8 push-pull half-bridges, 0.2 Ω typical, up to 1 A RMS each; pairs in parallel or as full bridges

Drive
:   Current regulation (CDR, integrated current sensing, low side) or voltage regulation (VDR, PWM duty)

HIT and HOLD
:   7-bit HIT and HOLD level and 8-bit HIT time per channel; chopping at 20 kHz to 100 kHz

Host interface
:   SPI mode 0 up to 10 MHz (5 MHz daisy-chained); the CMD pin selects command or data

Control pins
:   ENABLE, CMD, TRIGA, TRIGB (inputs); FAULT (open-drain output)

Supply
:   VM 4.5 V to 36 V; logic inputs switch at 1.2 V and take up to 6 V

Package
:   32-pin TQFN 5 mm × 5 mm, −40 °C to +85 °C; the MAX22200A adds selective fault clear

[Datasheet (Rev. 1, 3/25)](datasheet/MAX22200.pdf) ·
[Product page](https://www.analog.com/en/products/max22200.html)
</div>

## What you get

- **One object per chip.** `MAX22200<YourSpi>` takes your port and the board's full-scale current, and runs the
  datasheet's start-up flow in `Initialize()`. See [API guide](api-guide.md#initialise).
- **Real units.** HIT and HOLD in milliamps (CDR) or percent (VDR), HIT time in milliseconds, converted with the
  board's RREF and the chip's chopping frequency. See [Drive modes](drive-modes.md).
- **Fast switching.** Turning channels on and off is one 8-bit write of the ONCH byte; full-bridge pairs take
  forward, reverse, brake and Hi-Z. See [API guide](api-guide.md#switch-channels).
- **Faults by channel.** STATUS flags, per-channel OCP, HHF, open-load and plunger-movement flags, selective clear
  on the MAX22200A and the FAULT pin. See [API guide](api-guide.md#handle-faults).
- **Zero-overhead portability.** The port is a CRTP class: no virtual calls, no heap, no RTOS dependency.
  See [Porting](porting.md).
- **Tested without hardware.** Host unit tests run the driver against a model of the chip's SPI protocol.
  See [Testing and CI](testing.md).

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, pull in a solenoid in about 15 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, bus settings, timing, supply and the full-scale current resistor.

-   :material-layers-outline: **[Architecture](architecture.md)**

    ---

    The three layers and what each one owns.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Configure, switch, read and handle faults, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the SPI interface for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
