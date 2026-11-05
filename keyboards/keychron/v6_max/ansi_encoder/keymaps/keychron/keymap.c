/* Copyright 2024 ~ 2025 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "keychron_common.h"
#include "print.h"
#include "via.h"

enum layers {
    MAC_BASE,
    MAC_FN,
    WIN_BASE,
    WIN_FN,
};

#define FN_MAC MO(MAC_FN)
#define FN_WIN MO(WIN_FN)

/*
 * Hold the physical Fn + Esc keys for 2 seconds to enter STM32 DFU mode.
 * Esc = matrix [0, 0], Fn = matrix [5, 12] on the ANSI encoder layout.
 * Matrix coordinates keep this shortcut working even when VIA remaps keys.
 * Use a USB cable when flashing.
 */
#define V6MAX_DFU_HOLD_MS 2000

/*
 * USB HID console is deliberately off until the physical Fn + F16 keys
 * (matrix [5,12] + [0,20]) are held together for one second.
 * Repeating the same gesture disables logging. Wired USB is required.
 */
#define V6MAX_LOG_TOGGLE_HOLD_MS 1000

static bool v6max_key_log_enabled = false;

/*
 * VIA custom channel for the Windows QMK Toolbox HID Log button.
 * Send: [07, F1, 01, 0/1, 'V', '6', 'L', 'G', ...]
 * Reply: VIA echoes the packet, preserving state in byte 3.
 * This intentionally uses the existing Raw HID interface, not a
 * new USB endpoint. Physical Fn+F16 remains usable as before.
 */
#define V6MAX_LOG_CHANNEL 0xF1
#define V6MAX_LOG_CONTROL 0x01

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (length != 32 || data[1] != V6MAX_LOG_CHANNEL ||
        data[2] != V6MAX_LOG_CONTROL ||
        (data[0] != id_custom_set_value && data[0] != id_custom_get_value) ||
        data[4] != 'V' || data[5] != '6' || data[6] != 'L' || data[7] != 'G') {
        data[0] = id_unhandled;
        return;
    }

    if (data[0] == id_custom_set_value) {
        if (data[3] > 1) {
            data[0] = id_unhandled;
            return;
        }
        v6max_key_log_enabled = data[3] == 1;
        uprintf("[V6MAX] key logging %s (PC HID command)\n",
                v6max_key_log_enabled ? "ON" : "OFF");
    }

    data[3] = v6max_key_log_enabled ? 1 : 0;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (v6max_key_log_enabled) {
        uprintf("[V6MAX KEY] t=%lu layer=%u %s row=%u col=%u kc=0x%04X\n",
                (unsigned long)timer_read32(),
                (unsigned int)get_highest_layer(layer_state | default_layer_state),
                record->event.pressed ? "DOWN" : "UP",
                (unsigned int)record->event.key.row,
                (unsigned int)record->event.key.col,
                (unsigned int)keycode);
    }

    return true;
}


/*
 * Log the actual RGB brightness after QMK has applied a key/encoder change.
 * Fn + encoder is already mapped to UG_VALD / UG_VALU on both Fn layers.
 * Only emit via the existing USB HID console when PC logging is enabled.
 */
static void v6max_rgb_brightness_log_task(void) {
    static bool     initialized = false;
    static uint8_t  last_value = 0;
    static bool     last_enabled = false;

    if (!v6max_key_log_enabled) {
        initialized = false;
        return;
    }

    const uint8_t value = rgb_matrix_get_val();
    const bool    enabled = rgb_matrix_is_enabled();

    if (!initialized || last_value != value || last_enabled != enabled) {
        uprintf("[V6MAX RGB] brightness=%u/255 (%u%%), enabled=%s\n",
                (unsigned int)value,
                (unsigned int)(((uint16_t)value * 100U + 127U) / 255U),
                enabled ? "ON" : "OFF");
        initialized = true;
        last_value = value;
        last_enabled = enabled;
    }
}

void matrix_scan_user(void) {\n    v6max_rgb_brightness_log_task();
    static bool     combo_active = false;
    static bool     boot_requested = false;
    static uint32_t combo_start = 0;


    static bool     log_combo_active = false;
    static bool     log_combo_handled = false;
    static uint32_t log_combo_start = 0;

    const bool log_combo_down = matrix_is_on(0, 20) && matrix_is_on(5, 12);
    if (!log_combo_down) {
        log_combo_active  = false;
        log_combo_handled = false;
    } else if (!log_combo_active) {
        log_combo_active = true;
        log_combo_start  = timer_read32();
    } else if (!log_combo_handled && timer_elapsed32(log_combo_start) >= V6MAX_LOG_TOGGLE_HOLD_MS) {
        log_combo_handled   = true;
        v6max_key_log_enabled = !v6max_key_log_enabled;
        uprintf("[V6MAX] key logging %s (Fn+F16 held 1s)\n", v6max_key_log_enabled ? "ON" : "OFF");
    }

    const bool combo_down = matrix_is_on(0, 0) && matrix_is_on(5, 12);
    if (!combo_down) {
        combo_active   = false;
        boot_requested = false;
        return;
    }

    if (!combo_active) {
        combo_active = true;
        combo_start  = timer_read32();
    } else if (!boot_requested && timer_elapsed32(combo_start) >= V6MAX_DFU_HOLD_MS) {
        boot_requested = true;
        reset_keyboard();
    }
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_ansi_109(
        KC_ESC,   KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_MUTE,   KC_SNAP,  KC_SIRI,  UG_NEXT,   KC_F13,   KC_F14,   KC_F15,   KC_F16,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,   KC_INS,   KC_HOME,  KC_PGUP,   KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,   KC_DEL,   KC_END,   KC_PGDN,   KC_P7,    KC_P8,    KC_P9,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,                                   KC_P4,    KC_P5,    KC_P6,    KC_PPLS,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,             KC_UP,               KC_P1,    KC_P2,    KC_P3,
        KC_LCTL,  KC_LOPTN, KC_LCMMD,                               KC_SPC,                                 KC_RCMMD, KC_ROPTN, FN_MAC,   KC_RCTL,   KC_LEFT,  KC_DOWN,  KC_RGHT,   KC_P0,              KC_PDOT,  KC_PENT),

    [MAC_FN] = LAYOUT_ansi_109(
        _______,  KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   UG_TOGG,   _______,  _______,  UG_TOGG,   _______,  _______,  _______,  _______,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,  _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,            _______,                                  _______,  _______,  _______,  _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,             _______,             _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,            _______,  _______),

    [WIN_BASE] = LAYOUT_ansi_109(
        KC_ESC,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   KC_MUTE,   KC_PSCR,  KC_CTANA, UG_NEXT,   KC_F13,   KC_F14,   KC_F15,   KC_F16,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,   KC_INS,   KC_HOME,  KC_PGUP,   KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,   KC_DEL,   KC_END,   KC_PGDN,   KC_P7,    KC_P8,    KC_P9,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,                                   KC_P4,    KC_P5,    KC_P6,    KC_PPLS,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,             KC_UP,               KC_P1,    KC_P2,    KC_P3,
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  FN_WIN,   KC_RCTL,   KC_LEFT,  KC_DOWN,  KC_RGHT,   KC_P0,              KC_PDOT,  KC_PENT),

    [WIN_FN] = LAYOUT_ansi_109(
        _______,  KC_BRID,  KC_BRIU,  KC_TASK,  KC_FILE,  UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  UG_TOGG,   _______,  _______,  UG_TOGG,   _______,  _______,  _______,  _______,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,  _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,            _______,                                  _______,  _______,  _______,  _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,             _______,             _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,   _______,  _______,  _______,   _______,            _______,  _______)
};

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [MAC_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MAC_FN]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [WIN_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [WIN_FN]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
};
#endif // ENCODER_MAP_ENABLE
