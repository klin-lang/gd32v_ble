# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server** + **central scan/connect** +
**GATT client** + **Just Works bonding** + **custom UUID16/128** + **multi-service**
+ **passkey/PIN**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [140](https://github.com/klin-lang/klin/blob/main/issues/140-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adv_*` / `ble_gatts_*` / `ble_scan_*` / `ble_conn_*` /
`ble_gattc_*` / `app_sec_*`, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS / GATTC / security / scan events are **SDK contracts**,
not hidden Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.8.0`)

| API | Notes |
|---|---|
| `gatt_uuid16` / `gatt_uuid128` | Slot 0 UUIDs before `init` (16 bytes LE for 128-bit) |
| `gatt_add_uuid16` / `gatt_add_uuid128` / `gatt_clear` | Up to **4** services (1 chr each) |
| `gatt_set_at` / `gatt_get_at` / `gatt_len_at` / `gatt_notify_at` / `gatt_written_at` | Per-service index |
| `gattc_select` / `gattc_uuid16` / `gattc_uuid128` | Client discover target |
| `bond_passkey` / `bond_enable` / … | Same as `@v0.7.0` |
| `err_ok` | 0 |

`version()` → `8`. Default remains svc **0xFFF0** / chr **0xFFF1** when unset.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client / bonding** need a GigaDevice BLE image with those
roles (e.g. **msdk_ffd**).

Advertise does **not** pack a service UUID list into AD (GD32 `app_adv_create`
name-only path) — intentional deviation from `esp_ble` AD UUID fields.

## Requirements

- [Klin](https://github.com/klin-lang/klin) compiler
- Official SDK on the include/link path to build a board ELF
- AN152 BLE Development Guide (GigaDevice)

## Usage (128-bit + multi-service)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.gatt_clear()
    if e != ble.err_ok() {
        return
    }
    e = ble.gatt_add_uuid16(0xA001, 0xA002)
    if e != ble.err_ok() {
        return
    }
    let mut svc: [16]u8
    let mut chr: [16]u8
    // fill LE 128-bit UUIDs…
    e = ble.gatt_add_uuid128(cast(*u8, &svc[0]), cast(*u8, &chr[0]))
    if e != ble.err_ok() {
        return
    }
    e = ble.init()
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-multi")
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.8.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
