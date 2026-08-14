/* Thin BLE advertise + GATT + central scan + GATT client + bonding helpers for
 * Klin over the GigaDevice VW55x BLE SDK. Heap / OSAL BLE task / GAP / GATTS /
 * GATTC / security / scan events are SDK contracts (not Klin magic).
 *
 * `@v0.9.0` = … + LE privacy / RPA (`privacy_enable` before advertise/scan).
 * Peripheral GATT: up to KLIN_GD32V_BLE_GATT_SVC_MAX services (1 chr each),
 * 16-bit or 128-bit UUIDs via gatt_uuid* / gatt_add_* before init.
 * Default single svc 0xFFF0 / chr 0xFFF1 when unset.
 * Central: scan + connect + gattc_discover (slot select or gattc_uuid*).
 * Scan results use a fixed table (max 16) — no Klin / glue malloc.
 * Central / GATT client / bonding need an SDK build with those roles (e.g. msdk_ffd).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define KLIN_GD32V_BLE_GATT_VALUE_MAX 20
#define KLIN_GD32V_BLE_GATT_SVC_MAX 4
/** Default primary service UUID16 (when table empty at init). */
#define KLIN_GD32V_BLE_GATT_SVC_UUID16 0xFFF0
/** Default characteristic UUID16. */
#define KLIN_GD32V_BLE_GATT_CHR_UUID16 0xFFF1

/** Max scan results kept (deduped by address). Caller-visible contract. */
#define KLIN_GD32V_BLE_SCAN_MAX 16

/** Max GAP name bytes stored per scan result (NUL-terminated in C). */
#define KLIN_GD32V_BLE_SCAN_NAME_MAX 28

/** Replace slot 0 with 16-bit svc/chr. Before `init`. */
int klin_gd32v_ble_gatt_uuid16(int svc_uuid16, int chr_uuid16);
/** Replace slot 0 with 128-bit svc/chr (16 bytes LE each). Before `init`. */
int klin_gd32v_ble_gatt_uuid128(const unsigned char *svc16, const unsigned char *chr16);
/** Append 16-bit svc/chr (max KLIN_GD32V_BLE_GATT_SVC_MAX). Before `init`. */
int klin_gd32v_ble_gatt_add_uuid16(int svc_uuid16, int chr_uuid16);
/** Append 128-bit svc/chr. Before `init`. */
int klin_gd32v_ble_gatt_add_uuid128(const unsigned char *svc16, const unsigned char *chr16);
/** Clear service table (must add before init). */
int klin_gd32v_ble_gatt_clear(void);
/** Number of configured services (1 default before any calls). */
int klin_gd32v_ble_gatt_svc_count(void);
/** Slot 0 service UUID16, or 0 if 128-bit; default FFF0 if empty. */
int klin_gd32v_ble_gatt_svc_uuid16(void);
/** Slot 0 characteristic UUID16, or 0 if 128-bit; default FFF1 if empty. */
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
int klin_gd32v_ble_gatt_set_at(int index, const unsigned char *data, int len);
int klin_gd32v_ble_gatt_get(unsigned char *out, int max_len);
int klin_gd32v_ble_gatt_get_at(int index, unsigned char *out, int max_len);
int klin_gd32v_ble_gatt_len(void);
int klin_gd32v_ble_gatt_len_at(int index);
int klin_gd32v_ble_gatt_notify(void);
int klin_gd32v_ble_gatt_notify_at(int index);
int klin_gd32v_ble_gatt_written(void);
int klin_gd32v_ble_gatt_written_at(int index);

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

/** Select local slot UUID for `gattc_discover`. */
int klin_gd32v_ble_gattc_select(int index);
/** Override discover target (16-bit), independent of server table. */
int klin_gd32v_ble_gattc_uuid16(int svc_uuid16, int chr_uuid16);
/** Override discover target (128-bit, 16 bytes LE each). */
int klin_gd32v_ble_gattc_uuid128(const unsigned char *svc16, const unsigned char *chr16);

/**
 * Discover peer svc/chr from selected/override UUID (+ CCCD if present). Blocks.
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
 * Clears any prior `bond_passkey` config.
 */
int klin_gd32v_ble_bond_enable(void);

/**
 * Enable bonding with a fixed 6-digit passkey/PIN (`0..=999999`). MITM +
 * keyboard/display IO (`app_sec_set_authen` + `app_sec_pin_code_set`).
 * On input-key / numeric-compare requests the PIN is auto-injected / accepted.
 * Replaces Just Works SM config from `bond_enable`.
 */
int klin_gd32v_ble_bond_passkey(int passkey);

/** Configured passkey, or 0 if Just Works / unset. */
int klin_gd32v_ble_passkey(void);

/**
 * Last passkey action code (0 = none; 2 = input; 3 = display; 4 = numeric
 * compare — same numbering as NimBLE `BLE_SM_IOACT_*` for Klin parity).
 */
int klin_gd32v_ble_passkey_action(void);

/**
 * Manual inject for an outstanding INPUT action (usually not needed —
 * `bond_passkey` auto-injects).
 */
int klin_gd32v_ble_passkey_inject(int passkey);

/**
 * Start pairing on the active link (central preferred, else peripheral).
 * Requires `bond_enable` or `bond_passkey`. Completes via `app_sec` authen
 * callback.
 */
int klin_gd32v_ble_bond_start(void);
int klin_gd32v_ble_bonded(void);
int klin_gd32v_ble_wait_bonded(int timeout_ms);
int klin_gd32v_ble_bond_count(void);
int klin_gd32v_ble_bond_clear(void);

/**
 * Enable LE privacy (controller RPA via `ble_adp_privacy_recfg`). Call after
 * `init`, before advertise/scan/connect. Fails if advertising, scanning, or
 * connected (peripheral or central).
 */
int klin_gd32v_ble_privacy_enable(void);
/** Disable privacy; restore static/public own address type. Same idle constraint. */
int klin_gd32v_ble_privacy_disable(void);
int klin_gd32v_ble_privacy_enabled(void);
/**
 * Local addr type enum value (`BLE_GAP_LOCAL_ADDR_STATIC` = 0 /
 * `BLE_GAP_LOCAL_ADDR_RESOLVABLE` = 1 when privacy on).
 */
int klin_gd32v_ble_own_addr_type(void);
/**
 * Copy 6-byte own identity/public address into `out6`.
 * Privacy off: `ble_adp_public_addr_get`. Privacy on: identity via
 * `ble_adp_identity_addr_get` (`.addr`), else public fallback.
 */
int klin_gd32v_ble_own_addr(unsigned char *out6);

#ifdef __cplusplus
}
#endif
