"""The three layers of the driver: hardware-agnostic core, communication interface, platform port.

Reads: From top to bottom. Your application talks to one MAX22200<SpiType> object; its main calls (listed from
inc/max22200.hpp) configure channels, switch them, and read faults. Each call becomes the chip's two-phase SPI
access: a command byte with CMD high, then 8 or 32 data bits with CMD low. The communication interface
(SpiInterface in inc/max22200_spi_interface.hpp) is a CRTP base that forwards those transfers and the ENABLE, CMD
and FAULT pins to your class. The platform port is the only code you write. Below it is the hardware.
"""

import re
from pathlib import Path

from hardfoc_mkdocs_theme.figkit import Fig

ROOT = Path(__file__).resolve().parents[3]
DRIVER = "inc/max22200.hpp"
COMM = "inc/max22200_spi_interface.hpp"
# The calls the figure names, in reading order; data() fails if one disappears from the header.
FEATURED = ["Initialize", "ConfigureChannel", "EnableChannel", "SetChannelsOn", "SetFullBridgeState",
            "SetHoldCurrentMa", "ReadStatus", "ReadFaultRegister", "ClearChannelFaults", "ConfigureDpm",
            "ReadRegister32"]


def data():
    drv = (ROOT / DRIVER).read_text()
    public = drv.split("public:", 1)[1].split("private:", 1)[0]
    calls = []
    for name in re.findall(r"^\s+DriverStatus (\w+)\(", public, re.M):
        if name not in calls:
            calls.append(name)
    missing = [c for c in FEATURED if c not in calls]
    if missing:
        raise SystemExit(f"driver-layers: not in {DRIVER}: {', '.join(missing)}")
    comm = (ROOT / COMM).read_text()
    hooks = re.findall(r"^  \w[\w &]* (\w+)\([^)]*\)(?: const)? \{\s*\n?\s*(?:return )?static_cast", comm, re.M)
    return {
        "featured": FEATURED,
        "more": len(calls) - len(FEATURED),
        "port_hooks": hooks,
        "sources": [f"{DRIVER}:MAX22200 public calls", f"{COMM}:SpiInterface"],
    }


def build(d):
    f = Fig(
        "driver-layers",
        "Three layers, one of them yours",
        alt="Layer diagram: the application calls the hardware-agnostic MAX22200 driver, which uses the CRTP SPI "
        "interface, which calls the platform port you write, which drives the SPI bus and the ENABLE, CMD and "
        "FAULT pins of the MAX22200.",
        width=1200,
        kicker="Architecture",
        subtitle="The driver and the interface are the same on every MCU. The port is a small class you write once.",
        profile="overview",
        legend=[("brand", "this library"), ("teal", "you write"), ("slate", "hardware")],
    )
    W = f.body_w
    app = f.card(0, 0, W, "Your application", ["valve sequencing, relay control, motor direction ..."], tone="slate")
    top = app.b + 40
    names = [c + "()" for c in d["featured"]] + [f"+{d['more']} more"]
    chips_h = f.chips(16, f.region_inner_top(top), W - 32, names, tone="brand")
    reg = f.region(0, top, W, f.region_inner_top(top) - top + chips_h + 18, "Hardware-agnostic core", tone="brand",
                   sub="MAX22200<SpiType>, inc/max22200.hpp")
    comm = f.card(0, reg.b + 40, W, "Communication interface",
                  ["SpiInterface<Derived>: CRTP base. Command byte with CMD high, then 8 or 32 data bits",
                   "with CMD low; ENABLE, CMD and FAULT by name (CtrlPin) and state (GpioSignal)"],
                  tone="brand", accent=True)
    port = f.card(0, comm.b + 40, W, "Platform port (your class)", ["  ".join(h + "()" for h in d["port_hooks"])],
                  tone="teal", accent=True, mono=True)
    hw = f.card(0, port.b + 40, W, "MAX22200",
                ["SPI mode 0 up to 10 MHz  /  ENABLE, CMD, FAULT  /  OUT0 to OUT7, up to 36 V, 1 A RMS"],
                tone="slate")
    for a, b in ((app, reg), (reg, comm), (comm, port), (port, hw)):
        f.connect(a, b, "bottom", "top", tone="rail")
    return f
