/* BLE advertise + GATT + central scan + GATT client + bonding + UUID128 +
 * multi-service + LE privacy + Mesh Gen OnOff for Klin on GD32VW553. Real path:
 * GigaDevice VW55x Wi-Fi BLE SDK (AN152 `ble_init` / `app_adv_*` / `ble_gatts_*` /
 * `ble_scan_*` / `ble_conn_*` / `ble_gattc_*` / `app_sec_*` /
 * `ble_adp_privacy_recfg` / `bt_mesh_*`).
 * Host path: stubs when SDK headers are not on the include path.
 *
 * Scan table is fixed (max 16) — no glue malloc. Central / GATT client /
 * bonding need an SDK image with those roles (e.g. msdk_ffd).
 * Privacy: controller RPA via `ble_adp_privacy_recfg` + RESOLVABLE own_addr_type
 * for adv/scan/connect. `own_addr` uses public / identity getters (no LOC_ADDR
 * event cache — keep it simple).
 * Mesh: Zephyr-style GD32 mesh (`mesh_cfg.h` + Gen OnOff); needs BLE_MAX like
 * light_demo. Without mesh headers, mesh_enable returns -1.
 */
#include "ble_sdk.h"

#include <string.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include("ble_init.h")
#define KLIN_GD32V_BLE_HAVE_SDK 1
#endif
#endif

#ifdef KLIN_GD32V_BLE_HAVE_SDK
#include "ble_init.h"
#include "app_adapter_mgr.h"
#include "app_adv_mgr.h"
#include "app_sec_mgr.h"
#include "ble_gatt.h"
#include "ble_gatts.h"
#include "ble_gattc.h"
#include "ble_scan.h"
#include "ble_conn.h"
#include "ble_error.h"
#include "ble_types.h"
#include "ble_storage.h"
#include "ble_gap.h"
#include "ble_adapter.h"
#if defined(__has_include)
#if __has_include("wrapper_os.h")
#include "wrapper_os.h"
#define KLIN_GD32V_BLE_HAVE_OSAL 1
#endif
#endif
#if defined(__has_include)
#if __has_include("mesh_cfg.h") && __has_include("generic_server.h")
#include "mesh_cfg.h"
#include "api/mesh.h"
#include "mesh_kernel.h"
#include "generic_server.h"
#include "model_utils.h"
#define KLIN_GD32V_BLE_HAVE_MESH 1
#endif
#endif
#endif

#define KLIN_GD32V_BLE_UUID_KIND_16  16
#define KLIN_GD32V_BLE_UUID_KIND_128 128

typedef struct {
    unsigned char addr[6];
    unsigned char addr_type;
    int rssi;
    char name[KLIN_GD32V_BLE_SCAN_NAME_MAX];
} klin_gd32v_ble_scan_row_t;

typedef struct {
    uint8_t kind; /* 16 or 128 */
    uint16_t svc16;
    uint16_t chr16;
    uint8_t svc128[16];
    uint8_t chr128[16];
    unsigned char value[KLIN_GD32V_BLE_GATT_VALUE_MAX];
    int value_len;
    int written;
#ifdef KLIN_GD32V_BLE_HAVE_SDK
    uint8_t svc_id;
    uint16_t cccd;
    int added;
#endif
} klin_gd32v_ble_gatt_slot_t;

static int s_inited;
static int s_advertising;
static int s_connected;
static int s_scanning;
static int s_central_connected;
static int s_central_conn_idx;

static klin_gd32v_ble_scan_row_t s_scan[KLIN_GD32V_BLE_SCAN_MAX];
static int s_scan_count;

static uint16_t s_gattc_val_handle;
static uint16_t s_gattc_cccd_handle;
static int s_gattc_ready;
static int s_gattc_op_done;
static int s_gattc_op_rc;
static int s_gattc_disc_done;
static int s_gattc_svc_found;
static unsigned char s_gattc_buf[KLIN_GD32V_BLE_GATT_VALUE_MAX];
static int s_gattc_buf_len;
static int s_gattc_notified;

static int s_bond_enabled;
static int s_bonded;
/* Just Works: bond_enable. Passkey/PIN: bond_passkey(pin). */
static int s_passkey_mode;
static uint32_t s_passkey;
static int s_passkey_action; /* 0 / 2=input / 3=disp / 4=numcmp */
static uint8_t s_passkey_conn_idx;

/* LE privacy / RPA (`privacy_enable`). own_addr_type tracks GAP local enum. */
static int s_privacy;
static uint8_t s_own_addr_type;

/* Mesh Gen OnOff (`mesh_enable`). */
static int s_mesh_on;
static int s_mesh_inited;
static uint16_t s_mesh_primary;
static uint8_t s_mesh_onoff;
static int s_mesh_onoff_changed;
static uint32_t s_mesh_oob;

/* Server GATT table (max KLIN_GD32V_BLE_GATT_SVC_MAX). Built before init. */
static klin_gd32v_ble_gatt_slot_t s_slots[KLIN_GD32V_BLE_GATT_SVC_MAX];
static int s_slot_count;

/* Client discover target (defaults to slot 0 / override via gattc_uuid*). */
static int s_gattc_sel;
static int s_gattc_override;
static uint8_t s_gattc_kind;
static uint16_t s_gattc_svc16;
static uint16_t s_gattc_chr16;
static uint8_t s_gattc_svc128[16];
static uint8_t s_gattc_chr128[16];

static void klin_gd32v_ble_scan_clear(void)
{
    s_scan_count = 0;
    memset(s_scan, 0, sizeof(s_scan));
}

static void klin_gd32v_ble_gattc_reset(void)
{
    s_gattc_val_handle = 0;
    s_gattc_cccd_handle = 0;
    s_gattc_ready = 0;
    s_gattc_op_done = 0;
    s_gattc_op_rc = 0;
    s_gattc_disc_done = 0;
    s_gattc_svc_found = 0;
    s_gattc_buf_len = 0;
    s_gattc_notified = 0;
    memset(s_gattc_buf, 0, sizeof(s_gattc_buf));
}

static void klin_gd32v_ble_bond_link_reset(void)
{
    s_bonded = 0;
}

static void klin_gd32v_ble_passkey_reset(void)
{
    s_passkey_mode = 0;
    s_passkey = 0;
    s_passkey_action = 0;
    s_passkey_conn_idx = 0;
}

static void klin_gd32v_ble_privacy_reset(void)
{
    s_privacy = 0;
    /* BLE_GAP_LOCAL_ADDR_STATIC == 0 */
    s_own_addr_type = 0;
}

static void klin_gd32v_ble_mesh_state_reset(void)
{
    s_mesh_on = 0;
    s_mesh_inited = 0;
    s_mesh_primary = 0;
    s_mesh_onoff = 0;
    s_mesh_onoff_changed = 0;
    s_mesh_oob = 0;
}

static void klin_gd32v_ble_slots_ensure_default(void)
{
    if (s_slot_count > 0) {
        return;
    }
    memset(&s_slots[0], 0, sizeof(s_slots[0]));
    s_slots[0].kind = KLIN_GD32V_BLE_UUID_KIND_16;
    s_slots[0].svc16 = (uint16_t)KLIN_GD32V_BLE_GATT_SVC_UUID16;
    s_slots[0].chr16 = (uint16_t)KLIN_GD32V_BLE_GATT_CHR_UUID16;
    s_slot_count = 1;
}

static void klin_gd32v_ble_gatt_values_reset(void)
{
    int i;

    for (i = 0; i < KLIN_GD32V_BLE_GATT_SVC_MAX; i++) {
        s_slots[i].value_len = 0;
        s_slots[i].written = 0;
        memset(s_slots[i].value, 0, sizeof(s_slots[i].value));
#ifdef KLIN_GD32V_BLE_HAVE_SDK
        s_slots[i].cccd = 0;
#endif
    }
}

static int klin_gd32v_ble_slot_limit(void)
{
    return s_slot_count > 0 ? s_slot_count : 1;
}

#ifdef KLIN_GD32V_BLE_HAVE_SDK

enum {
    KLIN_GATT_IDX_SVC = 0,
    KLIN_GATT_IDX_CHAR,
    KLIN_GATT_IDX_VAL,
    KLIN_GATT_IDX_CCCD,
    KLIN_GATT_IDX_NB
};

static uint8_t s_conn_idx;
static int s_scan_cb_reg;
static int s_conn_cb_reg;
static int s_gattc_svc_reg;
static ble_uuid_t s_gattc_reg_uuid;

static ble_gatt_attr_desc_t s_gatt_db[KLIN_GD32V_BLE_GATT_SVC_MAX][KLIN_GATT_IDX_NB];
static uint8_t s_svc_uuid_bytes[KLIN_GD32V_BLE_GATT_SVC_MAX][16];

static void klin_gd32v_ble_sleep_ms(int ms)
{
    int i;
#ifdef KLIN_GD32V_BLE_HAVE_OSAL
    for (i = 0; i < ms; i++) {
        sys_ms_sleep(1);
    }
#else
    (void)ms;
    (void)i;
#endif
}

static int klin_gd32v_ble_slot_by_svc_id(uint8_t svc_id)
{
    int i;

    for (i = 0; i < s_slot_count; i++) {
        if (s_slots[i].added && s_slots[i].svc_id == svc_id) {
            return i;
        }
    }
    return -1;
}

static void klin_gd32v_ble_fill_attr_db(int idx)
{
    ble_gatt_attr_desc_t *db;
    uint16_t info;

    db = s_gatt_db[idx];
    memset(db, 0, sizeof(s_gatt_db[idx]));
    memset(s_svc_uuid_bytes[idx], 0, 16);

    db[KLIN_GATT_IDX_SVC].uuid[0] = (uint8_t)(BLE_GATT_DECL_PRIMARY_SERVICE & 0xff);
    db[KLIN_GATT_IDX_SVC].uuid[1] = (uint8_t)((BLE_GATT_DECL_PRIMARY_SERVICE >> 8) & 0xff);
    db[KLIN_GATT_IDX_SVC].info = PROP(RD);
    db[KLIN_GATT_IDX_SVC].ext_info = 0;

    db[KLIN_GATT_IDX_CHAR].uuid[0] = (uint8_t)(BLE_GATT_DECL_CHARACTERISTIC & 0xff);
    db[KLIN_GATT_IDX_CHAR].uuid[1] = (uint8_t)((BLE_GATT_DECL_CHARACTERISTIC >> 8) & 0xff);
    db[KLIN_GATT_IDX_CHAR].info = PROP(RD);
    db[KLIN_GATT_IDX_CHAR].ext_info = 0;

    info = (uint16_t)(PROP(RD) | PROP(WR) | PROP(NTF));
    if (s_slots[idx].kind == KLIN_GD32V_BLE_UUID_KIND_128) {
        memcpy(s_svc_uuid_bytes[idx], s_slots[idx].svc128, 16);
        memcpy(db[KLIN_GATT_IDX_VAL].uuid, s_slots[idx].chr128, 16);
        info = (uint16_t)(info | ATT_UUID(128));
    } else {
        s_svc_uuid_bytes[idx][0] = (uint8_t)(s_slots[idx].svc16 & 0xff);
        s_svc_uuid_bytes[idx][1] = (uint8_t)((s_slots[idx].svc16 >> 8) & 0xff);
        db[KLIN_GATT_IDX_VAL].uuid[0] = (uint8_t)(s_slots[idx].chr16 & 0xff);
        db[KLIN_GATT_IDX_VAL].uuid[1] = (uint8_t)((s_slots[idx].chr16 >> 8) & 0xff);
    }
    db[KLIN_GATT_IDX_VAL].info = info;
    db[KLIN_GATT_IDX_VAL].ext_info = (uint16_t)(OPT(NO_OFFSET) | KLIN_GD32V_BLE_GATT_VALUE_MAX);

    db[KLIN_GATT_IDX_CCCD].uuid[0] = (uint8_t)(BLE_GATT_DESC_CLIENT_CHAR_CFG & 0xff);
    db[KLIN_GATT_IDX_CCCD].uuid[1] = (uint8_t)((BLE_GATT_DESC_CLIENT_CHAR_CFG >> 8) & 0xff);
    db[KLIN_GATT_IDX_CCCD].info = (uint16_t)(PROP(RD) | PROP(WR));
    db[KLIN_GATT_IDX_CCCD].ext_info = OPT(NO_OFFSET);
}

static void klin_gd32v_ble_fill_uuid_from_target(ble_uuid_t *u, int want_svc)
{
    uint8_t kind;
    uint16_t u16;
    const uint8_t *u128;

    memset(u, 0, sizeof(*u));
    if (s_gattc_override) {
        kind = s_gattc_kind;
        if (want_svc) {
            u16 = s_gattc_svc16;
            u128 = s_gattc_svc128;
        } else {
            u16 = s_gattc_chr16;
            u128 = s_gattc_chr128;
        }
    } else {
        klin_gd32v_ble_slots_ensure_default();
        if (s_gattc_sel < 0 || s_gattc_sel >= s_slot_count) {
            kind = KLIN_GD32V_BLE_UUID_KIND_16;
            u16 = want_svc ? (uint16_t)KLIN_GD32V_BLE_GATT_SVC_UUID16
                           : (uint16_t)KLIN_GD32V_BLE_GATT_CHR_UUID16;
            u128 = NULL;
        } else {
            kind = s_slots[s_gattc_sel].kind;
            if (want_svc) {
                u16 = s_slots[s_gattc_sel].svc16;
                u128 = s_slots[s_gattc_sel].svc128;
            } else {
                u16 = s_slots[s_gattc_sel].chr16;
                u128 = s_slots[s_gattc_sel].chr128;
            }
        }
    }
    if (kind == KLIN_GD32V_BLE_UUID_KIND_128 && u128 != NULL) {
        u->type = BLE_UUID_TYPE_128;
        memcpy(u->data.uuid_128, u128, 16);
    } else {
        u->type = BLE_UUID_TYPE_16;
        u->data.uuid_16 = u16;
    }
}

static void klin_gd32v_ble_gattc_unreg(void)
{
    if (!s_gattc_svc_reg) {
        return;
    }
    (void)ble_gattc_svc_unreg(&s_gattc_reg_uuid);
    s_gattc_svc_reg = 0;
    memset(&s_gattc_reg_uuid, 0, sizeof(s_gattc_reg_uuid));
}

/* Forward decls for callbacks used before definition. */
static ble_status_t klin_gd32v_ble_gattc_cb(ble_gattc_msg_info_t *info);
static ble_status_t klin_gd32v_ble_gatt_cb(ble_gatts_msg_info_t *info);

static int klin_gd32v_ble_gattc_ensure_reg_cb(void)
{
    ble_uuid_t u;

    klin_gd32v_ble_fill_uuid_from_target(&u, 1);
    if (s_gattc_svc_reg) {
        if (s_gattc_reg_uuid.type == u.type) {
            if (u.type == BLE_UUID_TYPE_16 &&
                s_gattc_reg_uuid.data.uuid_16 == u.data.uuid_16) {
                return 0;
            }
            if (u.type == BLE_UUID_TYPE_128 &&
                memcmp(s_gattc_reg_uuid.data.uuid_128, u.data.uuid_128, 16) == 0) {
                return 0;
            }
        }
        klin_gd32v_ble_gattc_unreg();
    }
    if (ble_gattc_svc_reg(&u, klin_gd32v_ble_gattc_cb) == BLE_ERR_NO_ERROR) {
        s_gattc_reg_uuid = u;
        s_gattc_svc_reg = 1;
        return 0;
    }
    return -1;
}

/* AD types: 0x08 short local name, 0x09 complete local name. */
static void klin_gd32v_ble_parse_name(const uint8_t *data, uint16_t len, char *out)
{
    uint16_t i;
    uint8_t field_len;
    uint8_t type;
    uint8_t n;

    out[0] = '\0';
    if (data == NULL || len == 0) {
        return;
    }
    i = 0;
    while (i + 1 < len) {
        field_len = data[i];
        if (field_len == 0) {
            break;
        }
        if ((uint16_t)(i + 1 + field_len) > len) {
            break;
        }
        type = data[i + 1];
        if ((type == 0x08 || type == 0x09) && field_len >= 2) {
            n = (uint8_t)(field_len - 1);
            if (n >= KLIN_GD32V_BLE_SCAN_NAME_MAX) {
                n = KLIN_GD32V_BLE_SCAN_NAME_MAX - 1;
            }
            memcpy(out, data + i + 2, n);
            out[n] = '\0';
            if (type == 0x09) {
                return;
            }
        }
        i = (uint16_t)(i + 1 + field_len);
    }
}

static void klin_gd32v_ble_scan_add(const ble_gap_adv_report_info_t *rpt)
{
    int i;
    char name[KLIN_GD32V_BLE_SCAN_NAME_MAX];

    if (rpt == NULL) {
        return;
    }
    klin_gd32v_ble_parse_name(rpt->data.p_data, rpt->data.len, name);

    for (i = 0; i < s_scan_count; i++) {
        if (s_scan[i].addr_type == rpt->peer_addr.addr_type &&
            memcmp(s_scan[i].addr, rpt->peer_addr.addr, 6) == 0) {
            s_scan[i].rssi = rpt->rssi;
            if (name[0] != '\0') {
                memcpy(s_scan[i].name, name, KLIN_GD32V_BLE_SCAN_NAME_MAX);
            }
            return;
        }
    }
    if (s_scan_count >= KLIN_GD32V_BLE_SCAN_MAX) {
        return;
    }
    i = s_scan_count;
    memcpy(s_scan[i].addr, rpt->peer_addr.addr, 6);
    s_scan[i].addr_type = rpt->peer_addr.addr_type;
    s_scan[i].rssi = rpt->rssi;
    memcpy(s_scan[i].name, name, KLIN_GD32V_BLE_SCAN_NAME_MAX);
    s_scan_count = s_scan_count + 1;
}

static void klin_gd32v_ble_scan_cb(ble_scan_evt_t event, ble_scan_data_u *p_data)
{
    if (p_data == NULL) {
        return;
    }
    if (event == BLE_SCAN_EVT_ADV_RPT) {
        klin_gd32v_ble_scan_add(p_data->p_adv_rpt);
    } else if (event == BLE_SCAN_EVT_STATE_CHG) {
        if (p_data->scan_state.scan_state == BLE_SCAN_STATE_DISABLED) {
            s_scanning = 0;
        } else if (p_data->scan_state.scan_state == BLE_SCAN_STATE_ENABLED) {
            s_scanning = 1;
        }
    }
}

static void klin_gd32v_ble_conn_cb(ble_conn_evt_t event, ble_conn_data_u *p_data)
{
    ble_conn_state_chg_t *st;

    if (p_data == NULL) {
        return;
    }
    if (event != BLE_CONN_EVT_STATE_CHG) {
        return;
    }
    st = &p_data->conn_state;
    if (st->state == BLE_CONN_STATE_CONNECTED) {
        if (st->info.conn_info.role == BLE_MASTER) {
            s_central_connected = 1;
            s_central_conn_idx = st->info.conn_info.conn_idx;
            s_scanning = 0;
            s_advertising = 0;
        }
    } else if (st->state == BLE_CONN_STATE_DISCONNECTD) {
        if (s_central_connected &&
            st->info.discon_info.conn_idx == (uint8_t)s_central_conn_idx) {
            s_central_connected = 0;
            klin_gd32v_ble_gattc_reset();
            klin_gd32v_ble_bond_link_reset();
        }
    }
}

static void klin_gd32v_ble_gattc_disc_done(uint8_t conn_idx, uint16_t status)
{
    (void)conn_idx;
    s_gattc_op_rc = (int)status;
    s_gattc_disc_done = 1;
}

static ble_status_t klin_gd32v_ble_gattc_cb(ble_gattc_msg_info_t *info)
{
    ble_gattc_op_info_t *op;
    ble_gattc_read_rsp_t *rd;
    ble_gattc_write_rsp_t *wr;
    ble_gattc_ntf_ind_t *ntf;
    int n;

    if (info == NULL) {
        return BLE_ERR_NO_ERROR;
    }
    if (info->cli_msg_type != BLE_CLI_EVT_GATT_OPERATION) {
        return BLE_ERR_NO_ERROR;
    }
    op = &info->msg_data.gattc_op_info;
    if (op->gattc_op_sub_evt == BLE_CLI_EVT_SVC_DISC_DONE_RSP) {
        s_gattc_svc_found = op->gattc_op_data.svc_dis_done_ind.is_found ? 1 : 0;
    } else if (op->gattc_op_sub_evt == BLE_CLI_EVT_READ_RSP) {
        rd = &op->gattc_op_data.read_rsp;
        s_gattc_op_rc = (int)rd->status;
        if (rd->status == BLE_ERR_NO_ERROR && rd->p_value != NULL) {
            n = (int)rd->length;
            if (n > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
                n = KLIN_GD32V_BLE_GATT_VALUE_MAX;
            }
            if (n > 0) {
                memcpy(s_gattc_buf, rd->p_value, (size_t)n);
            }
            s_gattc_buf_len = n;
        } else {
            s_gattc_buf_len = 0;
        }
        s_gattc_op_done = 1;
    } else if (op->gattc_op_sub_evt == BLE_CLI_EVT_WRITE_RSP) {
        wr = &op->gattc_op_data.write_rsp;
        s_gattc_op_rc = (int)wr->status;
        s_gattc_op_done = 1;
    } else if (op->gattc_op_sub_evt == BLE_CLI_EVT_NTF_IND_RCV) {
        ntf = &op->gattc_op_data.ntf_ind;
        if (ntf->p_value != NULL) {
            n = (int)ntf->length;
            if (n > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
                n = KLIN_GD32V_BLE_GATT_VALUE_MAX;
            }
            if (n > 0) {
                memcpy(s_gattc_buf, ntf->p_value, (size_t)n);
            }
            s_gattc_buf_len = n;
            s_gattc_notified = 1;
        }
    }
    return BLE_ERR_NO_ERROR;
}

static int klin_gd32v_ble_gattc_resolve_handles(void)
{
    ble_gattc_uuid_info_t svc;
    ble_gattc_uuid_info_t chr;
    ble_gattc_uuid_info_t desc;
    uint16_t handle;

    memset(&svc, 0, sizeof(svc));
    memset(&chr, 0, sizeof(chr));
    memset(&desc, 0, sizeof(desc));
    svc.instance_id = 0;
    chr.instance_id = 0;
    klin_gd32v_ble_fill_uuid_from_target(&svc.ble_uuid, 1);
    klin_gd32v_ble_fill_uuid_from_target(&chr.ble_uuid, 0);

    handle = 0;
    if (ble_gattc_find_char_handle((uint8_t)s_central_conn_idx, &svc, &chr,
                                   &handle) != BLE_ERR_NO_ERROR ||
        handle == 0) {
        return -1;
    }
    s_gattc_val_handle = handle;

    desc.instance_id = 0;
    desc.ble_uuid.type = BLE_UUID_TYPE_16;
    desc.ble_uuid.data.uuid_16 = BLE_GATT_DESC_CLIENT_CHAR_CFG;
    handle = 0;
    if (ble_gattc_find_desc_handle((uint8_t)s_central_conn_idx, &svc, &chr,
                                   &desc, &handle) == BLE_ERR_NO_ERROR) {
        s_gattc_cccd_handle = handle;
    } else {
        s_gattc_cccd_handle = 0;
    }
    return 0;
}

static int klin_gd32v_ble_wait_flag(volatile int *flag, int timeout_ms)
{
    int waited;

    waited = 0;
    while (1) {
        if (*flag) {
            return 0;
        }
        if (timeout_ms >= 0 && waited >= timeout_ms) {
            return -1;
        }
        klin_gd32v_ble_sleep_ms(1);
        waited = waited + 1;
    }
}

static ble_status_t klin_gd32v_ble_gatt_cb(ble_gatts_msg_info_t *info)
{
    ble_gatts_conn_state_change_ind_t *ind;
    ble_gatts_op_info_t *op;
    ble_gatts_read_req_t *rd;
    ble_gatts_write_req_t *wr;
    int slot;
    int n;
    int i;

    if (info == NULL) {
        return BLE_ERR_NO_ERROR;
    }

    if (info->srv_msg_type == BLE_SRV_EVT_CONN_STATE_CHANGE_IND) {
        ind = &info->msg_data.conn_state_change_ind;
        if (ind->conn_state == BLE_CONN_STATE_CONNECTED) {
            s_connected = 1;
            s_advertising = 0;
            s_conn_idx = ind->info.conn_info.conn_idx;
            for (i = 0; i < s_slot_count; i++) {
                s_slots[i].cccd = 0;
            }
        } else if (ind->conn_state == BLE_CONN_STATE_DISCONNECTD) {
            s_connected = 0;
            for (i = 0; i < s_slot_count; i++) {
                s_slots[i].cccd = 0;
            }
            klin_gd32v_ble_bond_link_reset();
        }
        return BLE_ERR_NO_ERROR;
    }

    if (info->srv_msg_type != BLE_SRV_EVT_GATT_OPERATION) {
        return BLE_ERR_NO_ERROR;
    }

    op = &info->msg_data.gatts_op_info;
    if (op->gatts_op_sub_evt == BLE_SRV_EVT_READ_REQ) {
        rd = &op->gatts_op_data.read_req;
        slot = klin_gd32v_ble_slot_by_svc_id(rd->svc_id);
        if (slot < 0) {
            return BLE_ERR_NO_ERROR;
        }
        if (rd->att_idx == KLIN_GATT_IDX_VAL) {
            if (rd->offset > (uint16_t)s_slots[slot].value_len) {
                rd->val_len = 0;
                rd->att_len = (uint16_t)s_slots[slot].value_len;
                return BLE_ERR_NO_ERROR;
            }
            n = s_slots[slot].value_len - (int)rd->offset;
            if (n > (int)rd->max_len) {
                n = (int)rd->max_len;
            }
            rd->val_len = (uint16_t)n;
            rd->att_len = (uint16_t)s_slots[slot].value_len;
            if (rd->p_val != NULL && n > 0) {
                memcpy(rd->p_val, s_slots[slot].value + rd->offset, (size_t)n);
            }
        } else if (rd->att_idx == KLIN_GATT_IDX_CCCD) {
            rd->val_len = BLE_GATT_CCCD_LEN;
            rd->att_len = BLE_GATT_CCCD_LEN;
            if (rd->p_val != NULL) {
                memcpy(rd->p_val, &s_slots[slot].cccd, BLE_GATT_CCCD_LEN);
            }
        }
        return BLE_ERR_NO_ERROR;
    }

    if (op->gatts_op_sub_evt == BLE_SRV_EVT_WRITE_REQ) {
        wr = &op->gatts_op_data.write_req;
        slot = klin_gd32v_ble_slot_by_svc_id(wr->svc_id);
        if (slot < 0) {
            return BLE_ERR_NO_ERROR;
        }
        if (wr->att_idx == KLIN_GATT_IDX_VAL) {
            if (wr->p_val == NULL || wr->val_len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
                return BLE_ERR_NO_ERROR;
            }
            memcpy(s_slots[slot].value, wr->p_val, wr->val_len);
            s_slots[slot].value_len = (int)wr->val_len;
            s_slots[slot].written = 1;
        } else if (wr->att_idx == KLIN_GATT_IDX_CCCD) {
            if (wr->p_val != NULL && wr->val_len == BLE_GATT_CCCD_LEN) {
                memcpy(&s_slots[slot].cccd, wr->p_val, BLE_GATT_CCCD_LEN);
            }
        }
    }

    return BLE_ERR_NO_ERROR;
}

int klin_gd32v_ble_init(void)
{
    int i;
    uint8_t svc_info;

    if (s_inited) {
        return 0;
    }
    klin_gd32v_ble_slots_ensure_default();
    ble_init(1);
    if (ble_wait_ready() != 0) {
        return -1;
    }
    for (i = 0; i < s_slot_count; i++) {
        klin_gd32v_ble_fill_attr_db(i);
        svc_info = (s_slots[i].kind == KLIN_GD32V_BLE_UUID_KIND_128) ? SVC_UUID(128)
                                                                     : SVC_UUID(16);
        if (ble_gatts_svc_add(&s_slots[i].svc_id, s_svc_uuid_bytes[i], 0, svc_info,
                              s_gatt_db[i], KLIN_GATT_IDX_NB,
                              klin_gd32v_ble_gatt_cb) != BLE_ERR_NO_ERROR) {
            while (i > 0) {
                i--;
                if (s_slots[i].added) {
                    (void)ble_gatts_svc_rmv(s_slots[i].svc_id);
                    s_slots[i].added = 0;
                }
            }
            ble_deinit();
            return -1;
        }
        s_slots[i].added = 1;
    }
    (void)klin_gd32v_ble_gattc_ensure_reg_cb();
    if (ble_scan_callback_register(klin_gd32v_ble_scan_cb) == BLE_ERR_NO_ERROR) {
        s_scan_cb_reg = 1;
    }
    if (ble_conn_callback_register(klin_gd32v_ble_conn_cb) == BLE_ERR_NO_ERROR) {
        s_conn_cb_reg = 1;
    }
    s_inited = 1;
    klin_gd32v_ble_gatt_values_reset();
    klin_gd32v_ble_gattc_reset();
    klin_gd32v_ble_bond_link_reset();
    klin_gd32v_ble_passkey_reset();
    klin_gd32v_ble_privacy_reset();
    klin_gd32v_ble_mesh_state_reset();
    s_bond_enabled = 0;
    klin_gd32v_ble_scan_clear();
    return 0;
}

int klin_gd32v_ble_advertise(const char *name)
{
    app_adv_param_t p;
    size_t n;

    if (!s_inited || name == NULL || name[0] == '\0') {
        return -1;
    }
    n = strlen(name);
    if (n > 29) {
        return -1;
    }
    if (!app_adp_set_name((char *)name, (uint16_t)n)) {
        return -1;
    }
    memset(&p, 0, sizeof(p));
    p.type = BLE_ADV_TYPE_LEGACY;
    p.prop = BLE_GAP_ADV_PROP_UNDIR_CONN;
    p.disc_mode = BLE_GAP_ADV_MODE_GEN_DISC;
    p.adv_intv = APP_ADV_INT_MIN;
    p.own_addr_type = s_own_addr_type;
    if (app_adv_create(&p) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    s_advertising = 1;
    return 0;
}

int klin_gd32v_ble_stop_advertise(void)
{
    if (s_advertising) {
        (void)app_adv_stop(0, 1);
    }
    s_advertising = 0;
    return 0;
}

int klin_gd32v_ble_connected(void)
{
    return s_connected;
}

int klin_gd32v_ble_advertising(void)
{
    return s_advertising;
}

int klin_gd32v_ble_wait_connected(int timeout_ms)
{
    int waited;

    if (!s_inited || !s_advertising) {
        return -1;
    }
    waited = 0;
    while (1) {
        if (s_connected) {
            return 0;
        }
        if (timeout_ms >= 0 && waited >= timeout_ms) {
            return -1;
        }
        klin_gd32v_ble_sleep_ms(1);
        waited = waited + 1;
    }
}

int klin_gd32v_ble_stop(void)
{
    int i;

    (void)klin_gd32v_ble_scan_stop();
    (void)klin_gd32v_ble_central_disconnect();
    (void)klin_gd32v_ble_stop_advertise();
    if (s_scan_cb_reg) {
        (void)ble_scan_callback_unregister(klin_gd32v_ble_scan_cb);
        s_scan_cb_reg = 0;
    }
    if (s_conn_cb_reg) {
        (void)ble_conn_callback_unregister(klin_gd32v_ble_conn_cb);
        s_conn_cb_reg = 0;
    }
    klin_gd32v_ble_gattc_unreg();
    for (i = 0; i < s_slot_count; i++) {
        if (s_slots[i].added) {
            (void)ble_gatts_svc_rmv(s_slots[i].svc_id);
            s_slots[i].added = 0;
        }
    }
    s_connected = 0;
    s_central_connected = 0;
    s_inited = 0;
    s_bond_enabled = 0;
    klin_gd32v_ble_gatt_values_reset();
    klin_gd32v_ble_gattc_reset();
    klin_gd32v_ble_bond_link_reset();
    klin_gd32v_ble_passkey_reset();
    klin_gd32v_ble_privacy_reset();
    klin_gd32v_ble_mesh_state_reset();
    klin_gd32v_ble_scan_clear();
    ble_deinit();
    return 0;
}

int klin_gd32v_ble_gatt_notify_at(int index)
{
    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_slot_count) {
        return -1;
    }
    if (!s_connected || (s_slots[index].cccd & BLE_GATT_CCCD_NTF_BIT) == 0) {
        return 0;
    }
    if (!s_slots[index].added) {
        return -1;
    }
    if (ble_gatts_ntf_ind_send(s_conn_idx, s_slots[index].svc_id, KLIN_GATT_IDX_VAL,
                               s_slots[index].value, (uint16_t)s_slots[index].value_len,
                               BLE_GATT_NOTIFY) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_gatt_notify(void)
{
    return klin_gd32v_ble_gatt_notify_at(0);
}

int klin_gd32v_ble_scan_start(int duration_ms)
{
    ble_gap_scan_param_t param;
    uint16_t dur10;

    if (!s_inited || duration_ms <= 0) {
        return -1;
    }
    (void)klin_gd32v_ble_stop_advertise();
    (void)klin_gd32v_ble_scan_stop();
    klin_gd32v_ble_scan_clear();

    memset(&param, 0, sizeof(param));
    param.type = BLE_GAP_SCAN_TYPE_OBSERVER;
    param.prop = (uint8_t)(BLE_GAP_SCAN_PROP_PHY_1M_BIT | BLE_GAP_SCAN_PROP_ACTIVE_1M_BIT);
    param.dup_filt_pol = 1;
    param.scan_intv_1m = 0x10;
    param.scan_win_1m = 0x10;
    dur10 = (uint16_t)((duration_ms + 9) / 10);
    if (dur10 == 0) {
        dur10 = 1;
    }
    param.duration = dur10;
    if (ble_scan_param_set(s_own_addr_type, &param) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (ble_scan_enable() != BLE_ERR_NO_ERROR) {
        return -1;
    }
    s_scanning = 1;
    klin_gd32v_ble_sleep_ms(duration_ms);
    (void)klin_gd32v_ble_scan_stop();
    return 0;
}

int klin_gd32v_ble_scan_stop(void)
{
    if (s_scanning) {
        (void)ble_scan_disable();
    }
    s_scanning = 0;
    return 0;
}

int klin_gd32v_ble_central_connect(int index, int timeout_ms)
{
    ble_gap_addr_t peer;

    (void)timeout_ms;
    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_scan_count) {
        return -1;
    }
    (void)klin_gd32v_ble_stop_advertise();
    (void)klin_gd32v_ble_scan_stop();
    if (s_central_connected) {
        (void)klin_gd32v_ble_central_disconnect();
    }
    memset(&peer, 0, sizeof(peer));
    peer.addr_type = s_scan[index].addr_type;
    memcpy(peer.addr, s_scan[index].addr, 6);
    if (ble_conn_connect(NULL, s_own_addr_type, &peer, 0) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_central_wait_connected(int timeout_ms)
{
    int waited;

    if (!s_inited) {
        return -1;
    }
    waited = 0;
    while (1) {
        if (s_central_connected) {
            return 0;
        }
        if (timeout_ms >= 0 && waited >= timeout_ms) {
            return -1;
        }
        klin_gd32v_ble_sleep_ms(1);
        waited = waited + 1;
    }
}

int klin_gd32v_ble_central_disconnect(void)
{
    if (!s_inited) {
        return -1;
    }
    if (!s_central_connected) {
        return 0;
    }
    (void)ble_conn_disconnect((uint8_t)s_central_conn_idx,
                              BLE_ERROR_HL_TO_HCI(BLE_LL_ERR_REMOTE_USER_TERM_CON));
    s_central_connected = 0;
    klin_gd32v_ble_gattc_reset();
    return 0;
}

int klin_gd32v_ble_gattc_discover(int timeout_ms)
{
    if (!s_inited || !s_central_connected) {
        return -1;
    }
    if (klin_gd32v_ble_gattc_ensure_reg_cb() != 0) {
        return -1;
    }
    klin_gd32v_ble_gattc_reset();
    s_gattc_disc_done = 0;
    s_gattc_svc_found = 0;
    if (ble_gattc_start_discovery((uint8_t)s_central_conn_idx,
                                  klin_gd32v_ble_gattc_disc_done) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (klin_gd32v_ble_wait_flag(&s_gattc_disc_done, timeout_ms) != 0) {
        return -1;
    }
    if (s_gattc_op_rc != (int)BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (!s_gattc_svc_found) {
        return -1;
    }
    if (klin_gd32v_ble_gattc_resolve_handles() != 0) {
        return -1;
    }
    s_gattc_ready = 1;
    return 0;
}

int klin_gd32v_ble_gattc_ready(void)
{
    return (s_gattc_ready && s_central_connected) ? 1 : 0;
}

int klin_gd32v_ble_gattc_read(int timeout_ms)
{
    if (!s_inited || !s_gattc_ready || !s_central_connected ||
        s_gattc_val_handle == 0) {
        return -1;
    }
    s_gattc_op_done = 0;
    s_gattc_op_rc = 0;
    s_gattc_buf_len = 0;
    if (ble_gattc_read((uint8_t)s_central_conn_idx, s_gattc_val_handle, 0, 0) !=
        BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (klin_gd32v_ble_wait_flag(&s_gattc_op_done, timeout_ms) != 0) {
        return -1;
    }
    return s_gattc_op_rc == (int)BLE_ERR_NO_ERROR ? 0 : -1;
}

int klin_gd32v_ble_gattc_write(const unsigned char *data, int len, int timeout_ms)
{
    if (!s_inited || !s_gattc_ready || !s_central_connected ||
        s_gattc_val_handle == 0 || data == NULL || len < 0 ||
        len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
        return -1;
    }
    s_gattc_op_done = 0;
    s_gattc_op_rc = 0;
    if (ble_gattc_write_req((uint8_t)s_central_conn_idx, s_gattc_val_handle,
                            (uint16_t)len, (uint8_t *)data) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (klin_gd32v_ble_wait_flag(&s_gattc_op_done, timeout_ms) != 0) {
        return -1;
    }
    return s_gattc_op_rc == (int)BLE_ERR_NO_ERROR ? 0 : -1;
}

int klin_gd32v_ble_gattc_subscribe(int timeout_ms)
{
    uint8_t cccd[2];

    if (!s_inited || !s_gattc_ready || !s_central_connected) {
        return -1;
    }
    if (s_gattc_cccd_handle == 0) {
        return -1;
    }
    cccd[0] = 0x01;
    cccd[1] = 0x00;
    s_gattc_op_done = 0;
    s_gattc_op_rc = 0;
    if (ble_gattc_write_req((uint8_t)s_central_conn_idx, s_gattc_cccd_handle, 2,
                            cccd) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    if (klin_gd32v_ble_wait_flag(&s_gattc_op_done, timeout_ms) != 0) {
        return -1;
    }
    return s_gattc_op_rc == (int)BLE_ERR_NO_ERROR ? 0 : -1;
}

static void klin_gd32v_ble_authen_cmpl(uint8_t conn_idx, uint8_t result)
{
    (void)conn_idx;
    if (result == BLE_ERR_NO_ERROR) {
        s_bonded = 1;
    } else {
        s_bonded = 0;
    }
}

static void klin_gd32v_ble_input_key_req(uint8_t conn_idx)
{
    s_passkey_conn_idx = conn_idx;
    s_passkey_action = 2; /* INPUT */
    if (s_passkey_mode) {
        app_sec_input_passkey(conn_idx, s_passkey);
    }
}

static void klin_gd32v_ble_key_cfm_req(uint8_t conn_idx, uint32_t key)
{
    (void)key;
    s_passkey_conn_idx = conn_idx;
    s_passkey_action = 4; /* NUMCMP */
    if (s_passkey_mode) {
        app_sec_num_compare(conn_idx, 1);
    }
}

static void klin_gd32v_ble_sec_callbacks_install(void)
{
    app_sec_callbacks cb;

    memset(&cb, 0, sizeof(cb));
    cb.authen_cmpl = klin_gd32v_ble_authen_cmpl;
    cb.input_key_req = klin_gd32v_ble_input_key_req;
    cb.key_cfm_req = klin_gd32v_ble_key_cfm_req;
    (void)app_sec_callbacks_set(cb);
}

int klin_gd32v_ble_bond_enable(void)
{
    if (!s_inited) {
        return -1;
    }
    /* Just Works: no IO / no MITM / SC + bond. Keys stored by SDK storage. */
    app_sec_set_authen(1, 0, 1, BLE_GAP_IO_CAP_NO_IO, 0, 0, 16);
    klin_gd32v_ble_sec_callbacks_install();
    klin_gd32v_ble_passkey_reset();
    s_bond_enabled = 1;
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

int klin_gd32v_ble_bond_passkey(int passkey)
{
    if (!s_inited) {
        return -1;
    }
    if (passkey < 0 || passkey > 999999) {
        return -1;
    }
    /* MITM + keyboard/display IO + SC + bond. Fixed PIN via pin_code_set. */
    app_sec_set_authen(1, 1, 1, BLE_GAP_IO_CAP_KEYBOARD_DISPLAY, 0, 0, 16);
    if (!app_sec_pin_code_set((uint32_t)passkey)) {
        return -1;
    }
    klin_gd32v_ble_sec_callbacks_install();
    s_passkey = (uint32_t)passkey;
    s_passkey_mode = 1;
    s_passkey_action = 0;
    s_passkey_conn_idx = 0;
    s_bond_enabled = 1;
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

int klin_gd32v_ble_passkey(void)
{
    return s_passkey_mode ? (int)s_passkey : 0;
}

int klin_gd32v_ble_passkey_action(void)
{
    return s_passkey_action;
}

int klin_gd32v_ble_passkey_inject(int passkey)
{
    uint8_t idx;

    if (!s_inited || !s_passkey_mode) {
        return -1;
    }
    if (passkey < 0 || passkey > 999999) {
        return -1;
    }
    if (s_central_connected) {
        idx = (uint8_t)s_central_conn_idx;
    } else if (s_connected) {
        idx = s_conn_idx;
    } else if (s_passkey_action != 0) {
        idx = s_passkey_conn_idx;
    } else {
        return -1;
    }
    s_passkey_action = 2;
    app_sec_input_passkey(idx, (uint32_t)passkey);
    return 0;
}

int klin_gd32v_ble_bond_start(void)
{
    uint8_t idx;

    if (!s_inited || !s_bond_enabled) {
        return -1;
    }
    if (s_central_connected) {
        idx = (uint8_t)s_central_conn_idx;
    } else if (s_connected) {
        idx = s_conn_idx;
    } else {
        return -1;
    }
    klin_gd32v_ble_bond_link_reset();
    /* Display path uses app_sec_env.pin_code when set (bond_passkey). */
    if (s_passkey_mode) {
        s_passkey_action = 3; /* DISP expected / fixed PIN advertised */
        s_passkey_conn_idx = idx;
    }
    app_sec_send_bond_req(idx);
    return 0;
}

int klin_gd32v_ble_bonded(void)
{
    return s_bonded ? 1 : 0;
}

int klin_gd32v_ble_wait_bonded(int timeout_ms)
{
    if (!s_inited) {
        return -1;
    }
    return klin_gd32v_ble_wait_flag(&s_bonded, timeout_ms);
}

int klin_gd32v_ble_bond_count(void)
{
    uint8_t num;
    ble_gap_addr_t addrs[BLE_PEER_NUM_MAX];

    if (!s_inited) {
        return 0;
    }
    num = BLE_PEER_NUM_MAX;
    if (ble_peer_all_addr_get(&num, addrs) != BLE_ERR_NO_ERROR) {
        return 0;
    }
    return (int)num;
}

int klin_gd32v_ble_bond_clear(void)
{
    uint8_t num;
    uint8_t i;
    ble_gap_addr_t addrs[BLE_PEER_NUM_MAX];

    if (!s_inited) {
        return -1;
    }
    num = BLE_PEER_NUM_MAX;
    if (ble_peer_all_addr_get(&num, addrs) != BLE_ERR_NO_ERROR) {
        klin_gd32v_ble_bond_link_reset();
        return 0;
    }
    for (i = 0; i < num; i++) {
        (void)ble_peer_data_delete(&addrs[i]);
    }
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

static int klin_gd32v_ble_privacy_radio_busy(void)
{
    return (s_advertising || s_scanning || s_connected || s_central_connected) ? 1 : 0;
}

int klin_gd32v_ble_privacy_enable(void)
{
    if (!s_inited) {
        return -1;
    }
    if (klin_gd32v_ble_privacy_radio_busy()) {
        return -1;
    }
    if (ble_adp_privacy_recfg((uint8_t)BLE_GAP_PRIV_CFG_PRIV_EN_BIT, NULL) !=
        BLE_ERR_NO_ERROR) {
        return -1;
    }
    s_privacy = 1;
    s_own_addr_type = (uint8_t)BLE_GAP_LOCAL_ADDR_RESOLVABLE;
    return 0;
}

int klin_gd32v_ble_privacy_disable(void)
{
    if (!s_inited) {
        return -1;
    }
    if (klin_gd32v_ble_privacy_radio_busy()) {
        return -1;
    }
    if (ble_adp_privacy_recfg(0, NULL) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    s_privacy = 0;
    s_own_addr_type = (uint8_t)BLE_GAP_LOCAL_ADDR_STATIC;
    return 0;
}

int klin_gd32v_ble_privacy_enabled(void)
{
    return s_privacy ? 1 : 0;
}

int klin_gd32v_ble_own_addr_type(void)
{
    return (int)s_own_addr_type;
}

int klin_gd32v_ble_own_addr(unsigned char *out6)
{
    ble_gap_addr_t id;

    if (out6 == NULL) {
        return -1;
    }
    if (!s_inited) {
        return -1;
    }
    if (s_privacy) {
        if (ble_adp_identity_addr_get(&id) == BLE_ERR_NO_ERROR) {
            memcpy(out6, id.addr, 6);
            return 0;
        }
        /* Fallback: public address if identity is not ready yet. */
    }
    if (ble_adp_public_addr_get(out6) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    return 0;
}

#if defined(KLIN_GD32V_BLE_HAVE_MESH)

static uint8_t s_mesh_dev_uuid[16];
static struct bt_mesh_health_srv s_mesh_health_srv;
BT_MESH_HEALTH_PUB_DEFINE(s_mesh_health_pub, 0);
BT_MESH_MODEL_PUB_DEFINE(s_mesh_onoff_pub, NULL, 5);

static void klin_gd32v_ble_mesh_onoff_cb(void *user_data,
                                        enum bt_mesh_srv_callback_evt evt,
                                        void *state)
{
    struct bt_mesh_gen_onoff_state *onoff = state;

    (void)user_data;
    if (evt != BT_MESH_SRV_GEN_ONOFF_EVT || onoff == NULL) {
        return;
    }
    s_mesh_onoff = onoff->onoff ? 1 : 0;
    s_mesh_onoff_changed = 1;
}

static struct bt_mesh_srv_callbacks s_mesh_onoff_cb = {
    .state_change = klin_gd32v_ble_mesh_onoff_cb,
};

static struct bt_mesh_gen_onoff_srv s_mesh_onoff_srv = {
    .cb = &s_mesh_onoff_cb,
};

static const struct bt_mesh_model s_mesh_root_models[] = {
    BT_MESH_MODEL_CFG_SRV,
    BT_MESH_MODEL_HEALTH_SRV(&s_mesh_health_srv, &s_mesh_health_pub),
    BT_MESH_MODEL_GEN_ONOFF_SRV(&s_mesh_onoff_srv, &s_mesh_onoff_pub),
};

static const struct bt_mesh_model s_mesh_vnd_models[] = {
};

static const struct bt_mesh_elem s_mesh_elements[] = {
    BT_MESH_ELEM(0, s_mesh_root_models, s_mesh_vnd_models),
};

static const struct bt_mesh_comp s_mesh_comp = {
    .cid = 0xFFFF,
    .elem = s_mesh_elements,
    .elem_count = 1,
};

static void klin_gd32v_ble_mesh_prov_complete(uint16_t net_idx, uint16_t addr)
{
    (void)net_idx;
    s_mesh_primary = addr;
}

static int klin_gd32v_ble_mesh_output_number(bt_mesh_output_action_t action,
                                             uint32_t number)
{
    (void)action;
    s_mesh_oob = number;
    return 0;
}

static const struct bt_mesh_prov s_mesh_prov = {
    .uuid = s_mesh_dev_uuid,
    .output_size = 6,
    .output_actions = BT_MESH_DISPLAY_NUMBER,
    .output_number = klin_gd32v_ble_mesh_output_number,
    .complete = klin_gd32v_ble_mesh_prov_complete,
};

int klin_gd32v_ble_mesh_enable(void)
{
    int err;
    unsigned char pub[6];

    if (!s_inited) {
        return -1;
    }
    if (s_mesh_inited) {
        s_mesh_on = 1;
        return 0;
    }

    if (s_advertising) {
        (void)klin_gd32v_ble_stop_advertise();
    }

    memset(s_mesh_dev_uuid, 0, sizeof(s_mesh_dev_uuid));
    if (ble_adp_public_addr_get(pub) == BLE_ERR_NO_ERROR) {
        memcpy(&s_mesh_dev_uuid[2], pub, 6);
    }

    mesh_kernel_init();
    err = bt_mesh_init(&s_mesh_prov, &s_mesh_comp);
    if (err) {
        return -1;
    }
    s_mesh_inited = 1;
    s_mesh_on = 1;

    if (bt_mesh_is_provisioned()) {
        s_mesh_primary = bt_mesh_primary_addr();
    } else {
        err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
        if (err) {
            return -1;
        }
    }
    return 0;
}

int klin_gd32v_ble_mesh_enabled(void)
{
    return s_mesh_on ? 1 : 0;
}

int klin_gd32v_ble_mesh_provisioned(void)
{
    if (!s_mesh_inited) {
        return 0;
    }
    return bt_mesh_is_provisioned() ? 1 : 0;
}

int klin_gd32v_ble_mesh_primary_addr(void)
{
    if (!s_mesh_inited || !bt_mesh_is_provisioned()) {
        return 0;
    }
    if (s_mesh_primary == 0) {
        s_mesh_primary = bt_mesh_primary_addr();
    }
    return (int)s_mesh_primary;
}

int klin_gd32v_ble_mesh_onoff(void)
{
    return (int)s_mesh_onoff;
}

int klin_gd32v_ble_mesh_onoff_set(int onoff)
{
    if (!s_mesh_inited) {
        return -1;
    }
    gen_onoff_config(&s_mesh_onoff_srv, onoff ? 1 : 0);
    s_mesh_onoff = onoff ? 1 : 0;
    s_mesh_onoff_changed = 1;
    return 0;
}

int klin_gd32v_ble_mesh_onoff_changed(void)
{
    int c = s_mesh_onoff_changed;
    s_mesh_onoff_changed = 0;
    return c;
}

int klin_gd32v_ble_mesh_oob_number(void)
{
    return (int)s_mesh_oob;
}

int klin_gd32v_ble_mesh_reset(void)
{
    int err;

    if (!s_mesh_inited) {
        return -1;
    }
    bt_mesh_reset();
    s_mesh_primary = 0;
    s_mesh_oob = 0;
    s_mesh_onoff = 0;
    s_mesh_onoff_changed = 0;
    err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
    if (err) {
        return -1;
    }
    return 0;
}

#else /* HAVE_SDK but no mesh headers */

int klin_gd32v_ble_mesh_enable(void)
{
    (void)s_mesh_on;
    return -1; /* mesh not in this SDK build */
}

int klin_gd32v_ble_mesh_enabled(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_provisioned(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_primary_addr(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_onoff(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_onoff_set(int onoff)
{
    (void)onoff;
    return -1;
}

int klin_gd32v_ble_mesh_onoff_changed(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_oob_number(void)
{
    return 0;
}

int klin_gd32v_ble_mesh_reset(void)
{
    return -1;
}

#endif /* KLIN_GD32V_BLE_HAVE_MESH */


#else /* host stubs — no SDK headers */

int klin_gd32v_ble_init(void)
{
    klin_gd32v_ble_slots_ensure_default();
    s_inited = 1;
    s_scanning = 0;
    s_central_connected = 0;
    s_scan_count = 0;
    s_bond_enabled = 0;
    memset(s_scan, 0, sizeof(s_scan));
    klin_gd32v_ble_gatt_values_reset();
    klin_gd32v_ble_gattc_reset();
    klin_gd32v_ble_bond_link_reset();
    klin_gd32v_ble_passkey_reset();
    klin_gd32v_ble_privacy_reset();
    klin_gd32v_ble_mesh_state_reset();
    return 0;
}

int klin_gd32v_ble_advertise(const char *name)
{
    if (!s_inited || name == NULL || name[0] == '\0') {
        return -1;
    }
    s_advertising = 1;
    s_connected = 0;
    return 0;
}

int klin_gd32v_ble_stop_advertise(void)
{
    s_advertising = 0;
    return 0;
}

int klin_gd32v_ble_connected(void)
{
    return s_connected;
}

int klin_gd32v_ble_advertising(void)
{
    return s_advertising;
}

int klin_gd32v_ble_wait_connected(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited || !s_advertising) {
        return -1;
    }
    s_connected = 1;
    return 0;
}

int klin_gd32v_ble_stop(void)
{
    s_advertising = 0;
    s_connected = 0;
    s_scanning = 0;
    s_central_connected = 0;
    s_inited = 0;
    s_bond_enabled = 0;
    s_scan_count = 0;
    memset(s_scan, 0, sizeof(s_scan));
    klin_gd32v_ble_gatt_values_reset();
    klin_gd32v_ble_gattc_reset();
    klin_gd32v_ble_bond_link_reset();
    klin_gd32v_ble_passkey_reset();
    klin_gd32v_ble_privacy_reset();
    klin_gd32v_ble_mesh_state_reset();
    return 0;
}

int klin_gd32v_ble_gatt_notify_at(int index)
{
    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_slot_count) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_gatt_notify(void)
{
    return klin_gd32v_ble_gatt_notify_at(0);
}

int klin_gd32v_ble_scan_start(int duration_ms)
{
    static const unsigned char stub_addr[6] = {1, 2, 3, 4, 5, 6};

    if (!s_inited || duration_ms <= 0) {
        return -1;
    }
    s_advertising = 0;
    s_scanning = 0;
    klin_gd32v_ble_scan_clear();
    memcpy(s_scan[0].addr, stub_addr, 6);
    s_scan[0].addr_type = 0;
    s_scan[0].rssi = -40;
    memcpy(s_scan[0].name, "stub", 5);
    s_scan_count = 1;
    return 0;
}

int klin_gd32v_ble_scan_stop(void)
{
    s_scanning = 0;
    return 0;
}

int klin_gd32v_ble_central_connect(int index, int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_scan_count) {
        return -1;
    }
    s_advertising = 0;
    s_scanning = 0;
    s_central_connected = 1;
    s_central_conn_idx = 0;
    return 0;
}

int klin_gd32v_ble_central_wait_connected(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited) {
        return -1;
    }
    if (!s_central_connected) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_central_disconnect(void)
{
    s_central_connected = 0;
    klin_gd32v_ble_gattc_reset();
    return 0;
}

int klin_gd32v_ble_gattc_discover(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited || !s_central_connected) {
        return -1;
    }
    klin_gd32v_ble_gattc_reset();
    s_gattc_val_handle = 1;
    s_gattc_cccd_handle = 2;
    s_gattc_ready = 1;
    return 0;
}

int klin_gd32v_ble_gattc_ready(void)
{
    return (s_gattc_ready && s_central_connected) ? 1 : 0;
}

int klin_gd32v_ble_gattc_read(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited || !s_gattc_ready || !s_central_connected) {
        return -1;
    }
    s_gattc_buf[0] = 0xA1;
    s_gattc_buf[1] = 0xA2;
    s_gattc_buf_len = 2;
    return 0;
}

int klin_gd32v_ble_gattc_write(const unsigned char *data, int len, int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited || !s_gattc_ready || !s_central_connected || data == NULL ||
        len < 0 || len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
        return -1;
    }
    if (len > 0) {
        memcpy(s_gattc_buf, data, (size_t)len);
    }
    s_gattc_buf_len = len;
    return 0;
}

int klin_gd32v_ble_gattc_subscribe(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited || !s_gattc_ready || !s_central_connected) {
        return -1;
    }
    if (s_gattc_cccd_handle == 0) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_bond_enable(void)
{
    if (!s_inited) {
        return -1;
    }
    klin_gd32v_ble_passkey_reset();
    s_bond_enabled = 1;
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

int klin_gd32v_ble_bond_passkey(int passkey)
{
    if (!s_inited) {
        return -1;
    }
    if (passkey < 0 || passkey > 999999) {
        return -1;
    }
    s_passkey = (uint32_t)passkey;
    s_passkey_mode = 1;
    s_passkey_action = 0;
    s_bond_enabled = 1;
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

int klin_gd32v_ble_passkey(void)
{
    return s_passkey_mode ? (int)s_passkey : 0;
}

int klin_gd32v_ble_passkey_action(void)
{
    return s_passkey_action;
}

int klin_gd32v_ble_passkey_inject(int passkey)
{
    if (!s_inited || !s_passkey_mode) {
        return -1;
    }
    if (passkey < 0 || passkey > 999999) {
        return -1;
    }
    s_passkey_action = 2;
    return 0;
}

int klin_gd32v_ble_bond_start(void)
{
    if (!s_inited || !s_bond_enabled) {
        return -1;
    }
    if (!s_central_connected && !s_connected) {
        return -1;
    }
    if (s_passkey_mode) {
        s_passkey_action = 3;
    }
    s_bonded = 1;
    return 0;
}

int klin_gd32v_ble_bonded(void)
{
    return s_bonded ? 1 : 0;
}

int klin_gd32v_ble_wait_bonded(int timeout_ms)
{
    (void)timeout_ms;
    if (!s_inited) {
        return -1;
    }
    if (!s_bonded) {
        return -1;
    }
    return 0;
}

int klin_gd32v_ble_bond_count(void)
{
    return s_bonded ? 1 : 0;
}

int klin_gd32v_ble_bond_clear(void)
{
    if (!s_inited) {
        return -1;
    }
    klin_gd32v_ble_bond_link_reset();
    return 0;
}

static int klin_gd32v_ble_privacy_radio_busy_stub(void)
{
    return (s_advertising || s_scanning || s_connected || s_central_connected) ? 1 : 0;
}

int klin_gd32v_ble_privacy_enable(void)
{
    if (!s_inited) {
        return -1;
    }
    if (klin_gd32v_ble_privacy_radio_busy_stub()) {
        return -1;
    }
    s_privacy = 1;
    s_own_addr_type = 1; /* BLE_GAP_LOCAL_ADDR_RESOLVABLE */
    return 0;
}

int klin_gd32v_ble_privacy_disable(void)
{
    if (!s_inited) {
        return -1;
    }
    if (klin_gd32v_ble_privacy_radio_busy_stub()) {
        return -1;
    }
    s_privacy = 0;
    s_own_addr_type = 0; /* BLE_GAP_LOCAL_ADDR_STATIC */
    return 0;
}

int klin_gd32v_ble_privacy_enabled(void)
{
    return s_privacy ? 1 : 0;
}

int klin_gd32v_ble_own_addr_type(void)
{
    return (int)s_own_addr_type;
}

int klin_gd32v_ble_own_addr(unsigned char *out6)
{
    static const unsigned char stub_pub[6] = {1, 2, 3, 4, 5, 6};
    static const unsigned char stub_priv[6] = {2, 3, 4, 5, 6, 7};

    if (out6 == NULL) {
        return -1;
    }
    if (!s_inited) {
        return -1;
    }
    memcpy(out6, s_privacy ? stub_priv : stub_pub, 6);
    return 0;
}

int klin_gd32v_ble_mesh_enable(void)
{
    if (!s_inited) {
        return -1;
    }
    s_mesh_on = 1;
    s_mesh_inited = 1;
    s_mesh_primary = 2;
    return 0;
}

int klin_gd32v_ble_mesh_enabled(void)
{
    return s_mesh_on ? 1 : 0;
}

int klin_gd32v_ble_mesh_provisioned(void)
{
    return (s_mesh_inited && s_mesh_primary != 0) ? 1 : 0;
}

int klin_gd32v_ble_mesh_primary_addr(void)
{
    return s_mesh_inited ? (int)s_mesh_primary : 0;
}

int klin_gd32v_ble_mesh_onoff(void)
{
    return (int)s_mesh_onoff;
}

int klin_gd32v_ble_mesh_onoff_set(int onoff)
{
    if (!s_mesh_inited) {
        return -1;
    }
    s_mesh_onoff = onoff ? 1 : 0;
    s_mesh_onoff_changed = 1;
    return 0;
}

int klin_gd32v_ble_mesh_onoff_changed(void)
{
    int c = s_mesh_onoff_changed;
    s_mesh_onoff_changed = 0;
    return c;
}

int klin_gd32v_ble_mesh_oob_number(void)
{
    return (int)s_mesh_oob;
}

int klin_gd32v_ble_mesh_reset(void)
{
    if (!s_mesh_inited) {
        return -1;
    }
    s_mesh_primary = 0;
    s_mesh_oob = 0;
    s_mesh_onoff = 0;
    s_mesh_onoff_changed = 0;
    return 0;
}


#endif

int klin_gd32v_ble_gatt_uuid16(int svc_uuid16, int chr_uuid16)
{
    if (svc_uuid16 <= 0 || svc_uuid16 > 0xFFFF || chr_uuid16 <= 0 ||
        chr_uuid16 > 0xFFFF) {
        return -1;
    }
    if (s_inited) {
        return -1;
    }
    if (s_slot_count < 1) {
        s_slot_count = 1;
        memset(&s_slots[0], 0, sizeof(s_slots[0]));
    }
    s_slots[0].kind = KLIN_GD32V_BLE_UUID_KIND_16;
    s_slots[0].svc16 = (uint16_t)svc_uuid16;
    s_slots[0].chr16 = (uint16_t)chr_uuid16;
    s_gattc_override = 0;
    s_gattc_sel = 0;
    return 0;
}

int klin_gd32v_ble_gatt_uuid128(const unsigned char *svc16, const unsigned char *chr16)
{
    if (svc16 == NULL || chr16 == NULL) {
        return -1;
    }
    if (s_inited) {
        return -1;
    }
    if (s_slot_count < 1) {
        s_slot_count = 1;
        memset(&s_slots[0], 0, sizeof(s_slots[0]));
    }
    s_slots[0].kind = KLIN_GD32V_BLE_UUID_KIND_128;
    memcpy(s_slots[0].svc128, svc16, 16);
    memcpy(s_slots[0].chr128, chr16, 16);
    s_gattc_override = 0;
    s_gattc_sel = 0;
    return 0;
}

int klin_gd32v_ble_gatt_add_uuid16(int svc_uuid16, int chr_uuid16)
{
    int i;

    if (svc_uuid16 <= 0 || svc_uuid16 > 0xFFFF || chr_uuid16 <= 0 ||
        chr_uuid16 > 0xFFFF) {
        return -1;
    }
    if (s_inited) {
        return -1;
    }
    if (s_slot_count >= KLIN_GD32V_BLE_GATT_SVC_MAX) {
        return -1;
    }
    i = s_slot_count++;
    memset(&s_slots[i], 0, sizeof(s_slots[i]));
    s_slots[i].kind = KLIN_GD32V_BLE_UUID_KIND_16;
    s_slots[i].svc16 = (uint16_t)svc_uuid16;
    s_slots[i].chr16 = (uint16_t)chr_uuid16;
    return 0;
}

int klin_gd32v_ble_gatt_add_uuid128(const unsigned char *svc16, const unsigned char *chr16)
{
    int i;

    if (svc16 == NULL || chr16 == NULL) {
        return -1;
    }
    if (s_inited) {
        return -1;
    }
    if (s_slot_count >= KLIN_GD32V_BLE_GATT_SVC_MAX) {
        return -1;
    }
    i = s_slot_count++;
    memset(&s_slots[i], 0, sizeof(s_slots[i]));
    s_slots[i].kind = KLIN_GD32V_BLE_UUID_KIND_128;
    memcpy(s_slots[i].svc128, svc16, 16);
    memcpy(s_slots[i].chr128, chr16, 16);
    return 0;
}

int klin_gd32v_ble_gatt_clear(void)
{
    if (s_inited) {
        return -1;
    }
    s_slot_count = 0;
    memset(s_slots, 0, sizeof(s_slots));
    return 0;
}

int klin_gd32v_ble_gatt_svc_count(void)
{
    if (s_slot_count == 0 && !s_inited) {
        return 1; /* default will be installed at init */
    }
    return s_slot_count > 0 ? s_slot_count : (s_inited ? s_slot_count : 1);
}

int klin_gd32v_ble_gatt_svc_uuid16(void)
{
    if (s_slot_count < 1) {
        return KLIN_GD32V_BLE_GATT_SVC_UUID16;
    }
    if (s_slots[0].kind != KLIN_GD32V_BLE_UUID_KIND_16) {
        return 0;
    }
    return (int)s_slots[0].svc16;
}

int klin_gd32v_ble_gatt_chr_uuid16(void)
{
    if (s_slot_count < 1) {
        return KLIN_GD32V_BLE_GATT_CHR_UUID16;
    }
    if (s_slots[0].kind != KLIN_GD32V_BLE_UUID_KIND_16) {
        return 0;
    }
    return (int)s_slots[0].chr16;
}

int klin_gd32v_ble_gatt_set_at(int index, const unsigned char *data, int len)
{
    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_slot_count) {
        return -1;
    }
    if (data == NULL || len < 0 || len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
        return -1;
    }
    if (len > 0) {
        memcpy(s_slots[index].value, data, (size_t)len);
    }
    s_slots[index].value_len = len;
    return 0;
}

int klin_gd32v_ble_gatt_set(const unsigned char *data, int len)
{
    return klin_gd32v_ble_gatt_set_at(0, data, len);
}

int klin_gd32v_ble_gatt_get_at(int index, unsigned char *out, int max_len)
{
    int n;

    if (!s_inited) {
        return -1;
    }
    if (index < 0 || index >= s_slot_count || out == NULL || max_len < 0) {
        return -1;
    }
    n = s_slots[index].value_len;
    if (n > max_len) {
        n = max_len;
    }
    if (n > 0) {
        memcpy(out, s_slots[index].value, (size_t)n);
    }
    return n;
}

int klin_gd32v_ble_gatt_get(unsigned char *out, int max_len)
{
    return klin_gd32v_ble_gatt_get_at(0, out, max_len);
}

int klin_gd32v_ble_gatt_len_at(int index)
{
    if (index < 0 || index >= s_slot_count) {
        return 0;
    }
    return s_slots[index].value_len;
}

int klin_gd32v_ble_gatt_len(void)
{
    return klin_gd32v_ble_gatt_len_at(0);
}

int klin_gd32v_ble_gatt_written_at(int index)
{
    int w;

    if (index < 0 || index >= s_slot_count) {
        return 0;
    }
    w = s_slots[index].written;
    s_slots[index].written = 0;
    return w ? 1 : 0;
}

int klin_gd32v_ble_gatt_written(void)
{
    return klin_gd32v_ble_gatt_written_at(0);
}

int klin_gd32v_ble_gattc_select(int index)
{
    if (index < 0 || index >= klin_gd32v_ble_slot_limit()) {
        return -1;
    }
    s_gattc_sel = index;
    s_gattc_override = 0;
    return 0;
}

int klin_gd32v_ble_gattc_uuid16(int svc_uuid16, int chr_uuid16)
{
    if (svc_uuid16 <= 0 || svc_uuid16 > 0xFFFF || chr_uuid16 <= 0 ||
        chr_uuid16 > 0xFFFF) {
        return -1;
    }
    s_gattc_kind = KLIN_GD32V_BLE_UUID_KIND_16;
    s_gattc_svc16 = (uint16_t)svc_uuid16;
    s_gattc_chr16 = (uint16_t)chr_uuid16;
    s_gattc_override = 1;
    return 0;
}

int klin_gd32v_ble_gattc_uuid128(const unsigned char *svc16, const unsigned char *chr16)
{
    if (svc16 == NULL || chr16 == NULL) {
        return -1;
    }
    s_gattc_kind = KLIN_GD32V_BLE_UUID_KIND_128;
    memcpy(s_gattc_svc128, svc16, 16);
    memcpy(s_gattc_chr128, chr16, 16);
    s_gattc_override = 1;
    return 0;
}

int klin_gd32v_ble_scan_count(void)
{
    return s_scan_count;
}

int klin_gd32v_ble_scan_rssi(int index)
{
    if (index < 0 || index >= s_scan_count) {
        return 0;
    }
    return s_scan[index].rssi;
}

int klin_gd32v_ble_scan_addr_type(int index)
{
    if (index < 0 || index >= s_scan_count) {
        return -1;
    }
    return (int)s_scan[index].addr_type;
}

int klin_gd32v_ble_scan_addr(int index, unsigned char *out6)
{
    if (out6 == NULL || index < 0 || index >= s_scan_count) {
        return -1;
    }
    memcpy(out6, s_scan[index].addr, 6);
    return 0;
}

int klin_gd32v_ble_scan_name(int index, unsigned char *out, int max_len)
{
    int n;

    if (out == NULL || max_len < 0 || index < 0 || index >= s_scan_count) {
        return -1;
    }
    n = (int)strlen(s_scan[index].name);
    if (n > max_len) {
        n = max_len;
    }
    if (n > 0) {
        memcpy(out, s_scan[index].name, (size_t)n);
    }
    return n;
}

int klin_gd32v_ble_central_connected(void)
{
    return s_central_connected ? 1 : 0;
}

int klin_gd32v_ble_gattc_notified(void)
{
    int n;

    n = s_gattc_notified;
    s_gattc_notified = 0;
    return n;
}

int klin_gd32v_ble_gattc_get(unsigned char *out, int max_len)
{
    int n;

    if (out == NULL || max_len < 0) {
        return -1;
    }
    n = s_gattc_buf_len;
    if (n > max_len) {
        n = max_len;
    }
    if (n > 0) {
        memcpy(out, s_gattc_buf, (size_t)n);
    }
    return n;
}

int klin_gd32v_ble_gattc_len(void)
{
    return s_gattc_buf_len;
}
