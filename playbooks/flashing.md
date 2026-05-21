# Playbook — Flashing the LLLP Buffers

One source tree, two firmware roles. Each physical unit gets the env that
matches its job. See [OVERLOOK.md](../OVERLOOK.md) for how the system works.

## Which firmware goes on which unit

| Unit | Env | Behavior |
|---|---|---|
| **BACK** buffer (×2-4) | `fly_buffer_f072c8` | Token chain; Y-junction handoff gate (waits for a clear junction before feeding) |
| **FRONT** buffer (×1) | `fly_buffer_front` | Standalone always-feed; Y-sensor runout backstop (stops when junction empty + own arm bottomed) |

Both read the shared Y-junction sensor on **PB14**. Debug variants
(`fly_buffer_debug`, `fly_buffer_front_debug`) add `SENSOR_DEBUG` — a live
sensor line over USB serial for bench/field diagnostics. Flash a debug
build to investigate; flash the plain build for production.

## Tools (one-time)

- PlatformIO CLI: `~/.platformio/penv/bin/pio` (or the PlatformIO terminal in VS Code).
- `dfu-util`: `brew install dfu-util`.

## Build

```sh
pio run                          # build all 4 envs
pio run -e fly_buffer_f072c8     # back buffer only
pio run -e fly_buffer_front      # front buffer only
```

Output binary: `.pio/build/<env>/firmware.bin`

## Flash (USB DFU)

1. Connect the unit by USB — **use a data cable** (charge-only cables power
   the board but enumerate nothing).
2. Enter DFU mode: **hold BOOT, tap RESET, release BOOT.**
3. Confirm: `dfu-util -l` shows 2 `Found DFU` interfaces.
4. Flash the matching binary:

```sh
# BACK buffer
dfu-util -a 0 -s 0x08000000:leave -D .pio/build/fly_buffer_f072c8/firmware.bin

# FRONT buffer
dfu-util -a 0 -s 0x08000000:leave -D .pio/build/fly_buffer_front/firmware.bin
```

`:leave` reboots the unit into the new firmware. Build + flash in one step
(board already in DFU mode): `pio run -e <env> -t upload`.

## Debug builds

```sh
dfu-util -a 0 -s 0x08000000:leave -D .pio/build/fly_buffer_debug/firmware.bin        # back
dfu-util -a 0 -s 0x08000000:leave -D .pio/build/fly_buffer_front_debug/firmware.bin  # front
```

The unit then streams a `DBG …` line (~200 ms) over USB serial at 115200
baud — halls, filament switch, token, `yClear`, keys, motor state.

## Gotchas

- dfu-util may print *"Device's firmware is corrupt"* at the start — harmless;
  it is a leftover DFU error state, dfu-util clears it and flashes anyway.
- **No serial port AND no DFU device** = no USB data link. Swap to a known
  data cable; try another port.
- After flashing, the unit re-enumerates as `/dev/cu.usbmodem*` (running app).
- macOS serial scripting needs `pyserial` (`pip install --break-system-packages pyserial`).
