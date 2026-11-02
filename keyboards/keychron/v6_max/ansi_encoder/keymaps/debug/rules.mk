# Keep the original Keychron VIA/Launcher interface intact.
VIA_ENABLE = yes

# Collect matrix scans through QMK's built-in counter without enabling a
# separate USB console (the USB FS IN endpoints are already in use).
OPT_DEFS += -DDEBUG_MATRIX_SCAN_RATE
