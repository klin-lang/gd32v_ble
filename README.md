# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server MVP** + **central scan/connect** +
**GATT client**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [130](https://github.com/klin-lang/klin/blob/main/issues/130-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adp_set_name` / `app_adv_create` / `ble_gatts_svc_add` /
`ble_scan_*` / `ble_conn_*` / `ble_gattc_*`, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS / GATTC / scan events are **SDK contracts**, not hidden
Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.4.0`)

| API | Notes |
|---|---|
| `init` | `ble_init(true)` + `ble_wait_ready` + `ble_gatts_svc_add` + scan/conn + `ble_gattc_svc_reg` |
| `advertise(name)` | GAP name + legacy undirected connectable (`app_adv_create`) |
| `wait_connected(timeout_ms)` | Host stub succeeds after `advertise`. On-device: polls GATTS connect (`-1` = forever) |
| `connected` / `advertising` | `i32` 1/0 |
| `stop_advertise` / `stop` | `app_adv_stop` / teardown + `ble_deinit` |
| `gatt_set` / `gatt_get` / `gatt_len` | Caller copies; max **20** bytes; `gatt_set` does **not** notify |
| `gatt_notify` | `ble_gatts_ntf_ind_send` if connected and CCCD notify enabled; else no-op |
| `gatt_written` | Poll-and-clear (`bool`); no Klin callbacks |
| `gatt_svc_uuid16` / `gatt_chr_uuid16` | Fixed **0xFFF0** / **0xFFF1** |
| `gatt_value_max` | `20` |
| `scan_max` | `16` (fixed table; dedupe by address; no glue malloc) |
| `scan_start(duration_ms)` | Active scan; stops advertising first; `duration_ms` must be `> 0` |
| `scan_stop` / `scan_count` / `scan_rssi` / `scan_addr_type` / `scan_addr` / `scan_name` | Result table accessors |
| `central_connect(index, timeout_ms)` | GAP connect to scan row; then `gattc_discover` |
| `central_wait_connected` / `central_connected` / `central_disconnect` | Poll / tear down central link |
| `gattc_discover(timeout_ms)` | Peer svc **0xFFF0** / chr **0xFFF1** (+ CCCD if present) |
| `gattc_ready` | `bool` after successful discover while still connected |
| `gattc_read` / `gattc_write` / `gattc_subscribe` | Blocking; max 20 bytes |
| `gattc_notified` / `gattc_get` / `gattc_len` | Poll-and-clear notify payload |
| `err_ok` | 0 |

`version()` → `4`.

Host `klin test` uses stubs when `ble_init.h` is not on the include path
(`__has_include`). Do **not** call the factory-style init on a host and expect
RF.

**Central / GATT client** need a GigaDevice BLE image with observer/central +
GATT client (e.g. **msdk_ffd**). Default peripheral-only **msdk** may not export
those APIs.

Bonding / custom UUID tables later (issue 130).

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
    e = ble.scan_start(5000)
    if e != ble.err_ok() {
        return
    }
    if ble.scan_count() < 1 {
        return
    }
    e = ble.central_connect(0, 10000)
    if e != ble.err_ok() {
        return
    }
    e = ble.central_wait_connected(15000)
    if e != ble.err_ok() {
        return
    }
    e = ble.gattc_discover(10000)
    if e != ble.err_ok() {
        return
    }
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.4.0
```

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
