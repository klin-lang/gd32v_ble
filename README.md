# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server MVP** + **central scan/connect** +
**GATT client** + **Just Works bonding**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [130](https://github.com/klin-lang/klin/blob/main/issues/130-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adv_*` / `ble_gatts_*` / `ble_scan_*` / `ble_conn_*` /
`ble_gattc_*` / `app_sec_*`, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS / GATTC / security / scan events are **SDK contracts**,
not hidden Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.5.0`)

| API | Notes |
|---|---|
| `init` | `ble_init(true)` + GATT svc + scan/conn + gattc reg (`app_sec_mgr_init` inside SDK `ble_init`) |
| `advertise` … `gattc_*` | Same as `@v0.4.0` |
| `bond_enable` | Just Works SM: no IO / no MITM / SC + bond (`app_sec_set_authen`) |
| `bond_start` | `app_sec_send_bond_req` on active link (central preferred, else peripheral) |
| `bonded` / `wait_bonded` | Poll / block until pair success |
| `bond_count` / `bond_clear` | SDK peer flash storage (`ble_peer_all_addr_get` / `ble_peer_data_delete`) |
| `err_ok` | 0 |

`version()` → `5`.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client / bonding** need a GigaDevice BLE image with those
roles (e.g. **msdk_ffd**).

Passkey / custom UUID tables later (issue 130).

## Requirements

- [Klin](https://github.com/klin-lang/klin) compiler
- Official SDK on the include/link path to build a board ELF
- AN152 BLE Development Guide (GigaDevice)

## Usage

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    if e != ble.err_ok() {
        return
    }
    e = ble.bond_enable()
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-bond")
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
    if e != ble.err_ok() {
        return
    }
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.5.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
