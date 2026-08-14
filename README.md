# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server** + **central scan/connect** +
**GATT client** + **Just Works bonding** + **custom UUID16/128** + **multi-service**
+ **passkey/PIN** + **LE privacy / RPA**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [140](https://github.com/klin-lang/klin/blob/main/issues/140-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adv_*` / `ble_gatts_*` / `ble_scan_*` / `ble_conn_*` /
`ble_gattc_*` / `app_sec_*` / `ble_adp_privacy_recfg`, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS / GATTC / security / scan events are **SDK contracts**,
not hidden Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.9.0`)

| API | Notes |
|---|---|
| `privacy_enable` / `privacy_disable` | Controller RPA after `init`, before advertise/scan |
| `privacy_enabled` / `own_addr_type` / `own_addr` | Query own address |
| `gatt_uuid16` / `gatt_uuid128` / `gatt_add_*` | Prior multi-service / UUID128 |
| `bond_passkey` / `bond_enable` / … | Prior bonding / passkey |
| `err_ok` | 0 |

`version()` → `9`. Default remains svc **0xFFF0** / chr **0xFFF1** when unset.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client / bonding** need a GigaDevice BLE image with those
roles (e.g. **msdk_ffd**).

Advertise does **not** pack a service UUID list into AD (GD32 `app_adv_create`
name-only path) — intentional deviation from `esp_ble` AD UUID fields.

Privacy uses **controller** RPA (`ble_adp_privacy_recfg` +
`BLE_GAP_LOCAL_ADDR_RESOLVABLE` for adv/scan/connect), not NimBLE host
`ble_hs_pvcy_rpa_config`. `own_addr` returns public / identity via
`ble_adp_public_addr_get` / `ble_adp_identity_addr_get` (no
`BLE_ADP_EVT_LOC_ADDR_INFO` cache).

## Requirements

- [Klin](https://github.com/klin-lang/klin) compiler
- Official SDK on the include/link path to build a board ELF
- AN152 BLE Development Guide (GigaDevice)

## Usage (privacy / RPA)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    if e != ble.err_ok() {
        return
    }
    e = ble.privacy_enable()
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-rpa")
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.9.0
```

## Contract

- Call `privacy_enable` **before** advertise / scan / connect (not while radio is active).
- No Klin GC / hidden heap. Errors are `i32` (0 = OK, `-1` = fail).

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
