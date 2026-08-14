# GATT on GD32VW553

Hardware demo for [`gd32v_ble`](../../README.md) `gatt_set` / `gatt_get` /
`gatt_notify` / `gatt_written`.

Fixed service **0xFFF0**, characteristic **0xFFF1** (read / write / notify),
value max 20 bytes. Same Klin names as [`esp_ble`](https://github.com/klin-lang/esp_ble)
`@v0.2.0` — engine is GigaDevice `ble_gatts_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```

Phone: nRF Connect → enable notify on 0xFFF1 → write bytes → read back.
