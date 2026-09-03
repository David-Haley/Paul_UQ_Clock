# Paul_UQ_Clock

A 12-LED analog-style clock for the Raspberry Pi Pico W, built on FreeRTOS. Hour, minute, and second are each shown as a coloured marker on a 12-position addressable LED ring, with a separate LED indicating AM/PM, plus 7 auxiliary LEDs that can be set (and optionally flashed) over MQTT.

## Hardware

- Raspberry Pi Pico W
- A 20-LED addressable (WS2812-style) strip, driven via PIO/DMA using the [`Addressable_LED`](https://github.com/David-Haley/LED_Driver) class
  - LEDs 0-11 form the 12-position clock face
  - LEDs 12-18 are auxiliary LEDs, set from JSON delivered over MQTT
  - LED 19 is the AM/PM indicator

## Display

Each second, `Clock_Set` (in `clock.cpp`) reads the RP2040's onboard RTC and lights:

- **Red** — hour (`hour % 12`)
- **Green** — minute (`minute / 5`)
- **Blue** — second (`second / 5`)
- **Yellow** — AM, or hour+minute markers coincide on the same LED
- **Cyan** — PM, or minute+second markers coincide
- **Magenta** — hour+second markers coincide
- **White** — hour, minute, and second markers all coincide

Since each LED can only show one colour at a time, collisions between markers are shown as a distinct blended colour rather than silently overwriting one another.

## Time sync

On boot, the board connects to Wi-Fi and sets its RTC from NTP (preferring an
NTP server offered via DHCP, falling back to `pool.ntp.org`), then resyncs once an
hour. This runs as its own FreeRTOS task.

## Auxiliary LEDs

LEDs 12-18 are controlled independently of the clock face: a second FreeRTOS task
shares the same Wi-Fi connection, connects to an MQTT broker, and subscribes to a
topic carrying JSON messages of the form (see `aux_led.json schema` and
`Aux_LED_Examples/test_1.json`):

```json
{
  "aux_led": [
    { "led": 12, "colour": "red" },
    { "led": 18, "colour": "white", "flash": true }
  ]
}
```

Each message is authoritative for all 7 auxiliary LEDs — any of LEDs 12-18 not
mentioned in a message is turned off. `colour` is one of `red`, `yellow`, `green`,
`cyan`, `blue`, `magenta`, `white`, or `black`; `flash` (optional, default `false`)
makes that LED blink at 1Hz instead of staying solid.

## Building

Requires the Raspberry Pi Pico SDK and FreeRTOS Kernel, with `PICO_SDK_PATH` and `FREERTOS_KERNEL_PATH` set in the environment (or picked up automatically if installed as siblings under `~/pico/`).

You also need a `configuration.h` in the project root (not checked in — see
`.gitignore`) defining:

```c
#define WiFi_SSID     "..."
#define wiFi_Password "..."
#define UTC_Offset    600  // minutes to add to UTC to get local time
#define Broker        "..."  // MQTT broker hostname
#define User          "..."  // MQTT username
#define Password      "..."  // MQTT password
#define Topic         "..."  // MQTT topic carrying aux LED commands
```

```bash
./Build.sh
```

or manually:

```bash
cmake -S . -B ./build
cmake --build ./build
```

This produces `build/paul_uq_clock.uf2` (along with `.elf`, `.bin`, `.hex`). If CMake
reports an "Incompatible picotool installation" error, reconfigure with
`cmake -S . -B ./build -DPICOTOOL_FORCE_FETCH_FROM_GIT=1` so it builds a matching
`picotool` instead of using the system one.

## Flashing

Hold BOOTSEL while plugging in the Pico W (or reboot an already-flashed board into bootloader mode with `picotool reboot -f -u`), then copy the UF2 file onto the drive that appears:

```bash
cp build/paul_uq_clock.uf2 /media/$USER/RPI-RP2/
```

The board will automatically reboot and run the new firmware.

## License

GPLv3 — see [LICENSE](LICENSE).
