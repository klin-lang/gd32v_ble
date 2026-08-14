# GD32VW553 custom GATT UUID16 (`gd32v_ble` @v0.6.0)

Sets svc **0xA001** / chr **0xA002** via `gatt_uuid16` **before** `init`,
then advertises `klin-uuid`. Same Klin names as
[`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.6.0` — engine is
GigaDevice `ble_gatts_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```

Phone: nRF Connect → look for service **0xA001** / char **0xA002**.
