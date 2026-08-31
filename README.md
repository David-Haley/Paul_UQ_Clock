# Paul_UQ_Clock

A 12-LED analog-style clock for the Raspberry Pi Pico W, built on FreeRTOS. Hour, minute, and second are each shown as a coloured marker on a 12-position addressable LED ring, with a separate LED indicating AM/PM.

## Hardware

- Raspberry Pi Pico W
- A 20-LED addressable (WS2812-style) strip, driven via PIO/DMA using the [`Addressable_LED`](https://github.com/David-Haley/LED_Driver) class
  - LEDs 0-11 form the 12-position clock face
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

## Building

Requires the Raspberry Pi Pico SDK and FreeRTOS Kernel, with `PICO_SDK_PATH` and `FREERTOS_KERNEL_PATH` set in the environment (or picked up automatically if installed as siblings under `~/pico/`).

```bash
./Build.sh
```

or manually:

```bash
cmake -S . -B ./build
cmake --build ./build
```

This produces `build/paul_uq_clock.uf2` (along with `.elf`, `.bin`, `.hex`).

## Flashing

Hold BOOTSEL while plugging in the Pico W (or reboot an already-flashed board into bootloader mode with `picotool reboot -f -u`), then copy the UF2 file onto the drive that appears:

```bash
cp build/paul_uq_clock.uf2 /media/$USER/RPI-RP2/
```

The board will automatically reboot and run the new firmware.

## License

GPLv3 — see [LICENSE](LICENSE).
