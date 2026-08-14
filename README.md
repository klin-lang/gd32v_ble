# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server MVP** + **central scan/connect** +
**GATT client** + **Just Works bonding** + **custom UUID16**).

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

## Status (`@v0.6.0`)

| API | Notes |
|---|---|
| `gatt_uuid16(svc, chr)` | Own **16-bit** UUIDs — call **before** `init` (default remains `0xFFF0` / `0xFFF1`) |
| `gatt_svc_uuid16` / `gatt_chr_uuid16` | Active values (not hardcoded Klin constants) |
| `init` … `bond_*` | Same as `@v0.5.0`; GATT DB + `gattc_discover` use the active UUID16 |
| `err_ok` | 0 |

`version()` → `6`.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client / bonding** need a GigaDevice BLE image with those
roles (e.g. **msdk_ffd**).

`gatt_uuid16` before `init` only; after `init` → `-1`. Passkey / UUID128 later
(issue 140).

## Requirements

- [Klin](https://github.com/klin-lang/klin) compiler
- Official SDK on the include/link path to build a board ELF
- AN152 BLE Development Guide (GigaDevice)

## Usage (custom UUID16)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.gatt_uuid16(0xA001, 0xA002)
    if e != ble.err_ok() {
        return
    }
    e = ble.init()
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-uuid")
    if e != ble.err_ok() {
        return
    }
    e = ble.wait_connected(-1)
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.6.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
