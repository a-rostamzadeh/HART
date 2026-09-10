

```markdown
# HART Protocol Driver

A lightweight implementation of the **HART** (Highway Addressable Remote
Transducer) digital framing protocol for 8-bit AVR microcontrollers, written
in C and targeting the **ATmega8** at 8 MHz.

The driver handles the digital portion of HART communication — frame
construction, transmission, reception, validation, and device-address
filtering.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Hardware Requirements](#hardware-requirements)
4. [Project Layout](#project-layout)
5. [Toolchain](#toolchain)
6. [Building](#building)
7. [Flashing](#flashing)
8. [Configuration](#configuration)
9. [API Reference](#api-reference)
10. [Frame Format](#frame-format)
11. [Usage Examples](#usage-examples)
12. [Error Codes](#error-codes)
13. [HART Protocol Notes](#hart-protocol-notes)
14. [Known Limitations / TODO](#known-limitations--todo)
15. [References](#references)
16. [License](#license)
17. [Author](#author)

---

## Overview

HART is a hybrid analog + digital industrial communication standard. A
4–20 mA current loop carries the analog process variable, while a 1200 baud
FSK signal (±0.5 mA on top of the loop current) carries the digital data.

This driver implements **only the digital framing**:

- Preamble detection
- Start-character decoding (short / long frame, STX / ACK)
- Device-address parsing and filtering
- Command and data extraction
- XOR checksum generation and verification

The MCU is expected to be connected to an external HART modem that
performs the FSK modulation / demodulation and presents the MCU with a
plain UART byte stream.

---

## Features

- **Frame construction** — build a complete HART frame in a single call
- **Frame reception** — parse, validate, and dispatch incoming frames
- **Device-address filtering** — short and long frame formats
- **Checksum** — automatic XOR computation and verification
- **Start-character decoding** — all four standard values
  (`0x02`, `0x06`, `0x82`, `0x86`)
- **Bit-field flag struct** — parsed frame header exposed as a compact
  `HART_Flags_t` union
- **Named error codes** — every return value has a symbolic name
- **Small footprint** — suitable for the ATmega8's 8 KB flash
- **Backward compatible** — legacy `bits1.xxx` access still works

---

## Hardware Requirements

| Component        | Value / Part                                |
|------------------|---------------------------------------------|
| MCU              | ATmega8 (or ATmega16 / ATmega32 with edits) |
| Clock            | 8 MHz (internal RC or external crystal)     |
| HART modem       | AD5700, DS8500, or equivalent FSK modem     |
| UART             | 38400 baud, 8-N-1                           |
| Loop interface   | Standard 4–20 mA current loop               |

### Wiring (typical)

```
   ┌─────────────┐        UART          ┌──────────────┐
   │  ATmega8    │◄────────────────────►│  HART modem  │
   │  (this drv) │  TX / RX / RTS       │  (AD5700)    │
   └─────────────┘                      └──────┬───────┘
                                               │ FSK
                                               │
                                        ┌──────▼───────┐
                                        │  4–20 mA loop│
                                        └──────────────┘
```

---

## Project Layout

```
project/
├── README.md        ← this file
├── Makefile         ← build rules
├── HART.h           ← public interface, config, prototypes
├── HART.c           ← driver implementation
├── uart.c           ← UART driver (not shown)
├── uart.h           ← UART interface (not shown)
└── main.c           ← application entry point
```

> `uart.c` must implement `FlushUART()`, `TransmitByte()`, `ReceiveByte()`,
> `NumOfByteInRxBuffer`, `Status`, and `SetOverTimeControl()` — all declared
> in `HART.h`.

---

## Toolchain

| Tool       | Version tested       |
|------------|----------------------|
| `avr-gcc`  | 5.4.0 or newer       |
| `avr-libc` | 2.0.0 or newer       |
| `avrdude`  | 6.3 or newer         |
| `make`     | GNU Make 4.x         |

Install on Debian/Ubuntu:

```bash
sudo apt install gcc-avr avr-libc avrdude make
```

Install on macOS (Homebrew):

```bash
brew install avr-gcc avrdude make
```

On Windows, use [Microchip Studio](https://www.microchip.com/en-us/development-tools-tools-and-software/microchip-studio-for-avr-and-sam-microcontrollers)
or [WinAVR](https://sourceforge.net/projects/winavr/).

---

## Building

```bash
make clean
make
```

Produces:

| File         | Purpose                          |
|--------------|----------------------------------|
| `main.elf`   | Linked ELF executable            |
| `main.hex`   | Intel HEX image for flashing     |
| `main.map`   | Linker map (symbol / size info)  |

Check firmware size:

```bash
avr-size --mcu=atmega8 --format=avr main.elf
```

---

## Flashing

Set the fuses (only once, and only if not already correct):

```bash
avrdude -c usbasp -p m8 -U lfuse:w:0xE4:m -U hfuse:w:0xD9:m
```

Flash the firmware:

```bash
avrdude -c usbasp -p m8 -U flash:w:main.hex:i
```

> **Warning:** Setting fuses incorrectly can brick the chip. Verify the
> values against the ATmega8 datasheet before writing.

### Fuse reference

| Target    | Low fuse | High fuse |
|-----------|----------|-----------|
| ATmega8   | `0xE4`   | `0xD9`    |
| ATmega16  | `0xEF`   | `0xD9`    |
| ATmega32  | `0xEF`   | `0xD9`    |

- `0xE4` = internal 8 MHz RC, no divide-by-8, slow-rising power, long
  start-up.
- `0xD9` = disable JTAG, enable SPI programming, brown-out at 2.7 V.

---

## Configuration

All compile-time configuration lives in **`HART.h`**.

### Target selection

```c
#define ATMEGA8
/* #define ATMEGA16 */
/* #define ATMEGA32 */
```

### Clock

```c
#define XT8000
#define F_CPU 8000000UL
```

### UART

```c
#define UsingUART
#define UART_mode       1     /* 0 = polling, 1 = interrupt */
#define UART_Interrupt
#define UART_Interrupt_TxMode
#define BaudRate38400
```

### HART

```c
#define UsingHART
#define MAX_HART_Buffer_Length  120
#define HART_MyDeviceID         2
```

### Feature toggles

Comment out `UsingHART` to exclude the HART driver from the build.
Comment out `UsingUART` to exclude the UART layer (rarely useful, since
HART depends on it).

---

## API Reference

All functions are declared in `HART.h` under `#if defined UsingHART`.

### `void HART_ini(void)`

Initialise the HART frame header with default values:

- Preambles: `0xFF 0xFF`
- Start character: `0x82` (long frame, master → slave, STX)
- Manufacturer code: `0x00`
- Device type code: `0x32` (5850s)
- Device ID: `HART_MyDeviceID`

Call this once after reset, before any `SendCommand()` or
`ReceiveMessage()`.

---

### `void Set_HART_DeviceID(unsigned long DeviceID)`

Store a 24-bit device ID in the message header (`HART_Adrs_ID_3..5`).
Only the lower 24 bits of `DeviceID` are used.

```c
Set_HART_DeviceID(0x000123);   /* ID_3 = 0x00, ID_4 = 0x01, ID_5 = 0x23 */
```

---

### `void Set_HART_MyDeviceID(void)`

Store the local `HART_MyDeviceID` macro in the header, clearing the
upper two address bytes.

```c
Set_HART_MyDeviceID();   /* ID_3 = 0x00, ID_4 = 0x00, ID_5 = HART_MyDeviceID */
```

---

### `unsigned char Compare_HART_DeviceID(void)`

Return `1` if the address currently in the message header targets this
device, `0` otherwise.

---

### `unsigned char SendCommand(unsigned char Command, unsigned char DataBytesLen)`

Build and transmit a HART frame.

**Prerequisites:** the caller must populate
`HART_Msg[10 .. 10+DataBytesLen-1]` with the payload before calling.
The header fields and the checksum are filled in automatically.

**Returns:** `1` on success, `0` if the UART reported a transmission
error.

```c
HART_Msg[10] = 0x00;   /* payload byte 0 */
HART_Msg[11] = 0x00;   /* payload byte 1 */
SendCommand(0x00, 2);  /* command 0 with 2 data bytes */
```

---

### `unsigned char ReceiveMessage(void)`

Receive, parse, and validate one HART frame from the UART.

**Prerequisites:** a complete frame must already be sitting in the UART
RX buffer (`NumOfByteInRxBuffer` bytes).

**Returns:** `HART_OK` (100) on success, or one of the `HART_ERR_*`
codes on failure.

**On success**, the following are populated:

- `HART_Command` — command byte
- `HART_DataLen` — number of data bytes
- `HART_Msg[10 .. 10+DataLen-1]` — payload
- `HART_Flags` — parsed header flags

---

### `unsigned char CheckSum(void)`

Compute the XOR checksum over `HART_Msg[2 .. 9+HART_DataLen]`
(start character through the last data byte). Preambles are excluded.

---

## Frame Format

```
┌──────────┬───────┬─────────┬─────────┬─────┬──────┬──────────┐
│ Preamble │ Start │ Address │ Command │ Len │ Data │ Checksum │
│ 2 bytes  │ 1     │ 1 or 5  │ 1       │ 1   │ N    │ 1        │
│ FF FF    │ xx    │ ...     │ ...     │ N   │ ...  │ xx       │
└──────────┴───────┴─────────┴─────────┴─────┴──────┴──────────┘
```

### Start character encoding

| Bit 7 | Bit 6 | Bit 5 | Bits 4-3 | Bit 2 | Bit 1 | Bit 0 |
|-------|-------|-------|----------|-------|-------|-------|
| 1 = long frame | 0 | 0 | 0 | 1 = STX, 0 = ACK | 1 = master, 0 = slave | 0 |

Common values:

| Value  | Meaning                                  |
|--------|------------------------------------------|
| `0x02` | Short frame, master → slave, STX         |
| `0x06` | Short frame, slave → master, ACK         |
| `0x82` | Long frame, master → slave, STX          |
| `0x86` | Long frame, slave → master, ACK          |

### Address encoding

- **Short frame** — 1 byte; only the low nibble is the address.
- **Long frame** — 5 bytes: 1 byte of flags + 3 bytes of device ID.

### Checksum

XOR of all bytes from the start character through the last data byte
(inclusive). Preambles are **not** included.

---

## Usage Examples

### Minimal master — send a command 0

```c
#include "HART.h"

int main(void)
{
    uart0_init(1);           /* interrupt-driven UART */
    HART_ini();              /* default header        */

    /* Build the payload for "read primary variable" (command 0) */
    HART_Msg[10] = 0x00;     /* byte 0 */
    HART_Msg[11] = 0x00;     /* byte 1 */

    /* Transmit (command 0, 2 data bytes) */
    SendCommand(0x00, 2);

    while (1)
    {
        /* Application loop */
    }
}
```

### Minimal slave — receive and validate

```c
#include "HART.h"

int main(void)
{
    uart0_init(1);
    HART_ini();

    while (1)
    {
        if (NumOfByteInRxBuffer >= 11)
        {
            unsigned char result = ReceiveMessage();

            if (result == HART_OK)
            {
                /* Frame accepted — handle HART_Command */
                switch (HART_Command)
                {
                    case 0:
                        /* Read primary variable */
                        break;

                    case 1:
                        /* Read primary variable (percent) */
                        break;

                    default:
                        break;
                }
            }
            else
            {
                /* Optionally log the error code */
            }
        }
    }
}
```

### Changing the device ID

```c
HART_ini();
Set_HART_DeviceID(0x000123UL);   /* ID_3 = 0x00, ID_4 = 0x01, ID_5 = 0x23 */
```

---

## Error Codes

Returned by `ReceiveMessage()`:

| Value | Constant              | Meaning                                          |
|-------|-----------------------|--------------------------------------------------|
| 100   | `HART_OK`             | Frame received and validated                     |
| 0     | `HART_ERR_SHORT`      | RX buffer has fewer than 11 bytes                |
| 1     | `HART_ERR_PREAMBLE`   | Fewer than 2 leading `0xFF` bytes                |
| 2     | `HART_ERR_STARTCHAR`  | Start character not `0x02/06/82/86`              |
| 3     | `HART_ERR_MSGTYPE`    | Start char bits 0–2 not `2` or `6`               |
| 4     | `HART_ERR_ADDRESS`    | Device address does not match `HART_MyDeviceID`  |
| 5     | `HART_ERR_CHECKSUM`   | Checksum mismatch                                |
| 6     | `HART_ERR_TIMEOUT`    | Reception timed out                              |

Returned by `SendCommand()`:

| Value | Meaning                          |
|-------|----------------------------------|
| `1`   | Frame transmitted successfully   |
| `0`   | UART reported a transmission error |

---

## HART Protocol Notes

### Frame types

HART defines two frame formats:

- **Short frame** — 1-byte address. Used when the device is in
  point-to-point mode (single slave on the loop).
- **Long frame** — 5-byte address. Used for multi-drop mode (up to 15
  slaves on one loop) and for full device identification.

### Master / slave roles

- **Primary master** — the control system (DCS, PLC, or handheld).
- **Secondary master** — a secondary configuration tool.
- **Slave** — the field device.

This driver supports all three roles for a **single device ID**. Multi-drop
addressing is not scanned automatically; the caller must set the target
address before sending.

### Baud rate

HART runs at **1200 baud** on the FSK side. The UART between the MCU and
the modem can run faster — 38400 baud is configured by default in this
driver for low latency.

### Command types

| Range     | Type                                 |
|-----------|--------------------------------------|
| 0–30      | Universal commands (device info)     |
| 31–127    | Common-practice commands             |
| 128–253   | Device-specific commands             |
| 254       | Reserved                             |
| 255       | Reserved                             |

Universal commands every device should implement:

- `0` — Read primary variable
- `1` — Read primary variable (percent of range)
- `2` — Read current and percent of range
- `3` — Read dynamic variables and PV current

---


## References

- **HART Communication Foundation** —
  https://fieldcommgroup.org/
- **HART Protocol Specification** (FieldComm Group, HCF_SPEC-12)
- **ATmega8 Datasheet** —
  https://www.microchip.com/wwwproducts/en/ATmega8
- **AVR-Libc Reference Manual** —
  https://www.nongnu.org/avr-libc/user-manual/

---

## License

```c
/*
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
```

---

## Author

*Abolfazl Rostamzadeh*
*Contact: a.rostamzadeh@gmail.com*

```
