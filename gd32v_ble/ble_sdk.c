/* BLE advertise + GATT + central scan + GATT client for Klin on GD32VW553.
 * Real path: GigaDevice VW55x Wi-Fi BLE SDK (AN152 `ble_init` / `app_adv_*` /
 * `ble_gatts_*` / `ble_scan_*` / `ble_conn_*` / `ble_gattc_*`). Host path:
 * stubs when SDK headers are not on the include path (klin test).
 *
 * Scan table is fixed (max 16) — no glue malloc. Central / GATT client need
 * an SDK image with observer/central + GATT client (e.g. msdk_ffd).
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
#include "ble_gatt.h"
#include "ble_gatts.h"
#include "ble_gattc.h"
#include "ble_scan.h"
#include "ble_conn.h"
#include "ble_error.h"
#include "ble_types.h"
#if defined(__has_include)
#if __has_include("wrapper_os.h")
#include "wrapper_os.h"
#define KLIN_GD32V_BLE_HAVE_OSAL 1
#endif
#endif
#endif

typedef struct {
    unsigned char addr[6];
    unsigned char addr_type;
    int rssi;
    char name[KLIN_GD32V_BLE_SCAN_NAME_MAX];
} klin_gd32v_ble_scan_row_t;

static int s_inited;
static int s_advertising;
static int s_connected;
static int s_scanning;
static int s_central_connected;
static int s_central_conn_idx;

static unsigned char s_gatt_value[KLIN_GD32V_BLE_GATT_VALUE_MAX];
static int s_gatt_len;
static int s_gatt_written;

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

#ifdef KLIN_GD32V_BLE_HAVE_SDK

enum {
    KLIN_GATT_IDX_SVC = 0,
    KLIN_GATT_IDX_CHAR,
    KLIN_GATT_IDX_VAL,
    KLIN_GATT_IDX_CCCD,
    KLIN_GATT_IDX_NB
};

static uint8_t s_svc_id;
static uint8_t s_conn_idx;
static uint16_t s_cccd;
static int s_gatt_added;
static int s_scan_cb_reg;
static int s_conn_cb_reg;
static int s_gattc_svc_reg;

static const uint8_t s_svc_uuid[2] = UUID_16BIT_TO_ARRAY(KLIN_GD32V_BLE_GATT_SVC_UUID16);

static const ble_gatt_attr_desc_t s_gatt_db[KLIN_GATT_IDX_NB] = {
    [KLIN_GATT_IDX_SVC] = {UUID_16BIT_TO_ARRAY(BLE_GATT_DECL_PRIMARY_SERVICE), PROP(RD), 0},
    [KLIN_GATT_IDX_CHAR] = {UUID_16BIT_TO_ARRAY(BLE_GATT_DECL_CHARACTERISTIC), PROP(RD), 0},
    [KLIN_GATT_IDX_VAL] = {UUID_16BIT_TO_ARRAY(KLIN_GD32V_BLE_GATT_CHR_UUID16),
                           PROP(RD) | PROP(WR) | PROP(NTF),
                           OPT(NO_OFFSET) | KLIN_GD32V_BLE_GATT_VALUE_MAX},
    [KLIN_GATT_IDX_CCCD] = {UUID_16BIT_TO_ARRAY(BLE_GATT_DESC_CLIENT_CHAR_CFG),
                            PROP(RD) | PROP(WR), OPT(NO_OFFSET)},
};

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
    svc.ble_uuid.type = BLE_UUID_TYPE_16;
    svc.ble_uuid.data.uuid_16 = (uint16_t)KLIN_GD32V_BLE_GATT_SVC_UUID16;
    chr.instance_id = 0;
    chr.ble_uuid.type = BLE_UUID_TYPE_16;
    chr.ble_uuid.data.uuid_16 = (uint16_t)KLIN_GD32V_BLE_GATT_CHR_UUID16;

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
    int n;

    if (info == NULL) {
        return BLE_ERR_NO_ERROR;
    }

    if (info->srv_msg_type == BLE_SRV_EVT_CONN_STATE_CHANGE_IND) {
        ind = &info->msg_data.conn_state_change_ind;
        if (ind->conn_state == BLE_CONN_STATE_CONNECTED) {
            s_connected = 1;
            s_advertising = 0;
            s_conn_idx = ind->info.conn_info.conn_idx;
            s_cccd = 0;
        } else if (ind->conn_state == BLE_CONN_STATE_DISCONNECTD) {
            s_connected = 0;
            s_cccd = 0;
        }
        return BLE_ERR_NO_ERROR;
    }

    if (info->srv_msg_type != BLE_SRV_EVT_GATT_OPERATION) {
        return BLE_ERR_NO_ERROR;
    }

    op = &info->msg_data.gatts_op_info;
    if (op->gatts_op_sub_evt == BLE_SRV_EVT_READ_REQ) {
        rd = &op->gatts_op_data.read_req;
        if (rd->att_idx == KLIN_GATT_IDX_VAL) {
            if (rd->offset > (uint16_t)s_gatt_len) {
                rd->val_len = 0;
                rd->att_len = (uint16_t)s_gatt_len;
                return BLE_ERR_NO_ERROR;
            }
            n = s_gatt_len - (int)rd->offset;
            if (n > (int)rd->max_len) {
                n = (int)rd->max_len;
            }
            rd->val_len = (uint16_t)n;
            rd->att_len = (uint16_t)s_gatt_len;
            if (rd->p_val != NULL && n > 0) {
                memcpy(rd->p_val, s_gatt_value + rd->offset, (size_t)n);
            }
        } else if (rd->att_idx == KLIN_GATT_IDX_CCCD) {
            rd->val_len = BLE_GATT_CCCD_LEN;
            rd->att_len = BLE_GATT_CCCD_LEN;
            if (rd->p_val != NULL) {
                memcpy(rd->p_val, &s_cccd, BLE_GATT_CCCD_LEN);
            }
        }
        return BLE_ERR_NO_ERROR;
    }

    if (op->gatts_op_sub_evt == BLE_SRV_EVT_WRITE_REQ) {
        wr = &op->gatts_op_data.write_req;
        if (wr->att_idx == KLIN_GATT_IDX_VAL) {
            if (wr->p_val == NULL || wr->val_len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
                return BLE_ERR_NO_ERROR;
            }
            memcpy(s_gatt_value, wr->p_val, wr->val_len);
            s_gatt_len = (int)wr->val_len;
            s_gatt_written = 1;
        } else if (wr->att_idx == KLIN_GATT_IDX_CCCD) {
            if (wr->p_val != NULL && wr->val_len == BLE_GATT_CCCD_LEN) {
                memcpy(&s_cccd, wr->p_val, BLE_GATT_CCCD_LEN);
            }
        }
    }

    return BLE_ERR_NO_ERROR;
}

static void klin_gd32v_ble_gatt_reset(void)
{
    s_gatt_len = 0;
    s_gatt_written = 0;
    s_cccd = 0;
    memset(s_gatt_value, 0, sizeof(s_gatt_value));
}

int klin_gd32v_ble_init(void)
{
    ble_uuid_t svc_uuid;

    if (s_inited) {
        return 0;
    }
    ble_init(1);
    if (ble_wait_ready() != 0) {
        return -1;
    }
    if (ble_gatts_svc_add(&s_svc_id, s_svc_uuid, 0, SVC_UUID(16), s_gatt_db,
                          KLIN_GATT_IDX_NB, klin_gd32v_ble_gatt_cb) != BLE_ERR_NO_ERROR) {
        ble_deinit();
        return -1;
    }
    s_gatt_added = 1;
    memset(&svc_uuid, 0, sizeof(svc_uuid));
    svc_uuid.type = BLE_UUID_TYPE_16;
    svc_uuid.data.uuid_16 = (uint16_t)KLIN_GD32V_BLE_GATT_SVC_UUID16;
    if (ble_gattc_svc_reg(&svc_uuid, klin_gd32v_ble_gattc_cb) == BLE_ERR_NO_ERROR) {
        s_gattc_svc_reg = 1;
    }
    if (ble_scan_callback_register(klin_gd32v_ble_scan_cb) == BLE_ERR_NO_ERROR) {
        s_scan_cb_reg = 1;
    }
    if (ble_conn_callback_register(klin_gd32v_ble_conn_cb) == BLE_ERR_NO_ERROR) {
        s_conn_cb_reg = 1;
    }
    s_inited = 1;
    klin_gd32v_ble_gatt_reset();
    klin_gd32v_ble_gattc_reset();
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
    ble_uuid_t svc_uuid;

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
    if (s_gattc_svc_reg) {
        memset(&svc_uuid, 0, sizeof(svc_uuid));
        svc_uuid.type = BLE_UUID_TYPE_16;
        svc_uuid.data.uuid_16 = (uint16_t)KLIN_GD32V_BLE_GATT_SVC_UUID16;
        (void)ble_gattc_svc_unreg(&svc_uuid);
        s_gattc_svc_reg = 0;
    }
    if (s_gatt_added) {
        (void)ble_gatts_svc_rmv(s_svc_id);
        s_gatt_added = 0;
    }
    s_connected = 0;
    s_central_connected = 0;
    s_inited = 0;
    klin_gd32v_ble_gatt_reset();
    klin_gd32v_ble_gattc_reset();
    klin_gd32v_ble_scan_clear();
    ble_deinit();
    return 0;
}

int klin_gd32v_ble_gatt_notify(void)
{
    if (!s_inited || !s_connected || (s_cccd & BLE_GATT_CCCD_NTF_BIT) == 0) {
        return 0;
    }
    if (ble_gatts_ntf_ind_send(s_conn_idx, s_svc_id, KLIN_GATT_IDX_VAL, s_gatt_value,
                               (uint16_t)s_gatt_len, BLE_GATT_NOTIFY) != BLE_ERR_NO_ERROR) {
        return -1;
    }
    return 0;
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
    if (ble_scan_param_set(BLE_GAP_LOCAL_ADDR_STATIC, &param) != BLE_ERR_NO_ERROR) {
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
    if (ble_conn_connect(NULL, BLE_GAP_LOCAL_ADDR_STATIC, &peer, 0) != BLE_ERR_NO_ERROR) {
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

#else /* host stubs — no SDK headers */

int klin_gd32v_ble_init(void)
{
    s_inited = 1;
    s_gatt_len = 0;
    s_gatt_written = 0;
    s_scanning = 0;
    s_central_connected = 0;
    s_scan_count = 0;
    memset(s_gatt_value, 0, sizeof(s_gatt_value));
    memset(s_scan, 0, sizeof(s_scan));
    klin_gd32v_ble_gattc_reset();
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
    s_gatt_len = 0;
    s_gatt_written = 0;
    s_scan_count = 0;
    memset(s_gatt_value, 0, sizeof(s_gatt_value));
    memset(s_scan, 0, sizeof(s_scan));
    klin_gd32v_ble_gattc_reset();
    return 0;
}

int klin_gd32v_ble_gatt_notify(void)
{
    return 0;
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

#endif

int klin_gd32v_ble_gatt_set(const unsigned char *data, int len)
{
    if (!s_inited || data == NULL || len < 0 || len > KLIN_GD32V_BLE_GATT_VALUE_MAX) {
        return -1;
    }
    if (len > 0) {
        memcpy(s_gatt_value, data, (size_t)len);
    }
    s_gatt_len = len;
    return 0;
}

int klin_gd32v_ble_gatt_get(unsigned char *out, int max_len)
{
    int n;

    if (!s_inited || out == NULL || max_len < 0) {
        return -1;
    }
    n = s_gatt_len;
    if (n > max_len) {
        n = max_len;
    }
    if (n > 0) {
        memcpy(out, s_gatt_value, (size_t)n);
    }
    return n;
}

int klin_gd32v_ble_gatt_len(void)
{
    return s_gatt_len;
}

int klin_gd32v_ble_gatt_written(void)
{
    int w;

    w = s_gatt_written;
    s_gatt_written = 0;
    return w;
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
