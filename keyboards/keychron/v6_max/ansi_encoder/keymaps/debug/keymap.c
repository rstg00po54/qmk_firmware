/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * V6 Max debugging keymap.
 *
 * Reuse the stock Keychron keymap, including all four layers and encoder
 * mappings, without copying or changing the original firmware.
 */
#include "../keychron/keymap.c"

#include "via.h"
#include "timer.h"
#include "keyboard.h"

#define V6DBG_CHANNEL    0xF0
#define V6DBG_CONTROL    0x01
#define V6DBG_EVENTS     0x02
#define V6DBG_STATS      0x03

#define V6DBG_CAPACITY   32
#define V6DBG_TIMEOUT_MS 5000

typedef struct {
    uint32_t time_ms;
    uint16_t keycode;
    uint8_t  row;
    uint8_t  col_and_pressed;
} v6dbg_event_t;

static v6dbg_event_t v6dbg_events[V6DBG_CAPACITY];
static uint8_t       v6dbg_read_index;
static uint8_t       v6dbg_write_index;
static uint8_t       v6dbg_count;
static uint16_t      v6dbg_dropped;
static bool          v6dbg_enabled;
static uint32_t      v6dbg_last_poll_ms;

static void v6dbg_reset_queue(void) {
    v6dbg_read_index  = 0;
    v6dbg_write_index = 0;
    v6dbg_count       = 0;
    v6dbg_dropped     = 0;
}

static bool v6dbg_capture_enabled(void) {
    if (v6dbg_enabled && timer_elapsed32(v6dbg_last_poll_ms) > V6DBG_TIMEOUT_MS) {
        v6dbg_enabled = false;
        v6dbg_reset_queue();
    }
    return v6dbg_enabled;
}

// This hook runs only when a key event has survived matrix debouncing.
// Recording is disabled until the host explicitly starts a session.
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!v6dbg_capture_enabled()) {
        return true;
    }

    if (v6dbg_count == V6DBG_CAPACITY) {
        v6dbg_read_index = (v6dbg_read_index + 1) % V6DBG_CAPACITY;
        --v6dbg_count;
        if (v6dbg_dropped != UINT16_MAX) {
            ++v6dbg_dropped;
        }
    }

    v6dbg_event_t *event = &v6dbg_events[v6dbg_write_index];
    event->time_ms         = timer_read32();
    event->keycode         = keycode;
    event->row             = record->event.key.row;
    event->col_and_pressed = (record->event.key.col & 0x7F) | (record->event.pressed ? 0x80 : 0);
    v6dbg_write_index      = (v6dbg_write_index + 1) % V6DBG_CAPACITY;
    ++v6dbg_count;

    return true;  // Never intercept or modify a real key press.
}

// VIA custom-value extension: [command, 0xF0, value, ...].
// Uses the existing 32-byte Raw HID interface; no extra USB endpoint.
// Only a host which explicitly sends the "V6DB" start command can collect
// temporary key events. No persistent logging or unsolicited HID packets.
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (length != 32 || data[1] != V6DBG_CHANNEL) {
        data[0] = id_unhandled;
        return;
    }

    const uint8_t command = data[0];
    const uint8_t value   = data[2];

    if (command == id_custom_set_value && value == V6DBG_CONTROL) {
        if (data[4] != 'V' || data[5] != '6' || data[6] != 'D' || data[7] != 'B' ||
            (data[3] != 0 && data[3] != 1)) {
            data[0] = id_unhandled;
            return;
        }

        v6dbg_reset_queue();
        v6dbg_enabled      = data[3] != 0;
        v6dbg_last_poll_ms = timer_read32();
        data[3]            = v6dbg_enabled ? 1 : 0;
        return;
    }

    if (command != id_custom_get_value) {
        data[0] = id_unhandled;
        return;
    }

    const bool active = v6dbg_capture_enabled();
    if (value == V6DBG_STATS) {
        const uint32_t rate = get_matrix_scan_rate();
        const uint16_t hz   = rate > UINT16_MAX ? UINT16_MAX : (uint16_t)rate;

        if (active) {
            v6dbg_last_poll_ms = timer_read32();
        }
        data[3] = active ? 1 : 0;
        data[4] = hz & 0xFF;
        data[5] = hz >> 8;
        data[6] = v6dbg_dropped & 0xFF;
        data[7] = v6dbg_dropped >> 8;
        data[8] = v6dbg_count;
        return;
    }

    if (value != V6DBG_EVENTS) {
        data[0] = id_unhandled;
        return;
    }

    if (active) {
        v6dbg_last_poll_ms = timer_read32();
    }

    // Header: [8,F0,2,count,dropped_lo,dropped_hi,active,version].
    // Three events per report, each [time_ms:u32, keycode:u16, row:u8,
    // col:7 bits + pressed:top bit], all integers little-endian.
    data[3] = 0;
    data[4] = v6dbg_dropped & 0xFF;
    data[5] = v6dbg_dropped >> 8;
    data[6] = active ? 1 : 0;
    data[7] = 1;  // protocol version

    for (uint8_t n = 0; active && n < 3 && v6dbg_count != 0; ++n) {
        const v6dbg_event_t *event = &v6dbg_events[v6dbg_read_index];
        const uint8_t offset      = 8 + 8 * n;
        data[offset + 0]          = event->time_ms & 0xFF;
        data[offset + 1]          = (event->time_ms >> 8) & 0xFF;
        data[offset + 2]          = (event->time_ms >> 16) & 0xFF;
        data[offset + 3]          = (event->time_ms >> 24) & 0xFF;
        data[offset + 4]          = event->keycode & 0xFF;
        data[offset + 5]          = event->keycode >> 8;
        data[offset + 6]          = event->row;
        data[offset + 7]          = event->col_and_pressed;
        v6dbg_read_index          = (v6dbg_read_index + 1) % V6DBG_CAPACITY;
        --v6dbg_count;
        ++data[3];
    }
}
