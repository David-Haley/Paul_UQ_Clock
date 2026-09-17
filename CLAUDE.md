# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a Raspberry Pi Pico W that drives a 20-LED addressable (WS2812-style)
ring as a 12-position analog-style clock with 7 auxiliary LEDs, with overall LED
brightness set automatically from a VEML7700 ambient light sensor. LEDs 0-11 form
the clock face; LEDs 12-18 are auxiliary LEDs whose colour and optional 1Hz flash
are set from JSON delivered over MQTT; LED 19 is an AM/PM indicator. Runs on
single-core FreeRTOS with four tasks: `led_task` (updates the display twice a
second — the clock face from the RP2040's onboard RTC, the aux LEDs from the
latest MQTT message, all of it scaled to the latest ambient-light-derived
brightness — the 2Hz rate exists so aux LEDs marked "flash" can be inverted at
1Hz), `heartbeat_task` (once a second, prints the RTC's date/time over USB stdio
and reads the VEML7700 ambient light sensor over I2C0, appending its raw ALS
count to the printed line — or `No VELM7700` if the sensor isn't detected — and,
on a successful reading, pushing a log2-scaled brightness value to the display),
the NTP task (connects to Wi-Fi, sets the RTC from NTP, and resyncs hourly), and
the aux LED task (shares that Wi-Fi connection, connects to an MQTT broker, and
subscribes to a topic carrying aux LED colour/flash commands — see Architecture
below).

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
`wiFi_Password`, `UTC_Offset` (minutes to add to UTC for local time), and the MQTT
broker's `Broker` (hostname), `User`, `Password`, and `Topic`. The build fails
without it, since `wifi.cpp`/`ntp.cpp`/`aux_led.cpp` include it directly.

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

- `main.c` — FreeRTOS entry point. Calls `WiFi_Init()` (see `wifi.cpp` below)
  before creating any task, then creates `led_task` (priority
  `tskIDLE_PRIORITY + 2`), `heartbeat_task` (priority `tskIDLE_PRIORITY + 1`), and
  starts the NTP task via `NTP_Start()` and the aux LED task via `Aux_LED_Start()`
  (both `tskIDLE_PRIORITY + 1`), then starts the scheduler. `led_task` calls
  `Clock_Init()` once and `Clock_Set()` every `LED_DELAY_MS` (500ms — 2Hz, so
  `Clock_Set`'s internal `Flasher` toggle gives aux LEDs a 1Hz flash).
  `heartbeat_task` calls `VEML7700_Init()` once (see `veml7700.hpp`/`.cpp` below),
  then each second prints the RTC's current date/time (under `RTC_Mutex`, rather
  than a plain counter) and reads the VEML7700 ambient light sensor: on success it
  appends the raw ALS count to the printed line and calls `Clock_Set_Brightness`
  with a `VEML7700_Raw_To_Brightness`-derived value (picked up by `led_task` on
  its next `Clock_Set` call); on failure it appends `No VELM7700` instead and
  leaves the previously-set brightness in effect.
- `cpu_load.hpp` / `cpu_load.cpp` — drives GP7 high while the idle task is running
  and low otherwise, so CPU load can be watched on a scope/logic analyser.
  Implemented via the `traceTASK_SWITCHED_IN` FreeRTOS trace hook (wired up in
  `FreeRTOSConfig.h`, which `#include`s `cpu_load.hpp` for this), which fires on
  every context switch — an idle hook alone can't detect the CPU *leaving* idle,
  only that it's currently in it. Confirmed on hardware: GP7 reads high most of
  the time with occasional short low blips, consistent with this firmware's
  actual (light) workload.
- `clock.hpp` / `clock.cpp` — all clock/display logic, exposed as a plain C API
  (`Clock_Init`, `Clock_Set`, `Clock_Set_Aux_LEDs`, `Clock_Set_Brightness`) so it
  can be called from `main.c` and (for `Clock_Set_Aux_LEDs`) `aux_led.cpp`.
  Internally uses the `Addressable_LED` C++ class. Also owns `RTC_Mutex`, a
  FreeRTOS mutex shared with `ntp.cpp` that guards every RTC hardware access —
  `rtc_set_datetime()` briefly leaves the RTC in a transitional state, and once
  NTP resync added a second task that writes the RTC, `Clock_Set`'s reads needed
  to not race that.
  - `Clock_Init` seeds the RTC with a placeholder datetime (so `Clock_Set` always has
    something valid to read) and constructs the `Addressable_LED` driver on `pio0`,
    connector `J1`. The placeholder is overwritten by the NTP task's first sync.
    Also creates `Aux_LED_Mutex` and zeroes the 7-entry aux LED state bank (indices
    12-18), and creates `Brightness_Mutex` (`Current_Brightness` starts at 255,
    full brightness, since `led_task` — higher priority — runs `Clock_Set` well
    before `heartbeat_task`'s first VEML7700 reading can arrive) — all here,
    before the scheduler can run any lower-priority task — the same "exists
    before any other task can touch it" guarantee `RTC_Mutex` already relies on.
  - `Clock_Set` reads the RTC each call and maps hour/minute/second to LED indices
    (`hour % 12`, `min / 5`, `sec / 5`). Because up to three time components can
    land on the same LED, `Clock_Set` explicitly enumerates the coincidence cases
    (hour==minute==second, hour==minute, hour==second, minute==second, no collision)
    and blends colours accordingly (e.g. yellow = hour+minute overlap, white = all
    three overlap) rather than letting a later `Set_One` call silently overwrite an
    earlier one. AM/PM is a separate, unconditional `Set_One` on LED 19. It also
    reads the current brightness under `Brightness_Mutex` once per call and passes
    it as `Addressable_LED::Set_One`'s `Brightness` argument (2 = dimmest, 255 =
    brightest) on every one of those calls, inverts a `Flasher` bool every call
    (called at 2Hz, see `main.c`), and, under `Aux_LED_Mutex`, renders LEDs 12-18
    from the aux LED state bank — a LED marked "flash" shows black whenever
    `Flasher` is false, giving it a 1Hz visible cycle.
  - `Clock_Set_Aux_LEDs` (called by `aux_led.cpp` once per parsed MQTT message)
    replaces the whole 7-entry aux LED bank in one atomic step under
    `Aux_LED_Mutex` — every message is authoritative for all of LEDs 12-18, so an
    LED not mentioned in a message is turned off rather than left at its previous
    colour.
  - `Clock_Set_Brightness` (called by `main.c`'s `heartbeat_task` once per
    successful VEML7700 reading) atomically replaces `Current_Brightness` under
    `Brightness_Mutex`; a failed sensor reading simply skips the call, leaving
    the previous brightness in effect rather than blanking or maxing the display.
- `veml7700.hpp` / `veml7700.cpp` — VEML7700 ambient light sensor driver, used
  only by `main.c`'s `heartbeat_task`, so (unlike `RTC_Mutex`/`Aux_LED_Mutex`/
  `Brightness_Mutex`) no mutex guards its own I2C0 bus — add one if a second task
  ever needs it. `VEML7700_Init` (called once, from `heartbeat_task`, before its
  loop starts) configures I2C0 at 100kHz on GP0 (SDA) / GP1 (SCL) with pull-ups,
  then attempts to power the sensor on (`ALS_CONF` register: gain x1, 100ms
  integration). `VEML7700_Read_ALS` reads the raw 16-bit `ALS` register; on any
  I2C failure (NACK, timeout, or the sensor never having been detected) it
  returns false and leaves its output untouched, but also retries the power-on
  write on every call — self-healing if the sensor is connected after boot, with
  no throttling/backoff needed since a NACK returns almost instantly (unlike a
  Wi-Fi join). `VEML7700_Raw_To_Brightness` is a separate, pure conversion
  function — log2-scales a raw ALS count against the sensor's full 16-bit range
  into a `Set_One` `Brightness` value (2 = dimmest, 255 = brightest); it does no
  I2C access itself, so the `log2f` cost stays on the once-a-second
  `heartbeat_task` rather than the 2Hz `led_task`.
- `wifi.hpp` / `wifi.cpp` — `WiFi_Connect()` sets up a dedicated
  `async_context_freertos_t`, brings up cyw43/lwIP with an explicit country code
  (`CYW43_COUNTRY_AUSTRALIA`, not the default worldwide — worldwide's conservative
  channel scanning was pushing real connects past a 30s timeout even though the
  join/DHCP had actually succeeded), and blocks in
  `cyw43_arch_wifi_connect_timeout_ms()` (60s timeout, using `WiFi_SSID`/
  `wiFi_Password` from `configuration.h`) until associated and DHCP-bound. Never
  logs the password. Safe to call from more than one task — both `ntp.cpp` and
  `aux_led.cpp` need a connected Wi-Fi link, and `cyw43_arch_init()` must only run
  once — via a mutex + `EventGroupHandle_t` created by `WiFi_Init()` (called once
  from `main()` before any task exists): the first caller does the real connect,
  every other caller just blocks on the event group for the same result.
  Confirmed on hardware: Wi-Fi connect itself intermittently fails outright (the
  full 60s timeout) on this network for reasons unrelated to this code (AP/RF
  flakiness, not a regression) — the driver/async context are only ever set up
  once (`Init_Driver`), but the join itself (the 60s-timeout call) is retried
  with exponential backoff (`Connect_Backoff_Initial_Ms` 2s, doubling up to
  `Connect_Backoff_Max_Ms` 15s) for at least `Connect_Retry_Budget_Ms` (120s)
  before `Do_Connect` gives up — confirmed on hardware recovering a real join
  failure on attempt 2.
- `ntp.hpp` / `ntp.cpp` — the NTP task: calls `WiFi_Connect()` once, then loops
  forever fetching time over a raw UDP SNTP request (not lwIP's bundled `apps/sntp`,
  which owns its own internal resync timer — Instructions.txt wants an explicit
  RTOS task instead) and calling `rtc_set_datetime()` (under `RTC_Mutex`) with the
  result, converted to local time via `UTC_Offset` (minutes, from `configuration.h`).
  Resyncs hourly on success, retries after a minute on failure. Also implements
  `dhcp_set_ntp_servers()`, lwIP's DHCP-option-42 hook, so a DHCP-offered NTP server
  is preferred over the `pool.ntp.org` fallback.
- `aux_led.hpp` / `aux_led.cpp` — the aux LED task: calls `WiFi_Connect()` (shared
  with `ntp.cpp`, see `wifi.cpp` above), then waits a 500ms settle delay
  (`Wifi_Settle_Grace_Ms`) before doing anything else — a task woken by a *shared*
  `WiFi_Connect()` return can otherwise call `mqtt_client_connect()` before the
  network stack's routing state has settled, failing with `ERR_RTE` (confirmed on
  hardware). Resolves `Broker` (from `configuration.h`) via `lwip_getaddrinfo`,
  then connects to it with lwIP's bundled MQTT client (`pico_lwip_mqtt` in
  `CMakeLists.txt`) using `User`/`Password`, subscribing to `Topic`. All raw lwIP
  API calls (`mqtt_client_connect`, `mqtt_subscribe`) are wrapped in
  `cyw43_arch_lwip_begin()`/`cyw43_arch_lwip_end()`, required for any lwIP raw-API
  call made outside lwIP's own `tcpip_thread` while `LWIP_TCPIP_CORE_LOCKING` is
  on (`lwipopts.h`). Retries (reconnect, or DNS re-resolve) on any failure —
  DNS failure, a synchronous `mqtt_client_connect` error, or a non-accepted
  `Connection_Cb` status (refusal, disconnect, timeout) — with exponential
  backoff from `Mqtt_Retry_Initial_Ms` (2s) up to `Mqtt_Retry_Max_Ms` (60s),
  resetting back to the initial delay once a connection is actually accepted;
  confirmed on hardware both recovering a real `ERR_RTE` and backing off
  correctly (2s/4s/8s/16s...) against a host with no listener. Incoming
  publishes are accumulated into a bounded static buffer until
  the final fragment, then parsed as JSON with a vendored copy of **cJSON**
  (`cJSON.c`/`cJSON.h`, upstream `DaveGamble/cJSON` v1.7.19 — chosen over a
  hand-rolled parser) per `aux_led.json schema` (see
  `Aux_LED_Examples/test_1.json`). Invalid elements (out-of-range `led`, unknown
  `colour`) are logged and skipped rather than aborting the whole message; a
  successfully parsed message is applied in one call to `Clock_Set_Aux_LEDs`.
- The LED strip itself (PIO/DMA-driven WS2812 protocol, colour constants, `Solid`/
  `Set_One`/`Update`) is **not** in this repo — it's the `Addressable_LED` class from
  the sibling project at `/home/david/Pico_Projects/LED_Driver`
  (https://github.com/David-Haley/LED_Driver), referenced in `CMakeLists.txt` via a
  hardcoded include path and a prebuilt static lib
  (`LED_Driver/build/libaddressable_led.a`). `Set_One` has two overloads — the
  original `Set_One(Colour, LED_Number)`, and a newer
  `Set_One(Colour, LED_Number, Brightness)` that linearly scales each colour byte
  by `Brightness/255` (2 = dimmest non-zero, 255 = unscaled) — `clock.cpp` now
  uses only the 3-argument form (see above). Changes to LED colour/driving
  behavior belong in that repo, not this one — rebuild it before rebuilding this
  project if it changes. LED_Driver has no `Build.sh`; from `LED_Driver/`, run
  `source Set_Environement.sh && cmake -S . -B build && cmake --build build`.
  Confirmed on hardware: the prebuilt `.a` can silently go stale after a
  LED_Driver source change (it's a plain static lib, not rebuilt automatically by
  this project's own `./Build.sh`) — if a `Set_One` overload fails to link,
  rebuild LED_Driver first.
- `lwipopts.h` — lwIP configuration for `NO_SYS=0` (full FreeRTOS integration via
  `pico_cyw43_arch_lwip_sys_freertos`), giving blocking sockets and automatic
  servicing of the cyw43 driver/lwIP stack from their own FreeRTOS task.
- FreeRTOS config (`FreeRTOSConfig.h`): single core
  (`configNUMBER_OF_CORES=1`). Preemptive with time slicing, 1kHz tick, dynamic
  heap only (128KB), stack-overflow checking enabled.
- The project spec (formerly `Instructions.txt`, removed once implemented) asked
  for dual-core operation with lwIP dedicated to core 1. That's **not implemented**,
  and SMP was tried and abandoned, in two stages:
  - Pinning the cyw43 driver's async_context worker task to core 1 (via
    `async_context_freertos_config_t.task_core_id`) hung `cyw43_arch_init()`
    indefinitely on this SDK/FreeRTOS-Kernel combination. It also wouldn't have
    been sufficient anyway: lwIP's own `tcpip_thread` (created internally by
    `tcpip_init()` inside `lwip_freertos_init()`, via a plain `xTaskCreate()` in
    lwIP's `contrib/ports/freertos/sys_arch.c`) is a separate task, not reachable
    to pin without either patching lwIP or fetching its handle by name
    (`xTaskGetHandle("tcpip_thread")`) after `cyw43_arch_init()` returns and
    calling `vTaskCoreAffinitySet()` on it directly.
  - With that abandoned, `configNUMBER_OF_CORES=2` was kept briefly with
    `configTASK_DEFAULT_CORE_AFFINITY` defaulting every task to core 0 — but
    FreeRTOS creates its per-core idle tasks (`IDLE0`, `IDLE1`, ...) with plain
    `xTaskCreate()` too, so `IDLE1` (meant for core 1) inherited that same
    core-0-only default. Core 0 ended up round-robinning between `IDLE0` and
    `IDLE1` every tick whenever otherwise idle — a real bug (and likely left core
    1 unable to run anything, not even its own idle task), not genuine CPU load;
    it showed up as a misleading ~500Hz/50%-duty GP7 trace (`cpu_load.cpp`'s
    probe correctly treats "not `IDLE0`" as busy, so `IDLE1` masqueraded as work).
  Single-core was reverted to rather than fixing the idle tasks' affinity, since
  nothing currently has a reason to run on core 1. Revisit only if asked.
