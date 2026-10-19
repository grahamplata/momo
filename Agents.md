# Agents.md

Rules for anyone (human or agent) changing this repo. See [README.md](README.md) for layout and build commands.

## Project

A **bridge** (ESP32 DevKit V1 + MrDIY CAN shield v1.3) listens to a car's CAN bus and broadcasts every frame over ESP-NOW. **Gadgets** are ESP32s that receive those frames and act on the CAN IDs they care about. Inspired by [CANaBus](https://gitlab.com/MrDIYca/canabus), but not wire-compatible with it.

Expected load: one car, 500 kbps bus, roughly 1,000–3,000 frames/s while driving.

## Rules

1. **`lib/protocol/protocol.h` is the contract.**
   - Any change to `CanFrame` bumps `FRAME_VERSION` and updates the `static_assert` size. Every bridge and gadget is then reflashed together.
   - Gadgets read packets only through `decode_frame()`. Never `memcpy` raw ESP-NOW bytes into a struct yourself.
   - Every change to `protocol.h` comes with a test in `test/test_protocol/`.

2. **The bridge is listen-only.** It uses `TWAI_MODE_LISTEN_ONLY` and never ACKs or writes to the car's bus. Anything that transmits on the bus is a separate firmware that has to be enabled on purpose, never a flag on the bridge.

3. **ESP-NOW callbacks only validate and queue.** The receive callback runs on the Wi-Fi task. It calls `decode_frame()`, pushes to a FreeRTOS queue with zero timeout, and returns. All real work (serial, relays, displays, delays) happens in `loop()`. Follow `src/gadget_sample/`.

4. **Broadcast data is for display and logging only.** Anyone nearby with an ESP32 can forge a broadcast frame, and `mesh_id` is not a secret. A gadget that moves something physical (garage door, relay, lock) must use encrypted, paired, one-to-one ESP-NOW (PMK/LMK), not broadcast plus a `mesh_id` check.

5. **Count drops; never lose data silently.** Any place that can drop frames (ESP-NOW send queue, CAN receive queue, a gadget's queue) increments a counter that gets printed. If the counts are high on a real car, pack several CAN frames per ESP-NOW packet (250-byte limit), which is a protocol change under rule 1.

6. **Hardware lives in `platformio.ini`.** One env per firmware (`bridge`, `gadget_<name>`), sharing the `[esp32]` section. Board pins and per-device settings (`CAN_RX_PIN`, `MESH_ID`) are build flags, not literals in code. The platform is pinned (`espressif32 @ 6.13.0` = Arduino core 2.x); upgrading to 7.x+ (Arduino core 3.x) changes the ESP-NOW callback signature and is its own PR.

7. **Mark known limits.** A deliberate shortcut gets a comment like `// shortcut: <the limit>, <when to upgrade>`.

8. **Keep it small.** No abstraction with one implementation, no config nobody sets, no new library for a few lines. Shared code goes in `lib/`, but only once two firmwares actually need it.

9. **CI must pass.** Every PR runs `pio run` (all firmware) and `pio test -e native` (host tests). Pure logic, like parsing or decoding a CAN signal, goes in `lib/` so it can be tested natively.

10. **Untested on hardware means say so.** A PR states whether it ran on the car or only compiled.

## Hardware notes (shield v1.3)

| Function | GPIO |
|---|---|
| CAN RX / TX | 4 / 5 |
| Transceiver loopback (active HIGH, cuts the bus) | 33, held LOW by the bridge |
| LEDs | 26, 27 |
| Button (pulled up) | 25 |
| Battery voltage divider (needs `ADC D32` jumper) | 32 |
| Qwiic / I2C | pull-ups need the solder jumper |

- Leave the `CAN 120` termination jumper **open** on a car; the car's bus is already terminated.
- Input power 7–15.5 V when the voltage divider jumper is on.
