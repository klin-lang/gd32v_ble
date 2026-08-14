/* Thin BLE advertise + GATT + central scan + GATT client + bonding helpers for
 * Klin over the GigaDevice VW55x BLE SDK. Heap / OSAL BLE task / GAP / GATTS /
 * GATTC / security / scan events are SDK contracts (not Klin magic).
 *
 * `@v0.6.0` = … + custom 16-bit GATT UUIDs (`gatt_uuid16` before `init`).
 * Scan results use a fixed table (max 16) — no Klin / glue malloc.
 * Central / GATT client / bonding need an SDK build with those roles (e.g. msdk_ffd).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define KLIN_GD32V_BLE_GATT_VALUE_MAX 20
/** Default primary service UUID16 (override with `klin_gd32v_ble_gatt_uuid16`). */
#define KLIN_GD32V_BLE_GATT_SVC_UUID16 0xFFF0
/** Default characteristic UUID16. */
#define KLIN_GD32V_BLE_GATT_CHR_UUID16 0xFFF1

/** Max scan results kept (deduped by address). Caller-visible contract. */
#define KLIN_GD32V_BLE_SCAN_MAX 16

/** Max GAP name bytes stored per scan result (NUL-terminated in C). */
#define KLIN_GD32V_BLE_SCAN_NAME_MAX 28

/**
 * Set 16-bit svc/chr UUIDs for the peripheral GATT DB and `gattc_discover`.
 * Must be called before `init`. Default remains 0xFFF0 / 0xFFF1.
 * Returns 0 on OK, -1 on bad range or already inited.
 */
int klin_gd32v_ble_gatt_uuid16(int svc_uuid16, int chr_uuid16);
/** Active service UUID16 (default or last `gatt_uuid16`). */
int klin_gd32v_ble_gatt_svc_uuid16(void);
/** Active characteristic UUID16. */
int klin_gd32v_ble_gatt_chr_uuid16(void);

/** `ble_init(true)` + `ble_wait_ready` + GATT svc add + scan/conn/gattc. */
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

/**
 * Stop advertising (if any), clear results, run active scan for `duration_ms`
 * (must be > 0). Blocks until duration elapses (board). Returns 0 on OK.
 */
int klin_gd32v_ble_scan_start(int duration_ms);
int klin_gd32v_ble_scan_stop(void);
int klin_gd32v_ble_scan_count(void);
int klin_gd32v_ble_scan_rssi(int index);
int klin_gd32v_ble_scan_addr_type(int index);
int klin_gd32v_ble_scan_addr(int index, unsigned char *out6);
int klin_gd32v_ble_scan_name(int index, unsigned char *out, int max_len);

/**
 * Connect as central to scan result `index`. GAP only — then `gattc_discover`.
 * `timeout_ms` is reserved for wait helpers (`-1` = forever hint).
 */
int klin_gd32v_ble_central_connect(int index, int timeout_ms);
int klin_gd32v_ble_central_connected(void);
int klin_gd32v_ble_central_wait_connected(int timeout_ms);
int klin_gd32v_ble_central_disconnect(void);

/**
 * Discover peer svc/chr from active UUID16 (+ CCCD if present). Blocks.
 * Requires an active central connection. 0 = ready for read/write.
 */
int klin_gd32v_ble_gattc_discover(int timeout_ms);
int klin_gd32v_ble_gattc_ready(void);
int klin_gd32v_ble_gattc_read(int timeout_ms);
int klin_gd32v_ble_gattc_write(const unsigned char *data, int len, int timeout_ms);
int klin_gd32v_ble_gattc_subscribe(int timeout_ms);
int klin_gd32v_ble_gattc_notified(void);
int klin_gd32v_ble_gattc_get(unsigned char *out, int max_len);
int klin_gd32v_ble_gattc_len(void);

/**
 * Enable Just Works bonding (SM config via `app_sec_set_authen`). Call after
 * `init`, before `bond_start`. Keys stored by SDK peer storage (flash).
 */
int klin_gd32v_ble_bond_enable(void);

/**
 * Start pairing on the active link (central preferred, else peripheral).
 * Requires `bond_enable`. Completes via `app_sec` authen callback.
 */
int klin_gd32v_ble_bond_start(void);
int klin_gd32v_ble_bonded(void);
int klin_gd32v_ble_wait_bonded(int timeout_ms);
int klin_gd32v_ble_bond_count(void);
int klin_gd32v_ble_bond_clear(void);

#ifdef __cplusplus
}
#endif
