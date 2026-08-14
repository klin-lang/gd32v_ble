# Advertise on GD32VW553

Hardware demo for [`gd32v_ble`](../../README.md) `init` / `advertise`.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```

Do not use [`esp_ble`](https://github.com/klin-lang/esp_ble) on this SoC
(ESP-IDF NimBLE, wrong engine).
