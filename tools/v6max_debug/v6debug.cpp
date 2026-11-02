// SPDX-License-Identifier: GPL-2.0-or-later
// Diagnostic client for Keychron V6 Max ANSI Encoder debug keymap.
// No files are written; output is visible only in this terminal session.
#include <hidapi/hidapi.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

static std::atomic<bool> running{true};

static void on_signal(int) { running.store(false); }

static uint16_t read16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static bool transact(hid_device* dev, unsigned char command, unsigned char value,
                     unsigned char* response, unsigned char control = 0) {
    // hidapi requires a leading zero byte for devices without numbered reports.
    unsigned char request[33] = {};
    request[1] = command;
    request[2] = 0xF0;  // private VIA custom-value channel
    request[3] = value;
    if (command == 0x07) {
        request[4] = control;
        request[5] = 'V';
        request[6] = '6';
        request[7] = 'D';
        request[8] = 'B';
    }

    if (hid_write(dev, request, sizeof(request)) < 0) {
        std::fprintf(stderr, "HID write failed\n");
        return false;
    }

    unsigned char incoming[64] = {};
    for (int retry = 0; retry < 4; ++retry) {
        int received = hid_read_timeout(dev, incoming, sizeof(incoming), 400);
        if (received < 0) {
            std::fprintf(stderr, "HID read failed\n");
            return false;
        }
        if (received == 0) continue;

        // Some HID implementations include report ID zero in reads.
        const unsigned char* ptr = incoming;
        if (received >= 33 && incoming[0] == 0) {
            ptr = incoming + 1;
            --received;
        }
        if (received >= 32 && ptr[0] == command && ptr[1] == 0xF0 && ptr[2] == value) {
            std::memcpy(response, ptr, 32);
            return true;
        }
    }
    std::fprintf(stderr, "No matching debug response (is the debug firmware flashed?)\n");
    return false;
}

int main() {
    std::signal(SIGINT, on_signal);

    if (hid_init() != 0) {
        std::fprintf(stderr, "hid_init failed\n");
        return 1;
    }

    hid_device* device = nullptr;
    hid_device_info* devices = hid_enumerate(0x3434, 0x0960);
    for (hid_device_info* d = devices; d != nullptr; d = d->next) {
        if (d->usage_page == 0xFF60 && d->usage == 0x61) {
            device = hid_open_path(d->path);
            if (device) break;
        }
    }
    hid_free_enumeration(devices);

    if (!device) {
        std::fprintf(stderr, "V6 Max 3434:0960 Raw HID (usage FF60:0061) not found. Use USB Cable mode.\n");
        hid_exit();
        return 1;
    }

    unsigned char reply[32] = {};
    if (!transact(device, 0x07, 0x01, reply, 1) || reply[3] != 1) {
        std::fprintf(stderr, "Could not enable debug session\n");
        hid_close(device);
        hid_exit();
        return 1;
    }

    std::puts("V6 Max debug session started (Ctrl+C to stop).");
    std::puts("Prints keycodes/positions; do not type passwords while debugging.");
    std::fflush(stdout);

    uint16_t last_dropped = 0;
    auto last_stats = std::chrono::steady_clock::now();

    while (running.load()) {
        if (!transact(device, 0x08, 0x02, reply)) break;

        if (reply[7] != 1 || reply[6] == 0 || reply[3] > 3) {
            std::fprintf(stderr, "Debug session ended or invalid response\n");
            break;
        }
        const uint16_t dropped = read16(reply + 4);
        if (dropped != last_dropped) {
            std::printf("[WARN] dropped events total=%u\n", unsigned(dropped));
            last_dropped = dropped;
        }
        for (unsigned n = 0; n < reply[3]; ++n) {
            const unsigned char* p = reply + 8 + 8 * n;
            std::printf("[KEY] t=%u ms row=%u col=%u keycode=0x%04X %s\n",
                        unsigned(read32(p)), unsigned(p[6]), unsigned(p[7] & 0x7F),
                        unsigned(read16(p + 4)), (p[7] & 0x80) ? "DOWN" : "UP");
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_stats >= std::chrono::seconds(1)) {
            if (!transact(device, 0x08, 0x03, reply)) break;
            std::printf("[SCAN] %u scans/s, pending=%u, dropped=%u\n",
                        unsigned(read16(reply + 4)), unsigned(reply[8]),
                        unsigned(read16(reply + 6)));
            std::fflush(stdout);
            last_stats = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (transact(device, 0x07, 0x01, reply, 0)) {
        std::puts("Debug session stopped.");
    }
    hid_close(device);
    hid_exit();
    return 0;
}
