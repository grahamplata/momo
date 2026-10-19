# momo

CAN bus explorations. A **bridge** listens to the car's CAN bus and broadcasts every frame over ESP-NOW; **gadgets** are ESP-NOW receivers that filter for the CAN IDs they care about and do something useful.

Inspired by and built on the same hardware as [MrDIY's CANaBus](https://gitlab.com/MrDIYca/canabus).

## Hardware

- Bridge: DOIT ESP32 DevKit V1 + [MrDIY ESP32 CAN shield v1.3](https://store.mrdiy.ca/p/esp32-can-bus-shield/), plugged into the OBD2 port.
- Gadgets: any ESP32.

## Layout

```
lib/protocol/          CanFrame: the ESP-NOW wire format shared by bridge and gadgets
src/bridge/            CAN (TWAI, listen-only, 500 kbps) -> ESP-NOW broadcast
src/gadget_sample/     minimal gadget: prints received frames; copy to start a new one
platformio.ini         one env per firmware
```

## Build & flash

Requires [PlatformIO](https://platformio.org/install/cli).

```sh
pio run                                  # build everything
pio run -e bridge -t upload -t monitor   # flash + watch the bridge
pio run -e gadget_sample -t upload -t monitor
```

The bridge prints its `mesh_id` at boot. Put it in a gadget's `MESH_ID` build flag so the gadget ignores other bridges (`0` accepts any).

## Behaviour notes

- The bridge is **listen-only** and never writes to the bus.
- After 500 ms with no CAN traffic (car off) the bridge deep-sleeps and wakes every 6 s to check again, so it doesn't drain the 12 V battery.
- `CanFrame` is packed (18 bytes) and **not** wire-compatible with CANaBus gadgets. Changing it means reflashing every device.
- Bridge and gadgets share ESP-NOW channel 1 (`ESPNOW_CHANNEL`).

## Adding a gadget

1. Copy `src/gadget_sample/` to `src/gadget_<name>/`.
2. Add an `[env:gadget_<name>]` with `build_src_filter = +<gadget_<name>/>`.
3. Replace `on_frame()` with your logic, keyed on `f.can_id`.
