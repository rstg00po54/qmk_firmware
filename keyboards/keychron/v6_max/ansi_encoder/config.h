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

#pragma once

#ifdef RGB_MATRIX_ENABLE
/* RGB current tune: previous test = 0x30; this build = 0x40.
 * This is a programmed setting, not a measured LED/USB current.
 * Check LED specifications, USB input current and temperatures.
 */
#    define V6MAX_RGB_CURRENT_TUNE_VALUE 0x40U
#    define SNLED27351_CURRENT_TUNE \
        { V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, \
          V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, \
          V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, \
          V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE, V6MAX_RGB_CURRENT_TUNE_VALUE }

/* RGB Matrix Configuration */
#    define RGB_MATRIX_LED_COUNT 108

/* Indications */
#    define NUM_LOCK_INDEX 37
#    define CAPS_LOCK_INDEX 61
#    define LOW_BAT_IND_INDEX \
        { 97 }

#endif
