/* BLE advertise + GATT MVP for Klin apps on GD32VW553.
 * Real path: GigaDevice VW55x Wi-Fi BLE SDK (`ble_init` / `app_adv_*` /
 * `ble_gatts_svc_add`, AN152). Host path: stubs when SDK headers are not
 * on the include path (klin test).
 */
#include "ble_sdk.h"

#include <string.h>

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
#if defined(__has_include)
#if __has_include("wrapper_os.h")
#include "wrapper_os.h"
#define KLIN_GD32V_BLE_HAVE_OSAL 1
#endif
#endif
#endif

static int s_inited;
static int s_advertising;
static int s_connected;

static unsigned char s_gatt_value[KLIN_GD32V_BLE_GATT_VALUE_MAX];
static int s_gatt_len;
static int s_gatt_written;

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
    s_inited = 1;
    klin_gd32v_ble_gatt_reset();
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
#ifdef KLIN_GD32V_BLE_HAVE_OSAL
        sys_ms_sleep(1);
#endif
        waited = waited + 1;
    }
}

int klin_gd32v_ble_stop(void)
{
    (void)klin_gd32v_ble_stop_advertise();
    if (s_gatt_added) {
        (void)ble_gatts_svc_rmv(s_svc_id);
        s_gatt_added = 0;
    }
    s_connected = 0;
    s_inited = 0;
    klin_gd32v_ble_gatt_reset();
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

#else /* host stubs — no SDK headers */

int klin_gd32v_ble_init(void)
{
    s_inited = 1;
    s_gatt_len = 0;
    s_gatt_written = 0;
    memset(s_gatt_value, 0, sizeof(s_gatt_value));
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
    s_inited = 0;
    s_gatt_len = 0;
    s_gatt_written = 0;
    memset(s_gatt_value, 0, sizeof(s_gatt_value));
    return 0;
}

int klin_gd32v_ble_gatt_notify(void)
{
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
