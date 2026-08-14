# Central scan on GD32VW553

Hardware demo for [`gd32v_ble`](../../README.md) `scan_*` / `central_*`
(`@v0.3.0`).

Active scan (max 16 results, dedupe by address), then GAP connect to index 0.
Same Klin names as [`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.3.0` —
engine is GigaDevice `ble_scan_*` / `ble_conn_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
with **observer/central** roles (e.g. msdk_ffd) on the include/link path.
Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```
