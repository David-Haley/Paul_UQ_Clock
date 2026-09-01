# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a Raspberry Pi Pico W that drives a 20-LED addressable (WS2812-style)
ring as a 12-position analog-style clock. LEDs 0-11 form the clock face; LED 19 is
an AM/PM indicator. Runs on FreeRTOS (SMP, both RP2040 cores) with three tasks:
`led_task` (updates the display every second from the RP2040's onboard RTC),
`heartbeat_task` (prints a heartbeat over USB stdio), and the NTP task (connects to
Wi-Fi, sets the RTC from NTP, and resyncs hourly — see Architecture below).

## Build

```bash
./Build.sh
```

or manually:

```bash
cmake -S . -B ./build
cmake --build ./build
```

Produces `build/paul_uq_clock.uf2` (plus `.elf`, `.bin`, `.hex`). There is no test
suite or linter configured — verification is by building and flashing to hardware.

Requires `PICO_SDK_PATH` and `FREERTOS_KERNEL_PATH` env vars (or the SDK/kernel
installed as siblings under `~/pico/`). On this machine they are:
`PICO_SDK_PATH=/home/david/pico/pico-sdk`, `FREERTOS_KERNEL_PATH=/home/david/pico/FreeRTOS-Kernel`.

Also requires a project-root `configuration.h` (gitignored, not checked in — see
`.gitignore`'s "Private (security issue)" section) defining `WiFi_SSID`,
`wiFi_Password`, and `UTC_Offset` (minutes to add to UTC for local time). The build
fails without it, since `wifi.cpp`/`ntp.cpp` include it directly.

A clean build needs `-DPICOTOOL_FORCE_FETCH_FROM_GIT=1` passed to the first `cmake
-S . -B ./build` — the system's installed `picotool` version won't match what this
SDK checkout expects, and CMake otherwise fails with an "Incompatible picotool
installation" error rather than fetching a matching one itself.

## Flashing

Hold BOOTSEL while plugging in the Pico W (or `picotool reboot -f -u` on an
already-flashed board), then:

```bash
cp build/paul_uq_clock.uf2 /media/$USER/RPI-RP2/
```

## Architecture

- `main.c` — FreeRTOS entry point. Creates `led_task` (priority
  `tskIDLE_PRIORITY + 2`), `heartbeat_task` (priority `tskIDLE_PRIORITY + 1`), and
  starts the NTP task via `NTP_Start()`, then starts the scheduler. `led_task` calls
  `Clock_Init()` once and `Clock_Set()` every `LED_DELAY_MS` (1000ms).
- `clock.hpp` / `clock.cpp` — all clock/display logic, exposed as a plain C API
  (`Clock_Init`, `Clock_Set`) so it can be called from `main.c`. Internally uses the
  `Addressable_LED` C++ class. Also owns `RTC_Mutex`, a FreeRTOS mutex shared with
  `ntp.cpp` that guards every RTC hardware access — `rtc_set_datetime()` briefly
  leaves the RTC in a transitional state, and once NTP resync added a second,
  cross-core writer of the RTC, `Clock_Set`'s reads needed to not race that.
  - `Clock_Init` seeds the RTC with a placeholder datetime (so `Clock_Set` always has
    something valid to read) and constructs the `Addressable_LED` driver on `pio0`,
    connector `J1`. The placeholder is overwritten by the NTP task's first sync.
  - `Clock_Set` reads the RTC each call and maps hour/minute/second to LED indices
    (`hour % 12`, `min / 5`, `sec / 5`). Because up to three time components can
    land on the same LED, `Clock_Set` explicitly enumerates the coincidence cases
    (hour==minute==second, hour==minute, hour==second, minute==second, no collision)
    and blends colours accordingly (e.g. yellow = hour+minute overlap, white = all
    three overlap) rather than letting a later `Set_One` call silently overwrite an
    earlier one. AM/PM is a separate, unconditional `Set_One` on LED 19.
- `wifi.hpp` / `wifi.cpp` — `WiFi_Connect()` sets up a dedicated
  `async_context_freertos_t`, brings up cyw43/lwIP with an explicit country code
  (`CYW43_COUNTRY_AUSTRALIA`, not the default worldwide — worldwide's conservative
  channel scanning was pushing real connects past a 30s timeout even though the
  join/DHCP had actually succeeded), and blocks in
  `cyw43_arch_wifi_connect_timeout_ms()` (60s timeout, using `WiFi_SSID`/
  `wiFi_Password` from `configuration.h`) until associated and DHCP-bound. Never
  logs the password.
- `ntp.hpp` / `ntp.cpp` — the NTP task: calls `WiFi_Connect()` once, then loops
  forever fetching time over a raw UDP SNTP request (not lwIP's bundled `apps/sntp`,
  which owns its own internal resync timer — Instructions.txt wants an explicit
  RTOS task instead) and calling `rtc_set_datetime()` (under `RTC_Mutex`) with the
  result, converted to local time via `UTC_Offset` (minutes, from `configuration.h`).
  Resyncs hourly on success, retries after a minute on failure. Also implements
  `dhcp_set_ntp_servers()`, lwIP's DHCP-option-42 hook, so a DHCP-offered NTP server
  is preferred over the `pool.ntp.org` fallback.
- The LED strip itself (PIO/DMA-driven WS2812 protocol, colour constants, `Solid`/
  `Set_One`/`Update`) is **not** in this repo — it's the `Addressable_LED` class from
  the sibling project at `/home/david/Pico_Projects/LED_Driver`
  (https://github.com/David-Haley/LED_Driver), referenced in `CMakeLists.txt` via a
  hardcoded include path and a prebuilt static lib
  (`LED_Driver/build/libaddressable_led.a`). Changes to LED colour/driving behavior
  belong in that repo, not this one — rebuild it (its own `Build.sh`/CMake) before
  rebuilding this project if it changes.
- `lwipopts.h` — lwIP configuration for `NO_SYS=0` (full FreeRTOS integration via
  `pico_cyw43_arch_lwip_sys_freertos`), giving blocking sockets and automatic
  servicing of the cyw43 driver/lwIP stack from their own FreeRTOS task.
- FreeRTOS config (`FreeRTOSConfig.h`): SMP across both cores
  (`configNUMBER_OF_CORES=2`), with `configTASK_DEFAULT_CORE_AFFINITY` pinning every
  task to core 0 by default. Preemptive with time slicing, 1kHz tick, dynamic heap
  only (128KB), stack-overflow checking enabled.
- Instructions.txt (project spec, not checked in — see `.gitignore`) asks for lwIP
  dedicated to core 1. That's **not currently done**: `cyw43_arch_init()` hung
  indefinitely when the cyw43 driver's async_context worker task was pinned to core
  1 (via `async_context_freertos_config_t.task_core_id`) on this SDK/FreeRTOS-Kernel
  combination, and separately, lwIP's own `tcpip_thread` (created internally by
  `tcpip_init()` inside `lwip_freertos_init()`, via a plain `xTaskCreate()` in
  lwIP's `contrib/ports/freertos/sys_arch.c`) isn't reachable to pin at all without
  either patching lwIP or fetching its handle by name
  (`xTaskGetHandle("tcpip_thread")`) after `cyw43_arch_init()` returns and calling
  `vTaskCoreAffinitySet()` on it directly. Everything — lwIP included — currently
  runs with default affinity (core 0). Revisit only if asked; the working,
  user-confirmed state should not be regressed to chase this without discussion.
