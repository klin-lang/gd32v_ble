# GD32VW553 multi GATT + UUID128 (`gd32v_ble` @v0.8.0)

Registers three services (2× UUID16 + 1× UUID128) before `init`. Same Klin
names as [`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.8.0` — engine is
GigaDevice `ble_gatts_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```

Phone: nRF Connect → services **0xA001**, **0xB001**, plus the 128-bit service.
