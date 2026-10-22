# ESP32-S3 Captive Portal Firmware

The canonical home for this project's ESP32-S3 captive-portal work. Two source builds live here, plus a full firmware/partition backup of the flashed hardware.


## Source builds (`src_arduino/`)

### `ESP32S3_CaptivePortal/` — e-ink hardware build
The fuller-featured build, targeting an Adafruit ESP32-S3 Qualia board with a Waveshare 2.13" e-ink display:
- Captive-portal credential capture logged to LittleFS, latest capture mirrored live to the e-ink screen (two-column layout: SSID/channel/capture count on the left, latest credential on the right).
- **ON button** (GPIO0) wakes the board from deep sleep via EXT0; auto-sleeps after 5 minutes of inactivity.
- **UP button** (GPIO6) hops the AP through WiFi channels 1–13, debounced to fire once per press, restarts the AP on the new channel.
- `captureCount` and `currentChannel` persist across deep sleep in RTC memory.

### `EvilPortal_Custom/` — teaching/demo build
A receive-only WiFi security teaching demo: two selectable portal login pages switchable live via GPIO buttons, an `/admin` dashboard (captured credentials + clear), a passive channel scanner (ch 1/6/11), and a passive probe-request sniffer (MAC → requested SSID). No deauth or jamming — everything here only receives. Public mirror: [s3-captive-portal](https://github.com/nicklaststop22/s3-captive-portal).

Both builds target the Arduino/ESP32 core; see each folder's `.ino`/`.cpp` for the board-specific pin config to edit before flashing (`AP_SSID`, button/LED pins, e-ink SPI pins where applicable).

## Firmware/partition backup (`firmware/`, `scripts/`)

A full flash dump of the hardware as last verified working, for cloning a board without rebuilding from source.

### Hardware Specifications

- **SoC**: Espressif ESP32-S3 (QFN56) revision v0.2
- **CPU**: Dual-core 32-bit Xtensa LX7 @ up to 240 MHz + ULP coprocessor
- **Flash Size**: 16 MB (Quad SPI, 3.3V)
- **Embedded PSRAM**: 8 MB (AP_3v3)
- **Connectivity**: 2.4 GHz Wi-Fi (802.11 b/g/n) & Bluetooth 5 (LE) / Mesh
- **USB Interface**: Native USB Serial / JTAG (`VID: 0x303A`, `PID: 0x1001`)

### Partition Table Layout

The flash uses the standard ESP-IDF / Arduino 16MB partition map:

| Partition | Type | Subtype | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **bootloader** | bootloader | - | `0x000000` | 32 KB | 2nd stage bootloader |
| **partition_table** | sys | - | `0x008000` | 4 KB | Partition definitions |
| **nvs** | data | nvs (0x02) | `0x009000` | 20 KB | Non-volatile storage |
| **otadata** | data | ota (0x00) | `0x00e000` | 8 KB | OTA boot selection |
| **app0** | app | ota_0 (0x10) | `0x010000` | 3,264 KB | Primary application binary |
| **app1** | app | ota_1 (0x11) | `0x340000` | 3,264 KB | Secondary OTA slot |
| **spiffs** | data | spiffs (0x82) | `0x670000` | 1,536 KB | LittleFS filesystem assets |
| **coredump** | data | coredump (0x03) | `0x7f0000` | 64 KB | Crash telemetry |

### File Verification & Integrity

| File | Offset | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| `firmware/esp32s3_full_16MB_firmware_backup.bin` | `0x000000` | 16,777,216 bytes | `80BDCAF1041512DAADE41F87EC3FF3D5C423F0B3109C2ECBAE798C32DE30B5CC` |

### Flashing Instructions

#### Prerequisites
Install `esptool`:
```bash
pip install esptool
```

#### Option A: Flash the Full 16MB Image (Recommended for cloning)
Using the PowerShell script:
```powershell
.\scripts\flash_all.ps1 -Port COM5
```
Or via raw CLI:
```bash
esptool --chip esp32s3 --port COM5 --baud 921600 --before usb-reset write-flash 0x0 firmware/esp32s3_full_16MB_firmware_backup.bin
```

#### Option B: Flash Individual Partitions
Using the PowerShell script:
```powershell
.\scripts\flash_partitions.ps1 -Port COM5
```
Or via raw CLI:
```bash
esptool --chip esp32s3 --port COM5 --baud 921600 --before usb-reset write-flash \
    0x0000 firmware/partitions/bootloader.bin \
    0x8000 firmware/partitions/partition_table.bin \
    0xe000 firmware/partitions/otadata.bin \
    0x10000 firmware/partitions/app0_firmware.bin \
    0x670000 firmware/partitions/spiffs_littlefs.bin
```


## Disclaimer

For educational purposes only. See [DISCLAIMER.md](DISCLAIMER.md).
