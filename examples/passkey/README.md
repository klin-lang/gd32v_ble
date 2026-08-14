# GD32VW553 BLE passkey / PIN (`gd32v_ble` @v0.7.0)

Peripheral: `bond_passkey(123456)` → advertise → connect → `bond_start`.
Phone must enter PIN **123456**. Same Klin names as
[`esp_ble`](https://github.com/klin-lang/esp_ble) `@v0.7.0` — engine is
GigaDevice `app_sec_*` (AN152), not NimBLE.

Needs [GD32VW55x_WiFi_BLE_SDK](https://github.com/GigaDeviceSemiconductor/GD32VW55x_WiFi_BLE_SDK)
on the include/link path to produce an ELF. Host `make emit` uses C stubs.

```sh
make emit KLIN=/path/to/klin/bin/klin.dart
```
