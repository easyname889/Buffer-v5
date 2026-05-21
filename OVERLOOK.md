# LLLP Buffer — Architecture Overlook

Firmware for the **LLLP filament buffer**. MCU: **STM32F072C8T6** (Cortex-M0, 64 KB flash, 16 KB RAM). Built with PlatformIO + Arduino core. Flashed over **USB DFU**.

## System topology

```
 [spool] [spool] [spool] [spool]      <- up to 4 spools
    |       |       |       |
  BACK    BACK    BACK    BACK         <- 2-4 BACK LLLPs, token-chained
    \       |       |       /
     \------+--4-in-1--+----/          <- Y-junction merges all back outputs
                |
          [Y-junction sensor]          <- shared sensor, read by ALL units (PB14)
                |
             FRONT LLLP                <- 1 standalone, always-on buffer
                |
            [printhead]                <- large printer, long feed path
```

- **BACK LLLPs (2-4):** each on its own spool, chained by a **token**. Only the token-holder feeds; on spool runout it passes the token to the next buffer. They take turns delivering filament into the 4-in-1.
- **FRONT LLLP (1):** sits after the 4-in-1, **standalone**, always on. Pushes whatever the back buffers deliver toward the printhead. No token, no chain.

## How one buffer works

A spring-loaded **dancer arm** rides the filament loop between input and output. The arm passes three hall sensors; the firmware maps arm position to a **zone** and drives the feed motor to keep the arm mid-travel.

| Hall | Pin | Arm position | Zone | Motor response |
|---|---|---|---|---|
| HALL1 | PB2 | TOP (over-fed) | `AutoZoneSafety` | REVERSE |
| HALL2 | PB3 | middle (full) | `AutoZoneHigh` | trim down → Stop |
| HALL3 | PB4 | BOTTOM (empty) | `AutoZoneLow` | trim up → full forward |
| none | — | between | `AutoZoneWindow` | maintenance (trim decays to base) |

`update_auto_feed()` runs an adaptive speed-trim controller off the zone. Motor = **TMC2209** stepper (EN PA6, DIR PA7, STEP PC13, soft-UART PB1 @ 9600).

**Filament sensor:** `ENDSTOP_3` (PB7) — 0 = filament present, 1 = missing. `filament_missing()` true → motor stops, token passed away.

**Buttons:** KEY1 (PB13, "back"), KEY2 (PB12, "forward"). Pressed = 0.
- Single tap — KEY1 = pass token, KEY2 = claim token
- Double tap — either = toggle `is_error`
- Long hold (>0.5 s) — KEY2 = jog forward, KEY1 = jog reverse (sets `is_error` on release)

**Feed timeout:** continuous forward feed > `timeout` (default 60 000 ms, EEPROM) → `is_error` → stop. Crude runout/jam backstop.

## Token chain (back buffers)

- Only the **token-holder** feeds. `has_token` persists in EEPROM addr 20.
- Pass: drives `EXTENSION_PIN2` (PA3, chain-out) LOW for **250 ms** → received on the next buffer's `FRONT_SIGNAL_PIN` (PB5, chain-in), edge-triggered.
- On spool runout the token-holder auto-passes; KEY1 single tap passes manually.

## Y-junction sensor (`Y_JUNCTION_SENSOR`)

Shared filament sensor on the merged line just after the 4-in-1. One sensor, 3-wire bus (V/GND/signal), wired to **`CHAIN_Y_SENSOR_PIN` = PB14** on every back buffer. PB14 is a 5 V-tolerant FT pin — the 3-5 V push-pull sensor wires straight in (plain `INPUT`, no divider). `HIGH` = junction clear.

**Back-buffer use — handoff gate:** a buffer that takes the token waits at STOP until the Y-sensor reads clear (previous buffer's filament tail has left the junction), then **latches on** and feeds. Prevents two filaments colliding at the joint; replaces the old transit-time timer guess.

## Front LLLP specifics (`FRONT_BUFFER_STANDALONE`)

- No token gate, no chain. `filament_missing()` forced `false` — the feed loop always runs.
- Its own `ENDSTOP_3` fil sensor is **hardware-shorted** (and the firmware ignores it anyway).
- **Runout backstop:** the front buffer reads the shared Y-junction sensor (PB14) and stops feeding when the junction is empty **AND** its own dancer arm has bottomed (`zone Low`) — input dry and buffered loop exhausted. Auto-resumes when filament returns. Brief Y-empty during back-buffer hand-offs is ridden out by the loop. Replaces relying on the crude 60 s timeout.

## Firmware variants / build

`platformio.ini` — `[env]` base + four targets:

| Env | Target | Flags |
|---|---|---|
| `fly_buffer_f072c8` | Back / chained | `Y_JUNCTION_SENSOR` |
| `fly_buffer_front` | Front / standalone | `FRONT_BUFFER_STANDALONE` + `Y_JUNCTION_SENSOR` |
| `fly_buffer_debug` | Back, diagnostics | `+ SENSOR_DEBUG` |
| `fly_buffer_front_debug` | Front, diagnostics | `+ SENSOR_DEBUG` |

`SENSOR_DEBUG` streams a throttled live sensor line over USB serial (~200 ms) — off in production.

**Build/flash:** see [playbooks/flashing.md](playbooks/flashing.md). In short: `pio run -e <env>`, then hold BOOT / tap RESET / `dfu-util -a 0 -s 0x08000000:leave -D .pio/build/<env>/firmware.bin`.

## Repo

- `origin` = `easyname889/Buffer-v5` (this v5 lineage). `upstream` = `Fly3DTeam/Buffer` (read-only).
- v3 rewrite lives separately at `easyname889/Mellow-LLLPlus-infinite-filament-buffer`.

## Open items

- `DIR_PIN` is double-`#define`d in `buffer.h` (PA7 vs EXTENSION_PIN3/PB11) — pre-existing warning.
- Y-junction sensor is bench-verified; not yet validated on the full multi-buffer rig.
