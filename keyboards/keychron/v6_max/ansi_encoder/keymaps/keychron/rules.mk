VIA_ENABLE = yes
CONSOLE_ENABLE = yes
# STM32F401 has 3 USB user endpoints; combine the keyboard with shared HID
# so VIA raw HID and the QMK HID console can coexist.
# Note: shared keyboard endpoint can reduce compatibility with some BIOSes.
KEYBOARD_SHARED_EP = yes
