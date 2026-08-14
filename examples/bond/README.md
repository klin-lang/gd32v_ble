# Bonding on GD32VW553

Hardware demo for [`gd32v_ble`](../../README.md) `bond_*` (`@v0.5.0`).

Just Works pairing after peripheral connect. Same Klin names as
[`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.5.0` — engine is
GigaDevice `app_sec_*` / peer flash storage (AN152), not NimBLE NVS.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```
