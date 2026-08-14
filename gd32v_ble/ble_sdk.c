/* BLE advertise bring-up for Klin apps on GD32VW553.
 * Real path: GigaDevice VW55x Wi-Fi BLE SDK (`ble_init` / `app_adv_*`, AN152).
 * Host path: stubs when SDK headers are not on the include path (klin test).
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

#ifdef KLIN_GD32V_BLE_HAVE_SDK

int klin_gd32v_ble_init(void)
{
    if (s_inited) {
        return 0;
    }
    ble_init(1);
    if (ble_wait_ready() != 0) {
        return -1;
    }
    s_inited = 1;
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
    s_connected = 0;
    s_inited = 0;
    ble_deinit();
    return 0;
}

#else /* host stubs — no SDK headers */

int klin_gd32v_ble_init(void)
{
    s_inited = 1;
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
    return 0;
}

#endif
