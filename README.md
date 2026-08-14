# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server MVP** + **central scan/connect** +
**GATT client** + **Just Works bonding** + **custom UUID16** + **passkey/PIN**).

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

## Status (`@v0.7.0`)

| API | Notes |
|---|---|
| `bond_passkey(pin)` | Fixed **6-digit PIN** (`0..=999999`) — MITM + keyboard/display (`app_sec_set_authen` + `app_sec_pin_code_set`) |
| `passkey` / `passkey_action` / `passkey_inject` | Query / last action / manual inject |
| `bond_enable` | Just Works (clears passkey mode) |
| `init` … `gatt_uuid16` … `bond_*` | Same as `@v0.6.0` + passkey |
| `err_ok` | 0 |

`version()` → `7`.

`bond_enable` = Just Works; `bond_passkey` = MITM + PIN (replaces JW config).

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client / bonding** need a GigaDevice BLE image with those
roles (e.g. **msdk_ffd**).

Privacy / UUID128 later (issue 140).

## Requirements

- [Klin](https://github.com/klin-lang/klin) compiler
- Official SDK on the include/link path to build a board ELF
- AN152 BLE Development Guide (GigaDevice)

## Usage (passkey / PIN)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    if e != ble.err_ok() {
        return
    }
    e = ble.bond_passkey(123456)
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-pin")
    if e != ble.err_ok() {
        return
    }
    e = ble.wait_connected(-1)
    if e != ble.err_ok() {
        return
    }
    e = ble.bond_start()
    if e != ble.err_ok() {
        return
    }
    e = ble.wait_bonded(60000)
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.7.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
