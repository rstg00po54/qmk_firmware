# Keychron V6 Max USB debugging

This is an **opt-in, temporary** key-event/scan-rate diagnostic build for the
**ANSI encoder** V6 Max only. The original `keychron` keymap and the
`2025q3` branch are untouched.

## Firmware build (Ubuntu)

```bash
cd ~/KeychronQMK
git remote add myfork https://github.com/rstg00po54/qmk_firmware.git
git fetch myfork v6max_debug_hid
git switch --track -c v6max_debug_hid myfork/v6max_debug_hid
qmk compile -kb keychron/v6_max/ansi_encoder -km debug
```

The output is `keychron_v6_max_ansi_encoder_debug.bin`.

Do **not** flash the binary until you have verified your keyboard model/layout
and have a working recovery path. This code is a debug experiment, not an
official Keychron build. Do not use the QMK console for this keymap: it reuses
the existing VIA Raw HID endpoint (Usage Page `FF60`, Usage `0061`).

## Host client

Requires [hidapi](https://github.com/libusb/hidapi).

Ubuntu:

```bash
sudo apt install -y g++ libhidapi-dev
g++ -std=c++17 -O2 tools/v6max_debug/v6debug.cpp -o v6debug -lhidapi-hidraw
./v6debug
```

Windows (MSYS2 UCRT64 terminal):

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-hidapi
g++ -std=c++17 -O2 tools/v6max_debug/v6debug.cpp -o v6debug.exe -lhidapi
./v6debug.exe
```

Run the commands from the root of the checked-out repository. Connect via a
USB cable in Cable mode, and close Keychron Launcher / VIA while debugging.
On Linux, HID access may require appropriate udev permissions. Do not run as
root unless you understand the implications.

The host opens VID:PID `3434:0960`, interface Usage Page `FF60`, Usage
`0061`. Run it in a terminal and press Ctrl+C to stop.

## Output

```text
V6 Max debug session started (Ctrl+C to stop).
[KEY] t=12345 ms row=3 col=1 keycode=0x0004 DOWN
[KEY] t=12401 ms row=3 col=1 keycode=0x0004 UP
[SCAN] 1580 scans/s, pending=0, dropped=0
```

Numbers above are illustrative, not measurements.

**Privacy:** Key events may reveal typed input. The firmware buffers at most
32 recent events in RAM **only after** the desktop tool explicitly enables
debugging. The queue clears when debugging ends and expires after 5 seconds
without a host poll; it is never written to flash. Do not type passwords while
debugging. The C++ client prints to stdout only; it does not write log files.

## Wire protocol

Existing 32-byte VIA Raw HID reports. No additional endpoints. All requests
and replies have header `[cmd, 0xF0, value]`:

- `[0x07,0xF0,0x01,1,'V','6','D','B']`: start, reset queue, enable capture
- `[0x07,0xF0,0x01,0,'V','6','D','B']`: stop, reset queue
- `[0x08,0xF0,0x02]`: drain up to three key events
- `[0x08,0xF0,0x03]`: current scan rate and queue stats

The drain reply contains `[0x08, 0xF0, 0x02, count, dropped_lo,
dropped_hi, active, protocol_version]` followed by up to three 8-byte
records. Each record is `time_ms:u32le, keycode:u16le, row:u8,
col_and_pressed:u8`; bit 7 of the last byte is pressed.

The stats reply contains `[0x08,0xF0,0x03,active,
scan_rate_lo,scan_rate_hi,dropped_lo,dropped_hi,pending]`.

Unknown commands and channels still follow the original VIA / Keychron
handling. Debug protocol version is 1. After five seconds without a poll, the
temporary capture session expires. The scanner rate is measured by the
QMK `DEBUG_MATRIX_SCAN_RATE` counter. This counter describes actual matrix
passes per second, not USB polling frequency.
