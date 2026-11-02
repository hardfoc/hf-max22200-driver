# SPI protocol

How the MAX22200's two-phase SPI access works, byte by byte, and how to read it on a logic analyser or in a log.

Every register access is two SPI transactions: the 8-bit Command Register with CMD high, then the data with CMD
low. The driver's private `writeCommandRegister()` and `readData32()` / `writeData32()` / `readData8()` /
`writeData8()` do exactly this; the references are datasheet Table 9 and Figures 7 and 10 to 13.

## Command byte

<div class="hf-registers" markdown>

| Bit | Name | Meaning |
| --- | --- | --- |
| `7` | `RB/W` | 0 = read, 1 = write |
| `6:5` | `RFU` | Write 0 |
| `4:1` | `A_BNK` | Register: 0x00 STATUS, 0x01 to 0x08 CFG_CH0 to CFG_CH7, 0x09 FAULT, 0x0A CFG_DPM |
| `0` | `8bit/n32bits` | 1 = the data phase is 8 bits (bits [31:24] only), 0 = 32 bits |

</div>

`max22200::CommandReg::build(bank, write, mode8)` builds it. The bytes you will see most:

| Byte | Access |
| --- | --- |
| `0x00` / `0x80` | Read / write STATUS, 32 bits |
| `0x81` | Write STATUS[31:24] (ONCH), 8 bits: channel on/off |
| `0x02` / `0x82` | Read / write CFG_CH0, 32 bits (CFG_CHn: `0x02 + 2n`) |
| `0x12` | Read FAULT, 32 bits |
| `0x14` / `0x94` | Read / write CFG_DPM |

While CMD is high the chip shifts STATUS[7:0], the fault flag byte, out on SDO. `0x04` there is COMER: the chip
rejected the previous transfer. The driver keeps the byte; read it with `GetLastFaultByte()`.

The command stays in force: every following CMD-low transfer is of the same type until a new command byte. The
driver always sends a command before each data phase, so it never depends on this.

## Data phase

| Access | SDI (host to chip) | SDO (chip to host) |
| --- | --- | --- |
| Write, 32 bits | DATAIN[7:0], [15:8], [23:16], [31:24] | STATUS[7:0], command echo, `0x00`, `0x00` |
| Read, 32 bits | Don't care (FAULT on MAX22200A: clear mask) | DATA[31:24], [23:16], [15:8], [7:0] |
| Write, 8 bits | DATAIN[31:24] | STATUS[7:0] |
| Read, 8 bits | Don't care | DATA[31:24] |

32-bit writes go least significant byte first and 32-bit reads come most significant byte first; this is what the
datasheet's figures show, and the driver follows it. Within each byte, bits are MSB first.

The chip counts clocks on writes: a 32-bit write checks the first 32, then every 8; an 8-bit write checks every 8.
A wrong count is a communication error: the transfer is ignored and COMER sets (FAULT goes low only if `M_COMF` is
0; it is 1 after reset and `Initialize()` keeps it).

## Selective clear on read (MAX22200A)

Reading FAULT on the MAX22200A clears only the bits whose SDI bit is high during the read. The SDI bytes line up
with the SDO bytes: first OCP, then HHF, OLF, DPM. To clear the overcurrent flag of OUT4 alone, send
`0x10 0x00 0x00 0x00` while reading. `ClearChannelFaults(mask)` sends the mask in all four bytes;
`ReadFaultRegisterSelectiveClear()` takes one mask per fault type. The MAX22200 ignores SDI and clears every bit.

## Decoding a log

The ESP32 port prints every transfer with `ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING`. Two examples:

| Log | Meaning |
| --- | --- |
| `SPI(1) TX: 82 RX: 03`, then `SPI(4) TX: 00 06 50 28 RX: 03 82 00 00` | Write CFG_CH0 = `0x28500600`; fault byte `0x03` (ACTIVE, UVM), command echoed |
| `SPI(1) TX: 00 RX: 03`, then `SPI(4) TX: 00 00 00 00 RX: 00 04 00 03` | Read STATUS = `0x00040003`: no channel on, M_COMF set, ACTIVE = 1, UVM = 1 |

`0x28500600` decodes as HOLD = `0x28` (40 / 127 of IFS), HIT = `0x50` (80 / 127), HIT_T = 6 (6 × 40 / 25 kHz =
9.6 ms) and CDR, low side, FREQ_CFG = 00. A requested 10 ms reads back as 9.6 ms: the HIT time is an 8-bit count
of 40 chopping periods. When you test a write, compare the raw register value you wrote with the one you read; the
values in milliamps and milliseconds are quantised.

## Next

[Drive modes](drive-modes.md) explains what HIT, HOLD, HIT_T and FREQ_CFG do on the output.
