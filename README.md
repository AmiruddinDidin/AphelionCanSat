# AphelionCanSat

A CanSat telemetry system built around an ESP32-S3 Super Mini, transmitting live GPS/IMU/barometer data over LoRa, logging a black-box backup to SD, and displaying everything on a custom browser-based ground station.

## Hardware

| Component            | Interface | Notes                                  |
|-----------------------|-----------|-----------------------------------------|
| ESP32-S3 Super Mini   | —         | Main flight computer                    |
| Ebyte E220 (LoRa)     | UART      | Mode 0 (Normal/Transparent), M0/M1→GND  |
| u-blox GPS (NEO-M8N / NEO-7M) | UART | NMEA @ 9600 baud                  |
| Bosch BNO055 (IMU)    | I2C       | Address `0x29` (ADR pin high on this unit — verify with I2C scanner if swapping hardware) |
| MS5611 (barometer)    | I2C       | Address `0x77`; requires `reset(1)` math mode fix — see Known Issues |
| MicroSD module        | SPI       | Black-box logging backup                |

## Pinout

| Signal        | GPIO | Notes                        |
|---------------|------|-------------------------------|
| LoRa RX (E220 TX) | GP1  | UART1                     |
| LoRa TX (E220 RX) | GP2  | UART1                     |
| I2C SDA           | GP13 | Shared: BNO055 + MS5611   |
| I2C SCL           | GP12 | Shared: BNO055 + MS5611   |
| GPS RX (module TX) | GP18 | UART2                    |
| GPS TX (module RX) | GP17 | UART2                    |
| SD CS             | GP4  | SPI — verify pin is broken out on your board |
| SD MOSI           | GP5  | SPI                       |
| SD CLK            | GP6  | SPI                       |
| SD MISO           | GP7  | SPI                       |

## Firmware files

- **`cansat_esp32s3.ino`** — main flight firmware: reads all sensors, builds a CSV telemetry packet once per second, transmits over LoRa, mirrors to USB serial, and logs to SD.
- **`i2c_scanner.ino`** — standalone diagnostic to list I2C devices and their addresses; run this first whenever a sensor won't initialize.
- **`ms5611_diagnostic.ino`** — standalone diagnostic that prints MS5611 PROM calibration data and live read results, for isolating barometer issues.
- **`cansat_ble_addon.ino`** — optional BLE UART mirror of telemetry for phone-based bench debugging (short range only, not a flight telemetry path).
- **`cansat_mavlink_addon.ino`** — optional MAVLink packetizer if you want to view telemetry in Mission Planner/QGroundControl instead of the custom dashboard.
- **`cansat_sd_logger.ino`** — reference copy of the SD logging functions (already merged into the main sketch).
- **`cansat_gcs.html`** — standalone browser ground station (Chrome/Edge only, uses Web Serial API): live map, attitude indicator, altitude chart, and optional live video panel via USB capture card.

## Telemetry packet format

```
$TLM,millis,lat,lon,gpsAlt,sats,pressHpa,baroAlt,baroTemp,roll,pitch,yaw,ax,ay,az,calSys,calGyro,calAcc,calMag
```

Sent once per second over LoRa (transparent passthrough) and USB serial, and appended to `/Aphelion_log.csv` on the SD card.

## Required libraries (Arduino Library Manager)

- TinyGPSPlus (Mikal Hart)
- Adafruit BNO055 + Adafruit Unified Sensor + Adafruit BusIO
- MS5611 (Rob Tillaart) — use version 0.3.6 or newer
- SD + SPI (bundled with ESP32 Arduino core)

## Board settings (Arduino IDE 2.x)

- Board: **ESP32S3 Dev Module** (no dedicated "Super Mini" entry exists)
- **USB CDC On Boot: Enabled** — required for Serial Monitor over the native USB port
- Flash Size: 4MB, Partition Scheme: Default
- PSRAM: enable only if your specific board has it

## Setup checklist

1. Wire sensors per the pinout table above; verify actual GPIO breakout against your board's silkscreen before trusting any pin number here.
2. Upload `i2c_scanner.ino` first — confirm both `0x29` (or `0x28`) and `0x77` show up before touching the main sketch.
3. Upload the main sketch. Confirm `BNO055 OK`, `MS5611 OK`, and `SD card OK` all print at boot.
4. Set `SEA_LEVEL_HPA` in the main sketch to your local QNH for accurate barometric altitude.
5. Open `cansat_gcs.html` in Chrome/Edge, connect to the ground-side E220 (via USB-TTL) at 9600 baud to view live telemetry.

## Known issues / fixes already applied

- **BNO055 not detected**: this unit's ADR pin is pulled high, so it responds at `0x29`, not the library default `0x28`. Already fixed in the constructor.
- **MS5611 reporting ~half of actual pressure** (e.g. 466 hPa instead of ~930 hPa, producing a nonsense ~6000m altitude): known factor-of-2 math bug on some MS5611-compatible clone chips. Fixed by calling `ms5611.reset(1)` after `begin()` to select the alternate math mode.
- **Serial Monitor showing nothing but the ESP-ROM boot line**: means `USB CDC On Boot` is Disabled for the current sketch — this setting does not always persist across sketches in Arduino IDE 2.x and must be re-checked per sketch.

## Not yet implemented / possible next steps

- SD write error checking is minimal — no retry logic if a write silently fails mid-flight.
- No packet checksum on the LoRa/CSV telemetry — a corrupted byte mid-packet is dropped rather than corrected.
- No onboard event/state logic (e.g. apogee detection, deployment triggers) — this is sensor readout + transmission only.

