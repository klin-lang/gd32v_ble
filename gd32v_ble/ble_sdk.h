/* Thin BLE advertise helpers for Klin over the GigaDevice VW55x BLE SDK.
 * Heap / OSAL BLE task / GAP events are SDK contracts (not Klin magic).
 * First tag = peripheral advertise only (AN152).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** `ble_init(true)` + `ble_wait_ready`. Call once. */
int klin_gd32v_ble_init(void);

/** Set GAP name + start legacy undirected connectable advertise (`app_adv_create`). */
int klin_gd32v_ble_advertise(const char *name);

int klin_gd32v_ble_stop_advertise(void);
int klin_gd32v_ble_connected(void);
int klin_gd32v_ble_advertising(void);
int klin_gd32v_ble_wait_connected(int timeout_ms);
int klin_gd32v_ble_stop(void);

#ifdef __cplusplus
}
#endif
