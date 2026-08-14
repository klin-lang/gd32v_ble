/* Thin BLE advertise + GATT helpers for Klin over the GigaDevice VW55x BLE SDK.
 * Heap / OSAL BLE task / GAP / GATTS events are SDK contracts (not Klin magic).
 * `@v0.2.0` = peripheral advertise + GATT server MVP (AN152 `ble_gatts_*`).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define KLIN_GD32V_BLE_GATT_VALUE_MAX 20
#define KLIN_GD32V_BLE_GATT_SVC_UUID16 0xFFF0
#define KLIN_GD32V_BLE_GATT_CHR_UUID16 0xFFF1

/** `ble_init(true)` + `ble_wait_ready` + `ble_gatts_svc_add` (0xFFF0/0xFFF1). Call once. */
int klin_gd32v_ble_init(void);

/** Set GAP name + start legacy undirected connectable advertise (`app_adv_create`). */
int klin_gd32v_ble_advertise(const char *name);

int klin_gd32v_ble_stop_advertise(void);
int klin_gd32v_ble_connected(void);
int klin_gd32v_ble_advertising(void);
int klin_gd32v_ble_wait_connected(int timeout_ms);
int klin_gd32v_ble_stop(void);

int klin_gd32v_ble_gatt_set(const unsigned char *data, int len);
int klin_gd32v_ble_gatt_get(unsigned char *out, int max_len);
int klin_gd32v_ble_gatt_len(void);
int klin_gd32v_ble_gatt_notify(void);
int klin_gd32v_ble_gatt_written(void);

#ifdef __cplusplus
}
#endif
