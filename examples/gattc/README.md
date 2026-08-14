# GATT client on GD32VW553

Hardware demo for [`gd32v_ble`](../../README.md) `gattc_*` (`@v0.4.0`).

After central scan/connect: discover peer service **0xFFF0** / char **0xFFF1**,
subscribe (CCCD), read, write. Same Klin names as
[`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.4.0` — engine is
GigaDevice `ble_gattc_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
with **central + GATT client** (e.g. msdk_ffd) on the include/link path.
Pair with `examples/gatt/` on a second board/phone. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```
