# gd32v_ble

Thin **GigaDevice VW55x BLE** bindings for [Klin](https://github.com/klin-lang/klin)
(**peripheral advertise** + **GATT server** + **central scan/connect** +
**GATT client** + **Just Works bonding** + **custom UUID16/128** + **multi-service**
+ **passkey/PIN** + **LE privacy / RPA** + **Mesh Gen OnOff** + **Mesh provisioner** +
**Gen Level / vendor** + **Friend / LPN** + **interactive OOB**).

The radio is in the **silicon**; this package does **not** belong in
[`machine_gd32v`](https://github.com/klin-lang/machine_gd32v) (MMIO Pin…Adc).
Same split as [`esp_ble`](https://github.com/klin-lang/esp_ble) vs `machine_esp`
— see Klin [106](https://github.com/klin-lang/klin/blob/main/issues/106-esp-ble-idf.md)
and [140](https://github.com/klin-lang/klin/blob/main/issues/140-gd32v-ble-sdk.md).

C engine = **[GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)**
(`ble_init` / `app_adv_*` / `ble_gatts_*` / `ble_scan_*` / `ble_conn_*` /
`ble_gattc_*` / `app_sec_*` / `ble_adp_privacy_recfg` / `bt_mesh_*` / CDB, AN152).
Klin is a thin FFI client (`@[link("ble_sdk.c")]` + `@[cimport]`). SDK heap /
BLE task / GAP / GATTS / GATTC / security / mesh / scan events are **SDK contracts**,
not hidden Klin allocation.

**Not** [`esp_ble`](https://github.com/klin-lang/esp_ble) — that is ESP-IDF NimBLE.

Wi‑Fi is [`gd32v_wifi`](https://github.com/klin-lang/gd32v_wifi). Do **not**
put BLE in a board pack.

## Status (`@v0.14.0`)

| API | Notes |
|---|---|
| `mesh_oob_auth_set` / `mesh_oob_auth` | 0=none, 1=output, 2=input, 3=static |
| `mesh_oob_static_set` | Static OOB bytes (1..=16) |
| `mesh_oob_action` / `mesh_oob_input_number` / `mesh_oob_changed` | Interactive poll + inject |
| `mesh_prov_adv_begin` / `mesh_prov_gatt_begin` / `mesh_prov_busy` | Non-blocking provision |
| `mesh_lpn_*` / `mesh_friend_*` | LPN set/poll + Friend terminate; needs LOW_POWER / FRIEND |
| `mesh_provisioner_enable` | CDB + self-provision (addr 1); exclusive with `mesh_enable` |
| `mesh_unprov_*` / `mesh_prov_adv` / `mesh_prov_gatt` | Unprov UUID table + blocking PB-ADV/GATT |
| `mesh_level*` / `mesh_vnd*` | Gen Level + vendor button on `mesh_enable` node |
| Prior APIs | privacy / GATT / bond / UUID / scan / … |
| `err_ok` | 0 |

`version()` → `14`.

Host `klin test` uses stubs when `ble_init.h` is not on the include path.

**Mesh node** needs mesh headers + **BLE_MAX** (like `light_demo`).
**Mesh provisioner** also needs `CONFIG_BT_MESH_PROVISIONER` + `CONFIG_BT_MESH_CDB`
(like SDK `provisioner` example). Without those, `mesh_provisioner_enable` → `-1`.

Default auth is **none**. Interactive OOB uses `mesh_oob_auth_set` +
`mesh_prov_*_begin` (blocking `mesh_prov_adv`/`gatt` stay for none/static).

## Usage (interactive OOB)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    e = ble.mesh_provisioner_enable()
    e = ble.mesh_oob_auth_set(1) // output: device displays → we enter (action 2)
    e = ble.mesh_prov_adv_begin(0, 2)
    while ble.mesh_prov_busy() {
        if ble.mesh_oob_action() == 2 {
            e = ble.mesh_oob_input_number(123456) // number shown on the device
        }
    }
}
```

```sh
klin get github/klin-lang/gd32v_ble@v0.14.0
```

## Usage (Mesh provisioner)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    e = ble.mesh_provisioner_enable()
    // wait for unprov beacons…
    if ble.mesh_unprov_count() > 0 {
        e = ble.mesh_prov_adv(0, 2, 30000)
    }
}
```

## Usage (Friend / LPN)

```klin
import "github/klin-lang/gd32v_ble" ble

fn main() {
    let mut e = ble.init()
    e = ble.mesh_enable()
    if ble.mesh_lpn_supported() {
        e = ble.mesh_lpn_set(1)
    }
}
```

Without `CONFIG_BT_MESH_LOW_POWER` / `CONFIG_BT_MESH_FRIEND`, the matching
calls return `-1` / `supported` is false.

## Contract

- `mesh_enable` and `mesh_provisioner_enable` are mutually exclusive.
- Unprov table max **8** (no glue malloc).
- Default net/dev keys match SDK provisioner demo (fixed 16-byte constants).
- Interactive OOB: use `mesh_prov_*_begin` + poll; no hidden callbacks into Klin.
- No Klin GC / hidden heap. Errors are `i32` (0 = OK, `-1` = fail).

## Tests

```sh
dart run /path/to/klin/bin/klin.dart test gd32v_ble/
```

## License

MIT
