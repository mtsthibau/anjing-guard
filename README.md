# Hello World — M5Stack Cardputer Adv

A minimal "Hello, World!" firmware for the **M5Stack Cardputer Adv**.  
Type on the keyboard, see the text on screen, hear a startup chime — that's it.  
Built with **PlatformIO + Arduino** and designed to be flashed via the [bmorcelli/Launcher](https://bmorcelli.github.io/Launcher/).

---

## Table of Contents

1. [What you need](#1-what-you-need)
2. [Install the tools](#2-install-the-tools)
3. [Get the code](#3-get-the-code)
4. [Build](#4-build)
5. [Flash to the device](#5-flash-to-the-device)
6. [Monitor serial output](#6-monitor-serial-output)
7. [How it works](#7-how-it-works)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. What you need

**Hardware**

| Item | Details |
|------|---------|
| M5Stack Cardputer Adv | ESP32-S3FN8 @ 240 MHz, 8 MB flash |
| Display | ST7789V2 TFT 1.14" — 240 × 135 px |
| Keyboard | 56-key physical keyboard (TCA8418 I2C controller) |
| Speaker | NS4168 I2S |
| BtnA | Shoulder button on GPIO 0 |
| USB-C cable | For power and flashing |
| SD card *(optional)* | Only needed for the Launcher flash method |

**Software**

| Tool | Purpose |
|------|---------|
| [Git](https://git-scm.com/) | Clone this repository |
| [Python 3.6+](https://www.python.org/downloads/) | Required by PlatformIO |
| [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) | Build and flash tool |
| [bmorcelli/Launcher](https://bmorcelli.github.io/Launcher/) *(optional)* | Manage apps from an SD card menu |

> **VS Code users:** install the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) instead of the CLI — it bundles everything automatically.

---

## 2. Install the tools

### Option A — VS Code (recommended for beginners)

1. Install [VS Code](https://code.visualstudio.com/).
2. Open the **Extensions** panel (`Ctrl+Shift+X`).
3. Search for **PlatformIO IDE** and install it.
4. Restart VS Code. PlatformIO and all required toolchains download automatically.

### Option B — PlatformIO CLI

```bash
# Install PlatformIO using pip
pip install platformio

# Verify the installation
pio --version
```

The first time you run a build, PlatformIO downloads the ESP32 toolchain and the platform package (~200 MB). An internet connection is required.

---

## 3. Get the code

```bash
git clone https://github.com/your-org/anjing-guard.git
cd anjing-guard
```

The repository structure is intentionally minimal:

```
anjing-guard/
├── platformio.ini   ← build configuration (board, libraries, flags)
└── src/
    └── main.cpp     ← all application code (≈ 200 lines)
```

---

## 4. Build

```bash
pio run -e m5stack-cardputer-adv
```

PlatformIO will:
1. Download the ESP32 platform package (first run only).
2. Download and cache the two library dependencies (first run only).
3. Compile `src/main.cpp` and link the firmware.

A successful build ends with something like:

```
Building .pio/build/m5stack-cardputer-adv/firmware.bin
RAM:   [=         ]   9.3% (used 30432 bytes from 327680 bytes)
Flash: [==        ]  17.4% (used 458736 bytes from 8388608 bytes)
```

The compiled binary is at:

```
.pio/build/m5stack-cardputer-adv/firmware.bin
```

> **Board note:** PlatformIO does not yet ship a dedicated `m5stack-cardputer`
> board manifest for this platform release. The build targets `esp32-s3-devkitc-1`,
> which matches the Cardputer Adv's chip (ESP32-S3FN8). All device-specific
> settings (8 MB flash, QIO mode, USB-CDC) are applied in `platformio.ini`.

### Library dependencies

PlatformIO installs these automatically — no manual action needed.

| Library | Version | What it does |
|---------|---------|--------------|
| `m5stack/M5Unified` | ^0.2.2 | Abstracts display, buttons, and speaker |
| `adafruit/Adafruit TCA8418` | ^1.0.1 | Drives the I2C keyboard controller |

---

## 5. Flash to the device

Choose whichever method suits you.

### Method A — Via Launcher (SD card, no USB required)

This is the safest method — it does not overwrite the Launcher itself.

1. Build the firmware (`pio run …` above).
2. Copy `firmware.bin` to the **root of your SD card**.
3. Insert the SD card into the Cardputer Adv.
4. Power on and open the Launcher menu.
5. Navigate to `firmware.bin` and press **Enter / Select** — the Launcher flashes and launches it.

### Method B — Direct USB upload

This writes the firmware directly over USB, bypassing the Launcher.

1. Connect the Cardputer Adv to your PC with a USB-C cable.
2. Put the device into **bootloader mode**: hold **GPIO 0** (BtnA), then press **Reset**, then release both.
3. Run:

```bash
pio run -e m5stack-cardputer-adv -t upload --upload-port /dev/ttyACM0
```

Replace `/dev/ttyACM0` with your actual port:

| OS | Typical port |
|----|-------------|
| Linux | `/dev/ttyACM0` or `/dev/ttyUSB0` |
| macOS | `/dev/cu.usbmodem*` |
| Windows | `COM3`, `COM4`, … (check Device Manager) |

---

## 6. Monitor serial output

The firmware outputs debug information over USB-CDC at 115200 baud.

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

Press `Ctrl+C` to exit the monitor.

---

## 7. How it works

### What you see on screen

```
┌────────────────────────────┐
│      Hello, World!         │  ← title bar (blue)
│ fn+key: type  BtnA: clear  │  ← hint (grey)
│ Input: hello               │  ← live typed text (green)
│                            │
│ dbg r=3 c=13 -> ' ' 0x20   │  ← debug row (dark grey)
└────────────────────────────┘
```

### Controls

| Input | What happens |
|-------|-------------|
| Any printable key | Character appended to the input line (max 40 chars) |
| `Backspace` | Deletes the last character |
| `Enter` | Clears the input line |
| `Shift` + key | Uppercase letter or symbol |
| **BtnA** (shoulder button) | Clears input + plays a short beep |

### Startup chime

On boot the speaker plays three tones: **C (523 Hz) → E (659 Hz) → G (784 Hz)**.

### Keyboard internals

The keyboard controller (TCA8418) reports raw key codes. The firmware remaps them to the standard 4 × 14 physical layout using the M5Stack formula:

```
buffer   = (rawCode & 0x7F) - 1
tca_row  = buffer / 10
tca_col  = buffer % 10
phys_col = tca_row * 2 + (tca_col > 3 ? 1 : 0)   // 0 – 13
phys_row = (tca_col + 4) % 4                       // 0 – 3
```

---

## 8. Troubleshooting

### "Unknown board ID 'm5stack-cardputer'"

The platform release used here does not include a Cardputer board manifest. The build uses `esp32-s3-devkitc-1` instead — this is intentional and correct. See the note in [§4 Build](#4-build).

### Keys produce wrong characters

Open the serial monitor while pressing a key. The debug row on screen shows:

```
dbg r=X c=Y -> 'x' 0xNN
```

Find the physical row/col for the mismatched key and update the `kbNorm` / `kbShft` lookup tables in `src/main.cpp`.

Default modifier positions (may vary by unit):

| Modifier | Row | Col |
|----------|-----|-----|
| Shift | 3 | 0 |
| FN | 3 | 12 |
| Space | 3 | 13 |

### Upload fails / device not detected

- Make sure the USB-C cable supports data (some cables are power-only).
- Confirm you entered bootloader mode correctly: **hold BtnA → press Reset → release both → then run the upload command**.
- On Linux, add yourself to the `dialout` group if permission is denied:
  ```bash
  sudo usermod -aG dialout $USER
  # Log out and back in for the change to take effect
  ```

### PlatformIO can't find Python

PlatformIO requires Python 3.6+. Run `python3 --version` to check. If missing, install it from [python.org](https://www.python.org/downloads/) and then reinstall PlatformIO.
# anjing-guard
