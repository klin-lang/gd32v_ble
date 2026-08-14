# GD32VW553 BLE privacy / RPA (`gd32v_ble` @v0.9.0)

Enables controller RPA before advertising (`privacy_enable` → `advertise`).
Same Klin names as [`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.9.0`
— engine is GigaDevice `ble_adp_privacy_recfg` + `BLE_GAP_LOCAL_ADDR_RESOLVABLE`
(AN152), not NimBLE host RPA.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```
