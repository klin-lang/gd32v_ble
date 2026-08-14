# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server MVP**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [130](https://github.com/klin-lang/klin/blob/main/issues/130-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adp_set_name` / `app_adv_create` / `ble_gatts_svc_add`, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS events are **SDK contracts**, not hidden Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.2.0`)

| API | Notes |
|---|---|
| `init` | `ble_init(true)` + `ble_wait_ready` + `ble_gatts_svc_add` (once) |
| `advertise(name)` | GAP name + legacy undirected connectable (`app_adv_create`) |
| `wait_connected(timeout_ms)` | Host stub succeeds after `advertise`. On-device: polls a flag set by GATTS `BLE_SRV_EVT_CONN_STATE_CHANGE_IND` (`-1` = forever) |
| `connected` / `advertising` | `i32` 1/0 |
| `stop_advertise` / `stop` | `app_adv_stop` / `ble_gatts_svc_rmv` + `ble_deinit` |
| `gatt_set` / `gatt_get` / `gatt_len` | Caller copies; max **20** bytes; `gatt_set` does **not** notify |
| `gatt_notify` | `ble_gatts_ntf_ind_send` if connected and CCCD notify enabled; else no-op |
| `gatt_written` | Poll-and-clear (`bool`); no Klin callbacks |
| `gatt_svc_uuid16` / `gatt_chr_uuid16` | Fixed **0xFFF0** / **0xFFF1** |
| `gatt_value_max` | `20` |
| `err_ok` | 0 |

`version()` → `2`.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

Central / bonding later.

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
    let mut seed: [1]u8
    seed[0] = 42
    e = ble.gatt_set(cast(*u8, &seed[0]), 1)
    if e != ble.err_ok() {
        return
    }
    e = ble.advertise("klin-gatt")
    if e != ble.err_ok() {
        return
    }
    e = ble.wait_connected(-1)
    if e != ble.err_ok() {
        return
    }
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.2.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
